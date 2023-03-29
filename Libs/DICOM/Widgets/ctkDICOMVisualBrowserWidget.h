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

#ifndef __ctkDICOMVisualBrowserWidget_h
#define __ctkDICOMVisualBrowserWidget_h

#include "ctkDICOMWidgetsExport.h"

// Qt includes 
#include <QWidget>

// CTK includes
#include <ctkDICOMPatientItemWidget.h>
#include "ctkDICOMModel.h"

class ctkDICOMVisualBrowserWidgetPrivate;
class ctkDICOMDatabase;
class ctkDICOMServer;
class ctkDICOMTaskResults;

/// \ingroup DICOM_Widgets
class CTK_DICOM_WIDGETS_EXPORT ctkDICOMVisualBrowserWidget : public QWidget
{
  Q_OBJECT;
  Q_PROPERTY(QString filteringPatientID READ filteringPatientID WRITE setFilteringPatientID);
  Q_PROPERTY(QString filteringPatientName READ filteringPatientName WRITE setFilteringPatientName);
  Q_PROPERTY(int numberOfSeriesPerRow READ numberOfSeriesPerRow WRITE setNumberOfSeriesPerRow);
  Q_PROPERTY(bool sendActionVisible READ isSendActionVisible WRITE setSendActionVisible)

public:
  typedef QWidget Superclass;
  explicit ctkDICOMVisualBrowserWidget(QWidget* parent = nullptr);
  virtual ~ctkDICOMVisualBrowserWidget();

  /// Dicom Database
  Q_INVOKABLE ctkDICOMDatabase* dicomDatabase()const;
  Q_INVOKABLE void setDicomDatabase(ctkDICOMDatabase& dicomDatabase);

  /// See ctkDICOMDatabase for description - these accessors
  /// delegate to the corresponding routines of the internal
  /// instance of the database.
  /// @see ctkDICOMDatabase
  Q_INVOKABLE void setTagsToPrecache(const QStringList tags);
  Q_INVOKABLE const QStringList tagsToPrecache();

  /// Servers
  Q_INVOKABLE int getNumberOfServers();
  Q_INVOKABLE ctkDICOMServer* getNthServer(int id);
  Q_INVOKABLE ctkDICOMServer* getServer(const char* connectioName);
  Q_INVOKABLE void addServer(ctkDICOMServer* server);
  Q_INVOKABLE void removeServer(const char* connectioName);
  Q_INVOKABLE void removeNthServer(int id);
  Q_INVOKABLE QString getServerNameFromIndex(int id);
  Q_INVOKABLE int getServerIndexFromName(const char* connectioName);

  /// Query Filters
  /// Empty by default
  void setFilteringPatientID(const QString& filteringPatientID);
  QString filteringPatientID() const;
  /// Empty by default
  void setFilteringPatientName(const QString& filteringPatientName);
  QString filteringPatientName() const;
  /// Empty by default
  void setFilteringStudyDescription(const QString& filteringStudyDescription);
  QString filteringStudyDescription() const;
  /// Available values:
  /// Any,
  /// Today,
  /// Yesterday,
  /// LastWeek,
  /// LastMonth,
  /// LastYear.
  /// Any by default.
  void setFilteringDate(const ctkDICOMPatientItemWidget::DateType& filteringDate);
  ctkDICOMPatientItemWidget::DateType filteringDate() const;
  /// Empty by default
  void setFilteringSeriesDescription(const QString& filteringSeriesDescription);
  QString filteringSeriesDescription() const;
  /// ["Any", "CR", "CR", "CT", "MR", "NM", "US", "PT", "XA"] by default
  void setFilteringModalities(const QStringList& filteringModalities);
  QStringList filteringModalities() const;

  /// Number of series displayed per row
  /// 6 by default
  void setNumberOfSeriesPerRow(int numberOfSeriesPerRow);
  int numberOfSeriesPerRow() const;

  /// Set if send action on right click context menu is available
  /// false by default
  void setSendActionVisible(bool visible);
  bool isSendActionVisible() const;

  /// Add/Remove Patient item widget
  Q_INVOKABLE void addPatientItemWidget(const QString& patientItem);
  Q_INVOKABLE void removePatientItemWidget(const QString& patientItem);

public Q_SLOTS:
  void onFilteringPatientIDChanged();
  void onFilteringPatientNameChanged();
  void onFilteringStudyDescriptionChanged();
  void onFilteringSeriesDescriptionChanged();
  void onFilteringModalityCheckableComboBoxChanged();
  void onFilteringDateComboBoxChanged(int);
  void onQueryPatient();
  void updateGUIFromPoolManager(ctkDICOMTaskResults*);
  void onPatientItemChanged(int);
  void onClose();
  void onLoad();

Q_SIGNALS:
  /// Emitted when retrieveSeries finish to retrieve the series.
  void seriesRetrieved(const QStringList& seriesInstanceUIDs);
  /// Emitted when user requested network send. String list contains list of files to be exported.
  void sendRequested(const QStringList&);

protected:
  void closeEvent(QCloseEvent *);
  QScopedPointer<ctkDICOMVisualBrowserWidgetPrivate> d_ptr;

  /// Confirm with the user that they wish to delete the selected uids.
  /// Add information about the selected UIDs to a message box, checks
  /// for patient name, series description, study description, if all
  /// empty, uses the UID.
  /// Returns true if the user confirms the delete, false otherwise.
  /// Remembers if the user doesn't want to show the confirmation again.
  bool confirmDeleteSelectedUIDs(QStringList uids);

  /// Get file list for right click selection
  QStringList fileListForCurrentSelection(ctkDICOMModel::IndexType level, QWidget *selectedWidget);
  /// Show window that displays DICOM fields of all selected items
  void showMetadata(const QStringList& fileList);
  /// Remove items (both database and widget)
  void removeSelectedItems(ctkDICOMModel::IndexType level, QWidget *selectedWidget);
  /// Export the items associated with the selected widget
  void exportSelectedItems(ctkDICOMModel::IndexType level, QWidget *selectedWidget);
  /// Export the series associated with the selected UIDs
  void exportSeries(QString dirPath, QStringList uids);

protected Q_SLOTS:
  /// Called when a right mouse click is made on a patient tab widget
  void showPatientContextMenu(const QPoint& point);
  /// Called when a right mouse click is made in the studies table
  void showStudyContextMenu(const QPoint& point);
  /// Called when a right mouse click is made in the studies table
  void showSeriesContextMenu(const QPoint& point);

private:
  Q_DECLARE_PRIVATE(ctkDICOMVisualBrowserWidget);
  Q_DISABLE_COPY(ctkDICOMVisualBrowserWidget);
};

#endif
