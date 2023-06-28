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

// ctkDICOMCore includes
#include "ctkDICOMItem.h"
#include "ctkDICOMTaskResults.h"
#include "ctkLogger.h"

// DCMTK includes
#include "dcmtk/dcmnet/dimse.h"

static ctkLogger logger("org.commontk.dicom.DICOMTaskResults");


//------------------------------------------------------------------------------
class ctkDICOMTaskResultsPrivate: public QObject
{
  Q_DECLARE_PUBLIC(ctkDICOMTaskResults);

protected:
  ctkDICOMTaskResults* const q_ptr;

public:
  ctkDICOMTaskResultsPrivate(ctkDICOMTaskResults& obj);
  ~ctkDICOMTaskResultsPrivate();

  QString FilePath;
  bool CopyFile;
  bool OverwriteExistingDataset;

  ctkDICOMTaskResults::TaskType TypeOfTask;
  QString TaskUID;
  int NumberOfTotalResultsForTask;
  QString PatientID;
  QString StudyInstanceUID;
  QString SeriesInstanceUID;
  QString SOPInstanceUID;
  QString ConnectionName;
  QSharedPointer<ctkDICOMItem> Dataset;
  QMap<QString, QSharedPointer<ctkDICOMItem>> DatasetsMap;
};

//------------------------------------------------------------------------------
// ctkDICOMTaskResultsPrivate methods

//------------------------------------------------------------------------------
ctkDICOMTaskResultsPrivate::ctkDICOMTaskResultsPrivate(ctkDICOMTaskResults& obj)
  : q_ptr(&obj)
{
  this->TypeOfTask = ctkDICOMTaskResults::TaskType::FileIndexing;
  this->TaskUID = "";
  this->NumberOfTotalResultsForTask = 0;
  this->PatientID = "";
  this->StudyInstanceUID = "";
  this->SeriesInstanceUID = "";
  this->SOPInstanceUID = "";
  this->ConnectionName = "";
  this->Dataset = nullptr;
  this->CopyFile = false;
  this->OverwriteExistingDataset = false;
  this->FilePath = "";
}

//------------------------------------------------------------------------------
ctkDICOMTaskResultsPrivate::~ctkDICOMTaskResultsPrivate()
{
}

//------------------------------------------------------------------------------
// ctkDICOMTaskResults methods

//------------------------------------------------------------------------------
ctkDICOMTaskResults::ctkDICOMTaskResults(QObject* parent)
  : QObject(parent),
    d_ptr(new ctkDICOMTaskResultsPrivate(*this))
{
}

//------------------------------------------------------------------------------
ctkDICOMTaskResults::~ctkDICOMTaskResults()
{
}

//----------------------------------------------------------------------------
void ctkDICOMTaskResults::setFilePath(const QString &filePath)
{
  Q_D(ctkDICOMTaskResults);
  d->FilePath = filePath;

  if (d->FilePath.contains("server://"))
    {
    return;
    }

  d->Dataset = QSharedPointer<ctkDICOMItem>(new ctkDICOMItem);
  d->Dataset->InitializeFromFile(filePath);
}

//----------------------------------------------------------------------------
QString ctkDICOMTaskResults::filePath() const
{
  Q_D(const ctkDICOMTaskResults);
  return d->FilePath;
}

//----------------------------------------------------------------------------
void ctkDICOMTaskResults::setCopyFile(const bool &copyFile)
{
  Q_D(ctkDICOMTaskResults);
  d->CopyFile = copyFile;
}

//----------------------------------------------------------------------------
bool ctkDICOMTaskResults::copyFile() const
{
  Q_D(const ctkDICOMTaskResults);
  return d->CopyFile;
}

//----------------------------------------------------------------------------
void ctkDICOMTaskResults::setOverwriteExistingDataset(const bool &overwriteExistingDataset)
{
  Q_D(ctkDICOMTaskResults);
  d->OverwriteExistingDataset = overwriteExistingDataset;
}

//----------------------------------------------------------------------------
bool ctkDICOMTaskResults::overwriteExistingDataset() const
{
  Q_D(const ctkDICOMTaskResults);
  return d->OverwriteExistingDataset;
}

//----------------------------------------------------------------------------
void ctkDICOMTaskResults::setTypeOfTask(const ctkDICOMTaskResults::TaskType &typeOfTask)
{
  Q_D(ctkDICOMTaskResults);
  d->TypeOfTask = typeOfTask;
}

//------------------------------------------------------------------------------
ctkDICOMTaskResults::TaskType ctkDICOMTaskResults::typeOfTask() const
{
  Q_D(const ctkDICOMTaskResults);
  return d->TypeOfTask;
}

//------------------------------------------------------------------------------
void ctkDICOMTaskResults::setTaskUID(const QString &taskUID)
{
  Q_D(ctkDICOMTaskResults);
  d->TaskUID = taskUID;
}

//------------------------------------------------------------------------------
QString ctkDICOMTaskResults::taskUID() const
{
  Q_D(const ctkDICOMTaskResults);
  return d->TaskUID;
}

//------------------------------------------------------------------------------
void ctkDICOMTaskResults::setNumberOfTotalResultsForTask(const int &numberOfTotalResultsForTask)
{
  Q_D(ctkDICOMTaskResults);
  d->NumberOfTotalResultsForTask = numberOfTotalResultsForTask;
}

//------------------------------------------------------------------------------
int ctkDICOMTaskResults::numberOfTotalResultsForTask() const
{
  Q_D(const ctkDICOMTaskResults);
  return d->NumberOfTotalResultsForTask;
}

//------------------------------------------------------------------------------
void ctkDICOMTaskResults::setPatientID(const QString& patientID)
{
  Q_D(ctkDICOMTaskResults);
  d->PatientID = patientID;
}

//------------------------------------------------------------------------------
QString ctkDICOMTaskResults::patientID() const
{
  Q_D(const ctkDICOMTaskResults);
  return d->PatientID;
}

//------------------------------------------------------------------------------
void ctkDICOMTaskResults::setStudyInstanceUID(const QString& studyInstanceUID)
{
  Q_D(ctkDICOMTaskResults);
  d->StudyInstanceUID = studyInstanceUID;
}

//------------------------------------------------------------------------------
QString ctkDICOMTaskResults::studyInstanceUID() const
{
  Q_D(const ctkDICOMTaskResults);
  return d->StudyInstanceUID;
}

//----------------------------------------------------------------------------
void ctkDICOMTaskResults::setSeriesInstanceUID(const QString& seriesInstanceUID)
{
  Q_D(ctkDICOMTaskResults);
  d->SeriesInstanceUID = seriesInstanceUID;
}

//------------------------------------------------------------------------------
QString ctkDICOMTaskResults::seriesInstanceUID() const
{
  Q_D(const ctkDICOMTaskResults);
  return d->SeriesInstanceUID;
}

//----------------------------------------------------------------------------
void ctkDICOMTaskResults::setSOPInstanceUID(const QString& sopInstanceUID)
{
  Q_D(ctkDICOMTaskResults);
  d->SOPInstanceUID = sopInstanceUID;
}

//------------------------------------------------------------------------------
QString ctkDICOMTaskResults::sopInstanceUID() const
{
  Q_D(const ctkDICOMTaskResults);
  return d->SOPInstanceUID;
}

//------------------------------------------------------------------------------
void ctkDICOMTaskResults::setConnectionName(const QString &connectionName)
{
  Q_D(ctkDICOMTaskResults);
  d->ConnectionName = connectionName;
}

//------------------------------------------------------------------------------
QString ctkDICOMTaskResults::connectionName() const
{
  Q_D(const ctkDICOMTaskResults);
  return d->ConnectionName;
}

//------------------------------------------------------------------------------
void ctkDICOMTaskResults::setDataset(DcmDataset *dataset)
{
  Q_D(ctkDICOMTaskResults);
  d->Dataset = QSharedPointer<ctkDICOMItem>(new ctkDICOMItem);
  d->Dataset->InitializeFromItem(dataset);
}

//------------------------------------------------------------------------------
QSharedPointer<ctkDICOMItem> ctkDICOMTaskResults::dataset() const
{
  Q_D(const ctkDICOMTaskResults);
  return d->Dataset;
}

//------------------------------------------------------------------------------
void ctkDICOMTaskResults::setDatasetsMap(QMap<QString, DcmDataset *> datasetsMap)
{
  Q_D(ctkDICOMTaskResults);
  for(QString sopInstanceUID : datasetsMap.keys())
    {
    DcmDataset *dataset = datasetsMap.value(sopInstanceUID);
    if (!dataset)
      {
      continue;
      }

    QSharedPointer<ctkDICOMItem> ctkDataset = QSharedPointer<ctkDICOMItem>(new ctkDICOMItem);
    ctkDataset->InitializeFromItem(dataset);

    d->DatasetsMap.insert(sopInstanceUID, ctkDataset);
    }
}

//------------------------------------------------------------------------------
QMap<QString, QSharedPointer<ctkDICOMItem>> ctkDICOMTaskResults::datasetsMap() const
{
  Q_D(const ctkDICOMTaskResults);
  return d->DatasetsMap;
}
