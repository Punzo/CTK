/*=========================================================================

  Library:   CTK

  Copyright (c) Kitware Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Davide Punzo, punzodavide@hotmail.it,
  and development was supported by the Center for Intelligent Image-guided Interventions (CI3).

=========================================================================*/

// Qt includes
#include <QDebug>
#include <QPixmap>
#include <QTimer>
#include <QThread>
#include <QPainter>
#include <QFile>
#include <QMultiMap>

// CTK includes
#include <ctkLogger.h>

// ctkDICOMCore includes
#include "ctkDICOMSeriesModel.h"
#include "ctkDICOMDatabase.h"
#include "ctkDICOMScheduler.h"
#include "ctkDICOMJobResponseSet.h"
#include "ctkDICOMThumbnailGenerator.h"

static ctkLogger logger("org.commontk.DICOM.Core.DICOMSeriesModel");

//----------------------------------------------------------------------------
static void skipDelete(QObject* obj)
{
  Q_UNUSED(obj);
  // this deleter does not delete the object from memory
  // useful if the pointer is not owned by the smart pointer
}

//----------------------------------------------------------------------------
class ctkDICOMSeriesModelPrivate
{
  Q_DECLARE_PUBLIC(ctkDICOMSeriesModel);

protected:
  ctkDICOMSeriesModel* const q_ptr;

public:
  ctkDICOMSeriesModelPrivate(ctkDICOMSeriesModel& obj);
  ~ctkDICOMSeriesModelPrivate();

  void updateSeriesData();
  void loadSeriesForStudy();
  void generateThumbnailForSeries(const QString& seriesInstanceUID);
  QPixmap createModalityThumbnail(const QString& modality, int size);
  int findSeriesRow(const QString& seriesInstanceUID) const;
  bool matchesFilters(const QString& seriesItem) const;

  // Data storage
  struct SeriesInfo {
    QString seriesItem;              // Database row ID
    QString seriesInstanceUID;
    QString studyInstanceUID;
    QString patientID;
    QString seriesNumber;
    QString modality;
    QString seriesDescription;
    int instanceCount;
    int instancesLoaded;
    bool isCloud;
    bool isLoaded;
    bool isVisible;
    bool retrieveFailed;
    bool isSelected;
    int operationProgress;
    QString operationStatus;
    QString jobUID;
    QPixmap thumbnail;
    QString thumbnailPath;
    bool thumbnailGenerated;
  };

  QList<SeriesInfo> SeriesList;
  QStringList SelectedSeriesUIDs;

  // Database and scheduler
  QSharedPointer<ctkDICOMDatabase> DicomDatabase;
  QSharedPointer<ctkDICOMScheduler> Scheduler;

  // Filters
  QString StudyFilter;
  QStringList ModalityFilter;
  QString DescriptionFilter;
  QStringList AllowedServers;

  // Settings
  int ThumbnailSize;
  bool IsUpdating;

  // Async thumbnail generation
  QTimer* ThumbnailTimer;
  QStringList PendingThumbnails;
  
  void processPendingThumbnails();
};

//----------------------------------------------------------------------------
// ctkDICOMSeriesModelPrivate methods

//----------------------------------------------------------------------------
ctkDICOMSeriesModelPrivate::ctkDICOMSeriesModelPrivate(ctkDICOMSeriesModel& obj)
  : q_ptr(&obj)
{
  this->ThumbnailSize = 200;
  this->IsUpdating = false;
  this->DicomDatabase = nullptr;
  this->Scheduler = nullptr;

  // Setup thumbnail generation timer
  this->ThumbnailTimer = new QTimer();
  this->ThumbnailTimer->setSingleShot(true);
  this->ThumbnailTimer->setInterval(100); // Small delay to batch requests
}

//----------------------------------------------------------------------------
ctkDICOMSeriesModelPrivate::~ctkDICOMSeriesModelPrivate()
{
  if (this->ThumbnailTimer)
  {
    this->ThumbnailTimer->deleteLater();
  }
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModelPrivate::updateSeriesData()
{
  Q_Q(ctkDICOMSeriesModel);
  
  if (this->IsUpdating || !this->DicomDatabase || this->StudyFilter.isEmpty())
  {
    return;
  }

  this->IsUpdating = true;

  q->beginResetModel();
  this->SeriesList.clear();
  this->loadSeriesForStudy();
  q->endResetModel();

  this->IsUpdating = false;
  
  emit q->modelReady();
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModelPrivate::loadSeriesForStudy()
{
  if (!this->DicomDatabase)
  {
    return;
  }

  QStringList seriesList = this->DicomDatabase->seriesForStudy(this->StudyFilter);
  
  // Sort by series number
  QMultiMap<int, QString> sortedSeries;
  foreach (const QString& seriesItem, seriesList)
  {
    if (!this->matchesFilters(seriesItem))
    {
      continue;
    }

    QString seriesNumberStr = this->DicomDatabase->fieldForSeries("SeriesNumber", seriesItem);
    int seriesNumber = seriesNumberStr.toInt();
    sortedSeries.insert(seriesNumber, seriesItem);
  }

  // Load series data
  foreach (const QString& seriesItem, sortedSeries.values())
  {
    SeriesInfo info;
    info.seriesItem = seriesItem;
    info.seriesInstanceUID = this->DicomDatabase->fieldForSeries("SeriesInstanceUID", seriesItem);
    info.studyInstanceUID = this->StudyFilter;
    info.patientID = this->DicomDatabase->patientForStudy(this->StudyFilter);
    info.seriesNumber = this->DicomDatabase->fieldForSeries("SeriesNumber", seriesItem);
    info.modality = this->DicomDatabase->fieldForSeries("Modality", seriesItem);
    info.seriesDescription = this->DicomDatabase->fieldForSeries("SeriesDescription", seriesItem);
    
    if (info.seriesDescription.isEmpty())
    {
      info.seriesDescription = "UNDEFINED";
    }

    // Get instance counts
    QStringList instances = this->DicomDatabase->instancesForSeries(info.seriesInstanceUID);
    info.instanceCount = instances.count();
    
    QStringList files = this->DicomDatabase->filesForSeries(info.seriesInstanceUID);
    files.removeAll(QString(""));
    info.instancesLoaded = files.count();
    
    QStringList urls = this->DicomDatabase->urlsForSeries(info.seriesInstanceUID);
    urls.removeAll(QString(""));
    
    // Determine cloud status
    info.isCloud = (info.instanceCount > 0 && urls.count() > 0 && info.instancesLoaded < info.instanceCount);
    info.isLoaded = (info.instancesLoaded == info.instanceCount && info.instanceCount > 0);
    info.isVisible = this->DicomDatabase->visibleSeries().contains(info.seriesInstanceUID);
    
    // Initialize other fields
    info.retrieveFailed = false;
    info.isSelected = false;
    info.operationProgress = 0;
    info.operationStatus = "Ready";
    info.thumbnailGenerated = false;
    
    // Generate default thumbnail
    info.thumbnail = this->createModalityThumbnail(info.modality, this->ThumbnailSize);
    
    this->SeriesList.append(info);
  }
}

//----------------------------------------------------------------------------
bool ctkDICOMSeriesModelPrivate::matchesFilters(const QString& seriesItem) const
{
  if (!this->DicomDatabase)
  {
    return false;
  }

  // Check modality filter
  if (!this->ModalityFilter.isEmpty())
  {
    QString modality = this->DicomDatabase->fieldForSeries("Modality", seriesItem);
    if (!this->ModalityFilter.contains(modality))
    {
      return false;
    }
  }

  // Check description filter
  if (!this->DescriptionFilter.isEmpty())
  {
    QString description = this->DicomDatabase->fieldForSeries("SeriesDescription", seriesItem);
    if (!description.contains(this->DescriptionFilter, Qt::CaseInsensitive))
    {
      return false;
    }
  }

  return true;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModelPrivate::generateThumbnailForSeries(const QString& seriesInstanceUID)
{
  Q_Q(ctkDICOMSeriesModel);
  
  if (!this->DicomDatabase || seriesInstanceUID.isEmpty())
  {
    return;
  }

  int row = this->findSeriesRow(seriesInstanceUID);
  if (row < 0 || row >= this->SeriesList.size())
  {
    return;
  }

  SeriesInfo& info = this->SeriesList[row];
  if (info.thumbnailGenerated)
  {
    return; // Already generated
  }

  // Check if we have a cached thumbnail
  QString thumbnailPath = this->DicomDatabase->thumbnailPathForInstance(
    info.studyInstanceUID, info.seriesInstanceUID, "");
  
  if (!thumbnailPath.isEmpty() && QFile::exists(thumbnailPath))
  {
    QPixmap thumbnail(thumbnailPath);
    if (!thumbnail.isNull())
    {
      info.thumbnail = thumbnail.scaled(this->ThumbnailSize, this->ThumbnailSize, 
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation);
      info.thumbnailPath = thumbnailPath;
      info.thumbnailGenerated = true;
      
      QModelIndex index = q->createIndex(row, 0);
      emit q->dataChanged(index, index, {ctkDICOMSeriesModel::ThumbnailRole});
      emit q->thumbnailReady(index);
      return;
    }
  }

  // Need to generate thumbnail - add to pending list
  if (!this->PendingThumbnails.contains(seriesInstanceUID))
  {
    this->PendingThumbnails.append(seriesInstanceUID);
    
    // Start thumbnail generation timer
    if (!this->ThumbnailTimer->isActive())
    {
      QObject::connect(this->ThumbnailTimer, &QTimer::timeout, [this]() {
        this->processPendingThumbnails();
      });
      this->ThumbnailTimer->start();
    }
  }
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModelPrivate::processPendingThumbnails()
{
  // Process one thumbnail at a time to avoid overwhelming the system
  if (this->PendingThumbnails.isEmpty())
  {
    return;
  }

  QString seriesInstanceUID = this->PendingThumbnails.takeFirst();
  
  // TODO: Implement actual thumbnail generation using ctkDICOMThumbnailGenerator
  // For now, we'll use the modality thumbnail
  
  if (!this->PendingThumbnails.isEmpty())
  {
    this->ThumbnailTimer->start(); // Continue processing
  }
}

//----------------------------------------------------------------------------
QPixmap ctkDICOMSeriesModelPrivate::createModalityThumbnail(const QString& modality, int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(Qt::lightGray);
  
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  
  // Draw modality text
  QFont font = painter.font();
  font.setBold(true);
  font.setPixelSize(size / 8);
  painter.setFont(font);
  painter.setPen(Qt::black);
  painter.drawText(pixmap.rect(), Qt::AlignCenter, modality);
  
  return pixmap;
}

//----------------------------------------------------------------------------
int ctkDICOMSeriesModelPrivate::findSeriesRow(const QString& seriesInstanceUID) const
{
  for (int i = 0; i < this->SeriesList.size(); ++i)
  {
    if (this->SeriesList[i].seriesInstanceUID == seriesInstanceUID)
    {
      return i;
    }
  }
  return -1;
}

//----------------------------------------------------------------------------
// ctkDICOMSeriesModel methods

//----------------------------------------------------------------------------
ctkDICOMSeriesModel::ctkDICOMSeriesModel(QObject* parent)
  : Superclass(parent)
  , d_ptr(new ctkDICOMSeriesModelPrivate(*this))
{
}

//----------------------------------------------------------------------------
ctkDICOMSeriesModel::~ctkDICOMSeriesModel()
{
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::setDicomDatabase(ctkDICOMDatabase* database)
{
  Q_D(ctkDICOMSeriesModel);
  d->DicomDatabase = QSharedPointer<ctkDICOMDatabase>(database, skipDelete);
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::setDicomDatabase(QSharedPointer<ctkDICOMDatabase> database)
{
  Q_D(ctkDICOMSeriesModel);
  d->DicomDatabase = database;
}

//----------------------------------------------------------------------------
ctkDICOMDatabase* ctkDICOMSeriesModel::dicomDatabase() const
{
  Q_D(const ctkDICOMSeriesModel);
  return d->DicomDatabase.data();
}

//----------------------------------------------------------------------------
QSharedPointer<ctkDICOMDatabase> ctkDICOMSeriesModel::dicomDatabaseShared() const
{
  Q_D(const ctkDICOMSeriesModel);
  return d->DicomDatabase;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::setScheduler(ctkDICOMScheduler* scheduler)
{
  Q_D(ctkDICOMSeriesModel);
  d->Scheduler = QSharedPointer<ctkDICOMScheduler>(scheduler, skipDelete);
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::setScheduler(QSharedPointer<ctkDICOMScheduler> scheduler)
{
  Q_D(ctkDICOMSeriesModel);
  d->Scheduler = scheduler;
}

//----------------------------------------------------------------------------
ctkDICOMScheduler* ctkDICOMSeriesModel::scheduler() const
{
  Q_D(const ctkDICOMSeriesModel);
  return d->Scheduler.data();
}

//----------------------------------------------------------------------------
QSharedPointer<ctkDICOMScheduler> ctkDICOMSeriesModel::schedulerShared() const
{
  Q_D(const ctkDICOMSeriesModel);
  return d->Scheduler;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::setStudyFilter(const QString& studyInstanceUID)
{
  Q_D(ctkDICOMSeriesModel);
  if (d->StudyFilter != studyInstanceUID)
  {
    d->StudyFilter = studyInstanceUID;
    d->updateSeriesData();
    emit studyFilterChanged(studyInstanceUID);
  }
}

//----------------------------------------------------------------------------
QString ctkDICOMSeriesModel::studyFilter() const
{
  Q_D(const ctkDICOMSeriesModel);
  return d->StudyFilter;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::setModalityFilter(const QStringList& modalities)
{
  Q_D(ctkDICOMSeriesModel);
  if (d->ModalityFilter != modalities)
  {
    d->ModalityFilter = modalities;
    d->updateSeriesData();
    emit modalityFilterChanged(modalities);
  }
}

//----------------------------------------------------------------------------
QStringList ctkDICOMSeriesModel::modalityFilter() const
{
  Q_D(const ctkDICOMSeriesModel);
  return d->ModalityFilter;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::setDescriptionFilter(const QString& description)
{
  Q_D(ctkDICOMSeriesModel);
  if (d->DescriptionFilter != description)
  {
    d->DescriptionFilter = description;
    d->updateSeriesData();
    emit descriptionFilterChanged(description);
  }
}

//----------------------------------------------------------------------------
QString ctkDICOMSeriesModel::descriptionFilter() const
{
  Q_D(const ctkDICOMSeriesModel);
  return d->DescriptionFilter;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::setThumbnailSize(int size)
{
  Q_D(ctkDICOMSeriesModel);
  if (d->ThumbnailSize != size)
  {
    d->ThumbnailSize = size;
    
    // Regenerate all thumbnails
    for (int i = 0; i < d->SeriesList.size(); ++i)
    {
      d->SeriesList[i].thumbnailGenerated = false;
      d->SeriesList[i].thumbnail = d->createModalityThumbnail(d->SeriesList[i].modality, size);
    }
    
    emit dataChanged(createIndex(0, 0), createIndex(d->SeriesList.size() - 1, 0), 
                    {ThumbnailRole});
    emit thumbnailSizeChanged(size);
  }
}

//----------------------------------------------------------------------------
int ctkDICOMSeriesModel::thumbnailSize() const
{
  Q_D(const ctkDICOMSeriesModel);
  return d->ThumbnailSize;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::setAllowedServers(const QStringList& servers)
{
  Q_D(ctkDICOMSeriesModel);
  d->AllowedServers = servers;
}

//----------------------------------------------------------------------------
QStringList ctkDICOMSeriesModel::allowedServers() const
{
  Q_D(const ctkDICOMSeriesModel);
  return d->AllowedServers;
}

//----------------------------------------------------------------------------
QString ctkDICOMSeriesModel::seriesInstanceUID(const QModelIndex& index) const
{
  Q_D(const ctkDICOMSeriesModel);
  if (!index.isValid() || index.row() >= d->SeriesList.size())
  {
    return QString();
  }
  return d->SeriesList[index.row()].seriesInstanceUID;
}

//----------------------------------------------------------------------------
QString ctkDICOMSeriesModel::seriesItem(const QModelIndex& index) const
{
  Q_D(const ctkDICOMSeriesModel);
  if (!index.isValid() || index.row() >= d->SeriesList.size())
  {
    return QString();
  }
  return d->SeriesList[index.row()].seriesItem;
}

//----------------------------------------------------------------------------
QModelIndex ctkDICOMSeriesModel::indexForSeriesInstanceUID(const QString& seriesInstanceUID) const
{
  Q_D(const ctkDICOMSeriesModel);
  int row = d->findSeriesRow(seriesInstanceUID);
  if (row >= 0)
  {
    return createIndex(row, 0);
  }
  return QModelIndex();
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::refresh()
{
  Q_D(ctkDICOMSeriesModel);
  d->updateSeriesData();
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::generateThumbnails()
{
  Q_D(ctkDICOMSeriesModel);
  for (const auto& info : d->SeriesList)
  {
    d->generateThumbnailForSeries(info.seriesInstanceUID);
  }
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::generateThumbnail(const QModelIndex& index)
{
  Q_D(ctkDICOMSeriesModel);
  if (!index.isValid() || index.row() >= d->SeriesList.size())
  {
    return;
  }
  
  const auto& info = d->SeriesList[index.row()];
  d->generateThumbnailForSeries(info.seriesInstanceUID);
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::generateInstances(const QModelIndex& index, bool query, bool retrieve)
{
  Q_D(ctkDICOMSeriesModel);
  if (!index.isValid() || index.row() >= d->SeriesList.size() || !d->Scheduler)
  {
    return;
  }
  
  const auto& info = d->SeriesList[index.row()];
  
  if (query && info.instanceCount == 0)
  {
    d->Scheduler->queryInstances(info.patientID, info.studyInstanceUID, info.seriesInstanceUID,
                                QThread::NormalPriority, d->AllowedServers);
  }
  
  if (retrieve && !info.isLoaded)
  {
    d->Scheduler->retrieveSeries(info.patientID, info.studyInstanceUID, info.seriesInstanceUID,
                                QThread::NormalPriority, d->AllowedServers);
  }
}

//----------------------------------------------------------------------------
int ctkDICOMSeriesModel::rowCount(const QModelIndex& parent) const
{
  Q_D(const ctkDICOMSeriesModel);
  Q_UNUSED(parent);
  return d->SeriesList.size();
}

//----------------------------------------------------------------------------
QVariant ctkDICOMSeriesModel::data(const QModelIndex& index, int role) const
{
  Q_D(const ctkDICOMSeriesModel);
  
  if (!index.isValid() || index.row() >= d->SeriesList.size())
  {
    return QVariant();
  }
  
  const auto& info = d->SeriesList[index.row()];
  
  switch (role)
  {
    case Qt::DisplayRole:
      return QString("Series %1: %2").arg(info.seriesNumber).arg(info.seriesDescription);
      
    case Qt::ToolTipRole:
      return QString("Series: %1\nModality: %2\nDescription: %3\nInstances: %4")
        .arg(info.seriesNumber).arg(info.modality).arg(info.seriesDescription).arg(info.instanceCount);
        
    case SeriesInstanceUIDRole:
      return info.seriesInstanceUID;
      
    case SeriesItemRole:
      return info.seriesItem;
      
    case SeriesNumberRole:
      return info.seriesNumber;
      
    case ModalityRole:
      return info.modality;
      
    case SeriesDescriptionRole:
      return info.seriesDescription;
      
    case InstanceCountRole:
      return info.instanceCount;
      
    case InstancesLoadedRole:
      return info.instancesLoaded;
      
    case ThumbnailRole:
      // Trigger thumbnail generation if not done yet
      const_cast<ctkDICOMSeriesModelPrivate*>(d)->generateThumbnailForSeries(info.seriesInstanceUID);
      return info.thumbnail;
      
    case ThumbnailPathRole:
      return info.thumbnailPath;
      
    case IsCloudRole:
      return info.isCloud;
      
    case IsLoadedRole:
      return info.isLoaded;
      
    case IsVisibleRole:
      return info.isVisible;
      
    case RetrieveFailedRole:
      return info.retrieveFailed;
      
    case OperationProgressRole:
      return info.operationProgress;
      
    case OperationStatusRole:
      return info.operationStatus;
      
    case IsSelectedRole:
      return info.isSelected;
      
    case JobUIDRole:
      return info.jobUID;
      
    case PatientIDRole:
      return info.patientID;
      
    case StudyInstanceUIDRole:
      return info.studyInstanceUID;
      
    default:
      return QVariant();
  }
}

//----------------------------------------------------------------------------
bool ctkDICOMSeriesModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  Q_D(ctkDICOMSeriesModel);
  
  if (!index.isValid() || index.row() >= d->SeriesList.size())
  {
    return false;
  }
  
  auto& info = d->SeriesList[index.row()];
  bool changed = false;
  
  switch (role)
  {
    case IsSelectedRole:
      if (info.isSelected != value.toBool())
      {
        info.isSelected = value.toBool();
        changed = true;
        
        // Update selection list
        if (info.isSelected)
        {
          if (!d->SelectedSeriesUIDs.contains(info.seriesInstanceUID))
          {
            d->SelectedSeriesUIDs.append(info.seriesInstanceUID);
          }
        }
        else
        {
          d->SelectedSeriesUIDs.removeAll(info.seriesInstanceUID);
        }
        
        emit seriesSelectionChanged(d->SelectedSeriesUIDs);
      }
      break;
      
    case OperationProgressRole:
      if (info.operationProgress != value.toInt())
      {
        info.operationProgress = value.toInt();
        changed = true;
        emit operationProgressChanged(index, info.operationProgress);
      }
      break;
      
    case OperationStatusRole:
      if (info.operationStatus != value.toString())
      {
        info.operationStatus = value.toString();
        changed = true;
      }
      break;
      
    case RetrieveFailedRole:
      if (info.retrieveFailed != value.toBool())
      {
        info.retrieveFailed = value.toBool();
        changed = true;
      }
      break;
      
    case JobUIDRole:
      if (info.jobUID != value.toString())
      {
        info.jobUID = value.toString();
        changed = true;
      }
      break;
  }
  
  if (changed)
  {
    emit dataChanged(index, index, {role});
  }
  
  return changed;
}

//----------------------------------------------------------------------------
Qt::ItemFlags ctkDICOMSeriesModel::flags(const QModelIndex& index) const
{
  if (!index.isValid())
  {
    return Qt::NoItemFlags;
  }
  
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

//----------------------------------------------------------------------------
QHash<int, QByteArray> ctkDICOMSeriesModel::roleNames() const
{
  QHash<int, QByteArray> roles = Superclass::roleNames();
  roles[SeriesInstanceUIDRole] = "seriesInstanceUID";
  roles[SeriesItemRole] = "seriesItem";
  roles[SeriesNumberRole] = "seriesNumber";
  roles[ModalityRole] = "modality";
  roles[SeriesDescriptionRole] = "seriesDescription";
  roles[InstanceCountRole] = "instanceCount";
  roles[InstancesLoadedRole] = "instancesLoaded";
  roles[ThumbnailRole] = "thumbnail";
  roles[ThumbnailPathRole] = "thumbnailPath";
  roles[IsCloudRole] = "isCloud";
  roles[IsLoadedRole] = "isLoaded";
  roles[IsVisibleRole] = "isVisible";
  roles[RetrieveFailedRole] = "retrieveFailed";
  roles[OperationProgressRole] = "operationProgress";
  roles[OperationStatusRole] = "operationStatus";
  roles[IsSelectedRole] = "isSelected";
  roles[JobUIDRole] = "jobUID";
  roles[PatientIDRole] = "patientID";
  roles[StudyInstanceUIDRole] = "studyInstanceUID";
  return roles;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::onJobStarted(const QVariant& data)
{
  Q_UNUSED(data);
  // TODO: Implement job tracking
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::onJobFinished(const QVariant& data)
{
  Q_UNUSED(data);
  // TODO: Implement job tracking
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::onJobFailed(const QVariant& data)
{
  Q_UNUSED(data);
  // TODO: Implement job tracking
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::onJobUserStopped(const QVariant& data)
{
  Q_UNUSED(data);
  // TODO: Implement job tracking
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesModel::onThumbnailGenerated(const QString& seriesInstanceUID, const QPixmap& thumbnail)
{
  Q_D(ctkDICOMSeriesModel);
  
  int row = d->findSeriesRow(seriesInstanceUID);
  if (row >= 0)
  {
    auto& info = d->SeriesList[row];
    info.thumbnail = thumbnail;
    info.thumbnailGenerated = true;
    
    QModelIndex index = createIndex(row, 0);
    emit dataChanged(index, index, {ThumbnailRole});
    emit thumbnailReady(index);
  }
}