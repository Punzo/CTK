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
#include "ctkDICOMTaskPool.h"
#include "ctkDICOMTaskPool_p.h"
#include "ctkDICOMQuery.h"
#include "ctkDICOMQueryTask.h"
#include "ctkDICOMRetrieve.h"
#include "ctkDICOMRetrieveTask.h"
#include "ctkDICOMServer.h"
#include "ctkDICOMTaskResults.h"
#include "ctkDICOMUtil.h"
#include "ctkLogger.h"

static ctkLogger logger ( "org.commontk.dicom.DICOMTaskPool" );

//------------------------------------------------------------------------------
// ctkDICOMTaskPoolPrivate methods

//------------------------------------------------------------------------------
ctkDICOMTaskPoolPrivate::ctkDICOMTaskPoolPrivate(ctkDICOMTaskPool& obj)
  : q_ptr(&obj)
{    
  ctk::setDICOMLogLevel(ctkErrorLogLevel::Info);
  this->DicomDatabase = nullptr;
  this->Indexer = QSharedPointer<ctkDICOMIndexer> (new ctkDICOMIndexer);
  this->Indexer->setBackgroundImportEnabled(true);
  this->ThreadPool = QSharedPointer<QThreadPool> (new QThreadPool());

  this->MaximumNumberOfRetry = 3;
  this->RetryDelay = 100;
  this->MaximumPatientsQuery = 25;
}

//------------------------------------------------------------------------------
ctkDICOMTaskPoolPrivate::~ctkDICOMTaskPoolPrivate()
{
  Q_Q(ctkDICOMTaskPool);

  q->stopAllTasks();
  QObject::disconnect(this->Indexer.data(), SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)),
                      q, SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)));
  q->removeAllServers();
}

//------------------------------------------------------------------------------
QString ctkDICOMTaskPoolPrivate::generateUniqueTaskUID()
{
  return QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces);
}

//------------------------------------------------------------------------------
QString ctkDICOMTaskPoolPrivate::loggerQueryReport(ctkDICOMQueryTask *queryTask,
                                                   const QString& status)
{
  if (!queryTask)
    {
    return QString("");
    }

  switch (queryTask->queryLevel())
    {
    case ctkDICOMQueryTask::DICOMLevel::Patients:
      return QString("ctkDICOMTaskPool: query task at patients level %1.\n"
                     "TaskUID: %2\n"
                     "Server: %3")
                     .arg(status)
                     .arg(queryTask->taskUID())
                     .arg(queryTask->server()->connectionName());
      break;
    case ctkDICOMQueryTask::DICOMLevel::Studies:
      return QString("ctkDICOMTaskPool: query task at studies level %1.\n"
                     "TaskUID: %2\n"
                     "Server: %3\n"
                     "PatientID: %4")
                     .arg(status)
                     .arg(queryTask->taskUID())
                     .arg(queryTask->server()->connectionName())
                     .arg(queryTask->patientID());
      break;
    case ctkDICOMQueryTask::DICOMLevel::Series:
      return QString("ctkDICOMTaskPool: query task at series level %1.\n"
                     "TaskUID: %2\n"
                     "Server: %3\n"
                     "PatientID: %4\n"
                     "StudyInstanceUID: %5")
                     .arg(status)
                     .arg(queryTask->taskUID())
                     .arg(queryTask->server()->connectionName())
                     .arg(queryTask->patientID())
                     .arg(queryTask->studyInstanceUID());
      break;
    case ctkDICOMQueryTask::DICOMLevel::Instances:
      return QString("ctkDICOMTaskPool: query task at series level %1.\n"
                     "TaskUID: %2\n"
                     "Server: %3\n"
                     "PatientID: %4\n"
                     "StudyInstanceUID: %5\n"
                     "SeriesInstanceUID: %6")
                     .arg(status)
                     .arg(queryTask->taskUID())
                     .arg(queryTask->server()->connectionName())
                     .arg(queryTask->patientID())
                     .arg(queryTask->studyInstanceUID())
                     .arg(queryTask->seriesInstanceUID());
      break;
    }

  return QString("");
}

//------------------------------------------------------------------------------
QString ctkDICOMTaskPoolPrivate::loggerRetrieveReport(ctkDICOMRetrieveTask *retrieveTask,
                                                      const QString& status)
{
  if (!retrieveTask)
    {
    return QString("");
    }

  switch (retrieveTask->retrieveLevel())
    {
    case ctkDICOMRetrieveTask::DICOMLevel::Studies:
      return QString("ctkDICOMTaskPool: retrieve task at studies level %1.\n"
                     "TaskUID: %2\n"
                     "Server: %3\n"
                     "StudyInstanceUID: %4")
                     .arg(status)
                     .arg(retrieveTask->taskUID())
                     .arg(retrieveTask->server()->connectionName())
                     .arg(retrieveTask->studyInstanceUID());
      break;
    case ctkDICOMRetrieveTask::DICOMLevel::Series:
      return QString("ctkDICOMTaskPool: retrieve task at series level %1.\n"
                     "TaskUID: %2\n"
                     "Server: %3\n"
                     "StudyInstanceUID: %4\n"
                     "SeriesInstanceUID: %5")
                     .arg(status)
                     .arg(retrieveTask->taskUID())
                     .arg(retrieveTask->server()->connectionName())
                     .arg(retrieveTask->studyInstanceUID())
                     .arg(retrieveTask->seriesInstanceUID());
      break;
    case ctkDICOMRetrieveTask::DICOMLevel::Instances:
      return QString("ctkDICOMTaskPool: retrieve task at instances level %1.\n"
                     "TaskUID: %2\n"
                     "Server: %3\n"
                     "StudyInstanceUID: %4\n"
                     "SeriesInstanceUID: %5\n"
                     "SOPInstanceUID: %6")
                     .arg(status)
                     .arg(retrieveTask->taskUID())
                     .arg(retrieveTask->server()->connectionName())
                     .arg(retrieveTask->studyInstanceUID())
                     .arg(retrieveTask->seriesInstanceUID())
                     .arg(retrieveTask->sopInstanceUID());
      break;
    }

  return QString("");
}

//------------------------------------------------------------------------------
void ctkDICOMTaskPoolPrivate::init()
{
  Q_Q(ctkDICOMTaskPool);

  QObject::connect(this->Indexer.data(), SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)),
                   q, SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)));
}

//------------------------------------------------------------------------------
// ctkDICOMTaskPool methods

//------------------------------------------------------------------------------
ctkDICOMTaskPool::ctkDICOMTaskPool(QObject* parentObject)
  : Superclass(parentObject)
  , d_ptr(new ctkDICOMTaskPoolPrivate(*this))
{
  Q_D(ctkDICOMTaskPool);

  d->init();
}

//------------------------------------------------------------------------------
ctkDICOMTaskPool::~ctkDICOMTaskPool()
{
  this->waitForFinish();
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::queryPatients(QThread::Priority priority)
{
  Q_D(ctkDICOMTaskPool);

  foreach(QSharedPointer<ctkDICOMServer> server, d->Servers)
    {
    if (!server->queryRetrieveEnabled())
      {
      continue;
      }

    ctkDICOMQueryTask *task = new ctkDICOMQueryTask();
    task->setServer(*server);
    task->querier()->setMaximumPatientsQuery(d->MaximumPatientsQuery);
    task->setFilters(d->Filters);
    task->setQueryLevel(ctkDICOMQueryTask::DICOMLevel::Patients);
    task->setAutoDelete(false);

    QObject::connect(task, SIGNAL(started()), this, SLOT(taskStarted()), Qt::QueuedConnection);
    QObject::connect(task, SIGNAL(finished()), this, SLOT(taskFinished()), Qt::QueuedConnection);
    QObject::connect(task, SIGNAL(canceled()), this, SLOT(taskCanceled()), Qt::QueuedConnection);

    QString taskUID = d->generateUniqueTaskUID();
    task->setTaskUID(taskUID);
    d->Tasks.insert(taskUID, task);

    d->ThreadPool->start(task, priority);
    }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::queryStudies(const QString& patientID,
                                    QThread::Priority priority)
{
  Q_D(ctkDICOMTaskPool);

  foreach(QSharedPointer<ctkDICOMServer> server, d->Servers)
    {
    if (!server->queryRetrieveEnabled())
      {
      continue;
      }

    ctkDICOMQueryTask *task = new ctkDICOMQueryTask();
    task->setServer(*server);
    task->setFilters(d->Filters);
    task->setQueryLevel(ctkDICOMQueryTask::DICOMLevel::Studies);
    task->setPatientID(patientID);
    task->setAutoDelete(false);

    QObject::connect(task, SIGNAL(started()), this, SLOT(taskStarted()), Qt::QueuedConnection);
    QObject::connect(task, SIGNAL(finished()), this, SLOT(taskFinished()), Qt::QueuedConnection);
    QObject::connect(task, SIGNAL(canceled()), this, SLOT(taskCanceled()), Qt::QueuedConnection);

    QString taskUID = d->generateUniqueTaskUID();
    task->setTaskUID(taskUID);
    d->Tasks.insert(taskUID, task);

    d->ThreadPool->start(task, priority);
    }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::querySeries(const QString& patientID,
                                   const QString& studyInstanceUID,
                                   QThread::Priority priority)
{
  Q_D(ctkDICOMTaskPool);

  foreach(QSharedPointer<ctkDICOMServer> server, d->Servers)
    {
    if (!server->queryRetrieveEnabled())
      {
      continue;
      }

    ctkDICOMQueryTask *task = new ctkDICOMQueryTask();
    task->setServer(*server);
    task->setFilters(d->Filters);
    task->setQueryLevel(ctkDICOMQueryTask::DICOMLevel::Series);
    task->setPatientID(patientID);
    task->setStudyInstanceUID(studyInstanceUID);
    task->setAutoDelete(false);

    QObject::connect(task, SIGNAL(started()), this, SLOT(taskStarted()), Qt::QueuedConnection);
    QObject::connect(task, SIGNAL(finished()), this, SLOT(taskFinished()), Qt::QueuedConnection);
    QObject::connect(task, SIGNAL(canceled()), this, SLOT(taskCanceled()), Qt::QueuedConnection);

    QString taskUID = d->generateUniqueTaskUID();
    task->setTaskUID(taskUID);
    d->Tasks.insert(taskUID, task);

    d->ThreadPool->start(task, priority);
    }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::queryInstances(const QString& patientID,
                                      const QString& studyInstanceUID,
                                      const QString& seriesInstanceUID,
                                      QThread::Priority priority)
{
  Q_D(ctkDICOMTaskPool);

  foreach(QSharedPointer<ctkDICOMServer> server, d->Servers)
    {
    if (!server->queryRetrieveEnabled())
      {
      continue;
      }

    ctkDICOMQueryTask *task = new ctkDICOMQueryTask();
    task->setServer(*server);
    task->setFilters(d->Filters);
    task->setQueryLevel(ctkDICOMQueryTask::DICOMLevel::Instances);
    task->setPatientID(patientID);
    task->setStudyInstanceUID(studyInstanceUID);
    task->setSeriesInstanceUID(seriesInstanceUID);
    task->setAutoDelete(false);

    QObject::connect(task, SIGNAL(started()), this, SLOT(taskStarted()), Qt::QueuedConnection);
    QObject::connect(task, SIGNAL(finished()), this, SLOT(taskFinished()), Qt::QueuedConnection);
    QObject::connect(task, SIGNAL(canceled()), this, SLOT(taskCanceled()), Qt::QueuedConnection);

    QString taskUID = d->generateUniqueTaskUID();
    task->setTaskUID(taskUID);
    d->Tasks.insert(taskUID, task);

    d->ThreadPool->start(task, priority);
    }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::retrieveStudy(const QString &studyInstanceUID,
                                     QThread::Priority priority)
{
  Q_D(ctkDICOMTaskPool);

  foreach(QSharedPointer<ctkDICOMServer> server, d->Servers)
    {
    if (!server->queryRetrieveEnabled())
      {
      continue;
      }

    ctkDICOMRetrieveTask *task = new ctkDICOMRetrieveTask();
    task->setServer(*server);
    task->setRetrieveLevel(ctkDICOMRetrieveTask::DICOMLevel::Studies);
    task->setStudyInstanceUID(studyInstanceUID);
    task->setAutoDelete(false);

    QObject::connect(task, SIGNAL(started()), this, SLOT(taskStarted()), Qt::QueuedConnection);
    QObject::connect(task, SIGNAL(finished()), this, SLOT(taskFinished()), Qt::QueuedConnection);
    QObject::connect(task, SIGNAL(canceled()), this, SLOT(taskCanceled()), Qt::QueuedConnection);

    QString taskUID = d->generateUniqueTaskUID();
    task->setTaskUID(taskUID);
    d->Tasks.insert(taskUID, task);

    d->ThreadPool->start(task, priority);
    }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::retrieveSeries(const QString &studyInstanceUID,
                                      const QString &seriesInstanceUID,
                                      QThread::Priority priority)
{
  Q_D(ctkDICOMTaskPool);

  foreach(QSharedPointer<ctkDICOMServer> server, d->Servers)
    {
    if (!server->queryRetrieveEnabled())
      {
      continue;
      }

    ctkDICOMRetrieveTask *task = new ctkDICOMRetrieveTask();
    task->setServer(*server);
    task->setRetrieveLevel(ctkDICOMRetrieveTask::DICOMLevel::Series);
    task->setStudyInstanceUID(studyInstanceUID);
    task->setSeriesInstanceUID(seriesInstanceUID);
    task->setAutoDelete(false);

    QObject::connect(task, SIGNAL(started()), this, SLOT(taskStarted()), Qt::QueuedConnection);
    QObject::connect(task, SIGNAL(finished()), this, SLOT(taskFinished()), Qt::QueuedConnection);
    QObject::connect(task, SIGNAL(canceled()), this, SLOT(taskCanceled()), Qt::QueuedConnection);
    QObject::connect(task->retriever(), SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)),
                     this, SIGNAL(progressBarTaskDetail(ctkDICOMTaskResults*)), Qt::DirectConnection);

    QString taskUID = d->generateUniqueTaskUID();
    task->setTaskUID(taskUID);
    d->Tasks.insert(taskUID, task);

    d->ThreadPool->start(task, priority);
    }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::retrieveSOPInstance(const QString &studyInstanceUID,
                                           const QString &seriesInstanceUID,
                                           const QString &SOPInstanceUID,
                                           QThread::Priority priority)
{
  Q_D(ctkDICOMTaskPool);

  foreach(QSharedPointer<ctkDICOMServer> server, d->Servers)
    {
    if (!server->queryRetrieveEnabled())
      {
      continue;
      }

    ctkDICOMRetrieveTask *task = new ctkDICOMRetrieveTask();
    task->setServer(*server);
    task->setRetrieveLevel(ctkDICOMRetrieveTask::DICOMLevel::Instances);
    task->setStudyInstanceUID(studyInstanceUID);
    task->setSeriesInstanceUID(seriesInstanceUID);
    task->setSOPInstanceUID(SOPInstanceUID);
    task->setAutoDelete(false);

    QObject::connect(task, SIGNAL(started()), this, SLOT(taskStarted()), Qt::QueuedConnection);
    QObject::connect(task, SIGNAL(finished()), this, SLOT(taskFinished()), Qt::QueuedConnection);
    QObject::connect(task, SIGNAL(canceled()), this, SLOT(taskCanceled()), Qt::QueuedConnection);

    QString taskUID = d->generateUniqueTaskUID();
    task->setTaskUID(taskUID);
    d->Tasks.insert(taskUID, task);

    d->ThreadPool->start(task, priority);
    }
}

//----------------------------------------------------------------------------
static void skipDelete(QObject* obj)
{
  Q_UNUSED(obj);
  // this deleter does not delete the object from memory
  // useful if the pointer is not owned by the smart pointer
}

//----------------------------------------------------------------------------
ctkDICOMDatabase* ctkDICOMTaskPool::dicomDatabase()const
{
  Q_D(const ctkDICOMTaskPool);
  return d->DicomDatabase.data();
}

//----------------------------------------------------------------------------
QSharedPointer<ctkDICOMDatabase> ctkDICOMTaskPool::dicomDatabaseShared()const
{
  Q_D(const ctkDICOMTaskPool);
  return d->DicomDatabase;
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::setDicomDatabase(ctkDICOMDatabase& dicomDatabase)
{
  Q_D(ctkDICOMTaskPool);
  d->DicomDatabase = QSharedPointer<ctkDICOMDatabase>(&dicomDatabase, skipDelete);
  if (d->Indexer)
    {
    d->Indexer->setDatabase(d->DicomDatabase.data());
    }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::setDicomDatabase(QSharedPointer<ctkDICOMDatabase> dicomDatabase)
{
  Q_D(ctkDICOMTaskPool);
  d->DicomDatabase = dicomDatabase;
  if (d->Indexer)
    {
    d->Indexer->setDatabase(d->DicomDatabase.data());
    }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::setFilters(const QMap<QString, QVariant> &filters)
{
  Q_D(ctkDICOMTaskPool);
  d->Filters = filters;
}

//----------------------------------------------------------------------------
QMap<QString, QVariant> ctkDICOMTaskPool::filters() const
{
  Q_D(const ctkDICOMTaskPool);
  return d->Filters;
}

//----------------------------------------------------------------------------
int ctkDICOMTaskPool::getNumberOfServers()
{
  Q_D(ctkDICOMTaskPool);
  return d->Servers.size();
}

//----------------------------------------------------------------------------
int ctkDICOMTaskPool::getNumberOfQueryRetrieveServers()
{
  Q_D(ctkDICOMTaskPool);
  int numberOfServers = 0;
  foreach (QSharedPointer<ctkDICOMServer> server, d->Servers)
    {
    if (server && server->queryRetrieveEnabled())
      {
      numberOfServers++;
      }
    }
  return numberOfServers;
}

//----------------------------------------------------------------------------
int ctkDICOMTaskPool::getNumberOfStorageServers()
{
  Q_D(ctkDICOMTaskPool);
  int numberOfServers = 0;
  foreach (QSharedPointer<ctkDICOMServer> server, d->Servers)
    {
    if (server && server->storageEnabled())
      {
      numberOfServers++;
      }
    }
  return numberOfServers;
}

//----------------------------------------------------------------------------
ctkDICOMServer* ctkDICOMTaskPool::getNthServer(int id)
{
  Q_D(ctkDICOMTaskPool);
  if (id < 0 || id > d->Servers.size() - 1)
    {
    return nullptr;
    }
  return d->Servers[id].data();
}

//----------------------------------------------------------------------------
ctkDICOMServer* ctkDICOMTaskPool::getServer(const char *connectionName)
{
  return this->getNthServer(this->getServerIndexFromName(connectionName));
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::addServer(ctkDICOMServer* server)
{
  Q_D(ctkDICOMTaskPool);
  this->stopAllTasks();
  this->waitForFinish();
  QSharedPointer<ctkDICOMServer> QsharedServer = QSharedPointer<ctkDICOMServer>(server);
  d->Servers.push_back(QsharedServer);
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::removeServer(const char *connectionName)
{
  this->removeNthServer(this->getServerIndexFromName(connectionName));
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::removeNthServer(int id)
{
  Q_D(ctkDICOMTaskPool);
  if (id < 0 || id > d->Servers.size() - 1)
    {
    return;
    }

  this->stopAllTasks();
  this->waitForFinish();
  d->Servers.erase(d->Servers.begin() + id);
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::removeAllServers()
{
  Q_D(ctkDICOMTaskPool);
  this->stopAllTasks();
  this->waitForFinish();
  d->Servers.clear();
}

//----------------------------------------------------------------------------
QString ctkDICOMTaskPool::getServerNameFromIndex(int id)
{
  Q_D(ctkDICOMTaskPool);
  if (id < 0 || id > d->Servers.size() - 1)
    {
    return "";
    }

  QSharedPointer<ctkDICOMServer> server = d->Servers[id];
  if (!server)
    {
    return "";
    }

  return server->connectionName();
}

//----------------------------------------------------------------------------
int ctkDICOMTaskPool::getServerIndexFromName(const char *connectionName)
{
  Q_D(ctkDICOMTaskPool);
  if (!connectionName)
    {
    return -1;
    }
  for(int serverIndex = 0; serverIndex < d->Servers.size(); ++serverIndex)
    {
    QSharedPointer<ctkDICOMServer> server = d->Servers[serverIndex];
    if (server && server->connectionName() == connectionName)
      {
      // found
      return serverIndex;
      }
    }
  return -1;
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::waitForFinish(int msecTimeout)
{
  Q_D(ctkDICOMTaskPool);

  if (!d->ThreadPool)
    {
    return;
    }

  QElapsedTimer timer;
  timer.start();
  if (msecTimeout == -1)
    {
    if(d->ThreadPool->activeThreadCount())
      {
      d->ThreadPool->waitForDone(msecTimeout);
      }
    }
  else
    {
    while(d->Tasks.count() > 0 && timer.elapsed() <= msecTimeout * 5)
      {
      QCoreApplication::processEvents();
      if(d->ThreadPool->activeThreadCount())
        {
        d->ThreadPool->waitForDone(msecTimeout);
        }
      }
    }

  QCoreApplication::processEvents();
  this->deleteAllTasks();
}

//----------------------------------------------------------------------------
int ctkDICOMTaskPool::totalTasks()
{
  Q_D(ctkDICOMTaskPool);
  return d->Tasks.count();
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::stopAllTasksNotStarted()
{
  Q_D(ctkDICOMTaskPool);

  d->ThreadPool->clear();
  foreach (ctkAbstractTask* task, d->Tasks)
    {
    if (task->isRunning() || task->isFinished())
      {
      continue;
      }

    task->setStop(true);
    }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::stopAllTasks()
{
  Q_D(ctkDICOMTaskPool);

  d->ThreadPool->clear();
  foreach (ctkAbstractTask* task, d->Tasks)
    {
    task->setStop(true);
    if (!d->ThreadPool->tryTake(task))
      {
      logger.debug(QString("ctkDICOMTaskPool::stopAllTasks : failed to cancel task %1").arg(task->taskUID()));
      }
    }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::deleteTask(QString taskUID)
{
  Q_D(ctkDICOMTaskPool);

  QMap<QString, ctkAbstractTask*>::iterator it = d->Tasks.find(taskUID);
  if (it == d->Tasks.end())
    {
    return;
    }

  ctkAbstractTask* task = d->Tasks.value(taskUID);
  if (!task)
    {
    return;
    }

  logger.debug(QString("ctkDICOMTaskPool: deleting task object %1").arg(taskUID));

  task->disconnect();
  d->Tasks.remove(taskUID);
  task->deleteLater();
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::deleteAllTasks()
{
  Q_D(ctkDICOMTaskPool);

  foreach (ctkAbstractTask* task, d->Tasks)
    {
    if (!task)
      {
      continue;
      }
    this->deleteTask(task->taskUID());
    }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::stopTasks(const QString &studyInstanceUID,
                                 const QString &seriesInstanceUID,
                                 const QString &sopInstanceUID)
{
  Q_D(ctkDICOMTaskPool);

  foreach (ctkAbstractTask* task, d->Tasks)
    {
    ctkDICOMRetrieveTask* retrieveTask = qobject_cast<ctkDICOMRetrieveTask*>(task);
    if (retrieveTask)
      {
      if (retrieveTask->isFinished() ||
          retrieveTask->studyInstanceUID() != studyInstanceUID ||
          (!retrieveTask->seriesInstanceUID().isEmpty() && !seriesInstanceUID.isEmpty() &&
           retrieveTask->seriesInstanceUID() != seriesInstanceUID) ||
          (!retrieveTask->sopInstanceUID().isEmpty() && !sopInstanceUID.isEmpty() &&
           retrieveTask->sopInstanceUID() != sopInstanceUID))
        {
        continue;
        }

      retrieveTask->setStop(true);
      }

    ctkDICOMQueryTask* queryTask = qobject_cast<ctkDICOMQueryTask*>(task);
    if (queryTask)
      {
      if (queryTask->isFinished() ||
          queryTask->studyInstanceUID() != studyInstanceUID ||
          (!queryTask->seriesInstanceUID().isEmpty() && !seriesInstanceUID.isEmpty() &&
           queryTask->seriesInstanceUID() != seriesInstanceUID))
        {
        continue;
        }

      queryTask->setStop(true);
      }
  }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::lowerPriorityToAllTasks()
{
  Q_D(ctkDICOMTaskPool);

  foreach (ctkAbstractTask* task, d->Tasks)
    {
    // Lower priority for all other retrieve tasks
    if (d->ThreadPool->tryTake(task))
      {
      d->ThreadPool->start(task, QThread::LowPriority);
      }
    }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::raiseRetrieveFramesTasksPriorityForSeries(const QString &studyInstanceUID,
                                                                 const QString &seriesInstanceUID,
                                                                 QThread::Priority priority)
{
  Q_D(ctkDICOMTaskPool);

  foreach (ctkAbstractTask* task, d->Tasks)
    {
    ctkDICOMRetrieveTask* retrieveTask = qobject_cast<ctkDICOMRetrieveTask*>(task);
    if (!retrieveTask || retrieveTask->isRunning() || retrieveTask->isFinished())
      {
      continue;
      }

    // Raise priority for the tasks associated to the selected thumbnail
    if (retrieveTask->retrieveLevel() == ctkDICOMRetrieveTask::DICOMLevel::Series &&
        retrieveTask->studyInstanceUID() == studyInstanceUID &&
        retrieveTask->seriesInstanceUID() == seriesInstanceUID)
      {
      if (d->ThreadPool->tryTake(retrieveTask))
        {
        d->ThreadPool->start(task, priority);
        }
      }
    }
}

//----------------------------------------------------------------------------
int ctkDICOMTaskPool::maximumThreadCount() const
{
  Q_D(const ctkDICOMTaskPool);
  return d->ThreadPool->maxThreadCount();
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::setMaximumThreadCount(const int &maximumThreadCount)
{
  Q_D(ctkDICOMTaskPool);

  d->ThreadPool->setMaxThreadCount(maximumThreadCount);
}

//----------------------------------------------------------------------------
int ctkDICOMTaskPool::maximumNumberOfRetry() const
{
  Q_D(const ctkDICOMTaskPool);
  return d->MaximumNumberOfRetry;
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::setMaximumNumberOfRetry(const int &maximumNumberOfRetry)
{
  Q_D(ctkDICOMTaskPool);
  d->MaximumNumberOfRetry = maximumNumberOfRetry;
}

//----------------------------------------------------------------------------
int ctkDICOMTaskPool::retryDelay() const
{
  Q_D(const ctkDICOMTaskPool);
  return d->RetryDelay;
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::setRetryDelay(const int &retryDelay)
{
  Q_D(ctkDICOMTaskPool);
  d->RetryDelay = retryDelay;
}

//------------------------------------------------------------------------------
void ctkDICOMTaskPool::setMaximumPatientsQuery(const int maximumPatientsQuery)
{
  Q_D(ctkDICOMTaskPool);
  d->MaximumPatientsQuery = maximumPatientsQuery;
}

//------------------------------------------------------------------------------
int ctkDICOMTaskPool::maximumPatientsQuery()
{
  Q_D(const ctkDICOMTaskPool);
  return d->MaximumPatientsQuery;
}

//----------------------------------------------------------------------------
QSharedPointer<ctkDICOMIndexer> ctkDICOMTaskPool::Indexer() const
{
  Q_D(const ctkDICOMTaskPool);
  return d->Indexer;
}

//----------------------------------------------------------------------------
QSharedPointer<QThreadPool> ctkDICOMTaskPool::threadPool() const
{
  Q_D(const ctkDICOMTaskPool);
  return d->ThreadPool;
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::taskStarted()
{
  Q_D(ctkDICOMTaskPool);
  ctkDICOMQueryTask* queryTask = qobject_cast<ctkDICOMQueryTask*>(this->sender());
  if (queryTask)
    {
    logger.debug(d->loggerQueryReport(queryTask, "started"));
    }

  ctkDICOMRetrieveTask* retrieveTask = qobject_cast<ctkDICOMRetrieveTask*>(this->sender());
  if (retrieveTask)
    {
    logger.debug(d->loggerRetrieveReport(retrieveTask, "started"));
    }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::taskFinished()
{
  Q_D(ctkDICOMTaskPool);

  ctkDICOMQueryTask* queryTask = qobject_cast<ctkDICOMQueryTask*>(this->sender());
  if (queryTask)
    {
    logger.debug(d->loggerQueryReport(queryTask, "finished"));

    if (queryTask->taskResultsList().count() > 0 && !queryTask->isStopped())
      {
      d->Indexer->insertTaskResultsList(queryTask->taskResultsList());
      }
    else
      {
      emit this->progressTaskDetail(nullptr);
      }
    }

  ctkDICOMRetrieveTask* retrieveTask = qobject_cast<ctkDICOMRetrieveTask*>(this->sender());
  if (retrieveTask)
    {
    logger.debug(d->loggerRetrieveReport(retrieveTask, "finished"));

    QList<QSharedPointer<ctkDICOMTaskResults>> taskResultsList = retrieveTask->taskResultsList();
    ctkDICOMServer* server = retrieveTask->server();
    if (server && server->retrieveProtocol() == ctkDICOMServer::CMOVE && !retrieveTask->isStopped())
      {
      // it is a move retrieve
      // if the server owns a proxyServer:
      // 1) notify UI
      // 2) start a get operation to the proxy server.
      // if the task pool owns a storage listener -> ... To Do.

      foreach (QSharedPointer<ctkDICOMTaskResults> taskResults, taskResultsList)
        {
        emit this->progressTaskDetail(taskResults.data());
        }

      ctkDICOMServer* proxyServer = retrieveTask->server()->proxyServer();
      if (proxyServer && proxyServer->queryRetrieveEnabled())
        {
        QString newTaskUID = d->generateUniqueTaskUID();
        ctkDICOMRetrieveTask* newRetrieveTask = new ctkDICOMRetrieveTask();
        newRetrieveTask->setServer(*proxyServer);
        newRetrieveTask->setRetrieveLevel(retrieveTask->retrieveLevel());
        newRetrieveTask->setStudyInstanceUID(retrieveTask->studyInstanceUID());
        newRetrieveTask->setSeriesInstanceUID(retrieveTask->seriesInstanceUID());
        newRetrieveTask->setSOPInstanceUID(retrieveTask->sopInstanceUID());
        newRetrieveTask->setNumberOfRetry(retrieveTask->numberOfRetry() + 1);
        newRetrieveTask->setTaskUID(newTaskUID);
        newRetrieveTask->setAutoDelete(false);

        QObject::connect(newRetrieveTask, SIGNAL(started()), this, SLOT(taskStarted()), Qt::AutoConnection);
        QObject::connect(newRetrieveTask, SIGNAL(finished()), this, SLOT(taskFinished()), Qt::AutoConnection);
        QObject::connect(newRetrieveTask, SIGNAL(canceled()), this, SLOT(taskCanceled()), Qt::AutoConnection);
        if (newRetrieveTask->retrieveLevel() == ctkDICOMRetrieveTask::DICOMLevel::Series)
          {
          QObject::connect(newRetrieveTask->retriever(), SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)),
                           this, SIGNAL(progressBarTaskDetail(ctkDICOMTaskResults*)), Qt::DirectConnection);
          }

        d->Tasks.insert(newTaskUID, newRetrieveTask);
        QThread::Priority priority = QThread::LowPriority;
        if (newRetrieveTask->retriever()->getLastRetrieveType() == ctkDICOMRetrieve::RetrieveType::RetrieveSOPInstance)
          {
          priority = QThread::NormalPriority;
          }

        d->ThreadPool->start(newRetrieveTask, priority);
        }
      }
    else if (taskResultsList.count() > 0 && !retrieveTask->isStopped())
      {
      // it is a get retrieve -> insert the results in the database
      d->Indexer->insertTaskResultsList(taskResultsList);
      }
    else
      {
      // no results from the retrieve
      emit this->progressTaskDetail(nullptr);
      }
    }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::taskCanceled()
{
  Q_D(ctkDICOMTaskPool);

  ctkAbstractTask* task = qobject_cast<ctkAbstractTask*>(this->sender());
  if (!task)
    {
    return;
    }

  ctkDICOMQueryTask* queryTask = qobject_cast<ctkDICOMQueryTask*>(task);
  if (queryTask)
    {
    logger.debug(d->loggerQueryReport(queryTask, "canceled"));

    QString taskUID = queryTask->taskUID();
    if (queryTask->numberOfRetry() < d->MaximumNumberOfRetry && !queryTask->isStopped())
      {
      ctkDICOMQueryTask* newQueryTask = new ctkDICOMQueryTask();
      newQueryTask->setServer(*queryTask->server());
      newQueryTask->setFilters(d->Filters);
      newQueryTask->setQueryLevel(queryTask->queryLevel());
      newQueryTask->setStudyInstanceUID(queryTask->studyInstanceUID());
      newQueryTask->setSeriesInstanceUID(queryTask->seriesInstanceUID());
      newQueryTask->setNumberOfRetry(queryTask->numberOfRetry() + 1);
      newQueryTask->setTaskUID(taskUID);
      newQueryTask->setAutoDelete(false);

      QTimer::singleShot(d->RetryDelay, this, [=] () {taskRetry(newQueryTask); });
      }
    else if (!queryTask->isStopped())
      {
      logger.debug(d->loggerQueryReport(queryTask, "failed"));
      emit this->progressTaskDetail(nullptr);
      }

    this->deleteTask(taskUID);
    }

  ctkDICOMRetrieveTask* retrieveTask = qobject_cast<ctkDICOMRetrieveTask*>(task);
  if (retrieveTask)
    {
    logger.debug(d->loggerRetrieveReport(retrieveTask, "canceled"));

    QString taskUID = retrieveTask->taskUID();
    if (retrieveTask->numberOfRetry() < d->MaximumNumberOfRetry && !retrieveTask->isStopped())
      {
      ctkDICOMRetrieveTask* newRetrieveTask = new ctkDICOMRetrieveTask();
      newRetrieveTask->setServer(*retrieveTask->server());
      newRetrieveTask->setRetrieveLevel(retrieveTask->retrieveLevel());
      newRetrieveTask->setStudyInstanceUID(retrieveTask->studyInstanceUID());
      newRetrieveTask->setSeriesInstanceUID(retrieveTask->seriesInstanceUID());
      newRetrieveTask->setSOPInstanceUID(retrieveTask->sopInstanceUID());
      newRetrieveTask->setNumberOfRetry(retrieveTask->numberOfRetry() + 1);
      newRetrieveTask->setTaskUID(taskUID);
      newRetrieveTask->setAutoDelete(false);

      QTimer::singleShot(d->RetryDelay, this, [=] () {taskRetry(newRetrieveTask); });
      }
    else if (!retrieveTask->isStopped())
      {
      logger.debug(d->loggerRetrieveReport(retrieveTask, "failed"));
      }

    this->deleteTask(taskUID);
    }
}

//----------------------------------------------------------------------------
void ctkDICOMTaskPool::taskRetry(ctkAbstractTask *task,
                                 QThread::Priority priority)
{
  Q_D(ctkDICOMTaskPool);

  logger.debug("ctkDICOMTaskPool: retry task."
               "TaskUID: " + task->taskUID());

  QObject::connect(task, SIGNAL(started()), this, SLOT(taskStarted()), Qt::AutoConnection);
  QObject::connect(task, SIGNAL(finished()), this, SLOT(taskFinished()), Qt::AutoConnection);
  QObject::connect(task, SIGNAL(canceled()), this, SLOT(taskCanceled()), Qt::AutoConnection);

  ctkDICOMRetrieveTask* retrieveTask = qobject_cast<ctkDICOMRetrieveTask*>(task);
  if (retrieveTask && retrieveTask->retrieveLevel() == ctkDICOMRetrieveTask::DICOMLevel::Series)
    {
    QObject::connect(retrieveTask->retriever(), SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)),
                     this, SIGNAL(progressBarTaskDetail(ctkDICOMTaskResults*)), Qt::DirectConnection);
    }

  QString newTaskUID = d->generateUniqueTaskUID();
  task->setTaskUID(newTaskUID);
  d->Tasks.insert(newTaskUID, task);

  d->ThreadPool->start(task, priority);
}

