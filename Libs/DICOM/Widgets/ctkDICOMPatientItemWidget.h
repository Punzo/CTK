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

#ifndef __ctkDICOMPatientItemWidget_h
#define __ctkDICOMPatientItemWidget_h

#include "ctkDICOMWidgetsExport.h"

// Qt includes 
#include <QWidget>
class ctkDICOMPatientItemWidgetPrivate;

class ctkDICOMDatabase;
class ctkDICOMPoolManager;
class ctkDICOMStudyItemWidget;

/// \ingroup DICOM_Widgets
class CTK_DICOM_WIDGETS_EXPORT ctkDICOMPatientItemWidget : public QWidget
{
  Q_OBJECT;
  Q_PROPERTY(QString patientItem READ patientItem WRITE setPatientItem);
  Q_PROPERTY(int numberOfSeriesPerRow READ numberOfSeriesPerRow WRITE setNumberOfSeriesPerRow);

public:
  typedef QWidget Superclass;
  explicit ctkDICOMPatientItemWidget(QWidget* parent = nullptr);
  virtual ~ctkDICOMPatientItemWidget();

  /// Patient ID
  void setPatientItem(const QString& patientItem);
  QString patientItem() const;

  /// Query Filters
  /// Empty by default
  void setFilteringStudyDescription(const QString& filteringStudyDescription);
  QString filteringStudyDescription() const;
  /// Date filtering enum
  enum DateType
    {
    Any = 0,
    Today,
    Yesterday,
    LastWeek,
    LastMonth,
    LastYear
    };

  /// Available values:
  /// Any,
  /// Today,
  /// Yesterday,
  /// LastWeek,
  /// LastMonth,
  /// LastYear.
  /// Any by default.
  void setFilteringDate(const DateType& filteringDate);
  DateType filteringDate() const;
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

  /// Pool manager and dicom database references
  Q_INVOKABLE QSharedPointer<ctkDICOMDatabase> dicomDatabase()const;
  Q_INVOKABLE void setDicomDatabase(ctkDICOMDatabase& dicomDatabase);
  Q_INVOKABLE QSharedPointer<ctkDICOMPoolManager> poolManager()const;
  Q_INVOKABLE void setPoolManager(ctkDICOMPoolManager& poolManager);

  /// Return all the study item widgets for the patient
  Q_INVOKABLE QList<ctkDICOMStudyItemWidget*> studyItemWidgetsList()const;

  /// Return number of days from filtering date attribute
  Q_INVOKABLE static int getNDaysFromFilteringDate(ctkDICOMPatientItemWidget::DateType filteringDate);

  /// Add/Remove study item widgets
  Q_INVOKABLE void addStudyItemWidget(const QString &studyItem);
  Q_INVOKABLE void removeStudyItemWidget(const QString &studyItem);

public Q_SLOTS:
  void updateGUIFromPatientSelection();

protected:
  QScopedPointer<ctkDICOMPatientItemWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(ctkDICOMPatientItemWidget);
  Q_DISABLE_COPY(ctkDICOMPatientItemWidget);
};

#endif
