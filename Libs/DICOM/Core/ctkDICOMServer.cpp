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

#include <stdexcept>

// ctkDICOMCore includes
#include "ctkDICOMServer.h"
#include "ctkLogger.h"


static ctkLogger logger("org.commontk.dicom.DICOMServer");


//------------------------------------------------------------------------------
class ctkDICOMServerPrivate: public QObject
{
  Q_DECLARE_PUBLIC(ctkDICOMServer);

protected:
  ctkDICOMServer* const q_ptr;

public:
  ctkDICOMServerPrivate(ctkDICOMServer& obj);
  ~ctkDICOMServerPrivate();

  QString  ConnectionName;
  QString  CallingAETitle;
  QString  CalledAETitle;
  QString  Host;
  int      Port;
  bool     PreferCGET;
  bool     KeepAssociationOpen;
  QString  MoveDestinationAETitle;
};

//------------------------------------------------------------------------------
// ctkDICOMServerPrivate methods

//------------------------------------------------------------------------------
ctkDICOMServerPrivate::ctkDICOMServerPrivate(ctkDICOMServer& obj)
  : q_ptr(&obj)
{
  this->KeepAssociationOpen = false;
}

//------------------------------------------------------------------------------
ctkDICOMServerPrivate::~ctkDICOMServerPrivate()
{
}

//------------------------------------------------------------------------------
// ctkDICOMServer methods

//------------------------------------------------------------------------------
ctkDICOMServer::ctkDICOMServer(QObject* parent)
  : QObject(parent),
    d_ptr(new ctkDICOMServerPrivate(*this))
{
}

//------------------------------------------------------------------------------
ctkDICOMServer::~ctkDICOMServer()
{
}

//------------------------------------------------------------------------------
void ctkDICOMServer::setConnectionName( const QString& connectionName )
{
  Q_D(ctkDICOMServer);
  d->ConnectionName = connectionName;
}

//------------------------------------------------------------------------------
QString ctkDICOMServer::connectionName() const
{
  Q_D(const ctkDICOMServer);
  return d->ConnectionName;
}

//------------------------------------------------------------------------------
void ctkDICOMServer::setCallingAETitle( const QString& callingAETitle )
{
  Q_D(ctkDICOMServer);
  d->CallingAETitle = callingAETitle;
}

//------------------------------------------------------------------------------
QString ctkDICOMServer::callingAETitle() const
{
  Q_D(const ctkDICOMServer);
  return d->CallingAETitle;
}

//------------------------------------------------------------------------------
void ctkDICOMServer::setCalledAETitle( const QString& calledAETitle )
{
  Q_D(ctkDICOMServer);
  d->CalledAETitle = calledAETitle;
}

//------------------------------------------------------------------------------
QString ctkDICOMServer::calledAETitle()const
{
  Q_D(const ctkDICOMServer);
  return d->CalledAETitle;
}

//------------------------------------------------------------------------------
void ctkDICOMServer::setHost( const QString& host )
{
  Q_D(ctkDICOMServer);
  d->Host = host;
}

//------------------------------------------------------------------------------
QString ctkDICOMServer::host() const
{
  Q_D(const ctkDICOMServer);
  return d->Host;
}

//------------------------------------------------------------------------------
void ctkDICOMServer::setPort ( int port )
{
  Q_D(ctkDICOMServer);
  d->Port = port;
}

//------------------------------------------------------------------------------
int ctkDICOMServer::port()const
{
  Q_D(const ctkDICOMServer);
  return d->Port;
}

//------------------------------------------------------------------------------
void ctkDICOMServer::setPreferCGET ( bool preferCGET )
{
  Q_D(ctkDICOMServer);
  d->PreferCGET = preferCGET;
}

//------------------------------------------------------------------------------
bool ctkDICOMServer::preferCGET()const
{
  Q_D(const ctkDICOMServer);
  return d->PreferCGET;
}

//------------------------------------------------------------------------------
void ctkDICOMServer::setMoveDestinationAETitle(const QString& moveDestinationAETitle)
{
  Q_D(ctkDICOMServer);
  if (moveDestinationAETitle != d->MoveDestinationAETitle)
  {
    d->MoveDestinationAETitle = moveDestinationAETitle;
  }
}
//------------------------------------------------------------------------------
QString ctkDICOMServer::moveDestinationAETitle()const
{
  Q_D(const ctkDICOMServer);
  return d->MoveDestinationAETitle;
}

//------------------------------------------------------------------------------
void ctkDICOMServer::setKeepAssociationOpen(const bool keepOpen)
{
  Q_D(ctkDICOMServer);
  d->KeepAssociationOpen = keepOpen;
}

//------------------------------------------------------------------------------
bool ctkDICOMServer::keepAssociationOpen()
{
  Q_D(const ctkDICOMServer);
  return d->KeepAssociationOpen;
}
