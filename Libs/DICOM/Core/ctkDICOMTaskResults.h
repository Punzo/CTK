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

#ifndef __ctkDICOMTaskResults_h
#define __ctkDICOMTaskResults_h


// Qt includes
#include <QObject>
#include <QMap>

#include "ctkDICOMCoreExport.h"
#include "ctkDICOMItem.h"

class ctkDICOMTaskResultsPrivate;
class DcmDataset;

/// \ingroup DICOM_Core
class CTK_DICOM_CORE_EXPORT ctkDICOMTaskResults : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString filePath READ filePath WRITE setFilePath);
  Q_PROPERTY(bool copyFile READ copyFile WRITE setCopyFile);
  Q_PROPERTY(bool overwriteExistingDataset READ overwriteExistingDataset WRITE setOverwriteExistingDataset);
  Q_PROPERTY(TaskType typeOfTask READ typeOfTask WRITE setTypeOfTask);
  Q_PROPERTY(QString taskUID READ taskUID WRITE setTaskUID);
  Q_PROPERTY(int numberOfTotalResultsForTask READ numberOfTotalResultsForTask WRITE setNumberOfTotalResultsForTask);
  Q_PROPERTY(QString patientID READ patientID WRITE setPatientID);
  Q_PROPERTY(QString studyInstanceUID READ studyInstanceUID WRITE setStudyInstanceUID);
  Q_PROPERTY(QString seriesInstanceUID READ seriesInstanceUID WRITE setSeriesInstanceUID);
  Q_PROPERTY(QString sopInstanceUID READ sopInstanceUID WRITE setSOPInstanceUID);
  Q_PROPERTY(QString connectionName READ connectionName WRITE setConnectionName);

public:
  explicit ctkDICOMTaskResults(QObject* parent = 0);
  virtual ~ctkDICOMTaskResults();

  /// File Path
  void setFilePath(const QString& filePath);
  QString filePath() const;

  /// Copy File
  void setCopyFile(const bool& copyFile);
  bool copyFile() const;

  /// Overwrite existing dataset
  void setOverwriteExistingDataset(const bool& overwriteExistingDataset);
  bool overwriteExistingDataset() const;

  /// Task type
  enum TaskType {
    FileIndexing = 0,
    QueryStudies,
    QuerySeries,
    QueryInstances,
    RetrieveSOPInstance,
  };
  void setTypeOfTask(const TaskType& typeOfTask);
  TaskType typeOfTask() const;

  /// Task UID
  void setTaskUID(const QString& taskUID);
  QString taskUID() const;

  /// Count Reference of the number of total results object related to the taskUID
  void setNumberOfTotalResultsForTask(const int& numberOfTotalResultsForTask);
  int numberOfTotalResultsForTask() const;

  /// Patient ID
  void setPatientID(const QString& patientID);
  QString patientID() const;

  /// Study instance UID
  void setStudyInstanceUID(const QString& studyInstanceUID);
  QString studyInstanceUID() const;

  /// Series instance UID
  void setSeriesInstanceUID(const QString& seriesInstanceUID);
  QString seriesInstanceUID() const;

  /// SOP instance UID
  void setSOPInstanceUID(const QString& sopInstanceUID);
  QString sopInstanceUID() const;

  /// Connection name
  void setConnectionName(const QString& connectionName);
  QString connectionName() const;

  /// DCM dataset
  Q_INVOKABLE void setDataset(DcmDataset *dataset);
  Q_INVOKABLE QSharedPointer<ctkDICOMItem> dataset() const;

  /// DCM datasets map. Used when the logic needs to notify
  /// the UI only once with a larger subset of data.
  Q_INVOKABLE void setDatasetsMap(QMap<QString, DcmDataset *> datasetsMap);
  Q_INVOKABLE QMap<QString, QSharedPointer<ctkDICOMItem>> datasetsMap() const;

protected:
  QScopedPointer<ctkDICOMTaskResultsPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(ctkDICOMTaskResults);
  Q_DISABLE_COPY(ctkDICOMTaskResults);
};


#endif
