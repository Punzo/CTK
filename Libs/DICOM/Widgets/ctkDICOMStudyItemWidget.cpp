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
#include <QLabel>
#include <QScreen>
#include <QItemSelectionModel>
#include <QItemSelection>

// CTK includes
#include <ctkLogger.h>

// ctkDICOMCore includes
#include "ctkDICOMDatabase.h"
#include "ctkDICOMJob.h"
#include "ctkDICOMJobResponseSet.h"
#include "ctkDICOMScheduler.h"

// ctkDICOMWidgets includes
#include "ctkDICOMStudyItemWidget.h"
#include "ctkDICOMSeriesTableView.h"
#include "ctkDICOMSeriesModel.h"
#include "ctkDICOMSeriesDelegate.h"
#include "ui_ctkDICOMStudyItemWidget.h"

#include <math.h>

static ctkLogger logger("org.commontk.DICOM.Widgets.DICOMStudyItemWidget");

//----------------------------------------------------------------------------
static void skipDelete(QObject* obj)
{
  Q_UNUSED(obj);
  // this deleter does not delete the object from memory
  // useful if the pointer is not owned by the smart pointer
}

//----------------------------------------------------------------------------
class ctkDICOMStudyItemWidgetPrivate : public Ui_ctkDICOMStudyItemWidget
{
  Q_DECLARE_PUBLIC(ctkDICOMStudyItemWidget);

protected:
  ctkDICOMStudyItemWidget* const q_ptr;

public:
  ctkDICOMStudyItemWidgetPrivate(ctkDICOMStudyItemWidget& obj);
  ~ctkDICOMStudyItemWidgetPrivate();

  void init(QWidget* parent);
  void createSeries();
  int getScreenWidth() const;
  int getScreenHeight() const;
  int calculateNumerOfSeriesPerRow() const;
  int calculateThumbnailSizeInPixel(const ctkDICOMStudyItemWidget::ThumbnailSizeOption& thumbnailSize);
  void updateSeriesSelectionInternal();

  QString FilteringSeriesDescription;
  QStringList FilteringModalities;

  QSharedPointer<ctkDICOMDatabase> DicomDatabase;
  QSharedPointer<ctkDICOMScheduler> Scheduler;
  QSharedPointer<QWidget> VisualDICOMBrowser;

  ctkDICOMStudyItemWidget::ThumbnailSizeOption ThumbnailSize;
  int ThumbnailSizePixel;
  QString PatientID;
  QString StudyInstanceUID;
  QString StudyItem;

  QStringList AllowedServers;
  ctkDICOMStudyItemWidget::OperationStatus Status;
  QString StoppedJobUID;

  bool IsGUIUpdating;
  bool QueryOn;
  bool RetrieveOn;
  int FilteredSeriesCount;

  ctkDICOMSeriesModel* SeriesModel;
  ctkDICOMSeriesDelegate* SeriesDelegate;
  QModelIndexList PreviousSeriesSelection;
  QModelIndexList CurrentSeriesSelection;
};

//----------------------------------------------------------------------------
// ctkDICOMStudyItemWidgetPrivate methods

//----------------------------------------------------------------------------
ctkDICOMStudyItemWidgetPrivate::ctkDICOMStudyItemWidgetPrivate(ctkDICOMStudyItemWidget& obj)
  : q_ptr(&obj)
{
  this->ThumbnailSize = ctkDICOMStudyItemWidget::ThumbnailSizeOption::Medium;
  this->ThumbnailSizePixel = 200;

  this->AllowedServers = QStringList();
  this->Status = ctkDICOMStudyItemWidget::NoOperation;

  this->DicomDatabase = nullptr;
  this->Scheduler = nullptr;
  this->VisualDICOMBrowser = nullptr;

  this->IsGUIUpdating = false;
  this->QueryOn = true;
  this->RetrieveOn = true;
  this->FilteredSeriesCount = 0;
}

//----------------------------------------------------------------------------
ctkDICOMStudyItemWidgetPrivate::~ctkDICOMStudyItemWidgetPrivate()
{
  this->CurrentSeriesSelection.clear();
  this->PreviousSeriesSelection.clear();
}

//----------------------------------------------------------------------------
void ctkDICOMStudyItemWidgetPrivate::init(QWidget* parent)
{
  Q_Q(ctkDICOMStudyItemWidget);
  this->setupUi(q);

  this->VisualDICOMBrowser = QSharedPointer<QWidget>(parent, skipDelete);
  this->StudyItemCollapsibleGroupBox->setCollapsed(false);
  this->OperationStatusPushButton->hide();

  // Initialize the Model/View/Delegate architecture
  this->SeriesModel = new ctkDICOMSeriesModel(q);
  this->SeriesDelegate = new ctkDICOMSeriesDelegate(q);
  this->SeriesListTableView->setModel(this->SeriesModel);
  this->SeriesListTableView->setItemDelegate(this->SeriesDelegate);

  QObject::connect(this->StudySelectionCheckBox, SIGNAL(clicked(bool)),
                   q, SLOT(onStudySelectionClicked(bool)));
  QObject::connect(this->OperationStatusPushButton, SIGNAL(clicked(bool)),
                   q, SLOT(onOperationStatusButtonClicked(bool)));
  QObject::connect(this->SeriesListTableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                   q, SLOT(onSeriesSelectionChanged()));
}

//------------------------------------------------------------------------------
void ctkDICOMStudyItemWidgetPrivate::createSeries()
{
  Q_Q(ctkDICOMStudyItemWidget);
  if (this->IsGUIUpdating)
    {
    return;
    }

  if (!this->DicomDatabase)
  {
    logger.error("createSeries failed, no DICOM Database has been set. \n");
    return;
  }

  QStringList seriesList = this->DicomDatabase->seriesForStudy(this->StudyInstanceUID);
  if (seriesList.count() == 0)
  {
    q->setVisible(false);
    q->emit updateGUIFinished();
    return;
  }

  this->IsGUIUpdating = true;

  // Set up the model with database and study information
  this->SeriesModel->setDicomDatabase(this->DicomDatabase);
  this->SeriesModel->setStudyFilter(this->StudyInstanceUID);
  this->SeriesModel->setDescriptionFilter(this->FilteringSeriesDescription);
  this->SeriesModel->setModalityFilter(this->FilteringModalities);
  this->SeriesModel->setThumbnailSize(this->ThumbnailSizePixel);
  this->SeriesModel->setAllowedServers(this->AllowedServers);

  // Populate the model - this will automatically handle filtering
  QSettings settings;
  bool queryRetrieveEnabled = settings.value("DICOM/QueryRetrieveEnabled", "").toBool();
  bool queryEnabled = this->QueryOn && queryRetrieveEnabled;
  bool retrieveEnabled = this->RetrieveOn && queryRetrieveEnabled;
  
  //this->SeriesModel->updateSeriesList(queryEnabled, retrieveEnabled);
  
  this->FilteredSeriesCount = this->SeriesModel->rowCount();

  this->IsGUIUpdating = false;
  q->setVisible(this->FilteredSeriesCount != 0);
  q->emit updateGUIFinished();
}

//------------------------------------------------------------------------------
int ctkDICOMStudyItemWidgetPrivate::getScreenWidth() const
{
  QList<QScreen*> screens = QApplication::screens();
  int width = 1920;
  foreach (QScreen* screen, screens)
  {
    QRect rec = screen->geometry();
    if (rec.width() > width)
    {
      width = rec.width();
    }
  }

  return width;
}

//------------------------------------------------------------------------------
int ctkDICOMStudyItemWidgetPrivate::getScreenHeight() const
{
  QList<QScreen*> screens = QApplication::screens();
  int height = 1080;
  foreach (QScreen* screen, screens)
  {
    QRect rec = screen->geometry();
    if (rec.height() > height)
    {
      height = rec.height();
    }
  }

  return height;
}

//------------------------------------------------------------------------------
int ctkDICOMStudyItemWidgetPrivate::calculateNumerOfSeriesPerRow() const
{
  int width = this->getScreenWidth();
  int numberOfSeriesPerRow = 1;
  numberOfSeriesPerRow = floor(width / this->ThumbnailSizePixel) - 1;

  return numberOfSeriesPerRow;
}

//------------------------------------------------------------------------------
int ctkDICOMStudyItemWidgetPrivate::calculateThumbnailSizeInPixel(const ctkDICOMStudyItemWidget::ThumbnailSizeOption& thumbnailSize)
{
  int height = this->getScreenHeight();
  int thumbnailSizeInPixel = 1;
  switch (thumbnailSize)
  {
    case ctkDICOMStudyItemWidget::ThumbnailSizeOption::Small:
    {
      thumbnailSizeInPixel = floor(height / 7.);
    }
    break;
    case ctkDICOMStudyItemWidget::ThumbnailSizeOption::Medium:
    {
      thumbnailSizeInPixel = floor(height / 5.5);
    }
    break;
    case ctkDICOMStudyItemWidget::ThumbnailSizeOption::Large:
    {
      thumbnailSizeInPixel = floor(height / 4.);
    }
    break;
  }

  return thumbnailSizeInPixel;
}

//------------------------------------------------------------------------------
void ctkDICOMStudyItemWidgetPrivate::updateSeriesSelectionInternal()
{
  this->PreviousSeriesSelection = this->CurrentSeriesSelection;
  QItemSelectionModel* selectionModel = this->SeriesListTableView->selectionModel();
  if (selectionModel)
  {
    this->CurrentSeriesSelection = selectionModel->selectedIndexes();
  }
}

//----------------------------------------------------------------------------
// ctkDICOMStudyItemWidget methods

//----------------------------------------------------------------------------
ctkDICOMStudyItemWidget::ctkDICOMStudyItemWidget(QWidget* parent)
  : Superclass(parent)
  , d_ptr(new ctkDICOMStudyItemWidgetPrivate(*this))
{
  Q_D(ctkDICOMStudyItemWidget);
  d->init(parent);
}

//----------------------------------------------------------------------------
ctkDICOMStudyItemWidget::~ctkDICOMStudyItemWidget()
{
}

//------------------------------------------------------------------------------
CTK_SET_CPP(ctkDICOMStudyItemWidget, const QStringList&, setAllowedServers, AllowedServers);
CTK_GET_CPP(ctkDICOMStudyItemWidget, QStringList, allowedServers, AllowedServers);
CTK_SET_CPP(ctkDICOMStudyItemWidget, const OperationStatus&, setOperationStatus, Status);
CTK_GET_CPP(ctkDICOMStudyItemWidget, ctkDICOMStudyItemWidget::OperationStatus, operationStatus, Status);
CTK_SET_CPP(ctkDICOMStudyItemWidget, const QString&, setStudyItem, StudyItem);
CTK_GET_CPP(ctkDICOMStudyItemWidget, QString, studyItem, StudyItem);
CTK_SET_CPP(ctkDICOMStudyItemWidget, const QString&, setPatientID, PatientID);
CTK_GET_CPP(ctkDICOMStudyItemWidget, QString, patientID, PatientID);
CTK_SET_CPP(ctkDICOMStudyItemWidget, const QString&, setStudyInstanceUID, StudyInstanceUID);
CTK_GET_CPP(ctkDICOMStudyItemWidget, QString, studyInstanceUID, StudyInstanceUID);
CTK_GET_CPP(ctkDICOMStudyItemWidget, ctkDICOMStudyItemWidget::ThumbnailSizeOption, thumbnailSize, ThumbnailSize);
CTK_GET_CPP(ctkDICOMStudyItemWidget, int, thumbnailSizePixel, ThumbnailSizePixel);
CTK_SET_CPP(ctkDICOMStudyItemWidget, const QString&, setFilteringSeriesDescription, FilteringSeriesDescription);
CTK_GET_CPP(ctkDICOMStudyItemWidget, QString, filteringSeriesDescription, FilteringSeriesDescription);
CTK_SET_CPP(ctkDICOMStudyItemWidget, const QStringList&, setFilteringModalities, FilteringModalities);
CTK_GET_CPP(ctkDICOMStudyItemWidget, QStringList, filteringModalities, FilteringModalities);
CTK_GET_CPP(ctkDICOMStudyItemWidget, int, filteredSeriesCount, FilteredSeriesCount);
CTK_GET_CPP(ctkDICOMStudyItemWidget, QString, stoppedJobUID, StoppedJobUID);

//----------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::setTitle(const QString& title)
{
  Q_D(ctkDICOMStudyItemWidget);

  QString studyIDText;
  QString elidedText = title;
  QString truncatedText = "";
  QString studyIDSearchString = "  -  ID:";
  int index = title.indexOf(studyIDSearchString);
  if (index != -1)
  {
    studyIDText = title.mid(index);
    elidedText = title.left(index);
  }

  QFontMetrics metrics(d->StudyItemCollapsibleGroupBox->font());
  int textWidth = metrics.horizontalAdvance(elidedText);
  int widgetWidth = this->width();
  if (textWidth > widgetWidth)
  {
    elidedText = metrics.elidedText(elidedText, Qt::ElideRight, widgetWidth);
    int ellipsisPos = elidedText.indexOf("…");
    if (ellipsisPos != -1)
    {
      truncatedText = title.mid(ellipsisPos + 3);
      elidedText += "    ";
    }
  }

  d->StudyItemCollapsibleGroupBox->setTitle(elidedText);
  if (truncatedText.isEmpty())
  {
    studyIDText.replace(" - ", "");
    d->StudyItemCollapsibleGroupBox->setToolTip(studyIDText);
  }
  else
  {
    d->StudyItemCollapsibleGroupBox->setToolTip(title);
  }
}

//------------------------------------------------------------------------------
QString ctkDICOMStudyItemWidget::title() const
{
  Q_D(const ctkDICOMStudyItemWidget);
  return d->StudyItemCollapsibleGroupBox->title();
}

//----------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::setCollapsed(bool collapsed)
{
  Q_D(ctkDICOMStudyItemWidget);
  d->StudyItemCollapsibleGroupBox->setCollapsed(collapsed);
}

//------------------------------------------------------------------------------
bool ctkDICOMStudyItemWidget::collapsed() const
{
  Q_D(const ctkDICOMStudyItemWidget);
  return d->StudyItemCollapsibleGroupBox->collapsed();
}

//------------------------------------------------------------------------------
int ctkDICOMStudyItemWidget::numberOfSeriesPerRow() const
{
  Q_D(const ctkDICOMStudyItemWidget);
  return d->calculateNumerOfSeriesPerRow();
}

//------------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::setThumbnailSize(const ctkDICOMStudyItemWidget::ThumbnailSizeOption& thumbnailSize)
{
  Q_D(ctkDICOMStudyItemWidget);
  d->ThumbnailSize = thumbnailSize;
  d->ThumbnailSizePixel = d->calculateThumbnailSizeInPixel(d->ThumbnailSize);

  // Only update the model - delegate will query the model for size
  if (d->SeriesModel)
  {
    d->SeriesModel->setThumbnailSize(d->ThumbnailSizePixel);
  }
}

//------------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::setSelection(bool selected)
{
  Q_D(ctkDICOMStudyItemWidget);
  
  if (d->SeriesListTableView && d->SeriesListTableView->selectionModel())
  {
    if (selected)
    {
      d->SeriesListTableView->selectAll();
    }
    else
    {
      d->SeriesListTableView->clearSelection();
    }
  }

  d->StudySelectionCheckBox->setChecked(selected);
}

//------------------------------------------------------------------------------
bool ctkDICOMStudyItemWidget::selection() const
{
  Q_D(const ctkDICOMStudyItemWidget);
  return d->StudySelectionCheckBox->isChecked();
}

//----------------------------------------------------------------------------
ctkDICOMScheduler* ctkDICOMStudyItemWidget::scheduler() const
{
  Q_D(const ctkDICOMStudyItemWidget);
  return d->Scheduler.data();
}

//----------------------------------------------------------------------------
QSharedPointer<ctkDICOMScheduler> ctkDICOMStudyItemWidget::schedulerShared() const
{
  Q_D(const ctkDICOMStudyItemWidget);
  return d->Scheduler;
}

//----------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::setScheduler(ctkDICOMScheduler& scheduler)
{
  Q_D(ctkDICOMStudyItemWidget);
  d->Scheduler = QSharedPointer<ctkDICOMScheduler>(&scheduler, skipDelete);
  
  // Also set the scheduler on the series model for thumbnail generation
  if (d->SeriesModel)
  {
    d->SeriesModel->setScheduler(d->Scheduler);
  }
}

//----------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::setScheduler(QSharedPointer<ctkDICOMScheduler> scheduler)
{
  Q_D(ctkDICOMStudyItemWidget);
  d->Scheduler = scheduler;
  
  // Also set the scheduler on the series model for thumbnail generation
  if (d->SeriesModel)
  {
    d->SeriesModel->setScheduler(d->Scheduler);
  }
}

//----------------------------------------------------------------------------
ctkDICOMDatabase* ctkDICOMStudyItemWidget::dicomDatabase() const
{
  Q_D(const ctkDICOMStudyItemWidget);
  return d->DicomDatabase.data();
}

//----------------------------------------------------------------------------
QSharedPointer<ctkDICOMDatabase> ctkDICOMStudyItemWidget::dicomDatabaseShared() const
{
  Q_D(const ctkDICOMStudyItemWidget);
  return d->DicomDatabase;
}

//----------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::setDicomDatabase(ctkDICOMDatabase& dicomDatabase)
{
  Q_D(ctkDICOMStudyItemWidget);
  d->DicomDatabase = QSharedPointer<ctkDICOMDatabase>(&dicomDatabase, skipDelete);
  
  // Also set the database on the series model
  if (d->SeriesModel)
  {
    d->SeriesModel->setDicomDatabase(d->DicomDatabase);
  }
}

//----------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::setDicomDatabase(QSharedPointer<ctkDICOMDatabase> dicomDatabase)
{
  Q_D(ctkDICOMStudyItemWidget);
  d->DicomDatabase = dicomDatabase;
  
  // Also set the database on the series model
  if (d->SeriesModel)
  {
    d->SeriesModel->setDicomDatabase(d->DicomDatabase);
  }
}

//------------------------------------------------------------------------------
ctkDICOMSeriesTableView* ctkDICOMStudyItemWidget::seriesListTableView()
{
  Q_D(ctkDICOMStudyItemWidget);
  return d->SeriesListTableView;
}

//------------------------------------------------------------------------------
ctkDICOMSeriesModel* ctkDICOMStudyItemWidget::seriesModel()
{
  Q_D(ctkDICOMStudyItemWidget);
  return d->SeriesModel;
}

//------------------------------------------------------------------------------
QModelIndexList ctkDICOMStudyItemWidget::previousSelectedSeries() const
{
  Q_D(const ctkDICOMStudyItemWidget);
  return d->PreviousSeriesSelection;
}

//------------------------------------------------------------------------------
QModelIndexList ctkDICOMStudyItemWidget::currentSelectedSeries() const
{
  Q_D(const ctkDICOMStudyItemWidget);
  return d->CurrentSeriesSelection;
}

//------------------------------------------------------------------------------
ctkCollapsibleGroupBox* ctkDICOMStudyItemWidget::collapsibleGroupBox()
{
  Q_D(ctkDICOMStudyItemWidget);
  return d->StudyItemCollapsibleGroupBox;
}

//------------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::generateSeries(bool query, bool retrieve)
{
  Q_D(ctkDICOMStudyItemWidget);

  d->QueryOn = query;
  d->RetrieveOn = retrieve;
  d->createSeries();
  if (query && d->Scheduler &&
      d->Scheduler->queryRetrieveServersCount() > 0)
  {
    d->Scheduler->querySeries(d->PatientID,
                              d->StudyInstanceUID,
                              QThread::NormalPriority,
                              d->AllowedServers);
  }
}

//------------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::updateGUIFromScheduler(const QVariant& data)
{
  Q_D(ctkDICOMStudyItemWidget);

  ctkDICOMJobDetail td = data.value<ctkDICOMJobDetail>();
  if (td.JobUID.isEmpty())
  {
    d->createSeries();
    return;
  }

  if (td.StudyInstanceUID != d->StudyInstanceUID)
  {
    return;
  }

  if (td.JobType == ctkDICOMJobResponseSet::JobType::QuerySeries)
  {
    d->createSeries();
  }
  else if (td.JobType == ctkDICOMJobResponseSet::JobType::QueryInstances ||
           td.JobType == ctkDICOMJobResponseSet::JobType::RetrieveSOPInstance ||
           td.JobType == ctkDICOMJobResponseSet::JobType::StoreSOPInstance ||
           td.JobType == ctkDICOMJobResponseSet::JobType::ThumbnailGenerator ||
           td.JobType == ctkDICOMJobResponseSet::JobType::RetrieveSeries)
  {
  }
}

//------------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::onJobStarted(const QVariant &data)
{
  Q_D(ctkDICOMStudyItemWidget);

  ctkDICOMJobDetail td = data.value<ctkDICOMJobDetail>();
  if (td.JobUID.isEmpty() ||
      td.StudyInstanceUID != d->StudyInstanceUID)
  {
    return;
  }

  if (td.JobType == ctkDICOMJobResponseSet::JobType::QuerySeries)
  {
    d->Status = ctkDICOMStudyItemWidget::InProgress;
    d->OperationStatusPushButton->show();
    d->OperationStatusPushButton->setIcon(QIcon(":/Icons/pending.svg"));
  }
  else if (td.JobType == ctkDICOMJobResponseSet::JobType::QueryInstances ||
           td.JobType == ctkDICOMJobResponseSet::JobType::RetrieveSOPInstance ||
           td.JobType == ctkDICOMJobResponseSet::JobType::RetrieveSeries)
  {
  }
}

//------------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::onJobUserStopped(const QVariant &data)
{
  Q_D(ctkDICOMStudyItemWidget);

  ctkDICOMJobDetail td = data.value<ctkDICOMJobDetail>();
  if (td.JobUID.isEmpty() ||
      td.StudyInstanceUID != d->StudyInstanceUID)
  {
    return;
  }

  if (td.JobType == ctkDICOMJobResponseSet::JobType::QuerySeries)
  {
    d->Status = ctkDICOMStudyItemWidget::Failed;
    d->OperationStatusPushButton->setIcon(QIcon(":/Icons/error_red.svg"));
    d->StoppedJobUID = td.JobUID;
  }
  else if (td.JobType == ctkDICOMJobResponseSet::JobType::QueryInstances ||
           td.JobType == ctkDICOMJobResponseSet::JobType::RetrieveSOPInstance ||
           td.JobType == ctkDICOMJobResponseSet::JobType::RetrieveSeries)
  {
  }
}

//------------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::onJobFailed(const QVariant &data)
{
  Q_D(ctkDICOMStudyItemWidget);

  ctkDICOMJobDetail td = data.value<ctkDICOMJobDetail>();
  if (td.JobUID.isEmpty() ||
      td.StudyInstanceUID != d->StudyInstanceUID)
  {
    return;
  }

  if (td.JobType == ctkDICOMJobResponseSet::JobType::QuerySeries)
  {
    d->Status = ctkDICOMStudyItemWidget::Failed;
    d->OperationStatusPushButton->setIcon(QIcon(":/Icons/error_red.svg"));
    d->StoppedJobUID = td.JobUID;
  }
  else if (td.JobType == ctkDICOMJobResponseSet::JobType::QueryInstances ||
           td.JobType == ctkDICOMJobResponseSet::JobType::RetrieveSOPInstance ||
           td.JobType == ctkDICOMJobResponseSet::JobType::RetrieveSeries)
  {
  }
}

//------------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::onJobFinished(const QVariant &data)
{
  Q_D(ctkDICOMStudyItemWidget);

  ctkDICOMJobDetail td = data.value<ctkDICOMJobDetail>();
  if (td.JobUID.isEmpty() ||
      td.StudyInstanceUID != d->StudyInstanceUID)
  {
    return;
  }

  if (td.JobType == ctkDICOMJobResponseSet::JobType::QuerySeries)
  {
    d->Status = ctkDICOMStudyItemWidget::Completed;
    d->OperationStatusPushButton->setIcon(QIcon(":/Icons/accept.svg"));
  }
  else if (td.JobType == ctkDICOMJobResponseSet::JobType::RetrieveSeries ||
           td.JobType == ctkDICOMJobResponseSet::JobType::RetrieveSOPInstance)
  {

  }
  else if (td.JobType == ctkDICOMJobResponseSet::JobType::ThumbnailGenerator)
  {
    // Forward thumbnail generation completion to the series model
    if (d->SeriesModel)
    {
      d->SeriesModel->onJobFinished(data);
    }
  }
}

//------------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::onStudySelectionClicked(bool toggled)
{
  this->setSelection(toggled);
}

//------------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::onOperationStatusButtonClicked(bool)
{
  Q_D(ctkDICOMStudyItemWidget);

  ctkDICOMStudyItemWidget::OperationStatus status = d->Status;
  if (status == ctkDICOMStudyItemWidget::InProgress)
  {
    d->Scheduler->stopJobsByDICOMUIDs(QStringList(),
                                      QStringList(d->StudyInstanceUID));
  }
  else if (status > ctkDICOMStudyItemWidget::InProgress)
  {
    if (!d->Scheduler->retryJob(d->StoppedJobUID))
    {
    logger.info(QString("Unable to restart job %1 (study level) because the job has been fully cleared from the system. "
                        "Please initiate a new job if further processing is required.").arg(d->StoppedJobUID));
    }
  }
}

//------------------------------------------------------------------------------
void ctkDICOMStudyItemWidget::onSeriesSelectionChanged()
{
  Q_D(ctkDICOMStudyItemWidget);
  d->updateSeriesSelectionInternal();
}
