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
#include <QTableWidget>

// CTK includes
#include <ctkLogger.h>

// ctkDICOMCore includes
#include "ctkDICOMDatabase.h"
#include "ctkDICOMTaskPool.h"
#include "ctkDICOMTaskResults.h"

// ctkDICOMWidgets includes
#include "ctkDICOMSeriesItemWidget.h"
#include "ctkDICOMStudyItemWidget.h"
#include "ctkDICOMPatientItemWidget.h"
#include "ui_ctkDICOMPatientItemWidget.h"

static ctkLogger logger("org.commontk.DICOM.Widgets.ctkDICOMPatientItemWidget");

//----------------------------------------------------------------------------
class ctkDICOMPatientItemWidgetPrivate: public Ui_ctkDICOMPatientItemWidget
{
  Q_DECLARE_PUBLIC( ctkDICOMPatientItemWidget );

protected:
  ctkDICOMPatientItemWidget* const q_ptr;
  
public:
  ctkDICOMPatientItemWidgetPrivate(ctkDICOMPatientItemWidget& obj);
  ~ctkDICOMPatientItemWidgetPrivate();

  void init(QWidget* parentWidget);

  QString getPatientItemFromPatientID(const QString& patientID);
  QString formatDate(const QString&);
  bool isStudyItemAlreadyAdded(const QString& studyItem);
  void clearLayout(QLayout* layout, bool deleteWidgets = true);
  void createStudies();

  QSharedPointer<ctkDICOMDatabase> DicomDatabase;
  QSharedPointer<ctkDICOMTaskPool> TaskPool;

  int NumberOfSeriesPerRow;
  int MinimumThumbnailSize;

  QString PatientItem;
  QString PatientID;

  QString FilteringStudyDescription;
  ctkDICOMPatientItemWidget::DateType FilteringDate;

  QString FilteringSeriesDescription;
  QStringList FilteringModalities;

  QList<ctkDICOMStudyItemWidget*> StudyItemWidgetsList;

  QWidget* VisualDICOMBrowser;
};

//----------------------------------------------------------------------------
// ctkDICOMPatientItemWidgetPrivate methods

//----------------------------------------------------------------------------
ctkDICOMPatientItemWidgetPrivate::ctkDICOMPatientItemWidgetPrivate(ctkDICOMPatientItemWidget& obj)
  : q_ptr(&obj)
{
  this->NumberOfSeriesPerRow = 6;
  this->MinimumThumbnailSize = 300;

  this->DicomDatabase = nullptr;
  this->TaskPool = nullptr;
}

//----------------------------------------------------------------------------
ctkDICOMPatientItemWidgetPrivate::~ctkDICOMPatientItemWidgetPrivate()
{
  QLayout *StudiesListWidgetLayout = this->StudiesListWidget->layout();
  this->clearLayout(StudiesListWidgetLayout);
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidgetPrivate::init(QWidget* parentWidget)
{
  Q_Q(ctkDICOMPatientItemWidget);
  this->setupUi(q);

  this->VisualDICOMBrowser = parentWidget;

  this->PatientIDValueLabel->setWordWrap(true);
  this->PatientBirthDateValueLabel->setWordWrap(true);
  this->PatientSexValueLabel->setWordWrap(true);
}

//----------------------------------------------------------------------------
QString ctkDICOMPatientItemWidgetPrivate::getPatientItemFromPatientID(const QString& patientID)
{
  if (!this->DicomDatabase)
    {
    return "";
    }

  QStringList patientList = this->DicomDatabase->patients();
  foreach (QString patientItem, patientList)
    {
    QString patientItemID = this->DicomDatabase->fieldForPatient("PatientID", patientItem);

    if (patientID == patientItemID)
      {
      return patientItem;
      }
    }

  return "";
}

//----------------------------------------------------------------------------
QString ctkDICOMPatientItemWidgetPrivate::formatDate(const QString& date)
{
  QString formattedDate = date;
  formattedDate.replace(QString("-"), QString(""));
  return QDate::fromString(formattedDate, "yyyyMMdd").toString();
}

//----------------------------------------------------------------------------
bool ctkDICOMPatientItemWidgetPrivate::isStudyItemAlreadyAdded(const QString &studyItem)
{
  bool alreadyAdded = false;
  foreach (ctkDICOMStudyItemWidget* studyItemWidget, this->StudyItemWidgetsList)
    {
    if (!studyItemWidget)
      {
      continue;
      }

    if (studyItemWidget->studyItem() == studyItem)
      {
      alreadyAdded = true;
      break;
      }
    }

  return alreadyAdded;
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidgetPrivate::clearLayout(QLayout *layout, bool deleteWidgets)
{
  Q_Q(ctkDICOMPatientItemWidget);

  if (!layout)
    {
    return;
    }

  this->StudyItemWidgetsList.clear();
  foreach (ctkDICOMStudyItemWidget* studyItemWidget, this->StudyItemWidgetsList)
    {
    if (studyItemWidget == nullptr)
      {
      continue;
      }

    q->disconnect(studyItemWidget, SIGNAL(customContextMenuRequested(const QPoint&)),
                  this->VisualDICOMBrowser, SLOT(showStudyContextMenu(const QPoint&)));

    }

  while (QLayoutItem* item = layout->takeAt(0))
    {
    if (deleteWidgets)
      {
      if (QWidget* widget = item->widget())
        {
        widget->deleteLater();
        }
      }

    if (QLayout* childLayout = item->layout())
      {
      clearLayout(childLayout, deleteWidgets);
      }

    delete item;
  }
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidgetPrivate::createStudies()
{
  Q_Q(ctkDICOMPatientItemWidget);

  if (!this->DicomDatabase)
    {
    logger.error("createStudies failed, no DICOM Database has been set. \n");
    return;
    }

  QLayout *studiesListWidgetLayout = this->StudiesListWidget->layout();
  if (this->PatientItem.isEmpty())
    {
    this->PatientIDValueLabel->setText("");
    this->PatientSexValueLabel->setText("");
    this->PatientBirthDateValueLabel->setText("");
    return;
    }
  else
    {
    this->PatientIDValueLabel->setText(this->DicomDatabase->fieldForPatient("PatientID", this->PatientItem));
    this->PatientSexValueLabel->setText(this->DicomDatabase->fieldForPatient("PatientsSex", this->PatientItem));
    this->PatientBirthDateValueLabel->setText(this->formatDate(this->DicomDatabase->fieldForPatient("PatientsBirthDate", this->PatientItem)));
    }

  QStringList studyList = this->DicomDatabase->studiesForPatient(this->PatientItem);
  foreach (QString studyItem, studyList)
    {
    if (this->isStudyItemAlreadyAdded(studyItem))
      {
      continue;
      }

    QString studyDateString = this->DicomDatabase->fieldForStudy("StudyDate", studyItem);
    QString studyDescription = this->DicomDatabase->fieldForStudy("StudyDescription", studyItem);

    // Filter with studyDescription and studyDate
    if ((!this->FilteringStudyDescription.isEmpty() &&
         !studyDescription.contains(this->FilteringStudyDescription, Qt::CaseInsensitive)))
      {
      continue;
      }

    int nDays = ctkDICOMPatientItemWidget::getNDaysFromFilteringDate(this->FilteringDate);
    if (nDays != -1)
      {
      QDate endDate = QDate::currentDate();
      QDate startDate = endDate.addDays(-nDays);
      studyDateString.replace(QString("-"), QString(""));
      QDate studyDate = QDate::fromString(studyDateString, "yyyyMMdd");
      if (studyDate < startDate || studyDate > endDate)
        {
        continue;
        }
      }

    q->addStudyItemWidget(studyItem);

    if (studyItem != studyList[studyList.length() - 1])
      {
      QSpacerItem* verticalSpacer = new QSpacerItem(0, 10, QSizePolicy::Fixed, QSizePolicy::Fixed);
      studiesListWidgetLayout->addItem(verticalSpacer);
      }
    }

  QSpacerItem* verticalSpacer = new QSpacerItem(0, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
  studiesListWidgetLayout->addItem(verticalSpacer);
}

//----------------------------------------------------------------------------
// ctkDICOMPatientItemWidget methods

//----------------------------------------------------------------------------
ctkDICOMPatientItemWidget::ctkDICOMPatientItemWidget(QWidget* parentWidget)
  : Superclass(parentWidget) 
  , d_ptr(new ctkDICOMPatientItemWidgetPrivate(*this))
{
  Q_D(ctkDICOMPatientItemWidget);
  d->init(parentWidget);
}

//----------------------------------------------------------------------------
ctkDICOMPatientItemWidget::~ctkDICOMPatientItemWidget()
{
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setPatientItem(const QString &patientItem)
{
  Q_D(ctkDICOMPatientItemWidget);
  d->PatientItem = patientItem;
}

//------------------------------------------------------------------------------
QString ctkDICOMPatientItemWidget::patientItem() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->PatientItem;
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setPatientID(const QString &patientID)
{
  Q_D(ctkDICOMPatientItemWidget);
  d->PatientID = patientID;
}

//------------------------------------------------------------------------------
QString ctkDICOMPatientItemWidget::patientID() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->PatientID;
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setFilteringStudyDescription(const QString& filteringStudyDescription)
{
  Q_D(ctkDICOMPatientItemWidget);
  d->FilteringStudyDescription = filteringStudyDescription;
}

//------------------------------------------------------------------------------
QString ctkDICOMPatientItemWidget::filteringStudyDescription() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->FilteringStudyDescription;
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setFilteringDate(const ctkDICOMPatientItemWidget::DateType &filteringDate)
{
  Q_D(ctkDICOMPatientItemWidget);
  d->FilteringDate = filteringDate;
}

//------------------------------------------------------------------------------
ctkDICOMPatientItemWidget::DateType ctkDICOMPatientItemWidget::filteringDate() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->FilteringDate;
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setFilteringSeriesDescription(const QString& filteringSeriesDescription)
{
  Q_D(ctkDICOMPatientItemWidget);
  d->FilteringSeriesDescription = filteringSeriesDescription;
}

//------------------------------------------------------------------------------
QString ctkDICOMPatientItemWidget::filteringSeriesDescription() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->FilteringSeriesDescription;
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setFilteringModalities(const QStringList &filteringModalities)
{
  Q_D(ctkDICOMPatientItemWidget);
  d->FilteringModalities = filteringModalities;
}

//------------------------------------------------------------------------------
QStringList ctkDICOMPatientItemWidget::filteringModalities() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->FilteringModalities;
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setNumberOfSeriesPerRow(int numberOfSeriesPerRow)
{
  Q_D(ctkDICOMPatientItemWidget);
  d->NumberOfSeriesPerRow = numberOfSeriesPerRow;
}

//------------------------------------------------------------------------------
int ctkDICOMPatientItemWidget::numberOfSeriesPerRow() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->NumberOfSeriesPerRow;
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setMinimumThumbnailSize(int minimumThumbnailSize)
{
  Q_D(ctkDICOMPatientItemWidget);
  d->MinimumThumbnailSize = minimumThumbnailSize;
}

//----------------------------------------------------------------------------
int ctkDICOMPatientItemWidget::minimumThumbnailSize() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->MinimumThumbnailSize;
}

//----------------------------------------------------------------------------
static void skipDelete(QObject* obj)
{
  Q_UNUSED(obj);
  // this deleter does not delete the object from memory
  // useful if the pointer is not owned by the smart pointer
}

//----------------------------------------------------------------------------
ctkDICOMTaskPool* ctkDICOMPatientItemWidget::taskPool()const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->TaskPool.data();
}

//----------------------------------------------------------------------------
QSharedPointer<ctkDICOMTaskPool> ctkDICOMPatientItemWidget::taskPoolShared()const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->TaskPool;
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setTaskPool(ctkDICOMTaskPool& taskPool)
{
  Q_D(ctkDICOMPatientItemWidget);
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
void ctkDICOMPatientItemWidget::setTaskPool(QSharedPointer<ctkDICOMTaskPool> taskPool)
{
  Q_D(ctkDICOMPatientItemWidget);
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
ctkDICOMDatabase* ctkDICOMPatientItemWidget::dicomDatabase()const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->DicomDatabase.data();
}

//----------------------------------------------------------------------------
QSharedPointer<ctkDICOMDatabase> ctkDICOMPatientItemWidget::dicomDatabaseShared()const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->DicomDatabase;
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setDicomDatabase(ctkDICOMDatabase& dicomDatabase)
{
  Q_D(ctkDICOMPatientItemWidget);
  d->DicomDatabase = QSharedPointer<ctkDICOMDatabase>(&dicomDatabase, skipDelete);
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setDicomDatabase(QSharedPointer<ctkDICOMDatabase> dicomDatabase)
{
  Q_D(ctkDICOMPatientItemWidget);
  d->DicomDatabase = dicomDatabase;
}

//------------------------------------------------------------------------------
QList<ctkDICOMStudyItemWidget *> ctkDICOMPatientItemWidget::studyItemWidgetsList() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->StudyItemWidgetsList;
}

//------------------------------------------------------------------------------
int ctkDICOMPatientItemWidget::getNDaysFromFilteringDate(DateType FilteringDate)
{
  int nDays = 0;
  switch (FilteringDate)
    {
    case ctkDICOMPatientItemWidget::DateType::Any:
      nDays = -1;
      break;
    case ctkDICOMPatientItemWidget::DateType::Today:
      nDays = 0;
      break;
    case ctkDICOMPatientItemWidget::DateType::Yesterday:
      nDays = 1;
      break;
    case ctkDICOMPatientItemWidget::DateType::LastWeek:
      nDays = 7;
      break;
    case ctkDICOMPatientItemWidget::DateType::LastMonth:
      nDays = 30;
      break;
    case ctkDICOMPatientItemWidget::DateType::LastYear:
      nDays = 365;
      break;
    }

  return nDays;
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::addStudyItemWidget(const QString &studyItem)
{
  Q_D(ctkDICOMPatientItemWidget);

  if (!d->DicomDatabase)
    {
    logger.error("addStudyItemWidget failed, no DICOM Database has been set. \n");
    return;
    }

  QString studyInstanceUID = d->DicomDatabase->fieldForStudy("StudyInstanceUID", studyItem);
  QString studyID = d->DicomDatabase->fieldForStudy("StudyID", studyItem);
  QString studyDate = d->DicomDatabase->fieldForStudy("StudyDate", studyItem);
  QString formattedStudyDate = d->formatDate(studyDate);
  QString studyDescription = d->DicomDatabase->fieldForStudy("StudyDescription", studyItem);

  ctkDICOMStudyItemWidget* studyItemWidget = new ctkDICOMStudyItemWidget(d->VisualDICOMBrowser);
  studyItemWidget->setStudyItem(studyItem);
  studyItemWidget->setPatientID(d->PatientID);
  studyItemWidget->setStudyInstanceUID(studyInstanceUID);
  if (formattedStudyDate.isEmpty())
    {
    studyItemWidget->setTitle(tr("Study ID %1").arg(studyID));
    }
  else if (studyID.isEmpty())
    {
    studyItemWidget->setTitle(tr("Study --- %1").arg(formattedStudyDate));
    }
  else
    {
    studyItemWidget->setTitle(tr("Study ID  %1  ---  %2").arg(studyID).arg(formattedStudyDate));
    }

  studyItemWidget->setDescription(studyDescription);
  studyItemWidget->setNumberOfSeriesPerRow(d->NumberOfSeriesPerRow);
  if (this->parentWidget())
    {
    studyItemWidget->setThumbnailSize(std::max(int(this->parentWidget()->width() / d->NumberOfSeriesPerRow), d->MinimumThumbnailSize) * 0.94);
    }
  studyItemWidget->setFilteringSeriesDescription(d->FilteringSeriesDescription);
  studyItemWidget->setFilteringModalities(d->FilteringModalities);
  studyItemWidget->setDicomDatabase(d->DicomDatabase);
  studyItemWidget->setTaskPool(d->TaskPool);
  studyItemWidget->generateSeries();
  studyItemWidget->setContextMenuPolicy(Qt::CustomContextMenu);

  this->connect(studyItemWidget->seriesListTableWidget(), SIGNAL(itemDoubleClicked(QTableWidgetItem *)),
                d->VisualDICOMBrowser, SLOT(onLoad()));
  this->connect(studyItemWidget, SIGNAL(customContextMenuRequested(const QPoint&)),
                d->VisualDICOMBrowser, SLOT(showStudyContextMenu(const QPoint&)));
  this->connect(studyItemWidget->seriesListTableWidget(), SIGNAL(itemClicked(QTableWidgetItem *)),
                this, SLOT(onSeriesItemClicked()));
  this->connect(studyItemWidget->seriesListTableWidget(), SIGNAL(itemSelectionChanged()),
                this, SLOT(raiseRetrieveFramesTasksPriority()));

  d->StudiesListWidget->layout()->addWidget(studyItemWidget);
  d->StudyItemWidgetsList.append(studyItemWidget);
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::removeStudyItemWidget(const QString &studyItem)
{
  Q_D(ctkDICOMPatientItemWidget);

  for (int studyIndex = 0; studyIndex < d->StudyItemWidgetsList.size(); ++studyIndex)
    {
    ctkDICOMStudyItemWidget *studyItemWidget =
      qobject_cast<ctkDICOMStudyItemWidget*>(d->StudyItemWidgetsList[studyIndex]);
    if (!studyItemWidget || studyItemWidget->studyItem() != studyItem)
      {
      continue;
      }

    this->disconnect(studyItemWidget, SIGNAL(customContextMenuRequested(const QPoint&)),
                     d->VisualDICOMBrowser, SLOT(showStudyContextMenu(const QPoint&)));
    this->disconnect(studyItemWidget->seriesListTableWidget(), SIGNAL(itemClicked(QTableWidgetItem *)),
                     this, SLOT(onSeriesItemClicked()));
    this->disconnect(studyItemWidget->seriesListTableWidget(), SIGNAL(itemSelectionChanged()),
                     this, SLOT(raiseRetrieveFramesTasksPriority()));
    d->StudyItemWidgetsList.removeOne(studyItemWidget);
    delete studyItemWidget;
    break;
  }
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setSelection(bool selected)
{
  Q_D(ctkDICOMPatientItemWidget);
  foreach (ctkDICOMStudyItemWidget* studyItemWidget, d->StudyItemWidgetsList)
    {
    studyItemWidget->setSelection(selected);
    }
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::generateStudies()
{
  Q_D(ctkDICOMPatientItemWidget);

  d->createStudies();
  QStringList studiesList = d->DicomDatabase->studiesForPatient(d->PatientItem);
  if (studiesList.count() && d->TaskPool && d->TaskPool->getNumberOfQueryRetrieveServers() > 0)
    {
    d->TaskPool->queryStudies(d->PatientID, QThread::NormalPriority);
    }
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::updateGUIFromTaskPool(ctkDICOMTaskResults *taskResults)
{
  Q_D(ctkDICOMPatientItemWidget);

  if (!taskResults)
    {
    d->createStudies();
    }

  if (!taskResults ||
      taskResults->typeOfTask() != ctkDICOMTaskResults::TaskType::QueryStudies ||
      taskResults->patientID() != d->PatientID)
    {
    return;
    }

  d->createStudies();
  d->TaskPool->deleteTask(taskResults->taskUID());
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::raiseRetrieveFramesTasksPriority()
{
  Q_D(ctkDICOMPatientItemWidget);

  if (!d->TaskPool || d->TaskPool->getNumberOfQueryRetrieveServers() == 0)
    {
    logger.error("raiseRetrieveFramesTasksPriority failed, no task pool has been set. \n");
    return;
    }

  QList<ctkDICOMSeriesItemWidget *> selectedSeriesWidgets;
  foreach (ctkDICOMStudyItemWidget *studyItemWidget, d->StudyItemWidgetsList)
    {
    if (!studyItemWidget)
      {
      continue;
      }

    QTableWidget *seriesListTableWidget = studyItemWidget->seriesListTableWidget();
    QList<QTableWidgetItem*> selectedItems = seriesListTableWidget->selectedItems();
    foreach (QTableWidgetItem *selectedItem, selectedItems)
      {
      if (!selectedItem)
        {
        continue;
        }

      int row = selectedItem->row();
      int column = selectedItem->column();
      ctkDICOMSeriesItemWidget* seriesItemWidget =
        qobject_cast<ctkDICOMSeriesItemWidget*>(seriesListTableWidget->cellWidget(row, column));

      selectedSeriesWidgets.append(seriesItemWidget);
      }
    }

  if (selectedSeriesWidgets.count() == 0)
    {
    return;
    }

  d->TaskPool->lowerPriorityToAllTasks();
  foreach (ctkDICOMSeriesItemWidget* seriesWidget, selectedSeriesWidgets)
    {
    if (!seriesWidget || !seriesWidget->isCloud())
      {
      continue;
      }

    d->TaskPool->raiseRetrieveFramesTasksPriorityForSeries(
      seriesWidget->studyInstanceUID(), seriesWidget->seriesInstanceUID()
      );
    }
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::onSeriesItemClicked()
{
  Q_D(ctkDICOMPatientItemWidget);

  QTableWidget* seriesTable = qobject_cast<QTableWidget*>(sender());
  if (!seriesTable)
    {
    return;
    }

  if (QApplication::keyboardModifiers() &&
      (Qt::ControlModifier || Qt::ShiftModifier))
    {
    return;
    }

  if (seriesTable->selectedItems().count() != 1)
    {
    return;
    }

  foreach (ctkDICOMStudyItemWidget *studyItemWidget, d->StudyItemWidgetsList)
    {
    if (!studyItemWidget)
      {
      continue;
      }

    QTableWidget *studySeriesTable = studyItemWidget->seriesListTableWidget();
    if (studySeriesTable == seriesTable)
      {
      continue;
      }

    studySeriesTable->clearSelection();
    }
}
