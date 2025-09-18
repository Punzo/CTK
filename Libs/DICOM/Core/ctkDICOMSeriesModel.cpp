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

// STL includes
#include <algorithm>

// CTK includes
#include <ctkLogger.h>

// ctkDICOMCore includes
#include "ctkDICOMSeriesModel.h"
#include "ctkDICOMDatabase.h"
#include "ctkDICOMScheduler.h"
#include "ctkDICOMJobResponseSet.h"
#include "ctkDICOMJob.h"
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
    int rows;
    int columns;
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
  int GridColumns;
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
  this->GridColumns = 5; // Default to 5 columns
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

  // Clear any pending thumbnail requests when resetting the model
  this->PendingThumbnails.clear();
  if (this->ThumbnailTimer && this->ThumbnailTimer->isActive())
  {
    this->ThumbnailTimer->stop();
  }

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
    QString patientItem = this->DicomDatabase->patientForStudy(this->StudyFilter);
    info.patientID = this->DicomDatabase->fieldForPatient("PatientID", patientItem);
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

    // Get DICOM Rows/Columns from first instance (if available)
    info.rows = 0;
    info.columns = 0;
    if (!instances.isEmpty()) {
      QString firstInstance = instances.first();
      QString rowsStr = this->DicomDatabase->instanceValue(firstInstance, "0028,0010"); // Rows
      QString colsStr = this->DicomDatabase->instanceValue(firstInstance, "0028,0011"); // Columns
      info.rows = rowsStr.toInt();
      info.columns = colsStr.toInt();
    }

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
    
    // Check for existing cached thumbnail first
    QString thumbnailPath = this->DicomDatabase->thumbnailPathForInstance(
      info.studyInstanceUID, info.seriesInstanceUID, "");
    
    if (!thumbnailPath.isEmpty() && QFile::exists(thumbnailPath))
    {
      QPixmap cachedThumbnail(thumbnailPath);
      if (!cachedThumbnail.isNull())
      {
        // Use cached thumbnail
        info.thumbnail = cachedThumbnail.scaled(this->ThumbnailSize, this->ThumbnailSize,
                                              Qt::KeepAspectRatio, Qt::SmoothTransformation);
        info.thumbnailPath = thumbnailPath;
        info.thumbnailGenerated = true;
      }
      else
      {
        // Generate default thumbnail as fallback
        info.thumbnail = this->createModalityThumbnail(info.modality, this->ThumbnailSize);
      }
    }
    else
    {
      // Generate default thumbnail
      info.thumbnail = this->createModalityThumbnail(info.modality, this->ThumbnailSize);
    }
    
    this->SeriesList.append(info);
  }
  
  // Start async thumbnail generation for series without cached thumbnails
  Q_Q(ctkDICOMSeriesModel);
  QTimer::singleShot(0, q, [this]() {
    for (const auto& info : this->SeriesList)
    {
      // Only generate for series that don't have cached thumbnails
      if (!info.thumbnailGenerated)
      {
        this->generateThumbnailForSeries(info.seriesInstanceUID);
      }
    }
  });
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
      
      // Convert linear index to table coordinates
      int tableRow = row / this->GridColumns;
      int tableColumn = row % this->GridColumns;
      QModelIndex index = q->createIndex(tableRow, tableColumn);
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
  
  // Find series info to get required data
  int row = this->findSeriesRow(seriesInstanceUID);
  if (row >= 0 && row < this->SeriesList.size())
  {
    const SeriesInfo& info = this->SeriesList[row];
    
    if (this->Scheduler && this->DicomDatabase)
    {
      // Get a representative instance from the series
      QStringList instancesList = this->DicomDatabase->instancesForSeries(seriesInstanceUID);
      if (!instancesList.isEmpty())
      {
        QString instanceUID;
        
        // Sort instances by instance number to get the proper center frame
        if (instancesList.size() == 1)
        {
          instanceUID = instancesList.first();
        }
        else
        {
          // Sort by instance number (DICOM tag 0020,0013)
          QMap<int, QString> sortedInstancesMap;
          foreach (const QString& instanceItem, instancesList)
          {
            int instanceNumber = 0;
            QString instanceNumberString = this->DicomDatabase->instanceValue(instanceItem, "0020,0013");
            
            if (!instanceNumberString.isEmpty())
            {
              instanceNumber = instanceNumberString.toInt();
            }
            
            sortedInstancesMap[instanceNumber] = instanceItem;
          }
          
          if (!sortedInstancesMap.isEmpty())
          {
            QList<int> keys = sortedInstancesMap.keys();
            std::sort(keys.begin(), keys.end());
            
            int centerInstanceIndex = keys.count() / 2;
            int centerInstanceNumber = keys[centerInstanceIndex];
            instanceUID = sortedInstancesMap[centerInstanceNumber];
          }
          else
          {
            // Fallback to simple middle index if instance numbers are not available
            instanceUID = instancesList[instancesList.size() / 2];
          }
        }
        
        // Get the file path for this instance
        QString dicomFilePath = this->DicomDatabase->fileForInstance(instanceUID);
        
        if (!dicomFilePath.isEmpty() && QFile::exists(dicomFilePath))
        {
          // Mark as generating thumbnail
          SeriesInfo& mutableInfo = this->SeriesList[row];
          mutableInfo.operationStatus = ctkDICOMSeriesModel::GeneratingThumbnail;
          
          // Request thumbnail generation from scheduler
          QColor backgroundColor = Qt::lightGray; // Default background color
          this->Scheduler->generateThumbnail(dicomFilePath, 
                                             info.patientID,
                                             info.studyInstanceUID, 
                                             info.seriesInstanceUID,
                                             instanceUID, 
                                             info.modality,
                                             backgroundColor,
                                             QThread::LowPriority);
        }
      }
    }
  }
  
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
      if (!d->SeriesList[i].thumbnailGenerated)
      {
        continue;
      }
      d->SeriesList[i].thumbnailGenerated = false;
      d->generateThumbnailForSeries(d->SeriesList[i].seriesInstanceUID);
    }
    
    emit dataChanged(createIndex(0, 0), createIndex(d->SeriesList.size() - 1, 0), {ThumbnailRole});
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
void ctkDICOMSeriesModel::setGridColumns(int columns)
{
  Q_D(ctkDICOMSeriesModel);
  if (d->GridColumns != columns && columns > 0)
  {
    this->beginResetModel();
    d->GridColumns = columns;
    this->endResetModel();
    emit gridColumnsChanged(columns);
  }
}

//----------------------------------------------------------------------------
int ctkDICOMSeriesModel::gridColumns() const
{
  Q_D(const ctkDICOMSeriesModel);
  return d->GridColumns;
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
  if (!index.isValid())
  {
    return QString();
  }
  
  // Convert table coordinates to linear index
  int linearIndex = index.row() * d->GridColumns + index.column();
  
  if (linearIndex >= d->SeriesList.size())
  {
    return QString();
  }
  
  return d->SeriesList[linearIndex].seriesInstanceUID;
}

//----------------------------------------------------------------------------
QString ctkDICOMSeriesModel::seriesItem(const QModelIndex& index) const
{
  Q_D(const ctkDICOMSeriesModel);
  if (!index.isValid())
  {
    return QString();
  }
  
  // Convert table coordinates to linear index
  int linearIndex = index.row() * d->GridColumns + index.column();
  
  if (linearIndex >= d->SeriesList.size())
  {
    return QString();
  }
  
  return d->SeriesList[linearIndex].seriesItem;
}

//----------------------------------------------------------------------------
QModelIndex ctkDICOMSeriesModel::indexForSeriesInstanceUID(const QString& seriesInstanceUID) const
{
  Q_D(const ctkDICOMSeriesModel);
  int linearIndex = d->findSeriesRow(seriesInstanceUID);
  if (linearIndex >= 0)
  {
    // Convert linear index to table coordinates
    int row = linearIndex / d->GridColumns;
    int column = linearIndex % d->GridColumns;
    return createIndex(row, column);
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
  
  if (d->GridColumns <= 0)
  {
    return 0;
  }
  
  int totalSeries = d->SeriesList.size();
  return (totalSeries + d->GridColumns - 1) / d->GridColumns; // Ceiling division
}

//----------------------------------------------------------------------------
int ctkDICOMSeriesModel::columnCount(const QModelIndex& parent) const
{
  Q_D(const ctkDICOMSeriesModel);
  Q_UNUSED(parent);
  return d->GridColumns;
}

//----------------------------------------------------------------------------
QVariant ctkDICOMSeriesModel::data(const QModelIndex& index, int role) const
{
  Q_D(const ctkDICOMSeriesModel);
  
  if (!index.isValid())
  {
    return QVariant();
  }
  
  // Convert table coordinates (row, column) to linear series index
  int linearIndex = index.row() * d->GridColumns + index.column();
  
  if (linearIndex >= d->SeriesList.size())
  {
    return QVariant(); // Empty cell beyond the series list
  }
  
  const auto& info = d->SeriesList[linearIndex];
  
  switch (role)
  {
    case Qt::DisplayRole:
      return QString("Series %1: %2").arg(info.seriesNumber).arg(info.seriesDescription);

    case Qt::ToolTipRole:
      return QString("PatientID: %1\nStudyInstanceUID: %2\nSeriesInstanceUID: %3\nDescription: %4")
        .arg(info.patientID).arg(info.studyInstanceUID).arg(info.seriesInstanceUID).arg(info.seriesDescription);

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

    case RowsRole:
      return info.rows;

    case ColumnsRole:
      return info.columns;

    case ThumbnailRole:
      return info.thumbnail;

    case ThumbnailPathRole:
      return info.thumbnailPath;

    case ThumbnailSizeRole:
      return QSize(d->ThumbnailSize, d->ThumbnailSize);

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
QVariant ctkDICOMSeriesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  Q_UNUSED(section);
  Q_UNUSED(orientation);
  Q_UNUSED(role);
  
  // No headers needed for the series grid
  return QVariant();
}

//----------------------------------------------------------------------------
bool ctkDICOMSeriesModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  Q_D(ctkDICOMSeriesModel);
  
  if (!index.isValid())
  {
    return false;
  }
  
  // Convert table coordinates to linear index
  int linearIndex = index.row() * d->GridColumns + index.column();
  
  if (linearIndex >= d->SeriesList.size())
  {
    return false;
  }
  
  auto& info = d->SeriesList[linearIndex];
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
  Q_D(const ctkDICOMSeriesModel);
  
  if (!index.isValid())
  {
    return Qt::NoItemFlags;
  }
  
  // Convert table coordinates to linear index
  int linearIndex = index.row() * d->GridColumns + index.column();
  
  if (linearIndex >= d->SeriesList.size())
  {
    // Empty cell - no flags
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
  roles[RowsRole] = "rows";
  roles[ColumnsRole] = "columns";
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
  Q_D(ctkDICOMSeriesModel);
  
  // Extract job details
  ctkDICOMJobDetail jobDetail = data.value<ctkDICOMJobDetail>();
  
  // Check if this is a thumbnail generation job
  if (jobDetail.JobType == ctkDICOMJobResponseSet::JobType::ThumbnailGenerator)
  {
    // Find the series this thumbnail is for
    int row = d->findSeriesRow(jobDetail.SeriesInstanceUID);
    if (row >= 0 && row < d->SeriesList.size())
    {
      ctkDICOMSeriesModelPrivate::SeriesInfo& info = d->SeriesList[row];
      
      // Remove from pending list since job is complete
      d->PendingThumbnails.removeAll(jobDetail.SeriesInstanceUID);
      
      // Load the generated thumbnail
      QString thumbnailPath = d->DicomDatabase->thumbnailPathForInstance(
        jobDetail.StudyInstanceUID, jobDetail.SeriesInstanceUID, jobDetail.SOPInstanceUID);
      
      if (!thumbnailPath.isEmpty() && QFile::exists(thumbnailPath))
      {
        QPixmap thumbnail(thumbnailPath);
        if (!thumbnail.isNull())
        {
          // Scale and cache the thumbnail
          info.thumbnail = thumbnail.scaled(d->ThumbnailSize, d->ThumbnailSize,
                                          Qt::KeepAspectRatio, Qt::SmoothTransformation);
          info.thumbnailPath = thumbnailPath;
          info.thumbnailGenerated = true;
          info.operationStatus = "Ready";
          
          // Notify views that thumbnail is ready - convert linear index to table coordinates
          int tableRow = row / d->GridColumns;
          int tableColumn = row % d->GridColumns;
          QModelIndex index = this->createIndex(tableRow, tableColumn);
          emit this->dataChanged(index, index, {ThumbnailRole, OperationStatusRole});
          emit this->thumbnailReady(index);
        }
      }
      else
      {
        // Thumbnail generation failed - use fallback
        info.operationStatus = "Failed";
        int tableRow = row / d->GridColumns;
        int tableColumn = row % d->GridColumns;
        QModelIndex index = this->createIndex(tableRow, tableColumn);
        emit this->dataChanged(index, index, {OperationStatusRole});
      }
    }
    else
    {
      // Series not found in current model, but remove from pending anyway
      d->PendingThumbnails.removeAll(jobDetail.SeriesInstanceUID);
    }
  }
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
    
    // Convert linear index to table coordinates
    int tableRow = row / d->GridColumns;
    int tableColumn = row % d->GridColumns;
    QModelIndex index = createIndex(tableRow, tableColumn);
    emit dataChanged(index, index, {ThumbnailRole});
    emit thumbnailReady(index);
  }
}
