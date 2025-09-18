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

#ifndef __ctkDICOMSeriesTableView_h
#define __ctkDICOMSeriesTableView_h

// Qt includes
#include <QListView>
#include <QSize>

// CTK includes
#include "ctkDICOMWidgetsExport.h"

class ctkDICOMSeriesTableViewPrivate;
class ctkDICOMSeriesModel;
class ctkDICOMSeriesDelegate;

/// \ingroup DICOM_Widgets
/// \brief Custom table view for displaying DICOM series
///
/// This view extends QListView to provide a grid-like layout for DICOM series
/// with support for:
/// - Grid layout with configurable columns
/// - Integration with ctkDICOMSeriesModel and ctkDICOMSeriesDelegate
/// - Thumbnail-based series display
/// - Multi-selection support
/// - Context menu integration
/// - Drag and drop support
/// - Keyboard navigation
/// - Responsive layout
///
/// The view is designed to replace QTableWidget with ctkDICOMSeriesItemWidget
/// instances, providing better performance and consistency through the
/// Model/View/Delegate pattern.
class CTK_DICOM_WIDGETS_EXPORT ctkDICOMSeriesTableView : public QListView
{
  Q_OBJECT
  Q_PROPERTY(int gridColumns READ gridColumns WRITE setGridColumns NOTIFY gridColumnsChanged)
  Q_PROPERTY(QSize gridSize READ gridSize WRITE setGridSize NOTIFY gridSizeChanged)
  Q_PROPERTY(bool uniformItemSizes READ uniformItemSizes WRITE setUniformItemSizes)
  Q_PROPERTY(int spacing READ spacing WRITE setSpacing NOTIFY spacingChanged)

public:
  typedef QListView Superclass;
  explicit ctkDICOMSeriesTableView(QWidget* parent = nullptr);
  virtual ~ctkDICOMSeriesTableView();

  /// Reimplemented from QAbstractItemView
  void setModel(QAbstractItemModel* model) override;

  /// \name Grid layout configuration
  ///@{
  /// Set the number of columns in the grid layout
  /// When set to -1 (default), columns are automatically calculated based on view width
  void setGridColumns(int columns);
  int gridColumns() const;

  /// Set explicit grid size (item size)
  /// When set to invalid size, size is determined by delegate
  void setGridSize(const QSize& size);
  QSize gridSize() const;

  /// Set spacing between items
  void setSpacing(int spacing);
  int spacing() const;
  ///@}

  /// \name Item size optimization
  ///@{
  /// Enable/disable uniform item sizes for better performance
  void setUniformItemSizes(bool uniform);
  bool uniformItemSizes() const;
  ///@}

  /// \name Selection
  ///@{
  /// Get selected series instance UIDs
  QStringList selectedSeriesInstanceUIDs() const;

  /// Get currently selected series instance UID (single selection)
  QString currentSeriesInstanceUID() const;

  /// Select series by instance UID
  void selectSeriesInstanceUID(const QString& seriesInstanceUID);

  /// Clear all selections
  void clearSelection();

  /// Get number of selected items
  int selectedCount() const;
  ///@}

  /// \name Data access
  ///@{
  /// Get series instance UID for a given index
  QString seriesInstanceUID(const QModelIndex& index) const;

  /// Get index for a series instance UID
  QModelIndex indexForSeriesInstanceUID(const QString& seriesInstanceUID) const;

  /// Get all visible series instance UIDs
  QStringList visibleSeriesInstanceUIDs() const;

  /// Get number of visible items
  int visibleCount() const;
  ///@}

  /// \name Convenience methods
  ///@{
  /// Get the series model (cast from model())
  ctkDICOMSeriesModel* seriesModel() const;

  /// Scroll to series by instance UID
  void scrollToSeriesInstanceUID(const QString& seriesInstanceUID);

  /// Refresh the view layout
  void refreshLayout();
  ///@}

protected:
  /// Reimplemented from QListView
  void resizeEvent(QResizeEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;

  /// Update grid layout when view size changes
  void updateGridLayout();

  /// Calculate optimal number of columns based on view width
  int calculateOptimalColumns() const;

  /// Get the delegate as ctkDICOMSeriesDelegate
  ctkDICOMSeriesDelegate* seriesDelegate() const;

protected slots:
  /// Called when selection changes
  void onSelectionChanged();

  /// Called when model data changes
  void onDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles);

  /// Called when rows are inserted
  void onRowsInserted(const QModelIndex& parent, int first, int last);

  /// Called when rows are removed
  void onRowsRemoved(const QModelIndex& parent, int first, int last);

  /// Called when model is reset
  void onModelReset();

signals:
  /// Emitted when grid layout changes
  void gridColumnsChanged(int columns);
  void gridSizeChanged(const QSize& size);
  void spacingChanged(int spacing);

  /// Emitted when selection changes
  void seriesSelectionChanged(const QStringList& selectedSeriesInstanceUIDs);
  void currentSeriesChanged(const QString& seriesInstanceUID);

  /// Emitted when a series is activated (double-clicked)
  void seriesActivated(const QString& seriesInstanceUID);

  /// Emitted when context menu is requested
  void contextMenuRequested(const QPoint& globalPos, const QStringList& selectedSeriesInstanceUIDs);

  /// Emitted when view layout changes
  void layoutChanged();

private:
  Q_DECLARE_PRIVATE(ctkDICOMSeriesTableView);
  Q_DISABLE_COPY(ctkDICOMSeriesTableView);

protected:
  QScopedPointer<ctkDICOMSeriesTableViewPrivate> d_ptr;
};

#endif