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
#include <dcmtk/dcmdata/dcdeftag.h>

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
  QString PatientID;
  QString StudyInstanceUID;
  QString SeriesInstanceUID;
  QString SOPInstanceUID;
  QString ConnectionName;
  QSharedPointer<ctkDICOMItem> Dataset;
  QMap<QString, QSharedPointer<ctkDICOMItem>> ctkItemsMap;
};

//------------------------------------------------------------------------------
// ctkDICOMTaskResultsPrivate methods

//------------------------------------------------------------------------------
ctkDICOMTaskResultsPrivate::ctkDICOMTaskResultsPrivate(ctkDICOMTaskResults& obj)
  : q_ptr(&obj)
{
  this->TypeOfTask = ctkDICOMTaskResults::TaskType::FileIndexing;
  this->TaskUID = "";
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

  if (d->FilePath.contains("server://") || d->FilePath == "")
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
void ctkDICOMTaskResults::setDataset(DcmItem* dataset, bool takeOwnership)
{
  Q_D(ctkDICOMTaskResults);
  if (!dataset)
    {
    return;
    }

  d->Dataset = QSharedPointer<ctkDICOMItem>(new ctkDICOMItem);
  d->Dataset->InitializeFromItem(dataset, takeOwnership);
}

//------------------------------------------------------------------------------
DcmItem* ctkDICOMTaskResults::dataset()
{
  Q_D(const ctkDICOMTaskResults);
  if (!d->Dataset)
    {
    return nullptr;
    }

  return &d->Dataset->GetDcmItem();
}

//------------------------------------------------------------------------------
QSharedPointer<ctkDICOMItem> ctkDICOMTaskResults::ctkItem() const
{
  Q_D(const ctkDICOMTaskResults);
  return d->Dataset;
}

//------------------------------------------------------------------------------
void ctkDICOMTaskResults::setDatasetsMap(QMap<QString, DcmItem *> datasetsMap, bool takeOwnership)
{
  Q_D(ctkDICOMTaskResults);
  for(QString sopInstanceUID : datasetsMap.keys())
    {
    DcmItem *dataset = datasetsMap.value(sopInstanceUID);
    if (!dataset)
      {
      continue;
      }

    QSharedPointer<ctkDICOMItem> ctkDataset = QSharedPointer<ctkDICOMItem>(new ctkDICOMItem);
    ctkDataset->InitializeFromItem(dataset, takeOwnership);

    d->ctkItemsMap.insert(sopInstanceUID, ctkDataset);
    }
}

//------------------------------------------------------------------------------
QMap<QString, DcmItem *> ctkDICOMTaskResults::datasetsMap() const
{
  Q_D(const ctkDICOMTaskResults);
  QMap<QString, DcmItem *> datasetsMap;
  for(QString key : d->ctkItemsMap.keys())
    {
    QSharedPointer<ctkDICOMItem> ctkItem = d->ctkItemsMap.value(key);
    if (ctkItem == nullptr)
      {
      continue;
      }

    DcmItem *dataset = &ctkItem->GetDcmItem();
    if (dataset == nullptr)
      {
      continue;
      }

    OFString SOPInstanceUID;
    dataset->findAndGetOFString(DCM_SOPInstanceUID, SOPInstanceUID);
    datasetsMap.insert(SOPInstanceUID.c_str(), dataset);
    }

  return datasetsMap;
}

//------------------------------------------------------------------------------
QMap<QString, QSharedPointer<ctkDICOMItem>> ctkDICOMTaskResults::ctkItemsMap() const
{
  Q_D(const ctkDICOMTaskResults);
  return d->ctkItemsMap;
}

//------------------------------------------------------------------------------
void ctkDICOMTaskResults::deepCopy(ctkDICOMTaskResults *node)
{
  if (!node)
    {
    return;
    }

  this->setFilePath(node->filePath());
  this->setCopyFile(node->copyFile());
  this->setOverwriteExistingDataset(node->overwriteExistingDataset());
  this->setTypeOfTask(node->typeOfTask());
  this->setTaskUID(node->filePath());
  this->setPatientID(node->patientID());
  this->setStudyInstanceUID(node->studyInstanceUID());
  this->setSeriesInstanceUID(node->seriesInstanceUID());
  this->setSOPInstanceUID(node->sopInstanceUID());
  this->setConnectionName(node->connectionName());

  DcmDataset* nodedcmItem = static_cast<DcmDataset*>(node->dataset());
  if (nodedcmItem)
    {
    DcmDataset* dcmItem = new DcmDataset();
    dcmItem->copyFrom(*node->dataset());
    this->setDataset(dcmItem, true);
    }

  QMap<QString, DcmItem *> nodedcmItems = node->datasetsMap();
  if (nodedcmItems.size() != 0)
    {
    QMap<QString, DcmItem *> dcmItems;
    for(QString key : nodedcmItems.keys())
      {
      DcmDataset* nodedcmItem = static_cast<DcmDataset*>(nodedcmItems.value(key));
      if (!nodedcmItem)
        {
        continue;
        }

      DcmDataset* dcmItem = new DcmDataset();
      dcmItem->copyFrom(*nodedcmItem);
      OFString SOPInstanceUID;
      dcmItem->findAndGetOFString(DCM_SOPInstanceUID, SOPInstanceUID);
      dcmItems.insert(SOPInstanceUID.c_str(), dcmItem);
      }

    this->setDatasetsMap(dcmItems, true);
    }
}
