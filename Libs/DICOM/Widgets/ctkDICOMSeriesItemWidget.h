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

#ifndef __ctkDICOMSeriesItemWidget_h
#define __ctkDICOMSeriesItemWidget_h

#include "ctkDICOMWidgetsExport.h"

// Qt includes 
#include <QWidget>

class ctkDICOMSeriesItemWidgetPrivate;
class ctkDICOMDatabase;
class ctkDICOMTaskPool;
class ctkDICOMTaskResults;

/// \ingroup DICOM_Widgets
class CTK_DICOM_WIDGETS_EXPORT ctkDICOMSeriesItemWidget : public QWidget
{
  Q_OBJECT;
  Q_PROPERTY(QString seriesItem READ seriesItem WRITE setSeriesItem);
  Q_PROPERTY(QString studyInstanceUID READ studyInstanceUID WRITE setStudyInstanceUID);
  Q_PROPERTY(QString seriesInstanceUID READ seriesInstanceUID WRITE setSeriesInstanceUID);
  Q_PROPERTY(QString seriesNumber READ seriesNumber WRITE setSeriesNumber);
  Q_PROPERTY(QString modality READ modality WRITE setModality);
  Q_PROPERTY(QString seriesDescription READ seriesDescription WRITE setSeriesDescription);
  Q_PROPERTY(QString isCloud READ isCloud);
  Q_PROPERTY(int thumbnailSize READ thumbnailSize WRITE setThumbnailSize);

public:
  typedef QWidget Superclass;
  explicit ctkDICOMSeriesItemWidget(QWidget* parent = nullptr);
  virtual ~ctkDICOMSeriesItemWidget();

  /// Series Item
  void setSeriesItem(const QString& seriesItem);
  QString seriesItem() const;

  /// Study instance UID
  void setStudyInstanceUID(const QString& studyInstanceUID);
  QString studyInstanceUID() const;

  /// Series instance UID
  void setSeriesInstanceUID(const QString& seriesInstanceUID);
  QString seriesInstanceUID() const;

  /// Series Number
  void setSeriesNumber(const QString& seriesNumber);
  QString seriesNumber() const;

  /// Modality
  void setModality(const QString& modality);
  QString modality() const;

  /// Series Description
  void setSeriesDescription(const QString& seriesDescription);
  QString seriesDescription() const;

  /// Series lives in the server
  bool isCloud() const;

  /// Series has been loaded by the parent widget
  bool IsLoaded() const;

  /// Series is visible in the parent widget
  bool IsVisible() const;

  /// Series Thumbnail size
  /// 300 px by default
  void setThumbnailSize(int thumbnailSize);
  int thumbnailSize() const;

  /// task pool and dicom database references
  Q_INVOKABLE QSharedPointer<ctkDICOMDatabase> dicomDatabase()const;
  Q_INVOKABLE void setDicomDatabase(ctkDICOMDatabase& dicomDatabase);
  Q_INVOKABLE QSharedPointer<ctkDICOMTaskPool> TaskPool()const;
  Q_INVOKABLE void setTaskPool(ctkDICOMTaskPool& TaskPool);

public Q_SLOTS:
  void generateInstances();
  void updateGUIFromTaskPool(ctkDICOMTaskResults*);

protected:
  QScopedPointer<ctkDICOMSeriesItemWidgetPrivate> d_ptr;

  void mousePressEvent(QMouseEvent *event);

private:
  Q_DECLARE_PRIVATE(ctkDICOMSeriesItemWidget);
  Q_DISABLE_COPY(ctkDICOMSeriesItemWidget);
};

#endif
