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

#ifndef __ctkDICOMServer_h
#define __ctkDICOMServer_h

// Qt includes
#include <QObject>

#include "ctkDICOMCoreExport.h"

class ctkDICOMServerPrivate;

/// \ingroup DICOM_Core
class CTK_DICOM_CORE_EXPORT ctkDICOMServer : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString connectionName READ connectionName WRITE setConnectionName);
  Q_PROPERTY(QString callingAETitle READ callingAETitle WRITE setCallingAETitle);
  Q_PROPERTY(QString calledAETitle READ calledAETitle WRITE setCalledAETitle);
  Q_PROPERTY(QString host READ host WRITE setHost);
  Q_PROPERTY(int port READ port WRITE setPort);
  Q_PROPERTY(bool preferCGET READ preferCGET WRITE setPreferCGET);
  Q_PROPERTY(QString moveDestinationAETitle READ moveDestinationAETitle WRITE setMoveDestinationAETitle);
  Q_PROPERTY(bool keepAssociationOpen READ keepAssociationOpen WRITE setKeepAssociationOpen);

public:
  explicit ctkDICOMServer(QObject* parent = 0);
  virtual ~ctkDICOMServer();

  /// Set methods for connectivity
  void setConnectionName(const QString& connectionName);
  QString connectionName() const;
  /// CTK_AE - the AE string by which the peer host might 
  /// recognize your request
  void setCallingAETitle(const QString& callingAETitle);
  QString callingAETitle() const;
  /// CTK_AE - the AE of the service of peer host that you are calling
  /// which tells the host what you are requesting
  void setCalledAETitle(const QString& calledAETitle);
  QString calledAETitle() const;
  /// peer hostname being connected to
  void setHost(const QString& host);
  QString host() const;
  /// [0, 65365] port on peer host - e.g. 11112
  void setPort(int port);
  int port() const;
  /// Prefer CGET over CMOVE for retrieval of query results.
  /// false by default
  void setPreferCGET(bool preferCGET);
  bool preferCGET()const;
  /// Typically CTK_STORE or similar - needs to be something that the
  /// peer host knows about and is able to move data into
  /// Only used when calling moveSeries or moveStudy
  void setMoveDestinationAETitle(const QString& moveDestinationAETitle);
  QString moveDestinationAETitle() const;
  /// prefer to keep using the existing association to peer host when doing
  /// multiple requests (default true)
  void setKeepAssociationOpen(const bool keepOpen);
  bool keepAssociationOpen();

protected:
  QScopedPointer<ctkDICOMServerPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(ctkDICOMServer);
  Q_DISABLE_COPY(ctkDICOMServer);
};


#endif
