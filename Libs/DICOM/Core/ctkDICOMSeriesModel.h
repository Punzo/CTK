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

#ifndef __ctkDICOMSeriesModel_h
#define __ctkDICOMSeriesModel_h

// Qt includes
#include <QAbstractTableModel>
#include <QSharedPointer>
#include <QStringList>
#include <QPixmap>

// CTK includes
#include "ctkDICOMCoreExport.h"

class ctkDICOMDatabase;
class ctkDICOMScheduler;
class ctkDICOMSeriesModelPrivate;

/// \ingroup DICOM_Core
/// 
/// \brief Model for displaying DICOM series within a study.
/// 
/// This model manages series data for a specific study, including:
/// - Series metadata (description, modality, number, instance count)
/// - Thumbnail generation and caching
/// - Cloud status and download progress
/// - Selection and operation states
/// 
/// The model supports lazy loading and asynchronous thumbnail generation
/// for optimal performance with large datasets.
/// 
/// Usage:
/// \code
/// ctkDICOMSeriesModel* model = new ctkDICOMSeriesModel();
/// model->setDicomDatabase(database);
/// model->setStudyFilter("1.2.3.4.5.6"); // Study Instance UID
/// model->setGridColumns(5); // 5 columns in the table
/// 
/// QTableView* view = new QTableView();
/// view->setModel(model);
/// \endcode
class CTK_DICOM_CORE_EXPORT ctkDICOMSeriesModel : public QAbstractTableModel
{
  Q_OBJECT
  Q_PROPERTY(QString studyFilter READ studyFilter WRITE setStudyFilter NOTIFY studyFilterChanged)
  Q_PROPERTY(QStringList modalityFilter READ modalityFilter WRITE setModalityFilter NOTIFY modalityFilterChanged)
  Q_PROPERTY(QString descriptionFilter READ descriptionFilter WRITE setDescriptionFilter NOTIFY descriptionFilterChanged)
  Q_PROPERTY(int thumbnailSize READ thumbnailSize WRITE setThumbnailSize NOTIFY thumbnailSizeChanged)
  Q_PROPERTY(int gridColumns READ gridColumns WRITE setGridColumns NOTIFY gridColumnsChanged)

public:
  typedef QAbstractTableModel Superclass;

  /// Custom data roles for series information
  enum DataRoles {
    // Basic series information
    SeriesInstanceUIDRole = Qt::UserRole + 1,
    SeriesItemRole,                 ///< Database series item ID
    SeriesNumberRole,               ///< Series number
    ModalityRole,                   ///< Modality (CT, MR, etc.)
    SeriesDescriptionRole,          ///< Series description

    // Instance information
    InstanceCountRole,              ///< Number of instances in series
    InstancesLoadedRole,            ///< Number of instances downloaded locally
    RowsRole,                       ///< DICOM Rows (image height)
    ColumnsRole,                    ///< DICOM Columns (image width)

    // Visual data
    ThumbnailRole,                  ///< Series thumbnail as QPixmap
    ThumbnailPathRole,              ///< Path to cached thumbnail file
    ThumbnailSizeRole,              ///< Thumbnail size as QSize

    // Status information
    IsCloudRole,                    ///< Whether series is stored in cloud
    IsLoadedRole,                   ///< Whether all instances are local
    IsVisibleRole,                  ///< Whether series is marked as visible
    RetrieveFailedRole,             ///< Whether last retrieve operation failed
    StatusRole,                     ///< Current operation status (OperationStatus enum)

    // Operation status
    OperationProgressRole,          ///< Progress of current operation (0-100)
    OperationStatusRole,            ///< Status text for current operation

    // Selection
    IsSelectedRole,                 ///< Whether series is selected

    // Job tracking
    JobUIDRole,                     ///< UID of associated job

    // Internal data
    PatientIDRole,                  ///< Patient ID
    StudyInstanceUIDRole            ///< Study Instance UID
  };
  Q_ENUM(DataRoles)

  /// Operation status for series
  enum OperationStatus {
    NoOperation,
    Querying,
    Retrieving,
    GeneratingThumbnail,
    LoadingThumbnail,
    Failed,
    Completed
  };
  Q_ENUM(OperationStatus)

  explicit ctkDICOMSeriesModel(QObject* parent = nullptr);
  virtual ~ctkDICOMSeriesModel();

  ///@{
  /// DICOM Database
  void setDicomDatabase(ctkDICOMDatabase* database);
  void setDicomDatabase(QSharedPointer<ctkDICOMDatabase> database);
  ctkDICOMDatabase* dicomDatabase() const;
  QSharedPointer<ctkDICOMDatabase> dicomDatabaseShared() const;
  ///@}

  ///@{
  /// DICOM Scheduler for background operations
  void setScheduler(ctkDICOMScheduler* scheduler);
  void setScheduler(QSharedPointer<ctkDICOMScheduler> scheduler);
  ctkDICOMScheduler* scheduler() const;
  QSharedPointer<ctkDICOMScheduler> schedulerShared() const;
  ///@}

  ///@{
  /// Study filter - only series from this study will be shown
  void setStudyFilter(const QString& studyInstanceUID);
  QString studyFilter() const;
  ///@}

  ///@{
  /// Modality filter - only series with these modalities will be shown
  void setModalityFilter(const QStringList& modalities);
  QStringList modalityFilter() const;
  ///@}

  ///@{
  /// Description filter - only series containing this text will be shown
  void setDescriptionFilter(const QString& description);
  QString descriptionFilter() const;
  ///@}

  ///@{
  /// Thumbnail size in pixels
  void setThumbnailSize(int size);
  int thumbnailSize() const;
  ///@}

  ///@{
  /// Allowed servers for query/retrieve operations
  void setAllowedServers(const QStringList& servers);
  QStringList allowedServers() const;
  ///@}

  /// Get series instance UID for model index
  Q_INVOKABLE QString seriesInstanceUID(const QModelIndex& index) const;

  /// Get series item (database ID) for model index
  Q_INVOKABLE QString seriesItem(const QModelIndex& index) const;

  ///@{
  /// Grid layout configuration for table display
  void setGridColumns(int columns);
  int gridColumns() const;
  ///@}

  /// Find model index for series instance UID
  Q_INVOKABLE QModelIndex indexForSeriesInstanceUID(const QString& seriesInstanceUID) const;

  /// Refresh the model data
  Q_INVOKABLE void refresh();

  /// Generate thumbnails for all visible series
  Q_INVOKABLE void generateThumbnails();

  /// Generate thumbnail for specific series
  Q_INVOKABLE void generateThumbnail(const QModelIndex& index);

  /// Start query/retrieve for series instances
  Q_INVOKABLE void generateInstances(const QModelIndex& index, bool query = true, bool retrieve = true);

  // QAbstractTableModel interface
  virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
  virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  virtual QHash<int, QByteArray> roleNames() const override;

public slots:
  /// Update GUI from scheduler progress
  void onJobStarted(const QVariant& data);
  void onJobFinished(const QVariant& data);
  void onJobFailed(const QVariant& data);
  void onJobUserStopped(const QVariant& data);

  /// Thumbnail generation completed
  void onThumbnailGenerated(const QString& seriesInstanceUID, const QPixmap& thumbnail);

signals:
  /// Emitted when study filter changes
  void studyFilterChanged(const QString& studyInstanceUID);

  /// Emitted when modality filter changes
  void modalityFilterChanged(const QStringList& modalities);

  /// Emitted when description filter changes
  void descriptionFilterChanged(const QString& description);

  /// Emitted when thumbnail size changes
  void thumbnailSizeChanged(int size);

  /// Emitted when grid columns change
  void gridColumnsChanged(int columns);

  /// Emitted when series selection changes
  void seriesSelectionChanged(const QStringList& selectedSeriesInstanceUIDs);

  /// Emitted when thumbnail is ready
  void thumbnailReady(const QModelIndex& index);

  /// Emitted when operation progress changes
  void operationProgressChanged(const QModelIndex& index, int progress);

  /// Emitted when all data is loaded and ready
  void modelReady();

protected:
  QScopedPointer<ctkDICOMSeriesModelPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(ctkDICOMSeriesModel);
  Q_DISABLE_COPY(ctkDICOMSeriesModel);
};

#endif