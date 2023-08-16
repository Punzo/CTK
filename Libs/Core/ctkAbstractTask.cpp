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

#include "ctkAbstractTask.h"

// --------------------------------------------------------------------------
ctkAbstractTask::ctkAbstractTask()
{ 
  this->Stop = false;
  this->Running = false;
  this->Finished = false;
  this->TaskUID = "";
  this->NumberOfRetry = 0;
}

//----------------------------------------------------------------------------
ctkAbstractTask::~ctkAbstractTask()
{
}

//----------------------------------------------------------------------------
void ctkAbstractTask::setTaskUID(const QString &taskUID)
{
  this->TaskUID = taskUID;
}

//----------------------------------------------------------------------------
QString ctkAbstractTask::taskUID() const
{
  return this->TaskUID;
}

//----------------------------------------------------------------------------
bool ctkAbstractTask::isStopped() const
{
  return this->Stop;
}

//----------------------------------------------------------------------------
void ctkAbstractTask::setStop(const bool& stop)
{
  this->Stop = stop;
}

//----------------------------------------------------------------------------
bool ctkAbstractTask::isFinished() const
{
  return this->Finished;
}

//----------------------------------------------------------------------------
void ctkAbstractTask::setIsFinished(const bool& finished)
{
  if (finished && this->Running)
    {
    this->Running = false;
    }

  this->Finished = finished;
}

//----------------------------------------------------------------------------
bool ctkAbstractTask::isRunning() const
{
  return this->Running;
}

//----------------------------------------------------------------------------
void ctkAbstractTask::setIsRunning(const bool& running)
{
  this->Running = running;
}

//----------------------------------------------------------------------------
int ctkAbstractTask::numberOfRetry() const
{
  return this->NumberOfRetry;
}

//----------------------------------------------------------------------------
void ctkAbstractTask::setNumberOfRetry(const int& numberOfRetry)
{
  this->NumberOfRetry = numberOfRetry;
}

