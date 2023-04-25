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

//Qt includes
#include <QDebug>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>
#include <QTextItem>

// CTK includes
#include <ctkLogger.h>

// ctkDICOMCore includes
#include "ctkDICOMDatabase.h"
#include "ctkDICOMTaskPool.h"
#include "ctkDICOMTaskResults.h"
#include "ctkDICOMThumbnailGenerator.h"

// ctkDICOMWidgets includes
#include "ctkDICOMSeriesItemWidget.h"
#include "ui_ctkDICOMSeriesItemWidget.h"

static ctkLogger logger("org.commontk.DICOM.Widgets.ctkDICOMSeriesItemWidget");

//----------------------------------------------------------------------------
class ctkDICOMSeriesItemWidgetPrivate: public Ui_ctkDICOMSeriesItemWidget
{
  Q_DECLARE_PUBLIC( ctkDICOMSeriesItemWidget );

protected:
  ctkDICOMSeriesItemWidget* const q_ptr;
  
public:
  ctkDICOMSeriesItemWidgetPrivate(ctkDICOMSeriesItemWidget& obj);
  ~ctkDICOMSeriesItemWidgetPrivate();

  void init();
  QString getDICOMCenterFrameFromInstances(QStringList instancesList);
  void createThumbnail(ctkDICOMTaskResults *taskResults = nullptr);
  void drawThumbnail(const QString& file, int numberOfFrames);
  void drawTextWithShadow(QPainter *painter,
                          const QRect &r,
                          int flags,
                          const QString &text);
  void updateThumbnailProgressBar(const int& progress);
  void raiseRetrieveFramesTasksPriority();

  QSharedPointer<ctkDICOMDatabase> DicomDatabase;
  QSharedPointer<ctkDICOMTaskPool> TaskPool;

  QString SeriesItem;
  QString StudyInstanceUID;
  QString SeriesInstanceUID;
  QString CentralFrameSOPInstanceUID;
  QString SeriesNumber;
  QString Modality;

  bool IsCloud;
  bool IsLoaded;
  bool IsVisible;
  bool IsSeriesFetching;
  int ThumbnailSize;
};

//----------------------------------------------------------------------------
// ctkDICOMSeriesItemWidgetPrivate methods

//----------------------------------------------------------------------------
ctkDICOMSeriesItemWidgetPrivate::ctkDICOMSeriesItemWidgetPrivate(ctkDICOMSeriesItemWidget& obj)
  : q_ptr(&obj)
{
  this->IsCloud = false;
  this->IsLoaded = false;
  this->IsVisible = false;
  this->IsSeriesFetching = false;
  this->ThumbnailSize = 300;

  this->DicomDatabase = nullptr;
  this->TaskPool = nullptr;
}

//----------------------------------------------------------------------------
ctkDICOMSeriesItemWidgetPrivate::~ctkDICOMSeriesItemWidgetPrivate()
{
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidgetPrivate::init()
{
  Q_Q(ctkDICOMSeriesItemWidget);
  this->setupUi(q);

  this->SeriesThumbnail->setTransformationMode(Qt::TransformationMode::SmoothTransformation);
}

//----------------------------------------------------------------------------
QString ctkDICOMSeriesItemWidgetPrivate::getDICOMCenterFrameFromInstances(QStringList instancesList)
{
  if (!this->DicomDatabase)
    {
    logger.error("getDICOMCenterFrameFromInstances failed, no DICOM Database has been set. \n");
    return "";
    }

  if (instancesList.count() == 0)
    {
    return "";
    }

  // NOTE: we sort by the instance number.
  // We could sort for 3D spatial values (ImagePatientPosition and ImagePatientOrientation),
  // plus time information (for 4D datasets). However, this would require additional metadata fetching and logic, which can slow down.
  QMap<int, QString> DICOMInstances;
  foreach (QString instanceItem, instancesList)
    {
    int instanceNumber = 0;
    QString instanceNumberString = this->DicomDatabase->instanceValue(instanceItem, "0020,0013");

    if (instanceNumberString != "")
      {
      instanceNumber = instanceNumberString.toInt();
      }

    DICOMInstances[instanceNumber] = instanceItem;
    }

  if (DICOMInstances.count() == 1)
    {
    return instancesList[0];
    }

  QList<int> keys = DICOMInstances.keys();
  std::sort(keys.begin(), keys.end());

  int centerFrameIndex = floor(keys.count() / 2);
  if (keys.count() <= centerFrameIndex)
    {
    return instancesList[0];
    }

  int centerInstanceNumber = keys[centerFrameIndex];

  return DICOMInstances[centerInstanceNumber];
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidgetPrivate::createThumbnail(ctkDICOMTaskResults *taskResults)
{
  if (!this->DicomDatabase)
    {
    logger.error("importFiles failed, no DICOM Database has been set. \n");
    return;
    }

  ctkDICOMTaskResults::TaskType typeOfTask = ctkDICOMTaskResults::TaskType::FileIndexing;
  QString taskSopInstanceUID;
  if (taskResults)
    {
    taskSopInstanceUID = taskResults->sopInstanceUID();
    typeOfTask = taskResults->typeOfTask();
    }

  QStringList instancesList = this->DicomDatabase->instancesForSeries(this->SeriesInstanceUID);
  int numberOfFrames = instancesList.count();
  if (numberOfFrames == 0)
    {
    return;
    }

  QStringList filesList = this->DicomDatabase->filesForSeries(this->SeriesInstanceUID);
  int numberOfInstancesOnServer = filesList.filter("server://").count();
  if (!this->IsCloud && numberOfFrames > 0 && numberOfInstancesOnServer > 0)
    {
    this->IsCloud = true;
    this->SeriesThumbnail->operationProgressBar()->show();
    }

  if (!this->IsCloud)
    {
    if(this->DicomDatabase->visibleSeries().contains(this->SeriesInstanceUID))
      {
      this->IsVisible = true;
      }
    else if (this->DicomDatabase->loadedSeries().contains(this->SeriesInstanceUID))
      {
      this->IsLoaded = true;
      }
    else
      {
      this->IsVisible = false;
      this->IsLoaded = false;
      }
    }

  QString file;
  if (this->CentralFrameSOPInstanceUID.isEmpty())
    {
    this->CentralFrameSOPInstanceUID = this->getDICOMCenterFrameFromInstances(instancesList);
    file = this->DicomDatabase->fileForInstance(this->CentralFrameSOPInstanceUID);

    // Since getDICOMCenterFrameFromInstances is based on the sorting of the instance number,
    // which is not always reliable, it could fail to get the right central frame.
    // In these cases, we check if a frame has been already fetched and we use the first found one.
    if (file.contains("server://") && numberOfInstancesOnServer < numberOfFrames)
      {
      foreach(QString newFile, filesList)
        {
        if (newFile.contains("server://"))
          {
          continue;
          }

        file = newFile;
        this->CentralFrameSOPInstanceUID = this->DicomDatabase->instanceForFile(file);
        break;
        }
      }
    }
  else
    {
    file = this->DicomDatabase->fileForInstance(this->CentralFrameSOPInstanceUID);
    }

  if (this->TaskPool && this->TaskPool->getNumberOfServers() > 0)
    {
    // Get file for thumbnail
    if (file.contains("server://") && (typeOfTask == ctkDICOMTaskResults::TaskType::FileIndexing ||
                                       typeOfTask == ctkDICOMTaskResults::TaskType::QueryInstances))
      {
      this->TaskPool->retrieveSOPInstance(this->StudyInstanceUID,
                                             this->SeriesInstanceUID,
                                             this->CentralFrameSOPInstanceUID,
                                             QThread::NormalPriority);
      return;
      }

    // Get all the others frames (this will be run after getting the central frame, so the thumbnail will show up asap)
    if (this->IsCloud && !this->IsSeriesFetching)
      {
      this->IsSeriesFetching = true;
      foreach(QString file, filesList)
        {
        if (!file.contains("server://"))
          {
          continue;
          }

        QString sopInstanceUID = this->DicomDatabase->instanceForFile(file);
        if (sopInstanceUID == this->CentralFrameSOPInstanceUID)
          {
          continue;
          }

        this->TaskPool->retrieveSOPInstance(this->StudyInstanceUID,
                                               this->SeriesInstanceUID,
                                               sopInstanceUID);
        }
      }
    }

  if (taskSopInstanceUID.isEmpty() || taskSopInstanceUID == this->CentralFrameSOPInstanceUID)
    {
    this->drawThumbnail(file, numberOfFrames);
    }

  if (this->IsCloud)
    {
    float percentageOfInstancesOnLocal = float(numberOfFrames - numberOfInstancesOnServer) / numberOfFrames;
    int progress = ceil(percentageOfInstancesOnLocal * 100) ;
    this->updateThumbnailProgressBar(progress);
    }
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidgetPrivate::drawThumbnail(const QString &file, int numberOfFrames)
{
  if (!this->DicomDatabase)
    {
    logger.error("drawThumbnail failed, no DICOM Database has been set. \n");
    return;
    }

  int margin = 10;
  int fontSize = 12;
  if (!this->SeriesThumbnail->text().isEmpty())
    {
    margin = 5;
    fontSize = 14;
    }

  QPixmap resultPixmap(this->ThumbnailSize, this->ThumbnailSize);
  resultPixmap.fill(Qt::transparent);
  ctkDICOMThumbnailGenerator thumbnailGenerator;
  thumbnailGenerator.setWidth(this->ThumbnailSize);
  thumbnailGenerator.setHeight(this->ThumbnailSize);
  QImage thumbnailImage;
  if (!thumbnailGenerator.generateThumbnail(file, thumbnailImage))
    {
    thumbnailGenerator.generateBlankThumbnail(thumbnailImage);
    resultPixmap = QPixmap::fromImage(thumbnailImage);
    }
  else
    {
    QPainter painter;
    if (painter.begin(&resultPixmap))
      {
      painter.setRenderHint(QPainter::Antialiasing);
      QRect rect = resultPixmap.rect();
      painter.setFont(QFont("Arial", fontSize, QFont::Bold));
      int x = int((rect.width() / 2) - (thumbnailImage.rect().width() / 2));
      int y = int((rect.height() / 2) - (thumbnailImage.rect().height() / 2));
      painter.drawPixmap(x, y, QPixmap::fromImage(thumbnailImage));
      QString topLeft = ctkDICOMSeriesItemWidget::tr("Series: %1\n%2").arg(this->SeriesNumber).arg(this->Modality);
      this->drawTextWithShadow(&painter, rect.adjusted(margin, margin, margin, margin), Qt::AlignTop | Qt::AlignLeft, topLeft);
      QString bottomLeft = ctkDICOMSeriesItemWidget::tr("N.frames: ") + QString::number(numberOfFrames);
      this->drawTextWithShadow(&painter, rect.adjusted(margin, -margin, margin, -margin), Qt::AlignBottom | Qt::AlignLeft, bottomLeft);
      QString rows = this->DicomDatabase->instanceValue(this->CentralFrameSOPInstanceUID, "0028,0010");
      QString columns = this->DicomDatabase->instanceValue(this->CentralFrameSOPInstanceUID, "0028,0011");
      QString bottomRight = rows + "x" + columns;
      this->drawTextWithShadow(&painter, rect.adjusted(-margin, -margin, -margin, -margin), Qt::AlignBottom | Qt::AlignRight, bottomRight);
      QIcon statusIcon;
      if (this->IsCloud)
        {
        statusIcon = QIcon(":Icons/cloud.svg");
        }
      else if (this->IsVisible)
        {
        statusIcon = QIcon(":Icons/visible.svg");
        }
      else if (this->IsLoaded)
        {
        statusIcon = QIcon(":Icons/loaded.svg");
        }

      QPoint topRight = rect.topRight();
      statusIcon.paint(&painter, topRight.x() - 48 - margin, topRight.y() + margin, 48, 48, Qt::AlignTop | Qt::AlignRight);

      painter.end();
      }
    }

  this->SeriesThumbnail->setPixmap(resultPixmap);
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidgetPrivate::drawTextWithShadow(QPainter *painter,
                                                         const QRect &r,
                                                         int flags, const
                                                         QString &text)
{
  painter->setPen(Qt::darkGray);
  painter->drawText(r.adjusted(1, 1, 1, 1), flags, text);
  painter->setPen(QColor(Qt::gray));
  painter->drawText(r.adjusted(2, 2, 2, 2), flags, text);
  painter->setPen(QPen(QColor(41, 121, 255)));
  painter->drawText(r, flags, text);
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidgetPrivate::updateThumbnailProgressBar(const int& progress)
{
  this->SeriesThumbnail->setOperationProgress(progress);
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidgetPrivate::raiseRetrieveFramesTasksPriority()
{
  if (!this->TaskPool)
    {
    logger.error("raiseRetrieveFramesTasksPriority failed, no task pool has been set. \n");
    return;
    }

  if (!this->IsCloud || this->TaskPool->getNumberOfServers() == 0)
    {
    return;
    }

  // Check if any retrieve task is in queue and rerun with higher priority if yes.
  this->TaskPool->raiseRetrieveFramesTasksPriorityForSeries(this->StudyInstanceUID,
                                                               this->SeriesInstanceUID);
}

//----------------------------------------------------------------------------
// ctkDICOMSeriesItemWidget methods

//----------------------------------------------------------------------------
ctkDICOMSeriesItemWidget::ctkDICOMSeriesItemWidget(QWidget* parentWidget)
  : Superclass(parentWidget) 
  , d_ptr(new ctkDICOMSeriesItemWidgetPrivate(*this))
{
  Q_D(ctkDICOMSeriesItemWidget);
  d->init();
}

//----------------------------------------------------------------------------
ctkDICOMSeriesItemWidget::~ctkDICOMSeriesItemWidget()
{
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidget::setSeriesItem(const QString &seriesItem)
{
  Q_D(ctkDICOMSeriesItemWidget);
  d->SeriesItem = seriesItem;
}

//----------------------------------------------------------------------------
QString ctkDICOMSeriesItemWidget::seriesItem() const
{
  Q_D(const ctkDICOMSeriesItemWidget);
  return d->SeriesItem;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidget::setStudyInstanceUID(const QString& studyInstanceUID)
{
  Q_D(ctkDICOMSeriesItemWidget);
  d->StudyInstanceUID = studyInstanceUID;
}

//------------------------------------------------------------------------------
QString ctkDICOMSeriesItemWidget::studyInstanceUID() const
{
  Q_D(const ctkDICOMSeriesItemWidget);
  return d->StudyInstanceUID;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidget::setSeriesInstanceUID(const QString& seriesInstanceUID)
{
  Q_D(ctkDICOMSeriesItemWidget);
  d->SeriesInstanceUID = seriesInstanceUID;
}

//------------------------------------------------------------------------------
QString ctkDICOMSeriesItemWidget::seriesInstanceUID() const
{
  Q_D(const ctkDICOMSeriesItemWidget);
  return d->SeriesInstanceUID;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidget::setSeriesNumber(const QString& seriesNumber)
{
  Q_D(ctkDICOMSeriesItemWidget);
  d->SeriesNumber = seriesNumber;
}

//------------------------------------------------------------------------------
QString ctkDICOMSeriesItemWidget::seriesNumber() const
{
  Q_D(const ctkDICOMSeriesItemWidget);
  return d->SeriesNumber;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidget::setModality(const QString& modality)
{
  Q_D(ctkDICOMSeriesItemWidget);
  d->Modality = modality;
}

//------------------------------------------------------------------------------
QString ctkDICOMSeriesItemWidget::modality() const
{
  Q_D(const ctkDICOMSeriesItemWidget);
  return d->Modality;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidget::setSeriesDescription(const QString& seriesDescription)
{
  Q_D(ctkDICOMSeriesItemWidget);
  d->SeriesThumbnail->setText(seriesDescription);
}

//------------------------------------------------------------------------------
QString ctkDICOMSeriesItemWidget::seriesDescription() const
{
  Q_D(const ctkDICOMSeriesItemWidget);
  return d->SeriesThumbnail->text();
}

//----------------------------------------------------------------------------
bool ctkDICOMSeriesItemWidget::isCloud() const
{
  Q_D(const ctkDICOMSeriesItemWidget);
  return d->IsCloud;
}

//----------------------------------------------------------------------------
bool ctkDICOMSeriesItemWidget::IsLoaded() const
{
  Q_D(const ctkDICOMSeriesItemWidget);
  return d->IsLoaded;
}

//----------------------------------------------------------------------------
bool ctkDICOMSeriesItemWidget::IsVisible() const
{
  Q_D(const ctkDICOMSeriesItemWidget);
  return d->IsVisible;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidget::setThumbnailSize(int thumbnailSize)
{
  Q_D(ctkDICOMSeriesItemWidget);
  d->ThumbnailSize = thumbnailSize;
}

//------------------------------------------------------------------------------
int ctkDICOMSeriesItemWidget::thumbnailSize() const
{
  Q_D(const ctkDICOMSeriesItemWidget);
  return d->ThumbnailSize;
}

//----------------------------------------------------------------------------
static void skipDelete(QObject* obj)
{
  Q_UNUSED(obj);
  // this deleter does not delete the object from memory
  // useful if the pointer is not owned by the smart pointer
}

//----------------------------------------------------------------------------
ctkDICOMTaskPool* ctkDICOMSeriesItemWidget::taskPool()const
{
  Q_D(const ctkDICOMSeriesItemWidget);
  return d->TaskPool.data();
}

//----------------------------------------------------------------------------
QSharedPointer<ctkDICOMTaskPool> ctkDICOMSeriesItemWidget::taskPoolShared()const
{
  Q_D(const ctkDICOMSeriesItemWidget);
  return d->TaskPool;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidget::setTaskPool(ctkDICOMTaskPool& taskPool)
{
  Q_D(ctkDICOMSeriesItemWidget);
  if (d->TaskPool)
    {
    QObject::disconnect(d->TaskPool.data(), SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)),
                       this, SLOT(updateGUIFromTaskPool(ctkDICOMTaskResults*)));
    }

  d->TaskPool = QSharedPointer<ctkDICOMTaskPool>(&taskPool, skipDelete);

  if (d->TaskPool)
    {
    QObject::connect(d->TaskPool.data(), SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)),
                     this, SLOT(updateGUIFromTaskPool(ctkDICOMTaskResults*)));
    }
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidget::setTaskPool(QSharedPointer<ctkDICOMTaskPool> taskPool)
{
  Q_D(ctkDICOMSeriesItemWidget);
  if (d->TaskPool)
    {
    QObject::disconnect(d->TaskPool.data(), SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)),
                       this, SLOT(updateGUIFromTaskPool(ctkDICOMTaskResults*)));
    }

  d->TaskPool = taskPool;

  if (d->TaskPool)
    {
    QObject::connect(d->TaskPool.data(), SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)),
                     this, SLOT(updateGUIFromTaskPool(ctkDICOMTaskResults*)));
    }
}

//----------------------------------------------------------------------------
ctkDICOMDatabase* ctkDICOMSeriesItemWidget::dicomDatabase()const
{
  Q_D(const ctkDICOMSeriesItemWidget);
  return d->DicomDatabase.data();
}

//----------------------------------------------------------------------------
QSharedPointer<ctkDICOMDatabase> ctkDICOMSeriesItemWidget::dicomDatabaseShared()const
{
  Q_D(const ctkDICOMSeriesItemWidget);
  return d->DicomDatabase;
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidget::setDicomDatabase(ctkDICOMDatabase& dicomDatabase)
{
  Q_D(ctkDICOMSeriesItemWidget);
  d->DicomDatabase = QSharedPointer<ctkDICOMDatabase>(&dicomDatabase, skipDelete);
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidget::setDicomDatabase(QSharedPointer<ctkDICOMDatabase> dicomDatabase)
{
  Q_D(ctkDICOMSeriesItemWidget);
  d->DicomDatabase = dicomDatabase;
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesItemWidget::generateInstances()
{
  Q_D(ctkDICOMSeriesItemWidget);
  if (!d->DicomDatabase)
    {
    logger.error("generateInstances failed, no DICOM Database has been set. \n");
    return;
    }

  QStringList instancesList = d->DicomDatabase->instancesForSeries(d->SeriesInstanceUID);
  // Here we check if we have any instance metadata for the series, if not we query to the servers.
  // NOTE1: What if we have some instance metadata locally, but on the server they have been updated?
  // NOTE2: Usually DICOM does not allow updating instances,
  //        the user should submit to the server a new series with different SeriesInstanceUID
  if (instancesList.count() == 0 && d->TaskPool && d->TaskPool->getNumberOfServers() > 0)
    {
    d->TaskPool->queryInstances(d->StudyInstanceUID, d->SeriesInstanceUID, QThread::NormalPriority);
    }
  else
    {
    d->createThumbnail();
    }
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidget::updateGUIFromTaskPool(ctkDICOMTaskResults *taskResults)
{
  Q_D(ctkDICOMSeriesItemWidget);

  if (!taskResults ||
      (taskResults->typeOfTask() != ctkDICOMTaskResults::TaskType::QueryInstances &&
       taskResults->typeOfTask() != ctkDICOMTaskResults::TaskType::RetrieveSOPInstance) ||
      taskResults->studyInstanceUID() != d->StudyInstanceUID ||
      taskResults->seriesInstanceUID() != d->SeriesInstanceUID)
    {
    return;
    }

  d->createThumbnail(taskResults);
}

//----------------------------------------------------------------------------
void ctkDICOMSeriesItemWidget::mousePressEvent(QMouseEvent *event)
{
  Q_D(ctkDICOMSeriesItemWidget);

  if (!this->underMouse())
    {
    return;
    }

  d->raiseRetrieveFramesTasksPriority();

  QWidget::mousePressEvent(event);
}
