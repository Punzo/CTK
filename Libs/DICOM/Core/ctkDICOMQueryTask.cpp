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
#include "ctkDICOMQueryTask_p.h"
#include "ctkLogger.h"

static ctkLogger logger ( "org.commontk.dicom.DICOMQueryAbstractWorker" );

//------------------------------------------------------------------------------
// ctkDICOMQueryTaskPrivate methods

//------------------------------------------------------------------------------
ctkDICOMQueryTaskPrivate::ctkDICOMQueryTaskPrivate(ctkDICOMQueryTask* object)
 : q_ptr(object)
{
  this->Query = new ctkDICOMQuery;
  this->Server = nullptr;
  this->QueryLevel = ctkDICOMQueryTask::DICOMLevel::Studies;
  this->StudyInstanceUID = "";
  this->SeriesInstanceUID = "";
}

//------------------------------------------------------------------------------
ctkDICOMQueryTaskPrivate::~ctkDICOMQueryTaskPrivate()
{
  if (this->Query)
    {
    delete this->Query;
    this->Query = nullptr;
    }
}

//------------------------------------------------------------------------------
// ctkDICOMQueryTask methods

//------------------------------------------------------------------------------
ctkDICOMQueryTask::ctkDICOMQueryTask()
  : d_ptr(new ctkDICOMQueryTaskPrivate(this))
{
}

//------------------------------------------------------------------------------
ctkDICOMQueryTask::ctkDICOMQueryTask(ctkDICOMQueryTaskPrivate* pimpl)
  : d_ptr(pimpl)
{
}

//------------------------------------------------------------------------------
ctkDICOMQueryTask::~ctkDICOMQueryTask()
{
}

//----------------------------------------------------------------------------
void ctkDICOMQueryTask::setQueryLevel(DICOMLevel queryLevel)
{
  Q_D(ctkDICOMQueryTask);
  d->QueryLevel = queryLevel;
}

//----------------------------------------------------------------------------
ctkDICOMQueryTask::DICOMLevel ctkDICOMQueryTask::queryLevel() const
{
  Q_D(const ctkDICOMQueryTask);
  return d->QueryLevel;
}

//----------------------------------------------------------------------------
void ctkDICOMQueryTask::setStop(const bool& stop)
{
  Q_D(const ctkDICOMQueryTask);
  ctkAbstractTask::setStop(stop);
  d->Query->cancel();
}

//----------------------------------------------------------------------------
void ctkDICOMQueryTask::run()
{
  Q_D(const ctkDICOMQueryTask);
  if (this->isStopped())
    {
    this->setIsFinished(true);
    emit canceled();
    return;
    }

  this->setIsRunning(true);
  emit started();

  logger.debug("ctkDICOMQueryTask : running task on thread id " +
               QString::number(reinterpret_cast<long long>(QThread::currentThreadId()), 16));
  switch(d->QueryLevel)
    {
    case DICOMLevel::Patients:
      if (!d->Query->queryPatients(this->taskUID()))
        {
        this->setIsFinished(true);
        emit canceled();
        return;
        }
      break;
    case DICOMLevel::Studies:
      if (!d->Query->queryStudies(this->taskUID(),
                                  d->PatientID))
        {
        this->setIsFinished(true);
        emit canceled();
        return;
        }
      break;
    case DICOMLevel::Series:
      if (!d->Query->querySeries(this->taskUID(),
                                 d->PatientID,
                                 d->StudyInstanceUID))
        {
        this->setIsFinished(true);
        emit canceled();
        return;
        }
      break;
    case DICOMLevel::Instances:
      if (!d->Query->queryInstances(this->taskUID(),
                                    d->PatientID,
                                    d->StudyInstanceUID,
                                    d->SeriesInstanceUID))
        {
        this->setIsFinished(true);
        emit canceled();
        return;
        }
      break;
    }

  this->setIsFinished(true);
  emit finished();
}

//----------------------------------------------------------------------------
QList<QSharedPointer<ctkDICOMTaskResults>> ctkDICOMQueryTask::taskResultsList() const
{
  Q_D(const ctkDICOMQueryTask);
  return d->Query->taskResultsList();
}

//----------------------------------------------------------------------------
void ctkDICOMQueryTask::setFilters(const QMap<QString, QVariant> &filters)
{
  Q_D(ctkDICOMQueryTask);
  d->Query->setFilters(filters);
}

//----------------------------------------------------------------------------
QMap<QString, QVariant> ctkDICOMQueryTask::filters() const
{
  Q_D(const ctkDICOMQueryTask);
  return d->Query->filters();
}

//----------------------------------------------------------------------------
static void skipDelete(QObject* obj)
{
  Q_UNUSED(obj);
  // this deleter does not delete the object from memory
  // useful if the pointer is not owned by the smart pointer
}

//----------------------------------------------------------------------------
void ctkDICOMQueryTask::setServer(ctkDICOMServer& server)
{
  Q_D(ctkDICOMQueryTask);

  d->Server = QSharedPointer<ctkDICOMServer>(&server, skipDelete);
  if (!d->Server)
    {
    logger.debug("server is null");
    return;
    }

  d->Query->setConnectionName(d->Server->connectionName());
  d->Query->setCallingAETitle(d->Server->callingAETitle());
  d->Query->setCalledAETitle(d->Server->calledAETitle());
  d->Query->setHost(d->Server->host());
  d->Query->setPort(d->Server->port());
  d->Query->setConnectionTimeout(d->Server->connectionTimeout());
}

//----------------------------------------------------------------------------
ctkDICOMServer* ctkDICOMQueryTask::server()const
{
  Q_D(const ctkDICOMQueryTask);
  return d->Server.data();
}

//----------------------------------------------------------------------------
QSharedPointer<ctkDICOMServer> ctkDICOMQueryTask::serverShared()const
{
  Q_D(const ctkDICOMQueryTask);
  return d->Server;
}

//----------------------------------------------------------------------------
void ctkDICOMQueryTask::setPatientID(const QString &patientID)
{
  Q_D(ctkDICOMQueryTask);
  d->PatientID = patientID;
}

//----------------------------------------------------------------------------
QString ctkDICOMQueryTask::patientID() const
{
  Q_D(const ctkDICOMQueryTask);
  return d->PatientID;
}

//----------------------------------------------------------------------------
void ctkDICOMQueryTask::setStudyInstanceUID(const QString& studyInstanceUID)
{
  Q_D(ctkDICOMQueryTask);
  d->StudyInstanceUID = studyInstanceUID;
}

//------------------------------------------------------------------------------
QString ctkDICOMQueryTask::studyInstanceUID() const
{
  Q_D(const ctkDICOMQueryTask);
  return d->StudyInstanceUID;
}

//----------------------------------------------------------------------------
void ctkDICOMQueryTask::setSeriesInstanceUID(const QString& seriesInstanceUID)
{
  Q_D(ctkDICOMQueryTask);
  d->SeriesInstanceUID = seriesInstanceUID;
}

//------------------------------------------------------------------------------
QString ctkDICOMQueryTask::seriesInstanceUID() const
{
  Q_D(const ctkDICOMQueryTask);
  return d->SeriesInstanceUID;
}

//------------------------------------------------------------------------------
ctkDICOMQuery *ctkDICOMQueryTask::querier() const
{
  Q_D(const ctkDICOMQueryTask);
  return d->Query;
}
