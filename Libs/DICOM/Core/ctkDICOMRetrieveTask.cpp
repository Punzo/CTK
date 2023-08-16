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
#include "ctkDICOMRetrieveTask_p.h"
#include "ctkLogger.h"

static ctkLogger logger ( "org.commontk.dicom.DICOMRetrieveAbstractWorker" );

//------------------------------------------------------------------------------
// ctkDICOMRetrieveTaskPrivate methods

//------------------------------------------------------------------------------
ctkDICOMRetrieveTaskPrivate::ctkDICOMRetrieveTaskPrivate(ctkDICOMRetrieveTask* object)
 : q_ptr(object)
{
  this->Retrieve = new ctkDICOMRetrieve;
  this->Server = nullptr;
  this->RetrieveLevel = ctkDICOMRetrieveTask::DICOMLevel::Studies;

  this->StudyInstanceUID = "";
  this->SeriesInstanceUID = "";
  this->SOPInstanceUID = "";
}

//------------------------------------------------------------------------------
ctkDICOMRetrieveTaskPrivate::~ctkDICOMRetrieveTaskPrivate()
{
  if (this->Retrieve)
    {
    delete this->Retrieve;
    this->Retrieve = nullptr;
    }
}

//------------------------------------------------------------------------------
// ctkDICOMRetrieveTask methods

//------------------------------------------------------------------------------
ctkDICOMRetrieveTask::ctkDICOMRetrieveTask()
  : d_ptr(new ctkDICOMRetrieveTaskPrivate(this))
{
}

//------------------------------------------------------------------------------
ctkDICOMRetrieveTask::ctkDICOMRetrieveTask(ctkDICOMRetrieveTaskPrivate* pimpl)
  : d_ptr(pimpl)
{
}

//------------------------------------------------------------------------------
ctkDICOMRetrieveTask::~ctkDICOMRetrieveTask()
{
}

//------------------------------------------------------------------------------
void ctkDICOMRetrieveTask::setRetrieveLevel(const DICOMLevel& retrieveLevel)
{
  Q_D(ctkDICOMRetrieveTask);
  d->RetrieveLevel = retrieveLevel;
}

//------------------------------------------------------------------------------
ctkDICOMRetrieveTask::DICOMLevel ctkDICOMRetrieveTask::retrieveLevel() const
{
  Q_D(const ctkDICOMRetrieveTask);
  return d->RetrieveLevel;
}

//----------------------------------------------------------------------------
void ctkDICOMRetrieveTask::setTaskUID(const QString &taskUID)
{
  Q_D(const ctkDICOMRetrieveTask);

  Superclass::setTaskUID(taskUID);
  d->Retrieve->setTaskUID(taskUID);
}

//----------------------------------------------------------------------------
void ctkDICOMRetrieveTask::setStop(const bool& stop)
{
  Q_D(const ctkDICOMRetrieveTask);
  ctkAbstractTask::setStop(stop);
  d->Retrieve->cancel();
}

//----------------------------------------------------------------------------
void ctkDICOMRetrieveTask::run()
{
  Q_D(const ctkDICOMRetrieveTask);
  if (this->isStopped() || !d->Server)
    {
    this->setIsFinished(true);
    emit canceled();
    return;
    }

  this->setIsRunning(true);
  emit started();

  logger.debug("ctkDICOMRetrieveTask : running task on thread id " +
               QString::number(reinterpret_cast<long long>(QThread::currentThreadId()), 16));

  switch (d->Server->retrieveProtocol())
    {
    case ctkDICOMServer::CGET:
      switch(d->RetrieveLevel)
        {
        case DICOMLevel::Studies:
          if (!d->Retrieve->getStudy(d->StudyInstanceUID))
            {
            this->setIsFinished(true);
            emit canceled();
            return;
            }
          break;
        case DICOMLevel::Series:
          if (!d->Retrieve->getSeries(d->StudyInstanceUID,
                                      d->SeriesInstanceUID))
            {
            this->setIsFinished(true);
            emit canceled();
            return;
            }
          break;
        case DICOMLevel::Instances:
          if (!d->Retrieve->getSOPInstance(d->StudyInstanceUID,
                                           d->SeriesInstanceUID,
                                           d->SOPInstanceUID))
            {
            this->setIsFinished(true);
            emit canceled();
            return;
            }
          break;
        }
      break;
    case ctkDICOMServer::CMOVE:
      switch(d->RetrieveLevel)
        {
        case DICOMLevel::Studies:
          if (!d->Retrieve->moveStudy(d->StudyInstanceUID))
            {
            this->setIsFinished(true);
            emit canceled();
            return;
            }
          break;
        case DICOMLevel::Series:
          if (!d->Retrieve->moveSeries(d->StudyInstanceUID,
                                       d->SeriesInstanceUID))
            {
            this->setIsFinished(true);
            emit canceled();
            return;
            }
          break;
        case DICOMLevel::Instances:
          if (!d->Retrieve->moveSOPInstance(d->StudyInstanceUID,
                                            d->SeriesInstanceUID,
                                            d->SOPInstanceUID))
            {
            this->setIsFinished(true);
            emit canceled();
            return;
            }
          break;
        }
      break;
    //case ctkDICOMServer::WADO: // To Do
    }

  this->setIsFinished(true);
  emit finished();
}

//----------------------------------------------------------------------------
ctkDICOMServer* ctkDICOMRetrieveTask::server()const
{
  Q_D(const ctkDICOMRetrieveTask);
  return d->Server.data();
}

//----------------------------------------------------------------------------
QSharedPointer<ctkDICOMServer> ctkDICOMRetrieveTask::serverShared()const
{
  Q_D(const ctkDICOMRetrieveTask);
  return d->Server;
}

//----------------------------------------------------------------------------
static void skipDelete(QObject* obj)
{
  Q_UNUSED(obj);
  // this deleter does not delete the object from memory
  // useful if the pointer is not owned by the smart pointer
}

//----------------------------------------------------------------------------
void ctkDICOMRetrieveTask::setServer(ctkDICOMServer& server)
{
  Q_D(ctkDICOMRetrieveTask);

  d->Server = QSharedPointer<ctkDICOMServer>(&server, skipDelete);
  if (!d->Server)
    {
    logger.debug("server is null");
    return;
    }

  d->Retrieve->setConnectionName(d->Server->connectionName());
  d->Retrieve->setCallingAETitle(d->Server->callingAETitle());
  d->Retrieve->setCalledAETitle(d->Server->calledAETitle());
  d->Retrieve->setHost(d->Server->host());
  d->Retrieve->setPort(d->Server->port());
  d->Retrieve->setConnectionTimeout(d->Server->connectionTimeout());
  d->Retrieve->setMoveDestinationAETitle(d->Server->moveDestinationAETitle());
  d->Retrieve->setKeepAssociationOpen(d->Server->keepAssociationOpen());
}

//----------------------------------------------------------------------------
QList<QSharedPointer<ctkDICOMTaskResults>> ctkDICOMRetrieveTask::taskResultsList() const
{
  Q_D(const ctkDICOMRetrieveTask);
  return d->Retrieve->taskResultsList();
}

//----------------------------------------------------------------------------
void ctkDICOMRetrieveTask::setStudyInstanceUID(const QString& studyInstanceUID)
{
  Q_D(ctkDICOMRetrieveTask);
  d->StudyInstanceUID = studyInstanceUID;
}

//------------------------------------------------------------------------------
QString ctkDICOMRetrieveTask::studyInstanceUID() const
{
  Q_D(const ctkDICOMRetrieveTask);
  return d->StudyInstanceUID;
}

//----------------------------------------------------------------------------
void ctkDICOMRetrieveTask::setSeriesInstanceUID(const QString& seriesInstanceUID)
{
  Q_D(ctkDICOMRetrieveTask);
  d->SeriesInstanceUID = seriesInstanceUID;
}

//------------------------------------------------------------------------------
QString ctkDICOMRetrieveTask::seriesInstanceUID() const
{
  Q_D(const ctkDICOMRetrieveTask);
  return d->SeriesInstanceUID;
}

//----------------------------------------------------------------------------
void ctkDICOMRetrieveTask::setSOPInstanceUID(const QString& sopInstanceUID)
{
  Q_D(ctkDICOMRetrieveTask);
  d->SOPInstanceUID = sopInstanceUID;
}

//------------------------------------------------------------------------------
QString ctkDICOMRetrieveTask::sopInstanceUID() const
{
  Q_D(const ctkDICOMRetrieveTask);
  return d->SOPInstanceUID;
}

//------------------------------------------------------------------------------
ctkDICOMRetrieve *ctkDICOMRetrieveTask::retriever() const
{
  Q_D(const ctkDICOMRetrieveTask);
  return d->Retrieve;
}
