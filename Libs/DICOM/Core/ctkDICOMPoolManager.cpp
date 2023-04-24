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
#include "ctkDICOMPoolManager.h"
#include "ctkDICOMPoolManager_p.h"
#include "ctkDICOMQueryTask.h"
#include "ctkDICOMRetrieveTask.h"
#include "ctkDICOMServer.h"
#include "ctkDICOMTaskResults.h"
#include "ctkDICOMUtil.h"
#include "ctkLogger.h"

static ctkLogger logger ( "org.commontk.dicom.DICOMPoolManager" );

//------------------------------------------------------------------------------
// ctkDICOMPoolManagerPrivate methods

//------------------------------------------------------------------------------
ctkDICOMPoolManagerPrivate::ctkDICOMPoolManagerPrivate(ctkDICOMPoolManager& obj)
  : q_ptr(&obj)
{    
  ctk::setDICOMLogLevel(ctkErrorLogLevel::Info);
  this->DicomDatabase = nullptr;
  this->Indexer = QSharedPointer<ctkDICOMIndexer> (new ctkDICOMIndexer);
  this->Indexer->setBackgroundImportEnabled(true);
  this->ThreadPool = QSharedPointer<QThreadPool> (new QThreadPool());

  this->MaximumNumberOfRetry = 3;
  this->RetryDelay = 100;
}

//------------------------------------------------------------------------------
ctkDICOMPoolManagerPrivate::~ctkDICOMPoolManagerPrivate()
{
  Q_Q(ctkDICOMPoolManager);

  foreach (ctkAbstractTask* task, this->Tasks)
    {
    task->setStop(true);
    q->deleteTask(task->taskUID());
    }

  this->Tasks.clear();
  this->TaskResults.clear();

  QObject::disconnect(this->Indexer.data(), SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)),
                      q, SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)));
}

//------------------------------------------------------------------------------
QString ctkDICOMPoolManagerPrivate::generateUniqueTaskUID()
{
  return QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces);
}

//------------------------------------------------------------------------------
void ctkDICOMPoolManagerPrivate::init()
{
  Q_Q(ctkDICOMPoolManager);

  QObject::connect(this->Indexer.data(), SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)),
                   q, SIGNAL(progressTaskDetail(ctkDICOMTaskResults*)));
}

//------------------------------------------------------------------------------
// ctkDICOMPoolManager methods

//------------------------------------------------------------------------------
ctkDICOMPoolManager::ctkDICOMPoolManager(QObject* parentObject)
  : Superclass(parentObject)
  , d_ptr(new ctkDICOMPoolManagerPrivate(*this))
{
  Q_D(ctkDICOMPoolManager);

  d->init();
}

//------------------------------------------------------------------------------
ctkDICOMPoolManager::~ctkDICOMPoolManager()
{
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::queryStudies(QThread::Priority priority)
{
  Q_D(ctkDICOMPoolManager);

  d->TaskResults.clear();

  foreach(QSharedPointer<ctkDICOMServer> server, d->Servers)
    {
    ctkDICOMQueryTask *task = new ctkDICOMQueryTask();
    task->setServer(*server);
    task->setFilters(d->Filters);
    task->setQueryLevel(ctkDICOMQueryTask::DICOMLevel::Studies);
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
void ctkDICOMPoolManager::querySeries(const QString& studyInstanceUID,
                                      QThread::Priority priority)
{
  Q_D(ctkDICOMPoolManager);

  foreach(QSharedPointer<ctkDICOMServer> server, d->Servers)
    {
    ctkDICOMQueryTask *task = new ctkDICOMQueryTask();
    task->setServer(*server);
    task->setFilters(d->Filters);
    task->setQueryLevel(ctkDICOMQueryTask::DICOMLevel::Series);
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
void ctkDICOMPoolManager::queryInstances(const QString& studyInstanceUID,
                                         const QString& seriesInstanceUID,
                                         QThread::Priority priority)
{
  Q_D(ctkDICOMPoolManager);

  foreach(QSharedPointer<ctkDICOMServer> server, d->Servers)
    {
    ctkDICOMQueryTask *task = new ctkDICOMQueryTask();
    task->setServer(*server);
    task->setFilters(d->Filters);
    task->setQueryLevel(ctkDICOMQueryTask::DICOMLevel::Instances);
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
void ctkDICOMPoolManager::retrieveStudy(const QString &studyInstanceUID,
                                        QThread::Priority priority)
{
  Q_D(ctkDICOMPoolManager);

    foreach(QSharedPointer<ctkDICOMServer> server, d->Servers)
      {
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
void ctkDICOMPoolManager::retrieveSeries(const QString &studyInstanceUID,
                                         const QString &seriesInstanceUID,
                                         QThread::Priority priority)
{
    Q_D(ctkDICOMPoolManager);

    foreach(QSharedPointer<ctkDICOMServer> server, d->Servers)
      {
      ctkDICOMRetrieveTask *task = new ctkDICOMRetrieveTask();
      task->setServer(*server);
      task->setRetrieveLevel(ctkDICOMRetrieveTask::DICOMLevel::Series);
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
void ctkDICOMPoolManager::retrieveSOPInstance(const QString &studyInstanceUID,
                                              const QString &seriesInstanceUID,
                                              const QString &SOPInstanceUID,
                                              QThread::Priority priority)
{
   Q_D(ctkDICOMPoolManager);

   foreach(QSharedPointer<ctkDICOMServer> server, d->Servers)
     {
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
QSharedPointer<ctkDICOMDatabase> ctkDICOMPoolManager::dicomDatabase()const
{
  Q_D(const ctkDICOMPoolManager);
  return d->DicomDatabase;
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::setDicomDatabase(ctkDICOMDatabase& dicomDatabase)
{
  Q_D(ctkDICOMPoolManager);
  d->DicomDatabase = QSharedPointer<ctkDICOMDatabase>(&dicomDatabase, skipDelete);
  d->Indexer->setDatabase(d->DicomDatabase.data());
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::setFilters(const QMap<QString, QVariant> &filters)
{
  Q_D(ctkDICOMPoolManager);
  d->Filters = filters;
}

//----------------------------------------------------------------------------
QMap<QString, QVariant> ctkDICOMPoolManager::filters() const
{
  Q_D(const ctkDICOMPoolManager);
  return d->Filters;
}

//----------------------------------------------------------------------------
int ctkDICOMPoolManager::getNumberOfServers()
{
  Q_D(ctkDICOMPoolManager);
  return d->Servers.size();
}

//----------------------------------------------------------------------------
ctkDICOMServer* ctkDICOMPoolManager::getNthServer(int id)
{
  Q_D(ctkDICOMPoolManager);
  if (id < 0 || id > d->Servers.size())
    {
    return nullptr;
    }
  return d->Servers[id].data();
}

//----------------------------------------------------------------------------
ctkDICOMServer* ctkDICOMPoolManager::getServer(const char *connectioName)
{
  return this->getNthServer(this->getServerIndexFromName(connectioName));
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::addServer(ctkDICOMServer* server)
{
  Q_D(ctkDICOMPoolManager);
  QSharedPointer<ctkDICOMServer> QsharedServer = QSharedPointer<ctkDICOMServer>(server);
  d->Servers.push_back(QsharedServer);
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::removeServer(const char *connectioName)
{
  this->removeNthServer(this->getServerIndexFromName(connectioName));
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::removeNthServer(int id)
{
  Q_D(ctkDICOMPoolManager);
  if (id < 0 || id > d->Servers.size())
    {
    return;
    }

  d->Servers.erase(d->Servers.begin() + id);
}

//----------------------------------------------------------------------------
QString ctkDICOMPoolManager::getServerNameFromIndex(int id)
{
  Q_D(ctkDICOMPoolManager);
  if (id < 0 || id > d->Servers.size())
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
int ctkDICOMPoolManager::getServerIndexFromName(const char *connectioName)
{
  Q_D(ctkDICOMPoolManager);
  if (!connectioName)
    {
    return -1;
    }
  for(int serverIndex = 0; serverIndex < d->Servers.size(); ++serverIndex)
    {
    QSharedPointer<ctkDICOMServer> server = d->Servers[serverIndex];
    if (server && server->connectionName() == connectioName)
      {
      // found
      return serverIndex;
      }
    }
  return -1;
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::waitForDone(int msecs)
{
  Q_D(ctkDICOMPoolManager);

  if(d->ThreadPool->activeThreadCount())
    {
    d->ThreadPool->waitForDone(msecs);
    }
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::waitForFinish(int msecs)
{
  Q_D(ctkDICOMPoolManager);

  int numberOfTasks = d->Tasks.count();
  while(numberOfTasks > 0)
    {
    QCoreApplication::processEvents();
    this->waitForDone(msecs);
    numberOfTasks = d->Tasks.count();
    }
}

//----------------------------------------------------------------------------
int ctkDICOMPoolManager::totalTasks()
{
  Q_D(ctkDICOMPoolManager);
  return d->Tasks.count();
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::stopAllTasksNotStarted()
{
  Q_D(ctkDICOMPoolManager);

  d->ThreadPool->clear();
  foreach (ctkAbstractTask* task, d->Tasks)
    {
    if (task->isRunning() || task->isFinished())
      {
      continue;
      }

    task->setStop(true);
    this->deleteTask(task->taskUID());
    }
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::deleteAllTasks()
{
  Q_D(ctkDICOMPoolManager);

  d->ThreadPool->clear();
  foreach (ctkAbstractTask* task, d->Tasks)
    {
    task->setStop(true);
    this->deleteTask(task->taskUID());
    }
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::deleteTask(QString taskUID)
{
  Q_D(ctkDICOMPoolManager);

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

  logger.debug("ctkDICOMPoolManager: deleting task object " + taskUID);

  QObject::disconnect(task, SIGNAL(started()), this, SLOT(taskStarted()));
  QObject::disconnect(task, SIGNAL(finished()), this, SLOT(taskFinished()));
  QObject::disconnect(task, SIGNAL(canceled()), this, SLOT(taskCanceled()));

  d->Tasks.remove(taskUID);
  task->deleteLater();
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::stopTasks(const QString &studyInstanceUID,
                                    const QString &seriesInstanceUID,
                                    const QString &sopInstanceUID)
{
  Q_D(ctkDICOMPoolManager);

  foreach (ctkAbstractTask* task, d->Tasks)
    {
    ctkDICOMRetrieveTask* retrieveTask = qobject_cast<ctkDICOMRetrieveTask*>(task);
    if (retrieveTask)
      {
      if (retrieveTask->isRunning() || retrieveTask->isFinished() ||
          retrieveTask->studyInstanceUID() != studyInstanceUID ||
          (!retrieveTask->seriesInstanceUID().isEmpty() && !seriesInstanceUID.isEmpty() &&
           retrieveTask->seriesInstanceUID() != seriesInstanceUID) ||
          (!retrieveTask->sopInstanceUID().isEmpty() && !sopInstanceUID.isEmpty() &&
           retrieveTask->sopInstanceUID() != sopInstanceUID))
        {
        continue;
        }

      retrieveTask->setStop(true);
      if (d->ThreadPool->tryTake(retrieveTask))
        {
        this->deleteTask(retrieveTask->taskUID());
        }
      }

    ctkDICOMQueryTask* queryTask = qobject_cast<ctkDICOMQueryTask*>(task);
    if (queryTask)
      {
      if (queryTask->isRunning() || queryTask->isFinished() ||
          queryTask->studyInstanceUID() != studyInstanceUID ||
          (!queryTask->seriesInstanceUID().isEmpty() && !seriesInstanceUID.isEmpty() &&
           queryTask->seriesInstanceUID() != seriesInstanceUID))
        {
        continue;
        }

      queryTask->setStop(true);
      if (d->ThreadPool->tryTake(queryTask))
        {
        this->deleteTask(queryTask->taskUID());
        }
      }
  }
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::raiseRetrieveFramesTasksPriorityForSeries(const QString &studyInstanceUID,
                                                                    const QString &seriesInstanceUID,
                                                                    QThread::Priority priority)
{
  Q_D(ctkDICOMPoolManager);

  foreach (ctkAbstractTask* task, d->Tasks)
    {
    ctkDICOMRetrieveTask* retrieveTask = qobject_cast<ctkDICOMRetrieveTask*>(task);
    if (!retrieveTask || retrieveTask->isRunning() || retrieveTask->isFinished())
      {
      continue;
      }

    // Raise priority for the tasks associated to the clicked thumbnail
    if (retrieveTask->retrieveLevel() == ctkDICOMRetrieveTask::DICOMLevel::Instances &&
        retrieveTask->studyInstanceUID() == studyInstanceUID &&
        retrieveTask->seriesInstanceUID() == seriesInstanceUID)
      {
      if (d->ThreadPool->tryTake(retrieveTask))
        {
        d->ThreadPool->start(task, priority);
        }
      }

    // Lower priority for all other retrieve tasks
    if (d->ThreadPool->tryTake(retrieveTask))
      {
      d->ThreadPool->start(task, QThread::LowPriority);
      }
  }
}

//----------------------------------------------------------------------------
int ctkDICOMPoolManager::maximumThreadCount() const
{
  Q_D(const ctkDICOMPoolManager);
  return d->ThreadPool->maxThreadCount();
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::setMaximumThreadCount(const int &maximumThreadCount)
{
  Q_D(ctkDICOMPoolManager);

  d->ThreadPool->setMaxThreadCount(maximumThreadCount);
}

//----------------------------------------------------------------------------
int ctkDICOMPoolManager::maximumNumberOfRetry() const
{
  Q_D(const ctkDICOMPoolManager);
  return d->MaximumNumberOfRetry;
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::setMaximumNumberOfRetry(const int &maximumNumberOfRetry)
{
  Q_D(ctkDICOMPoolManager);
  d->MaximumNumberOfRetry = maximumNumberOfRetry;
}

//----------------------------------------------------------------------------
int ctkDICOMPoolManager::retryDelay() const
{
  Q_D(const ctkDICOMPoolManager);
  return d->RetryDelay;
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::setRetryDelay(const int &retryDelay)
{
  Q_D(ctkDICOMPoolManager);
  d->RetryDelay = retryDelay;
}

//----------------------------------------------------------------------------
QSharedPointer<ctkDICOMIndexer> ctkDICOMPoolManager::Indexer() const
{
  Q_D(const ctkDICOMPoolManager);
  return d->Indexer;
}

//----------------------------------------------------------------------------
QSharedPointer<QThreadPool> ctkDICOMPoolManager::threadPool() const
{
  Q_D(const ctkDICOMPoolManager);
  return d->ThreadPool;
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::taskStarted()
{
  ctkDICOMQueryTask* queryTask = qobject_cast<ctkDICOMQueryTask*>(this->sender());
  if (queryTask)
    {
    switch (queryTask->queryLevel())
      {
      case ctkDICOMQueryTask::DICOMLevel::Studies:
        logger.debug("ctkDICOMPoolManager: query task at studies level started. "
                     "TaskUID: " + queryTask->taskUID() +
                     " Server: " + queryTask->server()->connectionName());
        break;
      case ctkDICOMQueryTask::DICOMLevel::Series:
        logger.debug("ctkDICOMPoolManager: query task at series level started. "
                     "TaskUID: " + queryTask->taskUID() +
                     " Server: " + queryTask->server()->connectionName() +
                     " StudyInstanceUID: " + queryTask->studyInstanceUID());
        break;
      case ctkDICOMQueryTask::DICOMLevel::Instances:
        logger.debug("ctkDICOMPoolManager: query task at instances level started. "
                     "TaskUID: " + queryTask->taskUID() +
                     " Server: " + queryTask->server()->connectionName() +
                     " StudyInstanceUID: " + queryTask->studyInstanceUID() +
                     " SeriesInstanceUID: " + queryTask->seriesInstanceUID());
        break;
      }
    }

  ctkDICOMRetrieveTask* retrieveTask = qobject_cast<ctkDICOMRetrieveTask*>(this->sender());
  if (retrieveTask)
    {
    switch (retrieveTask->retrieveLevel())
      {
      case ctkDICOMRetrieveTask::DICOMLevel::Studies:
        logger.debug("ctkDICOMPoolManager: retrieve task at studies level started. "
                     "TaskUID: " + retrieveTask->taskUID() +
                     " Server: " + retrieveTask->server()->connectionName() +
                     " StudyInstanceUID: " + retrieveTask->studyInstanceUID());
        break;
      case ctkDICOMRetrieveTask::DICOMLevel::Series:
        logger.debug("ctkDICOMPoolManager: retrieve task at series level started. "
                     "TaskUID: " + retrieveTask->taskUID() +
                     " Server: " + retrieveTask->server()->connectionName() +
                     " StudyInstanceUID: " + retrieveTask->studyInstanceUID() +
                     " SeriesInstanceUID: " + retrieveTask->seriesInstanceUID());
        break;
      case ctkDICOMRetrieveTask::DICOMLevel::Instances:
        logger.debug("ctkDICOMPoolManager: retrieve task at instances level started. "
                     "TaskUID: " + retrieveTask->taskUID() +
                     " Server: " + retrieveTask->server()->connectionName() +
                     " StudyInstanceUID: " + retrieveTask->studyInstanceUID() +
                     " SeriesInstanceUID: " + retrieveTask->seriesInstanceUID() +
                     " SOPInstanceUID: " + retrieveTask->sopInstanceUID());
        break;
      }
    }
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::taskFinished()
{
  Q_D(ctkDICOMPoolManager);

  ctkDICOMQueryTask* queryTask = qobject_cast<ctkDICOMQueryTask*>(this->sender());
  if (queryTask)
    {
    switch (queryTask->queryLevel())
      {
      case ctkDICOMQueryTask::DICOMLevel::Studies:
        logger.debug("ctkDICOMPoolManager: query task at studies level finished. "
                     "TaskUID: " + queryTask->taskUID() +
                     " Server: " + queryTask->server()->connectionName());
        break;
      case ctkDICOMQueryTask::DICOMLevel::Series:
        logger.debug("ctkDICOMPoolManager: query task at series level finished. "
                     "TaskUID: " + queryTask->taskUID() +
                     " Server: " + queryTask->server()->connectionName() +
                     " StudyInstanceUID: " + queryTask->studyInstanceUID());
        break;
      case ctkDICOMQueryTask::DICOMLevel::Instances:
        logger.debug("ctkDICOMPoolManager: query task at instances level finished. "
                     "TaskUID: " + queryTask->taskUID() +
                     " Server: " + queryTask->server()->connectionName() +
                     " StudyInstanceUID: " + queryTask->studyInstanceUID() +
                     " SeriesInstanceUID: " + queryTask->seriesInstanceUID());
        break;
      }

    if (queryTask->taskResults().count() > 0)
      {
      foreach (QSharedPointer<ctkDICOMTaskResults> taskResults, queryTask->taskResults())
        {
        QSharedPointer<ctkDICOMTaskResults> copy =
          QSharedPointer<ctkDICOMTaskResults> (new ctkDICOMTaskResults);
        copy->deepCopy(taskResults.data());
        d->TaskResults.append(copy);
        d->Indexer->insertTaskResults(copy);
        }
      }
    else
      {
      emit this->progressTaskDetail(nullptr);
      }

    this->deleteTask(queryTask->taskUID());
    }

  ctkDICOMRetrieveTask* retrieveTask = qobject_cast<ctkDICOMRetrieveTask*>(this->sender());
  if (retrieveTask)
    {
    ctkDICOMRetrieveTask* retrieveTask = qobject_cast<ctkDICOMRetrieveTask*>(this->sender());
    if (retrieveTask)
      {
      switch (retrieveTask->retrieveLevel())
        {
        case ctkDICOMRetrieveTask::DICOMLevel::Studies:
          logger.debug("ctkDICOMPoolManager: retrieve task at studies level finished. "
                       "TaskUID: " + retrieveTask->taskUID() +
                       " Server: " + retrieveTask->server()->connectionName() +
                       " StudyInstanceUID: " + retrieveTask->studyInstanceUID());
          break;
        case ctkDICOMRetrieveTask::DICOMLevel::Series:
          logger.debug("ctkDICOMPoolManager: retrieve task at series level finished. "
                       "TaskUID: " + retrieveTask->taskUID() +
                       " Server: " + retrieveTask->server()->connectionName() +
                       " StudyInstanceUID: " + retrieveTask->studyInstanceUID() +
                       " SeriesInstanceUID: " + retrieveTask->seriesInstanceUID());
          break;
        case ctkDICOMRetrieveTask::DICOMLevel::Instances:
          logger.debug("ctkDICOMPoolManager: retrieve task at instances level finished. "
                       "TaskUID: " + retrieveTask->taskUID() +
                       " Server: " + retrieveTask->server()->connectionName() +
                       " StudyInstanceUID: " + retrieveTask->studyInstanceUID() +
                       " SeriesInstanceUID: " + retrieveTask->seriesInstanceUID() +
                       " SOPInstanceUID: " + retrieveTask->sopInstanceUID());
          break;
        }
      }

    if (retrieveTask->taskResults().count() > 0)
      {
      foreach (QSharedPointer<ctkDICOMTaskResults> taskResults, retrieveTask->taskResults())
        {
        QSharedPointer<ctkDICOMTaskResults> copy =
          QSharedPointer<ctkDICOMTaskResults> (new ctkDICOMTaskResults);
        copy->deepCopy(taskResults.data());
        d->TaskResults.append(copy);
        d->Indexer->insertTaskResults(copy);
        }
      }
    else
      {
      emit this->progressTaskDetail(nullptr);
      }

    this->deleteTask(retrieveTask->taskUID());
    }
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::taskCanceled()
{
  Q_D(ctkDICOMPoolManager);

  ctkAbstractTask* task = qobject_cast<ctkAbstractTask*>(this->sender());
  if (!task)
    {
    return;
    }

  ctkDICOMQueryTask* queryTask = qobject_cast<ctkDICOMQueryTask*>(task);
  if (queryTask)
    {
    switch (queryTask->queryLevel())
      {
      case ctkDICOMQueryTask::DICOMLevel::Studies:
        logger.debug("ctkDICOMPoolManager: query task at studies level canceled. "
                     "TaskUID: " + queryTask->taskUID() +
                     " Server: " + queryTask->server()->connectionName());
        break;
      case ctkDICOMQueryTask::DICOMLevel::Series:
        logger.debug("ctkDICOMPoolManager: query task at series level canceled. "
                     "TaskUID: " + queryTask->taskUID() +
                     " Server: " + queryTask->server()->connectionName() +
                     " StudyInstanceUID: " + queryTask->studyInstanceUID());
        break;
      case ctkDICOMQueryTask::DICOMLevel::Instances:
        logger.debug("ctkDICOMPoolManager: query task at instances level canceled. "
                     "TaskUID: " + queryTask->taskUID() +
                     " Server: " + queryTask->server()->connectionName() +
                     " StudyInstanceUID: " + queryTask->studyInstanceUID() +
                     " SeriesInstanceUID: " + queryTask->seriesInstanceUID());
        break;
      }

    QString taskUID = queryTask->taskUID();
    if (queryTask->numberOfRetry() < d->MaximumNumberOfRetry)
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
    else
      {
      switch (queryTask->queryLevel())
        {
        case ctkDICOMQueryTask::DICOMLevel::Studies:
          logger.warn("ctkDICOMPoolManager: query task at studies level failed. "
                      "Server: " + queryTask->server()->connectionName());
          break;
        case ctkDICOMQueryTask::DICOMLevel::Series:
          logger.warn("ctkDICOMPoolManager: query task at series level failed. "
                      "Server: " + queryTask->server()->connectionName() +
                      " StudyInstanceUID: " + queryTask->studyInstanceUID());
          break;
        case ctkDICOMQueryTask::DICOMLevel::Instances:
          logger.warn("ctkDICOMPoolManager: query task at instances level failed. "
                      "Server: " + queryTask->server()->connectionName() +
                      " StudyInstanceUID: " + queryTask->studyInstanceUID() +
                      " SeriesInstanceUID: " + queryTask->seriesInstanceUID());
          break;
        }
      }

    this->deleteTask(taskUID);
    }

  ctkDICOMRetrieveTask* retrieveTask = qobject_cast<ctkDICOMRetrieveTask*>(task);
  if (retrieveTask)
    {
    switch (retrieveTask->retrieveLevel())
      {
      case ctkDICOMRetrieveTask::DICOMLevel::Studies:
        logger.debug("ctkDICOMPoolManager: retrieve task at studies level canceled. "
                     "TaskUID: " + retrieveTask->taskUID() +
                     " Server: " + retrieveTask->server()->connectionName() +
                     " StudyInstanceUID: " + retrieveTask->studyInstanceUID());
        break;
      case ctkDICOMRetrieveTask::DICOMLevel::Series:
        logger.debug("ctkDICOMPoolManager: retrieve task at series level canceled. "
                     "TaskUID: " + retrieveTask->taskUID() +
                     " Server: " + retrieveTask->server()->connectionName() +
                     " StudyInstanceUID: " + retrieveTask->studyInstanceUID() +
                     " SeriesInstanceUID: " + retrieveTask->seriesInstanceUID());
        break;
      case ctkDICOMRetrieveTask::DICOMLevel::Instances:
        logger.debug("ctkDICOMPoolManager: retrieve task at instances level canceled. "
                     "TaskUID: " + retrieveTask->taskUID() +
                     " Server: " + retrieveTask->server()->connectionName() +
                     " StudyInstanceUID: " + retrieveTask->studyInstanceUID() +
                     " SeriesInstanceUID: " + retrieveTask->seriesInstanceUID() +
                     " SOPInstanceUID: " + retrieveTask->sopInstanceUID());
        break;
      }

    QString taskUID = retrieveTask->taskUID();
    if (retrieveTask->numberOfRetry() < d->MaximumNumberOfRetry)
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
    else
      {
      switch (retrieveTask->retrieveLevel())
        {
        case ctkDICOMRetrieveTask::DICOMLevel::Studies:
          logger.warn("ctkDICOMPoolManager: retrieve task at studies level failed. "
                      "Server: " + retrieveTask->server()->connectionName() +
                      " StudyInstanceUID: " + retrieveTask->studyInstanceUID());
          break;
        case ctkDICOMRetrieveTask::DICOMLevel::Series:
          logger.warn("ctkDICOMPoolManager: retrieve task at series level failed. "
                      "Server: " + retrieveTask->server()->connectionName() +
                      " StudyInstanceUID: " + retrieveTask->studyInstanceUID() +
                      " SeriesInstanceUID: " + retrieveTask->seriesInstanceUID());
          break;
        case ctkDICOMRetrieveTask::DICOMLevel::Instances:
          logger.warn("ctkDICOMPoolManager: retrieve task at instances level failed. "
                      "Server: " + retrieveTask->server()->connectionName() +
                      " StudyInstanceUID: " + retrieveTask->studyInstanceUID() +
                      " SeriesInstanceUID: " + retrieveTask->seriesInstanceUID() +
                      " SOPInstanceUID: " + retrieveTask->sopInstanceUID());
          break;
        }
      }

    this->deleteTask(taskUID);
    }
}

//----------------------------------------------------------------------------
void ctkDICOMPoolManager::taskRetry(ctkAbstractTask *task)
{
  Q_D(ctkDICOMPoolManager);

  logger.debug("ctkDICOMPoolManager: retry task."
               "TaskUID: " + task->taskUID());

  QObject::connect(task, SIGNAL(started()), this, SLOT(taskStarted()), Qt::AutoConnection);
  QObject::connect(task, SIGNAL(finished()), this, SLOT(taskFinished()), Qt::AutoConnection);
  QObject::connect(task, SIGNAL(canceled()), this, SLOT(taskCanceled()), Qt::AutoConnection);

  QString newTaskUID = d->generateUniqueTaskUID();
  task->setTaskUID(newTaskUID);
  d->Tasks.insert(newTaskUID, task);

  d->ThreadPool->start(task, QThread::LowPriority);
}

