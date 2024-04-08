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
#include <QPushButton>
#include <QTableWidget>

// CTK includes
#include <ctkLogger.h>
#include <ctkMessageBox.h>

// ctkDICOMCore includes
#include "ctkDICOMDatabase.h"
#include "ctkDICOMJob.h"
#include "ctkDICOMJobResponseSet.h"
#include "ctkDICOMScheduler.h"

// ctkDICOMWidgets includes
#include "ctkDICOMSeriesItemWidget.h"
#include "ctkDICOMPatientItemWidget.h"
#include "ui_ctkDICOMPatientItemWidget.h"

static ctkLogger logger("org.commontk.DICOM.Widgets.ctkDICOMPatientItemWidget");

//----------------------------------------------------------------------------
static void skipDelete(QObject* obj)
{
  Q_UNUSED(obj);
  // this deleter does not delete the object from memory
  // useful if the pointer is not owned by the smart pointer
}

//----------------------------------------------------------------------------
class ctkDICOMPatientItemWidgetPrivate : public Ui_ctkDICOMPatientItemWidget
{
  Q_DECLARE_PUBLIC(ctkDICOMPatientItemWidget);

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
  void createStudies(bool queryRetrieve = true);
  void updateEnabledServersUIFromDB();
  bool areAllActiveServersSecure();
  void setAllInsecureServerEnabledStatus(bool enabled);
  bool askUserActionForServerSecurity();
  void saveEnabledServersStringListFromUI();
  void saveEnabledServersStringListToChildren(const QStringList& enabledServers);

  QSharedPointer<ctkDICOMDatabase> DicomDatabase;
  QSharedPointer<ctkDICOMScheduler> Scheduler;
  QSharedPointer<QWidget> VisualDICOMBrowser;

  int NumberOfStudiesPerPatient;
  ctkDICOMStudyItemWidget::ThumbnailSizeOption ThumbnailSize;

  QString PatientItem;
  QString PatientID;
  QString PatientName;

  QString FilteringStudyDescription;
  ctkDICOMPatientItemWidget::DateType FilteringDate;

  QString FilteringSeriesDescription;
  QStringList FilteringModalities;

  QList<ctkDICOMStudyItemWidget*> StudyItemWidgetsList;
  QMap<QString, QMetaObject::Connection> StudyItemWidgetsConnectionMap;
  QSpacerItem* StudiesListVerticalSpacer;

  QStringList EnabledServers = QStringList("All");

  bool IsGUIUpdating;
};

//----------------------------------------------------------------------------
// ctkDICOMPatientItemWidgetPrivate methods

//----------------------------------------------------------------------------
ctkDICOMPatientItemWidgetPrivate::ctkDICOMPatientItemWidgetPrivate(ctkDICOMPatientItemWidget& obj)
  : q_ptr(&obj)
{
  this->FilteringDate = ctkDICOMPatientItemWidget::DateType::Any;
  this->NumberOfStudiesPerPatient = 2;
  this->ThumbnailSize = ctkDICOMStudyItemWidget::ThumbnailSizeOption::Medium;
  this->PatientItem = "";
  this->PatientID = "";
  this->PatientName = "";
  this->FilteringStudyDescription = "";
  this->FilteringSeriesDescription = "";

  this->DicomDatabase = nullptr;
  this->Scheduler = nullptr;
  this->VisualDICOMBrowser = nullptr;

  this->StudiesListVerticalSpacer = new QSpacerItem(0, 5, QSizePolicy::Fixed, QSizePolicy::Expanding);

  this->IsGUIUpdating = false;
}

//----------------------------------------------------------------------------
ctkDICOMPatientItemWidgetPrivate::~ctkDICOMPatientItemWidgetPrivate()
{
  QLayout* StudiesListWidgetLayout = this->StudiesListWidget->layout();
  this->clearLayout(StudiesListWidgetLayout);
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidgetPrivate::init(QWidget* parentWidget)
{
  Q_Q(ctkDICOMPatientItemWidget);
  this->setupUi(q);

  this->VisualDICOMBrowser = QSharedPointer<QWidget>(parentWidget, skipDelete);

  this->PatientNameValueLabel->setWordWrap(true);
  this->PatientIDValueLabel->setWordWrap(true);
  this->PatientBirthDateValueLabel->setWordWrap(true);
  this->PatientSexValueLabel->setWordWrap(true);

  q->connect(this->PatientServersCheckableComboBox, SIGNAL(checkedIndexesChanged()),
             q, SLOT(onPatientServersCheckableComboBoxChanged()));
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
bool ctkDICOMPatientItemWidgetPrivate::isStudyItemAlreadyAdded(const QString& studyItem)
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
void ctkDICOMPatientItemWidgetPrivate::clearLayout(QLayout* layout, bool deleteWidgets)
{
  if (!layout)
  {
    return;
  }

  this->StudyItemWidgetsList.clear();
  this->StudyItemWidgetsConnectionMap.clear();

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
void ctkDICOMPatientItemWidgetPrivate::createStudies(bool queryRetrieve)
{
  Q_Q(ctkDICOMPatientItemWidget);
  if (this->IsGUIUpdating)
  {
    return;
  }

  if (!this->DicomDatabase)
  {
    logger.error("createStudies failed, no DICOM Database has been set. \n");
    return;
  }

  QLayout* studiesListWidgetLayout = this->StudiesListWidget->layout();
  if (this->PatientItem.isEmpty())
  {
    this->PatientNameValueLabel->setText("");
    this->PatientIDValueLabel->setText("");
    this->PatientSexValueLabel->setText("");
    this->PatientBirthDateValueLabel->setText("");
    return;
  }
  else
  {
    QString patientName = this->DicomDatabase->fieldForPatient("PatientsName", this->PatientItem);
    patientName.replace(R"(^)", R"( )");
    this->PatientNameValueLabel->setText(patientName);
    this->PatientIDValueLabel->setText(this->DicomDatabase->fieldForPatient("PatientID", this->PatientItem));
    this->PatientSexValueLabel->setText(this->DicomDatabase->fieldForPatient("PatientsSex", this->PatientItem));
    this->PatientBirthDateValueLabel->setText(this->formatDate(this->DicomDatabase->fieldForPatient("PatientsBirthDate", this->PatientItem)));
  }

  QStringList studiesList = this->DicomDatabase->studiesForPatient(this->PatientItem);
  if (studiesList.count() == 0)
  {
    return;
  }

  this->IsGUIUpdating = true;

  studiesListWidgetLayout->removeItem(this->StudiesListVerticalSpacer);
  // Remove study widgets from vertical layout (need sorting)
  for (int studyIndex = 0; studyIndex < this->StudyItemWidgetsList.size(); ++studyIndex)
  {
    ctkDICOMStudyItemWidget* studyItemWidget =
        qobject_cast<ctkDICOMStudyItemWidget*>(this->StudyItemWidgetsList[studyIndex]);
    if (!studyItemWidget)
    {
      continue;
    }

    this->StudiesListWidget->layout()->removeWidget(studyItemWidget);
  }

  // Filter with studyDescription and studyDate
  foreach (QString studyItem, studiesList)
  {
    if (this->isStudyItemAlreadyAdded(studyItem))
    {
      continue;
    }

    QString studyInstanceUID = this->DicomDatabase->fieldForStudy("StudyInstanceUID", studyItem);
    if (studyInstanceUID.isEmpty())
    {
      continue;
    }

    QString studyDateString = this->DicomDatabase->fieldForStudy("StudyDate", studyItem);
    studyDateString.replace(QString("-"), QString(""));
    QString studyDescription = this->DicomDatabase->fieldForStudy("StudyDescription", studyItem);

    if (studyDateString.isEmpty())
    {
      studyDateString = QDate::currentDate().toString("yyyyMMdd");
    }

    if ((!this->FilteringStudyDescription.isEmpty() &&
         !studyDescription.contains(this->FilteringStudyDescription, Qt::CaseInsensitive)))
    {
      continue;
    }

    int nDays = ctkDICOMPatientItemWidget::getNDaysFromFilteringDate(this->FilteringDate);
    QDate studyDate = QDate::fromString(studyDateString, "yyyyMMdd");
    if (nDays != -1)
    {
      QDate endDate = QDate::currentDate();
      QDate startDate = endDate.addDays(-nDays);
      if (studyDate < startDate || studyDate > endDate)
      {
        continue;
      }
    }

    q->addStudyItemWidget(studyItem);
  }

  // sort by date
  QMap<long long, ctkDICOMStudyItemWidget*> studiesMap;
  for (int studyIndex = 0; studyIndex < this->StudyItemWidgetsList.size(); ++studyIndex)
  {
    ctkDICOMStudyItemWidget* studyItemWidget =
        qobject_cast<ctkDICOMStudyItemWidget*>(this->StudyItemWidgetsList[studyIndex]);
    if (!studyItemWidget)
    {
      continue;
    }

    QString studyItem = studyItemWidget->studyItem();
    QString studyDateString = this->DicomDatabase->fieldForStudy("StudyDate", studyItem);
    studyDateString.replace(QString("-"), QString(""));
    QDate studyDate = QDate::fromString(studyDateString, "yyyyMMdd");
    long long julianDate = studyDate.toJulianDay();
    // Add some significance zeros to the sorting key
    julianDate *= 100;
    // QMap automatically sort in ascending with the key,
    // but we want descending (latest study should be in the first row)
    long long key = LLONG_MAX - julianDate;
    // Increase the key if JulianDay is already present
    while (studiesMap.contains(key))
    {
      key++;
    }

    studiesMap[key] = studyItemWidget;
  }

  int cont = 0;
  foreach (ctkDICOMStudyItemWidget* studyItemWidget, studiesMap)
  {
    studiesListWidgetLayout->addWidget(studyItemWidget);
    if (cont < this->NumberOfStudiesPerPatient)
    {
      studyItemWidget->setCollapsed(false);
      studyItemWidget->generateSeries(queryRetrieve);
    }
    cont++;

    QString studyItem = studyItemWidget->studyItem();
    if (!this->StudyItemWidgetsConnectionMap.contains(studyItem))
    {
      auto toggleHandler = [q, studyItem](bool toggled) {
        q->generateSeriesAtToggle(toggled, studyItem);
      };
      QMetaObject::Connection m_connection;
      m_connection = q->connect(studyItemWidget->collapsibleGroupBox(), &QGroupBox::toggled, q, toggleHandler);
      this->StudyItemWidgetsConnectionMap.insert(studyItem, m_connection);
    }
  }

  studiesListWidgetLayout->addItem(this->StudiesListVerticalSpacer);

  this->IsGUIUpdating = false;
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidgetPrivate::updateEnabledServersUIFromDB()
{
  if (!this->DicomDatabase)
  {
    logger.error("updateEnabledServersUIFromDB, no DICOM Database has been set. \n");
    return;
  }

  if (this->PatientItem.isEmpty())
  {
    logger.error("updateEnabledServersUIFromDB, PatientItem has not been set. \n");
    return;
  }

  if (!this->Scheduler)
  {
    logger.error("updateEnabledServersUIFromDB, no scheduler has been set. \n");
    return;
  }

  // All active servers (either query/retrieve or storage is toggled).
  // We assume that the security by default is unknown (patially checked in the UI).
  // before running any query/retrieve/storage operation the UI needs to ask to the user permissions
  QStringList allActiveConnectionNames = this->Scheduler->getConnectionNamesForActiveServers();
  if (allActiveConnectionNames.count() == 0)
  {
    this->PatientServersCheckableComboBox->clear();
    return;
  }

  // connectionNamesFromDB contains connection names from where the patient has been fetched.
  // We assume that such server is secure by default
  QStringList allowList = this->DicomDatabase->enabledConnectionsForPatient(this->PatientItem);
  QStringList denyList = this->DicomDatabase->disabledConnectionsForPatient(this->PatientItem);
  QAbstractItemModel* model = this->PatientServersCheckableComboBox->checkableModel();
  int wasBlocking = this->PatientServersCheckableComboBox->blockSignals(true);

  this->PatientServersCheckableComboBox->clear();
  this->PatientServersCheckableComboBox->addItems(allActiveConnectionNames);
  this->EnabledServers.clear();
  for (int filterIndex = 0; filterIndex < this->PatientServersCheckableComboBox->count(); ++filterIndex)
  {
    QString connectionName = this->PatientServersCheckableComboBox->itemText(filterIndex);
    QModelIndex modelIndex = model->index(filterIndex, 0);

    Qt::CheckState checkState = Qt::CheckState::PartiallyChecked;
    if (allowList.contains(connectionName))
    {
      checkState = Qt::CheckState::Checked;
    }
    else if (denyList.contains(connectionName))
    {
      checkState = Qt::CheckState::Unchecked;
    }

    this->PatientServersCheckableComboBox->setCheckState(modelIndex, checkState);

    if (checkState != Qt::CheckState::Unchecked)
    {
      this->EnabledServers.append(connectionName);
    }
  }
  this->PatientServersCheckableComboBox->blockSignals(wasBlocking);

  this->saveEnabledServersStringListToChildren(this->EnabledServers);
}

//----------------------------------------------------------------------------
bool ctkDICOMPatientItemWidgetPrivate::areAllActiveServersSecure()
{
  QAbstractItemModel* model = this->PatientServersCheckableComboBox->checkableModel();
  for (int filterIndex = 0; filterIndex < this->PatientServersCheckableComboBox->count(); ++filterIndex)
  {
    QModelIndex modelIndex = model->index(filterIndex, 0);
    Qt::CheckState checkState = this->PatientServersCheckableComboBox->checkState(modelIndex);
    if (checkState == Qt::CheckState::PartiallyChecked)
    {
      return false;
    }
  }

  return true;
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidgetPrivate::setAllInsecureServerEnabledStatus(bool enabled)
{
  bool wasModifying = this->PatientServersCheckableComboBox->blockSignals(true);
  QAbstractItemModel* model = this->PatientServersCheckableComboBox->checkableModel();
  for (int filterIndex = 0; filterIndex < this->PatientServersCheckableComboBox->count(); ++filterIndex)
  {
    QModelIndex modelIndex = model->index(filterIndex, 0);
    Qt::CheckState checkState = this->PatientServersCheckableComboBox->checkState(modelIndex);
    if (checkState != Qt::CheckState::PartiallyChecked)
    {
      continue;
    }

    checkState = enabled ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;
    this->PatientServersCheckableComboBox->setCheckState(modelIndex, checkState);
  }
  this->PatientServersCheckableComboBox->blockSignals(wasModifying);
  this->saveEnabledServersStringListFromUI();
}

//----------------------------------------------------------------------------
bool ctkDICOMPatientItemWidgetPrivate::askUserActionForServerSecurity()
{
  Q_Q(ctkDICOMPatientItemWidget);

  bool secure = this->areAllActiveServersSecure();
  if (!secure)
  {
    QString warningString = q->tr("One or more active servers have been marked with an unrecognized security level. \n"
                                  "A server is considered insecure if it deviates from the cached source of patient data "
                                  "server stored in the local DICOM database.");
    ctkMessageBox serversMessageBox;
    serversMessageBox.setText(warningString);
    serversMessageBox.setIcon(QMessageBox::Warning);
    serversMessageBox.setStandardButtons(QMessageBox::Cancel | QMessageBox::YesAll | QMessageBox::NoAll | QMessageBox::Ok);
    serversMessageBox.setDefaultButton(QMessageBox::No);
    QPushButton *cancelAllButton = qobject_cast<QPushButton *>(serversMessageBox.button(QMessageBox::Cancel));
    QPushButton *yesAllButton = qobject_cast<QPushButton *>(serversMessageBox.button(QMessageBox::YesAll));
    QPushButton *noAllButton = qobject_cast<QPushButton *>(serversMessageBox.button(QMessageBox::NoAll));
    QPushButton *okButton = qobject_cast<QPushButton *>(serversMessageBox.button(QMessageBox::Ok));
    if (cancelAllButton)
      {
      cancelAllButton->setToolTip(q->tr("Cancel the operation"));
      }
    if (yesAllButton)
      {
      yesAllButton->setToolTip(q->tr("The visual DICOM browser will enable all insecure servers associated with this patient."));
      }
    if (noAllButton)
      {
      noAllButton->setToolTip(q->tr("The visual DICOM browser will disable all insecure servers associated with this patient."));
      }
    if (okButton)
      {
      okButton->setToolTip(q->tr("The visual DICOM browser will query/retrieve data from all active servers, "
                                 "including the insecure ones, without altering the current settings."));
      }

    int response = serversMessageBox.exec();
    if (response == QMessageBox::Cancel)
      {
      return false;
      }
    else if (response == QMessageBox::YesAll)
      {
      this->setAllInsecureServerEnabledStatus(true);
      }
    else if (response == QMessageBox::NoAll)
      {
      this->setAllInsecureServerEnabledStatus(false);
      }
  }

  return true;
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidgetPrivate::saveEnabledServersStringListFromUI()
{
  QStringList enabledServers;
  QStringList allowList;
  QStringList denyList;
  QAbstractItemModel* model = this->PatientServersCheckableComboBox->checkableModel();
  for (int filterIndex = 0; filterIndex < this->PatientServersCheckableComboBox->count(); ++filterIndex)
  {
    QModelIndex modelIndex = model->index(filterIndex, 0);
    Qt::CheckState checkState = this->PatientServersCheckableComboBox->checkState(modelIndex);
    QString connectionName = this->PatientServersCheckableComboBox->itemText(filterIndex);
    if (checkState == Qt::CheckState::Unchecked)
    {
      denyList.append(connectionName);
    }
    else if (checkState == Qt::CheckState::Checked)
    {
      allowList.append(connectionName);
    }

    if (checkState != Qt::CheckState::Unchecked)
    {
      enabledServers.append(connectionName);
    }
  }

  this->DicomDatabase->updateConnectionsForPatient(this->PatientItem, allowList, denyList);
  this->EnabledServers = enabledServers;
  this->saveEnabledServersStringListToChildren(this->EnabledServers);
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidgetPrivate::saveEnabledServersStringListToChildren(const QStringList& enabledServers)
{
  foreach (ctkDICOMStudyItemWidget* studyItemWidget, this->StudyItemWidgetsList)
  {
    if (!studyItemWidget)
    {
      continue;
    }

    studyItemWidget->setEnabledServers(enabledServers);

    foreach (ctkDICOMSeriesItemWidget* seriesItemWidget, studyItemWidget->seriesItemWidgetsList())
    {
      if (!seriesItemWidget)
      {
        continue;
      }

      seriesItemWidget->setEnabledServers(enabledServers);
    }
  }
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
void ctkDICOMPatientItemWidget::setPatientItem(const QString& patientItem)
{
  Q_D(ctkDICOMPatientItemWidget);
  d->PatientItem = patientItem;
  d->updateEnabledServersUIFromDB();
}

//------------------------------------------------------------------------------
QString ctkDICOMPatientItemWidget::patientItem() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->PatientItem;
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setPatientID(const QString& patientID)
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
void ctkDICOMPatientItemWidget::setPatientName(const QString& patientName)
{
  Q_D(ctkDICOMPatientItemWidget);
  d->PatientName = patientName;
}

//------------------------------------------------------------------------------
QString ctkDICOMPatientItemWidget::patientName() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->PatientName;
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
void ctkDICOMPatientItemWidget::setFilteringDate(const ctkDICOMPatientItemWidget::DateType& filteringDate)
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
void ctkDICOMPatientItemWidget::setFilteringModalities(const QStringList& filteringModalities)
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
void ctkDICOMPatientItemWidget::setEnabledServers(const QStringList& enabledServers)
{
  Q_D(ctkDICOMPatientItemWidget);
  d->EnabledServers = enabledServers;
}

//------------------------------------------------------------------------------
QStringList ctkDICOMPatientItemWidget::enabledServers() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->EnabledServers;
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setNumberOfStudiesPerPatient(int numberOfStudiesPerPatient)
{
  Q_D(ctkDICOMPatientItemWidget);
  d->NumberOfStudiesPerPatient = numberOfStudiesPerPatient;
}

//------------------------------------------------------------------------------
int ctkDICOMPatientItemWidget::numberOfStudiesPerPatient() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->NumberOfStudiesPerPatient;
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setThumbnailSize(const ctkDICOMStudyItemWidget::ThumbnailSizeOption& thumbnailSize)
{
  Q_D(ctkDICOMPatientItemWidget);
  d->ThumbnailSize = thumbnailSize;
}

//------------------------------------------------------------------------------
ctkDICOMStudyItemWidget::ThumbnailSizeOption ctkDICOMPatientItemWidget::thumbnailSize() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->ThumbnailSize;
}

//----------------------------------------------------------------------------
ctkDICOMScheduler* ctkDICOMPatientItemWidget::scheduler() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->Scheduler.data();
}

//----------------------------------------------------------------------------
QSharedPointer<ctkDICOMScheduler> ctkDICOMPatientItemWidget::schedulerShared() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->Scheduler;
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setScheduler(ctkDICOMScheduler& scheduler)
{
  Q_D(ctkDICOMPatientItemWidget);
  if (d->Scheduler)
  {
    QObject::disconnect(d->Scheduler.data(), SIGNAL(progressJobDetail(QVariant)),
                        this, SLOT(updateGUIFromScheduler(QVariant)));
  }

  d->Scheduler = QSharedPointer<ctkDICOMScheduler>(&scheduler, skipDelete);

  if (d->Scheduler)
  {
    QObject::connect(d->Scheduler.data(), SIGNAL(progressJobDetail(QVariant)),
                     this, SLOT(updateGUIFromScheduler(QVariant)));
  }
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::setScheduler(QSharedPointer<ctkDICOMScheduler> scheduler)
{
  Q_D(ctkDICOMPatientItemWidget);
  if (d->Scheduler)
  {
    QObject::disconnect(d->Scheduler.data(), SIGNAL(progressJobDetail(QVariant)),
                        this, SLOT(updateGUIFromScheduler(QVariant)));
  }

  d->Scheduler = scheduler;

  if (d->Scheduler)
  {
    QObject::connect(d->Scheduler.data(), SIGNAL(progressJobDetail(QVariant)),
                     this, SLOT(updateGUIFromScheduler(QVariant)));
  }
}

//----------------------------------------------------------------------------
ctkDICOMDatabase* ctkDICOMPatientItemWidget::dicomDatabase() const
{
  Q_D(const ctkDICOMPatientItemWidget);
  return d->DicomDatabase.data();
}

//----------------------------------------------------------------------------
QSharedPointer<ctkDICOMDatabase> ctkDICOMPatientItemWidget::dicomDatabaseShared() const
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
QList<ctkDICOMStudyItemWidget*> ctkDICOMPatientItemWidget::studyItemWidgetsList() const
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
void ctkDICOMPatientItemWidget::addStudyItemWidget(const QString& studyItem)
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

  ctkDICOMStudyItemWidget* studyItemWidget = new ctkDICOMStudyItemWidget(d->VisualDICOMBrowser.data());
  studyItemWidget->setStudyItem(studyItem);
  studyItemWidget->setPatientID(d->PatientID);
  studyItemWidget->setStudyInstanceUID(studyInstanceUID);
  if (formattedStudyDate.isEmpty() && studyID.isEmpty())
  {
    studyItemWidget->setTitle(tr("Study"));
  }
  else if (formattedStudyDate.isEmpty())
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
  studyItemWidget->setThumbnailSize(d->ThumbnailSize);
  studyItemWidget->setFilteringSeriesDescription(d->FilteringSeriesDescription);
  studyItemWidget->setFilteringModalities(d->FilteringModalities);
  studyItemWidget->setDicomDatabase(d->DicomDatabase);
  studyItemWidget->setScheduler(d->Scheduler);
  studyItemWidget->setEnabledServers(d->EnabledServers);
  studyItemWidget->setCollapsed(true);
  studyItemWidget->setContextMenuPolicy(Qt::CustomContextMenu);

  this->connect(studyItemWidget->seriesListTableWidget(), SIGNAL(itemDoubleClicked(QTableWidgetItem *)),
                d->VisualDICOMBrowser.data(), SLOT(onLoad()));
  this->connect(studyItemWidget, SIGNAL(customContextMenuRequested(const QPoint&)),
                d->VisualDICOMBrowser.data(), SLOT(showStudyContextMenu(const QPoint&)));
  this->connect(studyItemWidget->seriesListTableWidget(), SIGNAL(itemClicked(QTableWidgetItem *)),
                this, SLOT(onSeriesItemClicked()));
  this->connect(studyItemWidget->seriesListTableWidget(), SIGNAL(itemSelectionChanged()),
                this, SLOT(raiseSelectedSeriesJobsPriority()));

  d->StudyItemWidgetsList.append(studyItemWidget);
}

//----------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::removeStudyItemWidget(const QString& studyItem)
{
  Q_D(ctkDICOMPatientItemWidget);

  ctkDICOMStudyItemWidget* studyItemWidget = this->geStudyItemWidgetByStudyItem(studyItem);
  if (!studyItemWidget)
  {
    return;
  }

  if (d->StudyItemWidgetsConnectionMap.contains(studyItem))
    {
    this->disconnect(d->StudyItemWidgetsConnectionMap[studyItem]);
    }
  this->disconnect(studyItemWidget->seriesListTableWidget(), SIGNAL(itemDoubleClicked(QTableWidgetItem *)),
                   d->VisualDICOMBrowser.data(), SLOT(onLoad()));
  this->disconnect(studyItemWidget, SIGNAL(customContextMenuRequested(const QPoint&)),
                   d->VisualDICOMBrowser.data(), SLOT(showStudyContextMenu(const QPoint&)));
  this->disconnect(studyItemWidget->seriesListTableWidget(), SIGNAL(itemClicked(QTableWidgetItem *)),
                   this, SLOT(onSeriesItemClicked()));
  this->disconnect(studyItemWidget->seriesListTableWidget(), SIGNAL(itemSelectionChanged()),
                   this, SLOT(raiseSelectedSeriesJobsPriority()));
  d->StudyItemWidgetsList.removeOne(studyItemWidget);
  delete studyItemWidget;
}

//------------------------------------------------------------------------------
ctkDICOMStudyItemWidget* ctkDICOMPatientItemWidget::geStudyItemWidgetByStudyItem(const QString &studyItem)
{
  Q_D(ctkDICOMPatientItemWidget);

  for (int studyIndex = 0; studyIndex < d->StudyItemWidgetsList.size(); ++studyIndex)
  {
    ctkDICOMStudyItemWidget* studyItemWidget =
      qobject_cast<ctkDICOMStudyItemWidget*>(d->StudyItemWidgetsList[studyIndex]);
    if (!studyItemWidget || studyItemWidget->studyItem() != studyItem)
    {
      continue;
    }

    return studyItemWidget;
  }

  return nullptr;
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
void ctkDICOMPatientItemWidget::updateEnabledServersUIFromDB()
{
  Q_D(ctkDICOMPatientItemWidget);
  d->updateEnabledServersUIFromDB();
}

//------------------------------------------------------------------------------
bool ctkDICOMPatientItemWidget::askUserActionForServerSecurity()
{
  Q_D(ctkDICOMPatientItemWidget);
  return d->askUserActionForServerSecurity();
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::generateStudies(bool queryRetrieve)
{
  Q_D(ctkDICOMPatientItemWidget);

  if (queryRetrieve)
  {
    if (!d->askUserActionForServerSecurity())
    {
      return;
    }
  }

  d->createStudies(queryRetrieve);
  if (queryRetrieve && d->Scheduler && d->Scheduler->getNumberOfQueryRetrieveServers() > 0)
  {
    d->Scheduler->queryStudies(d->PatientID,
                               QThread::NormalPriority,
                               d->EnabledServers);
  }
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::generateSeriesAtToggle(bool toggled, const QString& studyItem)
{
  Q_D(ctkDICOMPatientItemWidget);
  if (!toggled || studyItem.isEmpty())
  {
    return;
  }

  if (!d->askUserActionForServerSecurity())
    {
      return;
    }

  ctkDICOMStudyItemWidget* studyItemWidget = this->geStudyItemWidgetByStudyItem(studyItem);
  if (!studyItemWidget)
  {
    return;
  }

  studyItemWidget->generateSeries();
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::updateGUIFromScheduler(const QVariant& data)
{
  Q_D(ctkDICOMPatientItemWidget);

  ctkDICOMJobDetail td = data.value<ctkDICOMJobDetail>();
  if (td.JobUID.isEmpty())
  {
    d->createStudies();
  }

  if (td.JobUID.isEmpty() ||
      td.JobType != ctkDICOMJobResponseSet::JobType::QueryStudies ||
      td.PatientID != d->PatientID)
  {
    return;
  }

  d->createStudies();
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::raiseSelectedSeriesJobsPriority()
{
  Q_D(ctkDICOMPatientItemWidget);

  if (!d->Scheduler || d->Scheduler->getNumberOfQueryRetrieveServers() == 0)
  {
    return;
  }

  QList<ctkDICOMSeriesItemWidget*> seriesWidgets;
  QList<ctkDICOMSeriesItemWidget*> selectedSeriesWidgets;
  foreach (ctkDICOMStudyItemWidget* studyItemWidget, d->StudyItemWidgetsList)
  {
    if (!studyItemWidget)
    {
      continue;
    }

    QTableWidget* seriesListTableWidget = studyItemWidget->seriesListTableWidget();
    for (int row = 0; row < seriesListTableWidget->rowCount(); row++)
    {
      for (int column = 0; column < seriesListTableWidget->columnCount(); column++)
      {
        ctkDICOMSeriesItemWidget* seriesItemWidget =
          qobject_cast<ctkDICOMSeriesItemWidget*>(seriesListTableWidget->cellWidget(row, column));
        seriesWidgets.append(seriesItemWidget);
      }
    }

    QList<QTableWidgetItem*> selectedItems = seriesListTableWidget->selectedItems();
    foreach (QTableWidgetItem* selectedItem, selectedItems)
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

  QStringList selectedSeriesInstanceUIDs;
  foreach (ctkDICOMSeriesItemWidget* seriesWidget, seriesWidgets)
  {
    if (!seriesWidget)
    {
      continue;
    }

    bool widgetIsSelected = selectedSeriesWidgets.contains(seriesWidget);
    if (widgetIsSelected)
    {
      selectedSeriesInstanceUIDs.append(seriesWidget->seriesInstanceUID());
    }

    seriesWidget->setRaiseJobsPriority(widgetIsSelected);
  }

  d->Scheduler->raiseJobsPriorityForSeries(selectedSeriesInstanceUIDs);
}

//------------------------------------------------------------------------------
void ctkDICOMPatientItemWidget::onPatientServersCheckableComboBoxChanged()
{
  Q_D(ctkDICOMPatientItemWidget);
  d->saveEnabledServersStringListFromUI();
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

  if (QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier))
  {
    return;
  }

  if (seriesTable->selectedItems().count() != 1)
  {
    return;
  }

  foreach (ctkDICOMStudyItemWidget* studyItemWidget, d->StudyItemWidgetsList)
  {
    if (!studyItemWidget)
    {
      continue;
    }

    QTableWidget* studySeriesTable = studyItemWidget->seriesListTableWidget();
    if (studySeriesTable == seriesTable)
    {
      continue;
    }

    studySeriesTable->clearSelection();
  }
}
