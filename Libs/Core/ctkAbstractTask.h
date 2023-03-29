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

#ifndef __ctkAbstractTask_h
#define __ctkAbstractTask_h

// Qt includes
#include <QObject>
#include <QRunnable>

// CTK includes
#include "ctkCoreExport.h"

//------------------------------------------------------------------------------
/// \ingroup Core
class CTK_CORE_EXPORT ctkAbstractTask : public QObject, public QRunnable
{
  Q_OBJECT
  Q_PROPERTY(QString taskUID READ taskUID WRITE setTaskUID);
  Q_PROPERTY(bool stop READ isStopped WRITE setStop);
  Q_PROPERTY(bool running READ isRunning WRITE setIsRunning);
  Q_PROPERTY(bool finished READ isFinished WRITE setIsFinished);
  Q_PROPERTY(bool numberOfRetry READ numberOfRetry WRITE setNumberOfRetry);

public:
  explicit ctkAbstractTask();
  virtual ~ctkAbstractTask();

  /// Execute task
  virtual void run() = 0;

  /// Task UID
  QString taskUID() const;
  virtual void setTaskUID(const QString& taskUID);

  /// Stop task
  bool isStopped() const;
  virtual void setStop(const bool& stop);

  /// Finished
  bool isFinished() const;
  void setIsFinished(const bool& finished);

  /// Running
  bool isRunning() const;
  void setIsRunning(const bool& running);

  /// Number of retries: current counter of how many times
  /// the task has been relunched on fails
  int numberOfRetry() const;
  void setNumberOfRetry(const int& numberOfRetry);

Q_SIGNALS:
  void started();
  void finished();
  void canceled();

private:
  Q_DISABLE_COPY(ctkAbstractTask)

  QString TaskUID;
  bool Stop;
  bool Running;
  bool Finished;
  int NumberOfRetry;
};


#endif // ctkAbstractTask_h
