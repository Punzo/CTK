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
#include <QCloseEvent>
#include <QDebug>
#include <QDate>
#include <QDesktopWidget>
#include <QFormLayout>
#include <QMap>
#include <QMenu>
#include <QTableWidget>

// CTK includes
#include <ctkFileDialog.h>
#include <ctkLogger.h>
#include <ctkMessageBox.h>

// ctkDICOMCore includes
#include "ctkDICOMDatabase.h"
#include "ctkDICOMIndexer.h"
#include "ctkDICOMPoolManager.h"
#include "ctkDICOMServer.h"
#include "ctkDICOMTaskResults.h"
#include "ctkUtils.h"

// ctkDICOMWidgets includes
#include "ctkDICOMObjectListWidget.h"
#include "ctkDICOMVisualBrowserWidget.h"
#include "ctkDICOMStudyItemWidget.h"
#include "ctkDICOMSeriesItemWidget.h"
#include "ui_ctkDICOMVisualBrowserWidget.h"

static ctkLogger logger("org.commontk.DICOM.Widgets.ctkDICOMVisualBrowserWidget");

class ctkDICOMMetadataDialog : public QDialog
{
public:
  ctkDICOMMetadataDialog(QWidget *parent = 0)
    : QDialog(parent)
  {
    this->setWindowFlags(Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint | Qt::Window);
    this->setModal(true);
    this->setSizeGripEnabled(true);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(0);
    this->tagListWidget = new ctkDICOMObjectListWidget();
    layout->addWidget(this->tagListWidget);
  }

  virtual ~ctkDICOMMetadataDialog()
  {
  }

  void setFileList(const QStringList& fileList)
  {
    this->tagListWidget->setFileList(fileList);
  }

  void closeEvent(QCloseEvent *evt)
  {
    // just hide the window when close button is clicked
    evt->ignore();
    this->hide();
  }

  void showEvent(QShowEvent *event)
  {
    QDialog::showEvent(event);
    // QDialog would reset window position and size when shown.
    // Restore its previous size instead (user may look at metadata
    // of different series one after the other and would be inconvenient to
    // set the desired size manually each time).
    if (!savedGeometry.isEmpty())
      {
      this->restoreGeometry(savedGeometry);
      if (this->isMaximized())
        {
        this->setGeometry(QApplication::desktop()->availableGeometry(this));
        }
      }
  }

  void hideEvent(QHideEvent *event)
  {
    this->savedGeometry = this->saveGeometry();
    QDialog::hideEvent(event);
  }

protected:
  ctkDICOMObjectListWidget* tagListWidget;
  QByteArray savedGeometry;
};

//----------------------------------------------------------------------------
class ctkDICOMVisualBrowserWidgetPrivate: public Ui_ctkDICOMVisualBrowserWidget
{
  Q_DECLARE_PUBLIC( ctkDICOMVisualBrowserWidget );

protected:
  ctkDICOMVisualBrowserWidget* const q_ptr;
  
public:
  ctkDICOMVisualBrowserWidgetPrivate(ctkDICOMVisualBrowserWidget& obj);
  ~ctkDICOMVisualBrowserWidgetPrivate();

  void init();
  void importDirectory(QString directory, ctkDICOMVisualBrowserWidget::ImportDirectoryMode mode);
  void importFiles(const QStringList& files, ctkDICOMVisualBrowserWidget::ImportDirectoryMode mode);
  void importOldSettings();
  void updateModalityCheckableComboBox();
  void updateGUIOnQueryPatient(ctkDICOMTaskResults *taskResults = nullptr);
  void updateFiltersWarnings();
  void retrieveSeries();
  bool updateServer(ctkDICOMServer* server);
  void removeAllPatientItemWidgets();
  bool isPatientTabAlreadyAdded(const QString& patientItem);
  QStringList filterPatientList(const QStringList& patientList,
                                const QString& filterName,
                                const QString& filterValue);
  QStringList filterStudyList(const QStringList& studyList,
                              const QString& filterName,
                              const QString& filterValue);
  QStringList filterSeriesList(const QStringList& seriesList,
                               const QString& filterName,
                               const QString& filterValue);
  QStringList filterSeriesList(const QStringList& seriesList,
                               const QString& filterName,
                               const QStringList& filterValues);

  // Return a sanitized version of the string that is safe to be used
  // as a filename component.
  // All non-ASCII characters are replaced, because they may be used on an internal hard disk,
  // but it may not be possible to use them on file systems of an external drive or network storage.
  QString filenameSafeString(const QString& str)
  {
    QString safeStr;
    const QString illegalChars("/\\<>:\"|?*");
    foreach (const QChar& c, str)
    {
      int asciiCode = c.toLatin1();
      if (asciiCode >= 32 && asciiCode <= 127 && !illegalChars.contains(c))
      {
        safeStr.append(c);
      }
      else
      {
        safeStr.append("_");
      }
    }
    // remove leading/trailing whitespaces
    return safeStr.trimmed();
  }

  // local count variables to keep track of the number of items
  // added to the database during an import operation
  int PatientsAddedDuringImport;
  int StudiesAddedDuringImport;
  int SeriesAddedDuringImport;
  int InstancesAddedDuringImport;
  bool IsImportFolder;
  ctkFileDialog* ImportDialog;

  QSharedPointer<ctkDICOMMetadataDialog> MetadataDialog;

  // Settings key that stores database directory
  QString DatabaseDirectorySettingsKey;

  // If database directory is specified with relative path then this directory will be used as a base
  QString DatabaseDirectoryBase;

  // Default database path to use if there is nothing in settings
  QString DefaultDatabaseDirectory;
  QString DatabaseDirectory;

  QSharedPointer<ctkDICOMDatabase> DicomDatabase;
  QSharedPointer<ctkDICOMPoolManager> PoolManager;

  QString FilteringPatientID;
  QString FilteringPatientName;

  QString FilteringStudyDescription;
  ctkDICOMPatientItemWidget::DateType FilteringDate;

  QString FilteringSeriesDescription;
  QStringList PreviousFilteringModalities;
  QStringList FilteringModalities;

  int NumberOfSeriesPerRow;
  bool SendActionVisible;
};

CTK_GET_CPP(ctkDICOMVisualBrowserWidget, QString, databaseDirectoryBase, DatabaseDirectoryBase);
CTK_SET_CPP(ctkDICOMVisualBrowserWidget, const QString&, setDatabaseDirectoryBase, DatabaseDirectoryBase);

//----------------------------------------------------------------------------
// ctkDICOMVisualBrowserWidgetPrivate methods

//----------------------------------------------------------------------------
ctkDICOMVisualBrowserWidgetPrivate::ctkDICOMVisualBrowserWidgetPrivate(ctkDICOMVisualBrowserWidget& obj)
  : q_ptr(&obj)
{
  this->PoolManager = QSharedPointer<ctkDICOMPoolManager> (new ctkDICOMPoolManager());
  this->DicomDatabase = QSharedPointer<ctkDICOMDatabase> (new ctkDICOMDatabase());
  this->PoolManager->setDicomDatabase(*this->DicomDatabase);
  this->MetadataDialog = QSharedPointer<ctkDICOMMetadataDialog> (new ctkDICOMMetadataDialog());
  this->MetadataDialog->setObjectName("DICOMMetadata");
  this->MetadataDialog->setWindowTitle(ctkDICOMVisualBrowserWidget::tr("DICOM File Metadata"));

  this->NumberOfSeriesPerRow = 6;
  this->SendActionVisible = false;

  this->FilteringModalities.append("Any");
  this->FilteringModalities.append("CR");
  this->FilteringModalities.append("CT");
  this->FilteringModalities.append("MR");
  this->FilteringModalities.append("NM");
  this->FilteringModalities.append("US");
  this->FilteringModalities.append("PT");
  this->FilteringModalities.append("XA");

  this->PatientsAddedDuringImport = 0;
  this->StudiesAddedDuringImport = 0;
  this->SeriesAddedDuringImport = 0;
  this->InstancesAddedDuringImport = 0;
  this->IsImportFolder = false;
  this->ImportDialog = nullptr;

  this->FilteringDate = ctkDICOMPatientItemWidget::DateType::Any;
}

//----------------------------------------------------------------------------
ctkDICOMVisualBrowserWidgetPrivate::~ctkDICOMVisualBrowserWidgetPrivate()
{
  this->removeAllPatientItemWidgets();
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidgetPrivate::init()
{
  Q_Q(ctkDICOMVisualBrowserWidget);
  this->setupUi(q);

  this->WarningPushButton->hide();
  QObject::connect(this->FilteringPatientIDSearchBox, SIGNAL(textChanged(QString)),
                   q, SLOT(onFilteringPatientIDChanged()));

  QObject::connect(this->FilteringPatientNameSearchBox, SIGNAL(textChanged(QString)),
                   q, SLOT(onFilteringPatientNameChanged()));

  QObject::connect(this->FilteringStudyDescriptionSearchBox, SIGNAL(textChanged(QString)),
                   q, SLOT(onFilteringStudyDescriptionChanged()));

  QObject::connect(this->FilteringSeriesDescriptionSearchBox, SIGNAL(textChanged(QString)),
                   q, SLOT(onFilteringSeriesDescriptionChanged()));

  QObject::connect(this->FilteringModalityCheckableComboBox, SIGNAL(checkedIndexesChanged()),
                   q, SLOT(onFilteringModalityCheckableComboBoxChanged()));
  this->updateModalityCheckableComboBox();

  QObject::connect(this->FilteringDateComboBox, SIGNAL(currentIndexChanged(int)),
                   q, SLOT(onFilteringDateComboBoxChanged(int)));

  QObject::connect(this->QueryPatientPushButton, SIGNAL(clicked()),
                   q, SLOT(onQueryPatient()));

  QObject::connect(this->PoolManager.data(), SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)),
                   q, SLOT(updateGUIFromPoolManager(ctkDICOMTaskResults*)));

  QSize iconSize(32, 32);
  this->PatientsTabWidget->setIconSize(iconSize);
  this->PatientsTabWidget->tabBar()->setExpanding(true);
  this->PatientsTabWidget->clear();

  QObject::connect(this->PatientsTabWidget, SIGNAL(currentChanged(int)),
                   q, SLOT(onPatientItemChanged(int)));

  QObject::connect(this->ClosePushButton, SIGNAL(clicked()),
                   q, SLOT(onClose()));

  QObject::connect(this->LoadPushButton, SIGNAL(clicked()),
                   q, SLOT(onLoad()));

  QObject::connect(this->ImportPushButton, SIGNAL(clicked()),
                   q, SLOT(onImport()));

  // Initialize directoryMode widget
  QFormLayout *layout = new QFormLayout;
  QComboBox* importDirectoryModeComboBox = new QComboBox();
  importDirectoryModeComboBox->addItem(ctkDICOMVisualBrowserWidget::tr("Add Link"), ctkDICOMVisualBrowserWidget::ImportDirectoryAddLink);
  importDirectoryModeComboBox->addItem(ctkDICOMVisualBrowserWidget::tr("Copy"), ctkDICOMVisualBrowserWidget::ImportDirectoryCopy);
  importDirectoryModeComboBox->setToolTip(
        ctkDICOMVisualBrowserWidget::tr("Indicate if the files should be copied to the local database"
           " directory or if only links should be created ?"));
  layout->addRow(new QLabel(ctkDICOMVisualBrowserWidget::tr("Import Directory Mode:")), importDirectoryModeComboBox);
  layout->setContentsMargins(0, 0, 0, 0);
  QWidget* importDirectoryBottomWidget = new QWidget();
  importDirectoryBottomWidget->setLayout(layout);

  // Default values
  importDirectoryModeComboBox->setCurrentIndex(
        importDirectoryModeComboBox->findData(q->importDirectoryMode()));

  //Initialize import widget
  this->ImportDialog = new ctkFileDialog();
  this->ImportDialog->setBottomWidget(importDirectoryBottomWidget);
  this->ImportDialog->setFileMode(QFileDialog::Directory);
  // XXX Method setSelectionMode must be called after setFileMode
  this->ImportDialog->setSelectionMode(QAbstractItemView::ExtendedSelection);
  this->ImportDialog->setLabelText(QFileDialog::Accept, ctkDICOMVisualBrowserWidget::tr("Import"));
  this->ImportDialog->setWindowTitle(ctkDICOMVisualBrowserWidget::tr("Import DICOM files from directory ..."));
  this->ImportDialog->setWindowModality(Qt::ApplicationModal);

  q->connect(this->ImportDialog, SIGNAL(filesSelected(QStringList)),
             q,SLOT(onImportDirectoriesSelected(QStringList)));

  q->connect(importDirectoryModeComboBox, SIGNAL(currentIndexChanged(int)),
             q, SLOT(onImportDirectoryComboBoxCurrentIndexChanged(int)));

  this->ProgressFrame->hide();

  q->connect(this->ProgressCancelButton, SIGNAL(clicked()), this->PoolManager->Indexer().data(), SLOT(cancel()));
  q->connect(this->PoolManager->Indexer().data(), SIGNAL(progress(int)), q, SLOT(onIndexingProgress(int)));
  q->connect(this->PoolManager->Indexer().data(), SIGNAL(progressStep(QString)), q, SLOT(onIndexingProgressStep(QString)));
  q->connect(this->PoolManager->Indexer().data(), SIGNAL(progressDetail(QString)), q, SLOT(onIndexingProgressDetail(QString)));
  q->connect(this->PoolManager->Indexer().data(), SIGNAL(indexingComplete(int,int,int,int)), q, SLOT(onIndexingComplete(int,int,int,int)));
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidgetPrivate::importDirectory(QString directory, ctkDICOMVisualBrowserWidget::ImportDirectoryMode mode)
{
  if (!QDir(directory).exists())
    {
    return;
    }
  // Start background indexing
  this->PoolManager->Indexer()->addDirectory(directory, mode == ctkDICOMVisualBrowserWidget::ImportDirectoryCopy);
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidgetPrivate::importFiles(const QStringList &files, ctkDICOMVisualBrowserWidget::ImportDirectoryMode mode)
{
  // Start background indexing
  this->PoolManager->Indexer()->addListOfFiles(files, mode == ctkDICOMVisualBrowserWidget::ImportDirectoryCopy);
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidgetPrivate::importOldSettings()
{
  // Backward compatibility
  QSettings settings;
  int dontConfirmCopyOnImport = settings.value("MainWindow/DontConfirmCopyOnImport", static_cast<int>(QMessageBox::InvalidRole)).toInt();
  if (dontConfirmCopyOnImport == QMessageBox::AcceptRole)
  {
    settings.setValue("DICOM/ImportDirectoryMode", static_cast<int>(ctkDICOMVisualBrowserWidget::ImportDirectoryCopy));
  }
  settings.remove("MainWindow/DontConfirmCopyOnImport");
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidgetPrivate::updateModalityCheckableComboBox()
{
  QAbstractItemModel* model = this->FilteringModalityCheckableComboBox->checkableModel();
  int wasBlocking = this->FilteringModalityCheckableComboBox->blockSignals(true);

  if ((!this->PreviousFilteringModalities.contains("Any") &&
       this->FilteringModalities.contains("Any")) ||
      this->FilteringModalities.count() == 0)
    {
    this->FilteringModalities.clear();
    this->FilteringModalities.append("Any");
    this->FilteringModalities.append("CR");
    this->FilteringModalities.append("CT");
    this->FilteringModalities.append("MR");
    this->FilteringModalities.append("NM");
    this->FilteringModalities.append("US");
    this->FilteringModalities.append("PT");
    this->FilteringModalities.append("XA");

    for (int i = 0; i < this->FilteringModalityCheckableComboBox->count(); ++i)
      {
      QModelIndex modelIndex = this->FilteringModalityCheckableComboBox->checkableModel()->index(i, 0);
      this->FilteringModalityCheckableComboBox->setCheckState(modelIndex, Qt::CheckState::Checked);
      }
    this->FilteringModalityCheckableComboBox->blockSignals(wasBlocking);
    return;
    }

  for (int i = 0; i < this->FilteringModalityCheckableComboBox->count(); ++i)
    {
    QModelIndex modelIndex = this->FilteringModalityCheckableComboBox->checkableModel()->index(i, 0);
    this->FilteringModalityCheckableComboBox->setCheckState(modelIndex, Qt::CheckState::Unchecked);
    }

  foreach (QString modality, this->FilteringModalities)
    {
    QModelIndexList indexList = model->match(model->index(0,0), 0, modality);
    if (indexList.length() == 0)
      {
      continue;
      }

    QModelIndex index = indexList[0];
    this->FilteringModalityCheckableComboBox->setCheckState(index, Qt::CheckState::Checked);
    }

  if (this->FilteringModalityCheckableComboBox->allChecked())
    {
    this->FilteringModalityCheckableComboBox->blockSignals(wasBlocking);
    return;
    }

  int anyCheckState = Qt::CheckState::Unchecked;
  QModelIndex anyModelIndex = this->FilteringModalityCheckableComboBox->checkableModel()->index(0, 0);
  for (int i = 1; i < this->FilteringModalityCheckableComboBox->count(); ++i)
    {
    QModelIndex modelIndex = this->FilteringModalityCheckableComboBox->checkableModel()->index(i, 0);
    if (this->FilteringModalityCheckableComboBox->checkState(modelIndex) != Qt::CheckState::Checked)
      {
      anyCheckState = Qt::CheckState::PartiallyChecked;
      break;
      }
    }

  if (anyCheckState == Qt::CheckState::PartiallyChecked)
    {
    this->FilteringModalityCheckableComboBox->setCheckState(anyModelIndex, Qt::CheckState::PartiallyChecked);
    this->FilteringModalities.removeAll("Any");
    }
  else
    {
    this->FilteringModalityCheckableComboBox->setCheckState(anyModelIndex, Qt::CheckState::Checked);
    this->FilteringModalities.append("Any");
    }

  this->FilteringModalityCheckableComboBox->blockSignals(wasBlocking);
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidgetPrivate::updateGUIOnQueryPatient(ctkDICOMTaskResults *taskResults)
{
  Q_Q(ctkDICOMVisualBrowserWidget);


  QStringList patientList = this->DicomDatabase->patients();
  if (patientList.count() == 0)
    {
    if (taskResults)
      {
      this->PoolManager->deleteTask(taskResults->taskUID(), taskResults);
      }
    return;
    }

  int wasBlocking = this->PatientsTabWidget->blockSignals(true);
  foreach (QString patientItem, patientList)
    {
    QString patientID = this->DicomDatabase->fieldForPatient("PatientID", patientItem);
    QString patientName = this->DicomDatabase->fieldForPatient("PatientsName", patientItem);
    if (taskResults)
      {
      this->PoolManager->deleteTask(taskResults->taskUID(), taskResults);
      }

    if (this->isPatientTabAlreadyAdded(patientItem))
      {
      continue;
      }

    // Filter with patientID and patientsName
    if ((!this->FilteringPatientID.isEmpty() && !patientID.contains(this->FilteringPatientID)) ||
        (!this->FilteringPatientName.isEmpty() && !patientName.contains(this->FilteringPatientName)))
      {
      continue;
      }

      q->addPatientItemWidget(patientItem);
    }

  this->PatientsTabWidget->setCurrentIndex(0);
  this->PatientsTabWidget->blockSignals(wasBlocking);
  q->onPatientItemChanged(0);
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidgetPrivate::updateFiltersWarnings()
{
  // Loop over all the data in the dicom database and apply the filters.
  // If there are no series, highlight which are the filters that produce no results
  QString background = "QWidget { background-color: white }";
  this->FilteringPatientIDSearchBox->setStyleSheet(this->FilteringPatientIDSearchBox->styleSheet() + background);
  this->FilteringPatientNameSearchBox->setStyleSheet(this->FilteringPatientNameSearchBox->styleSheet() + background);
  this->FilteringDateComboBox->setStyleSheet(this->FilteringDateComboBox->styleSheet() + background);
  this->FilteringStudyDescriptionSearchBox->setStyleSheet(this->FilteringStudyDescriptionSearchBox->styleSheet() + background);
  this->FilteringSeriesDescriptionSearchBox->setStyleSheet(this->FilteringSeriesDescriptionSearchBox->styleSheet() + background);
  this->FilteringModalityCheckableComboBox->setStyleSheet(this->FilteringModalityCheckableComboBox->styleSheet() + background);

  background = "QWidget { background-color: yellow }";
  QStringList patientList = this->DicomDatabase->patients();
  if (patientList.count() == 0)
    {
    this->FilteringPatientIDSearchBox->setStyleSheet(this->FilteringPatientIDSearchBox->styleSheet() + background);
    this->FilteringPatientNameSearchBox->setStyleSheet(this->FilteringPatientNameSearchBox->styleSheet() + background);
    return;
    }

  QStringList filteredPatientListByName =
    this->filterPatientList(patientList, "PatientsName", this->FilteringPatientName);
  QStringList filteredPatientListByID =
    this->filterPatientList(patientList, "PatientID", this->FilteringPatientID);

  if (filteredPatientListByName.count() == 0)
    {
    this->FilteringPatientNameSearchBox->setStyleSheet(this->FilteringPatientNameSearchBox->styleSheet() + background);
    }

  if (filteredPatientListByID.count() == 0)
    {
    this->FilteringPatientIDSearchBox->setStyleSheet(this->FilteringPatientIDSearchBox->styleSheet() + background);
    }

  QStringList filteredPatientList = filteredPatientListByName + filteredPatientListByID;
  if (filteredPatientList.count() == 0)
    {
    return;
    }

  QStringList studyList;
  foreach (QString patientItem, filteredPatientList)
    {
    studyList.append(this->DicomDatabase->studiesForPatient(patientItem));
    }

  QStringList filteredStudyListByDate =
    this->filterStudyList(studyList, "StudyDate",
      QString::number(ctkDICOMPatientItemWidget::getNDaysFromFilteringDate(this->FilteringDate)));
  QStringList filteredStudyListByDescription =
    this->filterStudyList(studyList, "StudyDescription", this->FilteringStudyDescription);

  if (filteredStudyListByDate.count() == 0)
    {
     this->FilteringDateComboBox->setStyleSheet(this->FilteringDateComboBox->styleSheet() + background);
    }

  if (filteredStudyListByDescription.count() == 0)
    {
    this->FilteringStudyDescriptionSearchBox->setStyleSheet(this->FilteringStudyDescriptionSearchBox->styleSheet() + background);
    }

  QStringList filteredStudyList = filteredStudyListByDate + filteredStudyListByDescription;
  if (filteredStudyList.count() == 0)
    {
    return;
    }

  QStringList seriesList;
  foreach (QString studyItem, filteredStudyList)
    {
    QString studyInstanceUID = this->DicomDatabase->fieldForStudy("StudyInstanceUID", studyItem);
    seriesList.append(this->DicomDatabase->seriesForStudy(studyInstanceUID));
    }

  QStringList filteredSeriesListByModality =
    this->filterSeriesList(seriesList, "Modality", this->FilteringModalities);
  QStringList filteredSeriesListByDescription =
    this->filterSeriesList(seriesList, "SeriesDescription", this->FilteringSeriesDescription);

  if (filteredSeriesListByModality.count() == 0)
    {
    this->FilteringModalityCheckableComboBox->setStyleSheet(this->FilteringModalityCheckableComboBox->styleSheet() + background);
    }

  if (filteredSeriesListByDescription.count() == 0)
    {
    this->FilteringSeriesDescriptionSearchBox->setStyleSheet(this->FilteringSeriesDescriptionSearchBox->styleSheet() + background);
    }
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidgetPrivate::retrieveSeries()
{
  Q_Q(ctkDICOMVisualBrowserWidget);

  QList<ctkDICOMSeriesItemWidget*> seriesWidgetsList;
  QList<ctkDICOMSeriesItemWidget*> selectedSeriesWidgetsList;

  ctkDICOMPatientItemWidget* currentPatientItemWidget =
    qobject_cast<ctkDICOMPatientItemWidget*>(this->PatientsTabWidget->currentWidget());
  if (!currentPatientItemWidget)
    {
    return;
    }

  for (int patientIndex = 0; patientIndex < this->PatientsTabWidget->count(); ++patientIndex)
    {
    ctkDICOMPatientItemWidget *patientItemWidget =
      qobject_cast<ctkDICOMPatientItemWidget*>(this->PatientsTabWidget->widget(patientIndex));
    if (!patientItemWidget || patientItemWidget == currentPatientItemWidget)
      {
      continue;
      }

    QList<ctkDICOMStudyItemWidget *> studyItemWidgetsList = currentPatientItemWidget->studyItemWidgetsList();
    foreach (ctkDICOMStudyItemWidget* studyItemWidget, studyItemWidgetsList)
      {
      QTableWidget* seriesListTableWidget = studyItemWidget->seriesListTableWidget();
      for (int row = 0; row < seriesListTableWidget->rowCount(); row++)
        {
        for (int column = 0 ; column < seriesListTableWidget->columnCount(); column++)
          {
          ctkDICOMSeriesItemWidget* seriesItemWidget =
            qobject_cast<ctkDICOMSeriesItemWidget*>(seriesListTableWidget->cellWidget(row, column));
          if (!seriesItemWidget)
            {
            continue;
            }

          seriesWidgetsList.append(seriesItemWidget);
          }
        }
      }
    }

  QList<ctkDICOMStudyItemWidget *> studyItemWidgetsList = currentPatientItemWidget->studyItemWidgetsList();
  foreach (ctkDICOMStudyItemWidget* studyItemWidget, studyItemWidgetsList)
    {
    QTableWidget* seriesListTableWidget = studyItemWidget->seriesListTableWidget();
    QModelIndexList indexList = seriesListTableWidget->selectionModel()->selectedIndexes();
    foreach (QModelIndex index, indexList)
      {
      ctkDICOMSeriesItemWidget* seriesItemWidget = qobject_cast<ctkDICOMSeriesItemWidget*>
        (seriesListTableWidget->cellWidget(index.row(), index.column()));
      if (!seriesItemWidget)
        {
        continue;
        }

      selectedSeriesWidgetsList.append(seriesItemWidget);
      }
    }

  QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
  foreach (ctkDICOMSeriesItemWidget* seriesItemWidget, seriesWidgetsList)
    {
    if (selectedSeriesWidgetsList.indexOf(seriesItemWidget) == -1 && seriesItemWidget->isCloud())
      {
      this->PoolManager->stopTasks(seriesItemWidget->studyInstanceUID(),
                                   seriesItemWidget->seriesInstanceUID());
      }
    }

  this->PoolManager->waitForFinish();

  QStringList seriesInstanceUIDs;
  foreach (ctkDICOMSeriesItemWidget* seriesItemWidget, selectedSeriesWidgetsList)
    {
    seriesInstanceUIDs.append(seriesItemWidget->seriesInstanceUID());
    }

  q->emit seriesRetrieved(seriesInstanceUIDs);
  QApplication::restoreOverrideCursor();
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidgetPrivate::removeAllPatientItemWidgets()
{
  Q_Q(ctkDICOMVisualBrowserWidget);

  int wasBlocking = this->PatientsTabWidget->blockSignals(true);
  for (int patientIndex = 0; patientIndex < this->PatientsTabWidget->count(); ++patientIndex)
    {
    ctkDICOMPatientItemWidget *patientItemWidget =
      qobject_cast<ctkDICOMPatientItemWidget*>(this->PatientsTabWidget->widget(patientIndex));
    if (!patientItemWidget)
      {
      continue;
      }

    this->PatientsTabWidget->removeTab(patientIndex);
    q->disconnect(patientItemWidget, SIGNAL(customContextMenuRequested(const QPoint&)),
                  q, SLOT(showPatientContextMenu(const QPoint&)));
    delete patientItemWidget;
    patientIndex--;
    }
  this->PatientsTabWidget->blockSignals(wasBlocking);
}

//----------------------------------------------------------------------------
bool ctkDICOMVisualBrowserWidgetPrivate::isPatientTabAlreadyAdded(const QString &patientItem)
{
  bool alreadyAdded = false;
  for (int index = 0; index < this->PatientsTabWidget->count(); ++index)
    {
    if (patientItem == this->PatientsTabWidget->tabWhatsThis(index))
      {
      alreadyAdded = true;
      break;
      }
    }

  return alreadyAdded;
}

//----------------------------------------------------------------------------
QStringList ctkDICOMVisualBrowserWidgetPrivate::filterPatientList(const QStringList& patientList,
                                                                  const QString& filterName,
                                                                  const QString& filterValue)
{
  QStringList filteredPatientList;
  foreach (QString patientItem, patientList)
    {
    QString filter = this->DicomDatabase->fieldForPatient(filterName, patientItem);
    if (!filter.contains(filterValue))
      {
      continue;
      }

    filteredPatientList.append(patientItem);
    }

  return filteredPatientList;
}

//----------------------------------------------------------------------------
QStringList ctkDICOMVisualBrowserWidgetPrivate::filterStudyList(const QStringList& studyList,
                                                                const QString &filterName,
                                                                const QString &filterValue)
{
  QStringList filteredStudyList;
  foreach (QString studyItem, studyList)
    {
    QString filter = this->DicomDatabase->fieldForStudy(filterName, studyItem);
    if (filterName == "StudyDate")
      {
      int nDays = filterValue.toInt();
      if (nDays != -1)
        {
        QDate endDate = QDate::currentDate();
        QDate startDate = endDate.addDays(-nDays);
        filter.replace(QString("-"), QString(""));
        QDate studyDate = QDate::fromString(filter, "yyyyMMdd");
        if (studyDate < startDate || studyDate > endDate)
          {
          continue;
          }
        }
      }
    else if (!filter.contains(filterValue))
      {
      continue;
      }

    filteredStudyList.append(studyItem);
    }

  return filteredStudyList;
}

//----------------------------------------------------------------------------
QStringList ctkDICOMVisualBrowserWidgetPrivate::filterSeriesList(const QStringList& seriesList,
                                                                 const QString &filterName,
                                                                 const QString &filterValue)
{
  QStringList filteredSeriesList;
  foreach (QString seriesItem, seriesList)
    {
    QString filter = this->DicomDatabase->fieldForSeries(filterName, seriesItem);
    if (!filter.contains(filterValue))
      {
      continue;
      }

    filteredSeriesList.append(seriesItem);
    }

  return filteredSeriesList;
}

//----------------------------------------------------------------------------
QStringList ctkDICOMVisualBrowserWidgetPrivate::filterSeriesList(const QStringList &seriesList,
                                                                 const QString &filterName,
                                                                 const QStringList &filterValues)
{
  QStringList filteredSeriesList;
  foreach (QString seriesItem, seriesList)
    {
    QString filter = this->DicomDatabase->fieldForSeries(filterName, seriesItem);
    if (!filterValues.contains("Any") && !filterValues.contains(filter))
      {
      continue;
      }

    filteredSeriesList.append(seriesItem);
    }

  return filteredSeriesList;
}

//----------------------------------------------------------------------------
// ctkDICOMVisualBrowserWidget methods

//----------------------------------------------------------------------------
ctkDICOMVisualBrowserWidget::ctkDICOMVisualBrowserWidget(QWidget* parentWidget)
  : Superclass(parentWidget) 
  , d_ptr(new ctkDICOMVisualBrowserWidgetPrivate(*this))
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->init();
}

//----------------------------------------------------------------------------
ctkDICOMVisualBrowserWidget::~ctkDICOMVisualBrowserWidget()
{
}

//----------------------------------------------------------------------------
QString ctkDICOMVisualBrowserWidget::databaseDirectory() const
{
  Q_D(const ctkDICOMVisualBrowserWidget);

  // If override settings is specified then try to get database directory from there first
  return d->DatabaseDirectory;
}

//----------------------------------------------------------------------------
QString ctkDICOMVisualBrowserWidget::databaseDirectorySettingsKey() const
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  return d->DatabaseDirectorySettingsKey;
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::setDatabaseDirectorySettingsKey(const QString &key)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->DatabaseDirectorySettingsKey = key;

  QSettings settings;
  QString databaseDirectory = ctk::absolutePathFromInternal(settings.value(d->DatabaseDirectorySettingsKey, "").toString(), d->DatabaseDirectoryBase);
  this->setDatabaseDirectory(databaseDirectory);
}

//----------------------------------------------------------------------------
ctkDICOMDatabase* ctkDICOMVisualBrowserWidget::dicomDatabase()const
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  return d->DicomDatabase.data();
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::setTagsToPrecache(const QStringList tags)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->DicomDatabase->setTagsToPrecache(tags);
}

//----------------------------------------------------------------------------
const QStringList ctkDICOMVisualBrowserWidget::tagsToPrecache()
{
  Q_D(ctkDICOMVisualBrowserWidget);
  return d->DicomDatabase->tagsToPrecache();
}

//----------------------------------------------------------------------------
int ctkDICOMVisualBrowserWidget::getNumberOfServers()
{
  Q_D(ctkDICOMVisualBrowserWidget);
  return d->PoolManager->getNumberOfServers();
}

//----------------------------------------------------------------------------
ctkDICOMServer* ctkDICOMVisualBrowserWidget::getNthServer(int id)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  return d->PoolManager->getNthServer(id);
}

//----------------------------------------------------------------------------
ctkDICOMServer* ctkDICOMVisualBrowserWidget::getServer(const char *connectioName)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  return d->PoolManager->getServer(connectioName);
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::addServer(ctkDICOMServer* server)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  return d->PoolManager->addServer(server);
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::removeServer(const char *connectioName)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  return d->PoolManager->removeServer(connectioName);
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::removeNthServer(int id)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  return d->PoolManager->removeNthServer(id);
}

//----------------------------------------------------------------------------
QString ctkDICOMVisualBrowserWidget::getServerNameFromIndex(int id)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  return d->PoolManager->getServerNameFromIndex(id);
}

//----------------------------------------------------------------------------
int ctkDICOMVisualBrowserWidget::getServerIndexFromName(const char *connectioName)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  return d->PoolManager->getServerIndexFromName(connectioName);
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::setFilteringPatientID(const QString& filteringPatientID)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->FilteringPatientID = filteringPatientID;
  d->FilteringPatientIDSearchBox->setText(d->FilteringPatientID);
}

//------------------------------------------------------------------------------
QString ctkDICOMVisualBrowserWidget::filteringPatientID() const
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  return d->FilteringPatientID;
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::setFilteringPatientName(const QString& filteringPatientName)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->FilteringPatientName = filteringPatientName;
  d->FilteringPatientNameSearchBox->setText(d->FilteringPatientName);
}

//------------------------------------------------------------------------------
QString ctkDICOMVisualBrowserWidget::filteringPatientName() const
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  return d->FilteringPatientName;
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::setFilteringStudyDescription(const QString& filteringStudyDescription)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->FilteringStudyDescription = filteringStudyDescription;
  d->FilteringStudyDescriptionSearchBox->setText(d->FilteringStudyDescription);
}

//------------------------------------------------------------------------------
QString ctkDICOMVisualBrowserWidget::filteringStudyDescription() const
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  return d->FilteringStudyDescription;
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::setFilteringDate(const ctkDICOMPatientItemWidget::DateType &filteringDate)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->FilteringDate = filteringDate;
  d->FilteringDateComboBox->setCurrentIndex(d->FilteringDate);
}

//------------------------------------------------------------------------------
ctkDICOMPatientItemWidget::DateType ctkDICOMVisualBrowserWidget::filteringDate() const
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  return d->FilteringDate;
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::setFilteringSeriesDescription(const QString& filteringSeriesDescription)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->FilteringSeriesDescription = filteringSeriesDescription;
  d->FilteringSeriesDescriptionSearchBox->setText(d->FilteringSeriesDescription);
}

//------------------------------------------------------------------------------
QString ctkDICOMVisualBrowserWidget::filteringSeriesDescription() const
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  return d->FilteringSeriesDescription;
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::setFilteringModalities(const QStringList &filteringModalities)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->FilteringModalities = filteringModalities;
  d->updateModalityCheckableComboBox();
}

//------------------------------------------------------------------------------
QStringList ctkDICOMVisualBrowserWidget::filteringModalities() const
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  return d->FilteringModalities;
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::setNumberOfSeriesPerRow(int numberOfSeriesPerRow)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->NumberOfSeriesPerRow = numberOfSeriesPerRow;
}

//------------------------------------------------------------------------------
int ctkDICOMVisualBrowserWidget::numberOfSeriesPerRow() const
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  return d->NumberOfSeriesPerRow;
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::setSendActionVisible(bool visible)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->SendActionVisible = visible;
}

//------------------------------------------------------------------------------
bool ctkDICOMVisualBrowserWidget::isSendActionVisible() const
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  return d->SendActionVisible;
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::addPatientItemWidget(const QString& patientItem)
{
  Q_D(ctkDICOMVisualBrowserWidget);

  QString patientName = d->DicomDatabase->fieldForPatient("PatientsName", patientItem);

  ctkDICOMPatientItemWidget *patientItemWidget = new ctkDICOMPatientItemWidget(this);
  patientItemWidget->setPatientItem(patientItem);
  patientItemWidget->setFilteringStudyDescription(d->FilteringStudyDescription);
  patientItemWidget->setFilteringDate(d->FilteringDate);
  patientItemWidget->setFilteringSeriesDescription(d->FilteringSeriesDescription);
  patientItemWidget->setFilteringModalities(d->FilteringModalities);
  patientItemWidget->setNumberOfSeriesPerRow(d->NumberOfSeriesPerRow);
  patientItemWidget->setDicomDatabase(*d->DicomDatabase);
  patientItemWidget->setPoolManager(*d->PoolManager);
  patientItemWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  this->connect(patientItemWidget, SIGNAL(customContextMenuRequested(const QPoint&)),
                this, SLOT(showPatientContextMenu(const QPoint&)));

  patientName.replace(R"(^)", R"(, )");
  int index = d->PatientsTabWidget->addTab(patientItemWidget, QIcon(":/Icons/patient.svg"), patientName);
  d->PatientsTabWidget->setTabWhatsThis(index, patientItem);
}

//----------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::removePatientItemWidget(const QString &patientItem)
{
  Q_D(ctkDICOMVisualBrowserWidget);

  for (int patientIndex = 0; patientIndex < d->PatientsTabWidget->count(); ++patientIndex)
    {
    ctkDICOMPatientItemWidget *patientItemWidget =
      qobject_cast<ctkDICOMPatientItemWidget*>(d->PatientsTabWidget->widget(patientIndex));

    if (!patientItemWidget || patientItemWidget->patientItem() != patientItem)
      {
      continue;
      }

    d->PatientsTabWidget->removeTab(patientIndex);
    this->disconnect(patientItemWidget, SIGNAL(customContextMenuRequested(const QPoint&)),
                     this, SLOT(showPatientContextMenu(const QPoint&)));
    delete patientItemWidget;
    break;
  }
}

//------------------------------------------------------------------------------
int ctkDICOMVisualBrowserWidget::patientsAddedDuringImport()
{
  Q_D(ctkDICOMVisualBrowserWidget);
  return d->PatientsAddedDuringImport;
}

//------------------------------------------------------------------------------
int ctkDICOMVisualBrowserWidget::studiesAddedDuringImport()
{
  Q_D(ctkDICOMVisualBrowserWidget);
  return d->StudiesAddedDuringImport;
}

//------------------------------------------------------------------------------
int ctkDICOMVisualBrowserWidget::seriesAddedDuringImport()
{
  Q_D(ctkDICOMVisualBrowserWidget);
  return d->SeriesAddedDuringImport;
}

//------------------------------------------------------------------------------
int ctkDICOMVisualBrowserWidget::instancesAddedDuringImport()
{
  Q_D(ctkDICOMVisualBrowserWidget);
  return d->InstancesAddedDuringImport;
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::resetItemsAddedDuringImportCounters()
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->PatientsAddedDuringImport = 0;
  d->StudiesAddedDuringImport = 0;
  d->SeriesAddedDuringImport = 0;
  d->InstancesAddedDuringImport = 0;
}

//------------------------------------------------------------------------------
ctkDICOMVisualBrowserWidget::ImportDirectoryMode ctkDICOMVisualBrowserWidget::importDirectoryMode() const
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  ctkDICOMVisualBrowserWidgetPrivate* mutable_d = const_cast<ctkDICOMVisualBrowserWidgetPrivate*>(d);
  mutable_d->importOldSettings();
  QSettings settings;
  return static_cast<ctkDICOMVisualBrowserWidget::ImportDirectoryMode>(settings.value(
    "DICOM/ImportDirectoryMode", static_cast<int>(ctkDICOMVisualBrowserWidget::ImportDirectoryAddLink)).toInt() );
}

//------------------------------------------------------------------------------
ctkFileDialog *ctkDICOMVisualBrowserWidget::importDialog() const
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  return d->ImportDialog;
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::setImportDirectoryMode(ImportDirectoryMode mode)
{
  Q_D(ctkDICOMVisualBrowserWidget);

  QSettings settings;
  settings.setValue("DICOM/ImportDirectoryMode", static_cast<int>(mode));
  if (!d->ImportDialog)
    {
    return;
    }
  if (!(d->ImportDialog->options() & QFileDialog::DontUseNativeDialog))
    {
    return;  // Native dialog does not support modifying or getting widget elements.
    }
  QComboBox* comboBox = d->ImportDialog->bottomWidget()->findChild<QComboBox*>();
  comboBox->setCurrentIndex(comboBox->findData(mode));
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::setDatabaseDirectory(const QString &directory)
{
  Q_D(ctkDICOMVisualBrowserWidget);

  QString absDirectory = ctk::absolutePathFromInternal(directory, d->DatabaseDirectoryBase);

  // close the active DICOM database
  d->DicomDatabase->closeDatabase();

  // open DICOM database on the directory
  QString databaseFileName = QDir(absDirectory).filePath("ctkDICOM.sql");

  bool success = true;
  if (!QDir(absDirectory).exists()
    || (!QDir(absDirectory).isEmpty() && !QFile(databaseFileName).exists()))
    {
    logger.warn(tr("Database folder does not contain ctkDICOM.sql file: ") + absDirectory + "\n");
    success = false;
    }

  if (success)
    {
    bool databaseOpenSuccess = false;
    try
      {
      d->DicomDatabase->openDatabase(databaseFileName);
      databaseOpenSuccess = d->DicomDatabase->isOpen();
      }
    catch (std::exception e)
      {
      databaseOpenSuccess = false;
      }
    if (!databaseOpenSuccess || d->DicomDatabase->schemaVersionLoaded().isEmpty())
      {
      logger.warn(tr("Database error: ") + d->DicomDatabase->lastError() + "\n");
      d->DicomDatabase->closeDatabase();
      success = false;
      }
    }

  if (success)
    {
    if (d->DicomDatabase->schemaVersionLoaded() != d->DicomDatabase->schemaVersion())
      {
      logger.warn(tr("Database version mismatch: version of selected database = ") +
                  d->DicomDatabase->schemaVersionLoaded() +
                  ", version required = " +
                  d->DicomDatabase->schemaVersion() +
                  "\n");
      d->DicomDatabase->closeDatabase();
      success = false;
      }
    }

  // Save new database directory in this object and in application settings.
  d->DatabaseDirectory = absDirectory;
  if (!d->DatabaseDirectorySettingsKey.isEmpty())
    {
    QSettings settings;
    settings.setValue(d->DatabaseDirectorySettingsKey, ctk::internalPathFromAbsolute(absDirectory, d->DatabaseDirectoryBase));
    settings.sync();
    }

  // pass DICOM database instance to Import widget
  emit databaseDirectoryChanged(absDirectory);
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::openImportDialog()
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->ImportDialog->exec();
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::importDirectories(QStringList directories, ImportDirectoryMode mode)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  if (!d->DicomDatabase || !d->PoolManager->Indexer())
    {
    qWarning() << Q_FUNC_INFO << " failed: database or indexer is invalid";
    return;
    }
  foreach (const QString& directory, directories)
    {
    d->importDirectory(directory, mode);
    }
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::importDirectory(QString directory, ImportDirectoryMode mode)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->importDirectory(directory, mode);
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::importFiles(const QStringList &files, ImportDirectoryMode mode)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  if (!d->DicomDatabase || !d->PoolManager->Indexer())
    {
    qWarning() << Q_FUNC_INFO << " failed: database or indexer is invalid";
    return;
    }
  d->importFiles(files, mode);
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::waitForImportFinished()
{
  Q_D(ctkDICOMVisualBrowserWidget);
  if (!d->PoolManager->Indexer())
    {
    qWarning() << Q_FUNC_INFO << " failed: indexer is invalid";
    return;
    }
  d->PoolManager->Indexer()->waitForImportFinished();
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onImportDirectory(QString directory, ImportDirectoryMode mode)
{
  this->importDirectory(directory, mode);
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onIndexingProgress(int percent)
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  if (!d->IsImportFolder)
    {
    return;
    }

  d->ProgressFrame->show();
  d->ProgressBar->setValue(percent);
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onIndexingProgressStep(const QString & step)
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  if (!d->IsImportFolder)
    {
    return;
    }

  d->ProgressLabel->setText(step);
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onIndexingProgressDetail(const QString & detail)
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  if (!d->IsImportFolder)
    {
    return;
    }

  if (detail.isEmpty())
    {
    d->ProgressDetailLineEdit->hide();
    }
  else
    {
    d->ProgressDetailLineEdit->setText(detail);
    d->ProgressDetailLineEdit->show();
    }
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onIndexingComplete(int patientsAdded, int studiesAdded, int seriesAdded, int imagesAdded)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  if (!d->IsImportFolder)
    {
    return;
    }

  d->PatientsAddedDuringImport += patientsAdded;
  d->StudiesAddedDuringImport += studiesAdded;
  d->SeriesAddedDuringImport += seriesAdded;
  d->InstancesAddedDuringImport += imagesAdded;

  d->ProgressFrame->hide();
  d->ProgressDetailLineEdit->hide();

  // allow users of this widget to know that the process has finished
  emit directoryImported();
  d->IsImportFolder = false;

  this->onQueryPatient(false);
}

//------------------------------------------------------------------------------
QStringList ctkDICOMVisualBrowserWidget::fileListForCurrentSelection(ctkDICOMModel::IndexType level,
                                                                     QWidget *selectedWidget)
{
  Q_D(const ctkDICOMVisualBrowserWidget);

  QStringList selectedStudyUIDs;
  if (level == ctkDICOMModel::PatientType)
    {
    ctkDICOMPatientItemWidget *patientItemWidget =
      qobject_cast<ctkDICOMPatientItemWidget*>(selectedWidget);
    if (patientItemWidget)
      {
      selectedStudyUIDs << d->DicomDatabase->studiesForPatient(patientItemWidget->patientItem());
      }
    }
  if (level == ctkDICOMModel::StudyType)
    {
    ctkDICOMStudyItemWidget* studyItemWidget =
      qobject_cast<ctkDICOMStudyItemWidget*>(selectedWidget);
    if (studyItemWidget)
      {
      selectedStudyUIDs << studyItemWidget->studyInstanceUID();
      }
    }

  QStringList selectedSeriesUIDs;
  if (level == ctkDICOMModel::SeriesType)
    {
    ctkDICOMSeriesItemWidget* seriesItemWidget =
      qobject_cast<ctkDICOMSeriesItemWidget*>(selectedWidget);
    if (seriesItemWidget)
      {
      selectedSeriesUIDs << seriesItemWidget->seriesInstanceUID();
      }
    }
  else
    {
    foreach(const QString& uid, selectedStudyUIDs)
      {
      selectedSeriesUIDs << d->DicomDatabase->seriesForStudy(uid);
      }
    }

  QStringList fileList;
  foreach(const QString& selectedSeriesUID, selectedSeriesUIDs)
    {
    fileList << d->DicomDatabase->filesForSeries(selectedSeriesUID);
    }
  return fileList;
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::showMetadata(const QStringList &fileList)
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  d->MetadataDialog->setFileList(fileList);
  d->MetadataDialog->show();
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::removeSelectedItems(ctkDICOMModel::IndexType level,
                                                      QWidget *selectedWidget)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  QStringList selectedPatientUIDs;
  QStringList selectedStudyUIDs;
  if (level == ctkDICOMModel::PatientType)
    {
    ctkDICOMPatientItemWidget *patientItemWidget =
      qobject_cast<ctkDICOMPatientItemWidget*>(selectedWidget);
    if (patientItemWidget)
      {
      QString patientUID = patientItemWidget->patientItem();
      selectedStudyUIDs << d->DicomDatabase->studiesForPatient(patientUID);
      if (!this->confirmDeleteSelectedUIDs(QStringList(patientUID)))
        {
        return;
        }

      this->removePatientItemWidget(patientUID);
      }
    }
  if (level == ctkDICOMModel::StudyType)
    {
    ctkDICOMStudyItemWidget* studyItemWidget =
      qobject_cast<ctkDICOMStudyItemWidget*>(selectedWidget);
    if (studyItemWidget)
      {
      selectedStudyUIDs << studyItemWidget->studyInstanceUID();

      if (!this->confirmDeleteSelectedUIDs(selectedStudyUIDs))
        {
        return;
        }

      ctkDICOMPatientItemWidget *patientItemWidget =
        qobject_cast<ctkDICOMPatientItemWidget*>(d->PatientsTabWidget->currentWidget());
      if (patientItemWidget)
        {
        patientItemWidget->removeStudyItemWidget(studyItemWidget->studyItem());
        }
      }
    }

  QStringList selectedSeriesUIDs;
  if (level == ctkDICOMModel::SeriesType)
    {
    ctkDICOMSeriesItemWidget* seriesItemWidget =
      qobject_cast<ctkDICOMSeriesItemWidget*>(selectedWidget);
    if (seriesItemWidget)
      {
      selectedSeriesUIDs << seriesItemWidget->seriesInstanceUID();

      if (!this->confirmDeleteSelectedUIDs(selectedSeriesUIDs))
        {
        return;
        }

      ctkDICOMPatientItemWidget *patientItemWidget =
        qobject_cast<ctkDICOMPatientItemWidget*>(d->PatientsTabWidget->currentWidget());
      if (patientItemWidget)
        {
        QList<ctkDICOMStudyItemWidget *> studyItemWidgetsList = patientItemWidget->studyItemWidgetsList();
        foreach (ctkDICOMStudyItemWidget *studyItemWidget, studyItemWidgetsList)
          {
          if (studyItemWidget == nullptr || studyItemWidget->studyInstanceUID() != seriesItemWidget->studyInstanceUID())
            {
            continue;
            }

          studyItemWidget->removeSeriesItemWidget(seriesItemWidget->seriesItem());
          break;
          }
        }
      }
    }
  else
    {
    foreach(const QString& uid, selectedStudyUIDs)
      {
      selectedSeriesUIDs << d->DicomDatabase->seriesForStudy(uid);
      }
    }

  foreach(const QString & uid, selectedSeriesUIDs)
    {
    d->DicomDatabase->removeSeries(uid);
    }
  foreach(const QString & uid, selectedStudyUIDs)
    {
    d->DicomDatabase->removeStudy(uid);
    }
  foreach(const QString & uid, selectedPatientUIDs)
    {
    d->DicomDatabase->removePatient(uid);
    }
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onFilteringPatientIDChanged()
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->FilteringPatientID = d->FilteringPatientIDSearchBox->text();
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onFilteringPatientNameChanged()
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->FilteringPatientName = d->FilteringPatientNameSearchBox->text();
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onFilteringStudyDescriptionChanged()
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->FilteringStudyDescription = d->FilteringStudyDescriptionSearchBox->text();
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onFilteringSeriesDescriptionChanged()
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->FilteringSeriesDescription = d->FilteringSeriesDescriptionSearchBox->text();
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onFilteringModalityCheckableComboBoxChanged()
{
  Q_D(ctkDICOMVisualBrowserWidget);

  d->PreviousFilteringModalities = d->FilteringModalities;
  d->FilteringModalities.clear();
  QModelIndexList indexList = d->FilteringModalityCheckableComboBox->checkedIndexes();
  foreach(QModelIndex index, indexList)
    {
    QVariant value = d->FilteringModalityCheckableComboBox->checkableModel()->data(index);
    d->FilteringModalities.append(value.toString());
    }
  d->updateModalityCheckableComboBox();
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onFilteringDateComboBoxChanged(int index)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->FilteringDate = static_cast<ctkDICOMPatientItemWidget::DateType>(index);
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onQueryPatient(bool forcefiltersEmpty)
{
  Q_D(ctkDICOMVisualBrowserWidget);

  if (!d->DicomDatabase || !d->PoolManager)
    {
    return;
    }

  d->removeAllPatientItemWidgets();

  bool filtersEmpty =
    d->FilteringPatientID.isEmpty() &&
    d->FilteringPatientName.isEmpty() &&
    d->FilteringStudyDescription.isEmpty() &&
    d->FilteringSeriesDescription.isEmpty() &&
    d->FilteringDate == ctkDICOMPatientItemWidget::DateType::Any &&
    d->FilteringModalities.contains("Any");

  filtersEmpty = forcefiltersEmpty ? forcefiltersEmpty : filtersEmpty;

  if (d->DicomDatabase->patients().count() == 0 &&
      filtersEmpty)
    {
    QString background = "QWidget { background-color: yellow }";
    d->FilteringPatientIDSearchBox->setStyleSheet(d->FilteringPatientIDSearchBox->styleSheet() + background);
    d->FilteringPatientNameSearchBox->setStyleSheet(d->FilteringPatientNameSearchBox->styleSheet() + background);
    d->FilteringDateComboBox->setStyleSheet(d->FilteringDateComboBox->styleSheet() + background);
    d->FilteringStudyDescriptionSearchBox->setStyleSheet(d->FilteringStudyDescriptionSearchBox->styleSheet() + background);
    d->FilteringSeriesDescriptionSearchBox->setStyleSheet(d->FilteringSeriesDescriptionSearchBox->styleSheet() + background);
    d->FilteringModalityCheckableComboBox->setStyleSheet(d->FilteringModalityCheckableComboBox->styleSheet() + background);

    d->WarningPushButton->setText("No filters have been set and no patients have been found in the local database."
                                  "\nPlease set at least one filter to query the servers");
    d->WarningPushButton->show();
    return;
    }
  else
    {
    d->WarningPushButton->hide();
    }

  if (filtersEmpty)
    {
    d->updateGUIOnQueryPatient();
    d->updateFiltersWarnings();
    }
  else if (d->PoolManager->getNumberOfServers() > 0)
    {
    this->onStop();

    QMap<QString, QVariant> parameters;
    parameters["Name"] = d->FilteringPatientName;
    parameters["ID"] = d->FilteringPatientID;
    parameters["Study"] = d->FilteringStudyDescription;
    parameters["Series"] = d->FilteringSeriesDescription;
    if (!d->FilteringModalities.contains("Any"))
      {
      parameters["Modalities"] = d->FilteringModalities;
      }

    int nDays = ctkDICOMPatientItemWidget::getNDaysFromFilteringDate(d->FilteringDate);
    if (nDays != -1)
      {
      QDate endDate = QDate::currentDate();
      QString formattedEndDate = endDate.toString("yyyyMMdd");

      QDate startDate = endDate.addDays(-nDays);
      QString formattedStartDate = startDate.toString("yyyyMMdd");

      parameters["StartDate"] = formattedStartDate;
      parameters["EndDate"] = formattedEndDate;
      }

    d->PoolManager->setFilters(parameters);
    d->PoolManager->queryStudies(QThread::NormalPriority);
    }
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::updateGUIFromPoolManager(ctkDICOMTaskResults *taskResults)
{
  Q_D(ctkDICOMVisualBrowserWidget);

  d->updateFiltersWarnings();

  if (!taskResults || taskResults->typeOfTask() != ctkDICOMTaskResults::TaskType::QueryStudies)
    {
    return;
    }

  d->updateGUIOnQueryPatient(taskResults);
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onPatientItemChanged(int index)
{
  Q_D(ctkDICOMVisualBrowserWidget);

  ctkDICOMPatientItemWidget* patientItem =
    qobject_cast<ctkDICOMPatientItemWidget*>(d->PatientsTabWidget->widget(index));
  if (!patientItem)
    {
    return;
    }

  patientItem->updateGUIFromPatientSelection();
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::showPatientContextMenu(const QPoint &point)
{
  Q_D(ctkDICOMVisualBrowserWidget);

  ctkDICOMPatientItemWidget *patientItemWidget =
    qobject_cast<ctkDICOMPatientItemWidget*>(d->PatientsTabWidget->currentWidget());
  if (patientItemWidget == nullptr)
    {
    return;
    }

  int currrentTotalTasks = d->PoolManager->totalTasks();

  QPoint globalPos = patientItemWidget->mapToGlobal(point);
  QMenu *patientMenu = new QMenu();

  QString metadataString = tr("View Patient DICOM metadata");
  QAction *metadataAction = new QAction(metadataString, patientMenu);
  patientMenu->addAction(metadataAction);

  QString deleteString = tr("Delete Patient");
  QAction *deleteAction = new QAction(deleteString, patientMenu);
  if (currrentTotalTasks == 0)
    {
    patientMenu->addAction(deleteAction);
    }
  else
    {
    delete deleteAction;
    }

  QString exportString = tr("Export Patient to file system");
  QAction *exportAction = new QAction(exportString, patientMenu);
  if (currrentTotalTasks == 0)
    {
    patientMenu->addAction(exportAction);
    }
  else
    {
    delete exportAction;
    }

  QString sendString = tr("Send Patient to DICOM server");
  QAction* sendAction = new QAction(sendString, patientMenu);
  if (this->isSendActionVisible() && currrentTotalTasks == 0)
    {
    patientMenu->addAction(sendAction);
    }
  else
    {
    delete sendAction;
    }

  QAction *selectedAction = patientMenu->exec(globalPos);
  if (selectedAction == metadataAction)
    {
    this->showMetadata(this->fileListForCurrentSelection(ctkDICOMModel::PatientType, patientItemWidget));
    }
  else if (selectedAction == deleteAction)
    {
    this->removeSelectedItems(ctkDICOMModel::PatientType, patientItemWidget);
    }
  else if (selectedAction == exportAction)
    {
    this->exportSelectedItems(ctkDICOMModel::PatientType, patientItemWidget);
    }
  else if (selectedAction == sendAction)
    {
    emit sendRequested(this->fileListForCurrentSelection(ctkDICOMModel::PatientType, patientItemWidget));
    }
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::showStudyContextMenu(const QPoint &point)
{
  Q_D(ctkDICOMVisualBrowserWidget);

  ctkDICOMStudyItemWidget* studyItemWidget =
    qobject_cast<ctkDICOMStudyItemWidget*>(QObject::sender());
  if (studyItemWidget == nullptr)
    {
    return;
    }

  int currrentTotalTasks = d->PoolManager->totalTasks();

  QPoint globalPos = studyItemWidget->mapToGlobal(point);
  QMenu *studyMenu = new QMenu();

  QString metadataString = tr("View Study DICOM metadata");
  QAction *metadataAction = new QAction(metadataString, studyMenu);
  studyMenu->addAction(metadataAction);

  QString deleteString = tr("Delete Study");
  QAction *deleteAction = new QAction(deleteString, studyMenu);
  if (currrentTotalTasks == 0)
    {
    studyMenu->addAction(deleteAction);
    }
  else
    {
    delete deleteAction;
    }

  QString exportString = tr("Export Study to file system");
  QAction *exportAction = new QAction(exportString, studyMenu);
  if (currrentTotalTasks == 0)
    {
    studyMenu->addAction(exportAction);
    }
  else
    {
    delete exportAction;
    }

  QString sendString = tr("Send Study to DICOM server");
  QAction* sendAction = new QAction(sendString, studyMenu);
  if (this->isSendActionVisible())
    {
    studyMenu->addAction(sendAction);
    }
  else
    {
    delete sendAction;
    }

  QAction *selectedAction = studyMenu->exec(globalPos);
  if (selectedAction == metadataAction)
    {
    this->showMetadata(this->fileListForCurrentSelection(ctkDICOMModel::StudyType, studyItemWidget));
    }
  else if (selectedAction == deleteAction)
    {
    this->removeSelectedItems(ctkDICOMModel::StudyType, studyItemWidget);
    }
  else if (selectedAction == exportAction)
    {
    this->exportSelectedItems(ctkDICOMModel::StudyType, studyItemWidget);
    }
  else if (selectedAction == sendAction)
    {
    emit sendRequested(this->fileListForCurrentSelection(ctkDICOMModel::StudyType, studyItemWidget));
    }
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::showSeriesContextMenu(const QPoint &point)
{
  Q_D(ctkDICOMVisualBrowserWidget);

  ctkDICOMSeriesItemWidget* seriesItemWidget =
    qobject_cast<ctkDICOMSeriesItemWidget*>(QObject::sender());
  if (seriesItemWidget == nullptr)
    {
    return;
    }

  int currrentTotalTasks = d->PoolManager->totalTasks();

  QPoint globalPos = seriesItemWidget->mapToGlobal(point);
  QMenu *seriesMenu = new QMenu();

  QString metadataString = tr("View Series DICOM metadata");
  QAction *metadataAction = new QAction(metadataString, seriesMenu);
  seriesMenu->addAction(metadataAction);

  QString deleteString = tr("Delete Series");
  QAction *deleteAction = new QAction(deleteString, seriesMenu);
  if (currrentTotalTasks == 0)
    {
    seriesMenu->addAction(deleteAction);
    }
  else
    {
    delete deleteAction;
    }

  QString exportString = tr("Export Series to file system");
  QAction *exportAction = new QAction(exportString, seriesMenu);
  if (currrentTotalTasks == 0)
    {
    seriesMenu->addAction(exportAction);
    }
  else
    {
    delete exportAction;
    }

  QString sendString = tr("Send Series to DICOM server");
  QAction* sendAction = new QAction(sendString, seriesMenu);
  if (this->isSendActionVisible())
    {
    seriesMenu->addAction(sendAction);
    }
  else
    {
    delete sendAction;
    }

  QAction *selectedAction = seriesMenu->exec(globalPos);
  if (selectedAction == metadataAction)
    {
    this->showMetadata(this->fileListForCurrentSelection(ctkDICOMModel::SeriesType, seriesItemWidget));
    }
  else if (selectedAction == deleteAction)
    {
    this->removeSelectedItems(ctkDICOMModel::SeriesType, seriesItemWidget);
    }
  else if (selectedAction == exportAction)
    {
    this->exportSelectedItems(ctkDICOMModel::SeriesType, seriesItemWidget);
    }
  else if (selectedAction == sendAction)
    {
    emit sendRequested(this->fileListForCurrentSelection(ctkDICOMModel::SeriesType, seriesItemWidget));
    }
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::exportSelectedItems(ctkDICOMModel::IndexType level,
                                                      QWidget *selectedWidget)
{
  Q_D(const ctkDICOMVisualBrowserWidget);
  ctkFileDialog* directoryDialog = new ctkFileDialog();
  directoryDialog->setOption(QFileDialog::ShowDirsOnly);
  directoryDialog->setFileMode(QFileDialog::DirectoryOnly);
  bool res = directoryDialog->exec();
  if (!res)
    {
    delete directoryDialog;
    return;
    }
  QStringList dirs = directoryDialog->selectedFiles();
  delete directoryDialog;
  QString dirPath = dirs[0];

  QStringList selectedStudyUIDs;
  if (level == ctkDICOMModel::PatientType)
    {
    ctkDICOMPatientItemWidget *patientItemWidget =
      qobject_cast<ctkDICOMPatientItemWidget*>(selectedWidget);
    if (patientItemWidget)
      {
      selectedStudyUIDs << d->DicomDatabase->studiesForPatient(patientItemWidget->patientItem());
      }
    }
  if (level == ctkDICOMModel::StudyType)
    {
    ctkDICOMStudyItemWidget* studyItemWidget =
      qobject_cast<ctkDICOMStudyItemWidget*>(selectedWidget);
    if (studyItemWidget)
      {
      selectedStudyUIDs << studyItemWidget->studyInstanceUID();
      }
    }

  QStringList selectedSeriesUIDs;
  if (level == ctkDICOMModel::SeriesType)
    {
    ctkDICOMSeriesItemWidget* seriesItemWidget =
      qobject_cast<ctkDICOMSeriesItemWidget*>(selectedWidget);
    if (seriesItemWidget)
      {
      selectedSeriesUIDs << seriesItemWidget->seriesInstanceUID();
      }
    }
  else
    {
    foreach(const QString& uid, selectedStudyUIDs)
      {
      selectedSeriesUIDs << d->DicomDatabase->seriesForStudy(uid);
      }
    }

  this->exportSeries(dirPath, selectedSeriesUIDs);
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::exportSeries(QString dirPath, QStringList uids)
{
  Q_D(ctkDICOMVisualBrowserWidget);

  foreach (const QString& uid, uids)
    {
    QStringList filesForSeries = d->DicomDatabase->filesForSeries(uid);

    // Use the first file to get the overall series information
    QString firstFilePath = filesForSeries[0];
    QHash<QString,QString> descriptions (d->DicomDatabase->descriptionsForFile(firstFilePath));
    QString patientName = descriptions["PatientsName"];
    QString patientIDTag = QString("0010,0020");
    QString patientID = d->DicomDatabase->fileValue(firstFilePath, patientIDTag);
    QString studyDescription = descriptions["StudyDescription"];
    QString seriesDescription = descriptions["SeriesDescription"];
    QString studyDateTag = QString("0008,0020");
    QString studyDate = d->DicomDatabase->fileValue(firstFilePath,studyDateTag);
    QString seriesNumberTag = QString("0020,0011");
    QString seriesNumber = d->DicomDatabase->fileValue(firstFilePath,seriesNumberTag);

    QString sep = "/";
    QString nameSep = "-";
    QString destinationDir = dirPath + sep + d->filenameSafeString(patientID);
    if (!patientName.isEmpty())
      {
      destinationDir += nameSep + d->filenameSafeString(patientName);
      }
    destinationDir += sep + d->filenameSafeString(studyDate);
    if (!studyDescription.isEmpty())
      {
      destinationDir += nameSep + d->filenameSafeString(studyDescription);
      }
    destinationDir += sep + d->filenameSafeString(seriesNumber);
    if (!seriesDescription.isEmpty())
      {
      destinationDir += nameSep + d->filenameSafeString(seriesDescription);
      }
    destinationDir += sep;

    // create the destination directory if necessary
    if (!QDir().exists(destinationDir))
      {
      if (!QDir().mkpath(destinationDir))
        {
        //: %1 is the destination directory
        QString errorString = tr("Unable to create export destination directory:\n\n%1"
            "\n\nHalting export.")
            .arg(destinationDir);
        ctkMessageBox createDirectoryErrorMessageBox(this);
        createDirectoryErrorMessageBox.setText(errorString);
        createDirectoryErrorMessageBox.setIcon(QMessageBox::Warning);
        createDirectoryErrorMessageBox.exec();
        return;
        }
      }

    int fileNumber = 0;
    foreach (const QString& filePath, filesForSeries)
      {
      // File name example: my/destination/folder/000001.dcm
      QString destinationFileName = QStringLiteral("%1%2.dcm").arg(destinationDir).arg(fileNumber, 6, 10, QLatin1Char('0'));

      if (!QFile::exists(filePath))
        {
        //: %1 is the file path
        QString errorString = tr("Export source file not found:\n\n%1"
            "\n\nHalting export.\n\nError may be fixed via Repair.")
            .arg(filePath);
        ctkMessageBox copyErrorMessageBox(this);
        copyErrorMessageBox.setText(errorString);
        copyErrorMessageBox.setIcon(QMessageBox::Warning);
        copyErrorMessageBox.exec();
        return;
        }
      if (QFile::exists(destinationFileName))
        {
        //: %1 is the destination file name
        QString errorString = tr("Export destination file already exists:\n\n%1"
            "\n\nHalting export.")
            .arg(destinationFileName);
        ctkMessageBox copyErrorMessageBox(this);
        copyErrorMessageBox.setText(errorString);
        copyErrorMessageBox.setIcon(QMessageBox::Warning);
        copyErrorMessageBox.exec();
        return;
        }

      bool copyResult = QFile::copy(filePath, destinationFileName);
      if (!copyResult)
        {
        //: %1 and %2 refers to source and destination file paths
        QString errorString = tr("Failed to copy\n\n%1\n\nto\n\n%2"
            "\n\nHalting export.")
            .arg(filePath)
            .arg(destinationFileName);
        ctkMessageBox copyErrorMessageBox(this);
        copyErrorMessageBox.setText(errorString);
        copyErrorMessageBox.setIcon(QMessageBox::Warning);
        copyErrorMessageBox.exec();
        return;
        }

      fileNumber++;
      }
  }
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onImportDirectoriesSelected(QStringList directories)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->IsImportFolder = true;
  this->importDirectories(directories, this->importDirectoryMode());

  // Clear selection
  d->ImportDialog->clearSelection();
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onImportDirectoryComboBoxCurrentIndexChanged(int index)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  Q_UNUSED(index);
  if (!(d->ImportDialog->options() & QFileDialog::DontUseNativeDialog))
    {
    return;  // Native dialog does not support modifying or getting widget elements.
    }
  QComboBox* comboBox = d->ImportDialog->bottomWidget()->findChild<QComboBox*>();
  ctkDICOMVisualBrowserWidget::ImportDirectoryMode mode =
      static_cast<ctkDICOMVisualBrowserWidget::ImportDirectoryMode>(comboBox->itemData(index).toInt());
  this->setImportDirectoryMode(mode);
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onClose()
{
  this->close();
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onLoad()
{
  Q_D(ctkDICOMVisualBrowserWidget);
  d->retrieveSeries();
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onImport()
{
  Q_D(ctkDICOMVisualBrowserWidget);
  if (d->PoolManager->totalTasks() != 0)
    {
    QString warningString = tr("The browser is already fetching/importing data."
        "\n\n The queued tasks will be deleted, please wait for the completion of the already running tasks.");
    ctkMessageBox warningMessageBox(this);
    warningMessageBox.setText(warningString);
    warningMessageBox.setIcon(QMessageBox::Warning);
    warningMessageBox.exec();

    this->onStop();
    }

  this->openImportDialog();
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::onStop()
{
  Q_D(ctkDICOMVisualBrowserWidget);

  QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
  d->PoolManager->stopAllTasksNotStarted();
  d->PoolManager->waitForDone(300);
  QApplication::restoreOverrideCursor();
}

//------------------------------------------------------------------------------
void ctkDICOMVisualBrowserWidget::closeEvent(QCloseEvent *event)
{
  Q_D(ctkDICOMVisualBrowserWidget);
  this->onStop();
  event->accept();
}

//------------------------------------------------------------------------------
bool ctkDICOMVisualBrowserWidget::confirmDeleteSelectedUIDs(QStringList uids)
{
  Q_D(ctkDICOMVisualBrowserWidget);

  if (uids.isEmpty())
    {
    return false;
    }

  ctkMessageBox confirmDeleteDialog(this);
  QString message = tr("Do you want to delete the following selected items?");

  // add the information about the selected UIDs
  int numUIDs = uids.size();
  for (int i = 0; i < numUIDs; ++i)
    {
    QString uid = uids.at(i);

    // try using the given UID to find a descriptive string
    QString patientName = d->DicomDatabase->nameForPatient(uid);
    QString studyDescription = d->DicomDatabase->descriptionForStudy(uid);
    QString seriesDescription = d->DicomDatabase->descriptionForSeries(uid);

    if (!patientName.isEmpty())
      {
      message += QString("\n") + patientName;
      }
    else if (!studyDescription.isEmpty())
      {
      message += QString("\n") + studyDescription;
      }
    else if (!seriesDescription.isEmpty())
      {
      message += QString("\n") + seriesDescription;
      }
    else
      {
      // if all other descriptors are empty, use the UID
      message += QString("\n") + uid;
      }
    }
  confirmDeleteDialog.setText(message);
  confirmDeleteDialog.setIcon(QMessageBox::Question);

  confirmDeleteDialog.addButton(tr("Delete"), QMessageBox::AcceptRole);
  confirmDeleteDialog.addButton(tr("Cancel"), QMessageBox::RejectRole);
  confirmDeleteDialog.setDontShowAgainSettingsKey( "MainWindow/DontConfirmDeleteSelected");

  int response = confirmDeleteDialog.exec();

  if (response == QMessageBox::AcceptRole)
    {
    return true;
    }
  else
    {
    return false;
    }
}






