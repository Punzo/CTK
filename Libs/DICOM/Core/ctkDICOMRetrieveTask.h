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

#ifndef __ctkDICOMRetrieveTask_h
#define __ctkDICOMRetrieveTask_h

// Qt includes 
#include <QObject>
#include <QSharedPointer>

// ctkDICOMCore includes
#include "ctkDICOMCoreExport.h"

#include <ctkAbstractTask.h>

class ctkDICOMRetrieve;
class ctkDICOMRetrieveTaskPrivate;
class ctkDICOMServer;
class ctkDICOMTaskResults;

/// \ingroup DICOM_Core
class CTK_DICOM_CORE_EXPORT ctkDICOMRetrieveTask : public ctkAbstractTask
{
  Q_OBJECT
  Q_ENUMS(DICOMLevel)
  Q_PROPERTY(QString studyInstanceUID READ studyInstanceUID WRITE setStudyInstanceUID);
  Q_PROPERTY(QString seriesInstanceUID READ seriesInstanceUID WRITE setSeriesInstanceUID);
  Q_PROPERTY(QString sopInstanceUID READ sopInstanceUID WRITE setSOPInstanceUID);
  Q_PROPERTY(DICOMLevel retrieveLevel READ retrieveLevel WRITE setRetrieveLevel);

public:
  typedef ctkAbstractTask Superclass;
  explicit ctkDICOMRetrieveTask();
  virtual ~ctkDICOMRetrieveTask();
  
  enum DICOMLevel{
    Studies,
    Series,
    Instances
  };

  /// Retrieve Level
  void setRetrieveLevel(const DICOMLevel& retrieveLevel);
  DICOMLevel retrieveLevel() const;

  /// Task UID
  void setTaskUID(const QString& taskUID);

  /// Stop task
  void setStop(const bool& stop);

  /// Execute task
  void run();

  /// Access the list of datasets from the last query.
  QList<QSharedPointer<ctkDICOMTaskResults>> taskResults()const;

  /// Server
  Q_INVOKABLE ctkDICOMServer* server()const;
  QSharedPointer<ctkDICOMServer> serverShared()const;
  Q_INVOKABLE void setServer(ctkDICOMServer& server);

  /// Study instance UID
  void setStudyInstanceUID(const QString& studyInstanceUID);
  QString studyInstanceUID() const;

  /// Series instance UID
  void setSeriesInstanceUID(const QString& seriesInstanceUID);
  QString seriesInstanceUID() const;

  /// SOP instance UID
  void setSOPInstanceUID(const QString& sopInstanceUID);
  QString sopInstanceUID() const;

  /// Retriever
  Q_INVOKABLE ctkDICOMRetrieve* retriever()const;

protected:
  QScopedPointer<ctkDICOMRetrieveTaskPrivate> d_ptr;

  /// Constructor allowing derived class to specify a specialized pimpl.
  ///
  /// \note You are responsible to call init() in the constructor of
  /// derived class. Doing so ensures the derived class is fully
  /// instantiated in case virtual method are called within init() itself.
  ctkDICOMRetrieveTask(ctkDICOMRetrieveTaskPrivate* pimpl);

private:
  Q_DECLARE_PRIVATE(ctkDICOMRetrieveTask);
  Q_DISABLE_COPY(ctkDICOMRetrieveTask);
};

#endif
