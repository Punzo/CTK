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

// Qt includes
#include <QApplication>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QItemSelectionModel>
#include <QItemSelection>
#include <QToolTip>
#include <QWheelEvent>

// CTK includes
#include "ctkDICOMSeriesTableView.h"
#include "ctkDICOMSeriesDelegate.h"
#include "ctkDICOMSeriesModel.h"

//------------------------------------------------------------------------------
class ctkDICOMSeriesTableViewPrivate
{
public:
  ctkDICOMSeriesTableViewPrivate();

  int GridColumns;
  
  // Layout state
  bool LayoutUpdatePending;
};

//------------------------------------------------------------------------------
ctkDICOMSeriesTableViewPrivate::ctkDICOMSeriesTableViewPrivate()
{
  this->GridColumns = 6;
  this->LayoutUpdatePending = false;
}

//------------------------------------------------------------------------------
ctkDICOMSeriesTableView::ctkDICOMSeriesTableView(QWidget* parent)
  : Superclass(parent)
  , d_ptr(new ctkDICOMSeriesTableViewPrivate)
{
  // Configure table view for horizontal series display (single row with multiple columns)
  this->setSelectionMode(QAbstractItemView::ExtendedSelection);
  this->setDragDropMode(QAbstractItemView::NoDragDrop); // Can be enabled later
  
  // Hide headers - we want a clean grid
  this->horizontalHeader()->setVisible(false);
  this->verticalHeader()->setVisible(false);
  
  // Configure table appearance
  this->setShowGrid(false);
  this->setAlternatingRowColors(false);
  this->setSelectionBehavior(QAbstractItemView::SelectItems);
  this->setFocusPolicy(Qt::StrongFocus);
  
  // Disable default selection highlighting since we handle it in the delegate
  this->setStyleSheet(
    "QTableView::item:selected { "
    "  background: transparent; "
    "  border: none; "
    "} "
    "QTableView::item:focus { "
    "  background: transparent; "
    "  border: none; "
    "  outline: none; "
    "} "
    "QTableView { "
    "  selection-background-color: transparent; "
    "}");
  
  // Remove any content margins that might create spacing around the viewport
  this->setContentsMargins(0, 0, 0, 0);

  // Ensure the table view always shows at least one thumbnail and its description
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

  // Remove viewport margins
  if (this->viewport())
  {
    this->viewport()->setContentsMargins(0, 0, 0, 0);
  }

  // Ensure headers are completely hidden and take no space
  this->horizontalHeader()->setMinimumSectionSize(0);
  this->horizontalHeader()->setDefaultSectionSize(0);
  this->verticalHeader()->setMinimumSectionSize(0);
  this->verticalHeader()->setDefaultSectionSize(0);
  
  // Also remove viewport margins to eliminate all possible spacing
  if (this->viewport())
  {
    this->viewport()->setContentsMargins(0, 0, 0, 0);
  }
  
  // Ensure headers are completely hidden and take no space
  this->horizontalHeader()->setMinimumSectionSize(0);
  this->horizontalHeader()->setDefaultSectionSize(0);
  this->verticalHeader()->setMinimumSectionSize(0);
  this->verticalHeader()->setDefaultSectionSize(0);
  
  // Configure scrolling - horizontal scroll for series navigation
  this->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  this->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
}

//------------------------------------------------------------------------------
ctkDICOMSeriesTableView::~ctkDICOMSeriesTableView()
{
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::setModel(QAbstractItemModel* model)
{
  // Disconnect from old model
  if (this->model())
  {
    disconnect(this->model(), nullptr, this, nullptr);
  }
  
  // Set new model
  Superclass::setModel(model);

  // Connect to new model
  if (model)
  {
    connect(this->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ctkDICOMSeriesTableView::onSelectionChanged);
    connect(model, &QAbstractItemModel::dataChanged,
            this, &ctkDICOMSeriesTableView::onDataChanged);
    connect(model, &QAbstractItemModel::rowsInserted,
            this, &ctkDICOMSeriesTableView::onRowsInserted);
    connect(model, &QAbstractItemModel::rowsRemoved,
            this, &ctkDICOMSeriesTableView::onRowsRemoved);
    connect(model, &QAbstractItemModel::modelReset,
            this, &ctkDICOMSeriesTableView::onModelReset);
  }
  
  // Configure the series model if it's a ctkDICOMSeriesModel
  ctkDICOMSeriesModel* seriesModel = qobject_cast<ctkDICOMSeriesModel*>(model);
  if (seriesModel)
  {
    // Set default grid columns if not set
    if (seriesModel->gridColumns() <= 0)
    {
      seriesModel->setGridColumns(6); // Default to 6 columns
    }
  }
  
  // Update layout
  this->updateGridLayout();
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::setGridColumns(int columns)
{
  Q_D(ctkDICOMSeriesTableView);
  
  if (d->GridColumns == columns)
  {
    return;
  }
  
  d->GridColumns = columns;
  
  // Update the model's grid columns
  ctkDICOMSeriesModel* seriesModel = this->seriesModel();
  if (seriesModel)
  {
    seriesModel->setGridColumns(columns);
  }
  
  this->updateGridLayout();
  
  emit gridColumnsChanged(columns);
}

//------------------------------------------------------------------------------
int ctkDICOMSeriesTableView::gridColumns() const
{
  Q_D(const ctkDICOMSeriesTableView);
  return d->GridColumns;
}

//------------------------------------------------------------------------------
QStringList ctkDICOMSeriesTableView::selectedSeriesInstanceUIDs() const
{
  QStringList seriesUIDs;
  
  QModelIndexList selectedIndexes = this->selectionModel()->selectedIndexes();
  for (const QModelIndex& index : selectedIndexes)
  {
    QString seriesUID = this->seriesInstanceUID(index);
    if (!seriesUID.isEmpty())
    {
      seriesUIDs.append(seriesUID);
    }
  }
  
  return seriesUIDs;
}

//------------------------------------------------------------------------------
QString ctkDICOMSeriesTableView::currentSeriesInstanceUID() const
{
  QModelIndex currentIndex = this->selectionModel()->currentIndex();
  return this->seriesInstanceUID(currentIndex);
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::selectSeriesInstanceUID(const QString& seriesInstanceUID)
{
  QModelIndex index = this->indexForSeriesInstanceUID(seriesInstanceUID);
  if (index.isValid())
  {
    this->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
    this->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Current);
    this->scrollTo(index);
  }
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::clearSelection()
{
  this->selectionModel()->clearSelection();
}

//------------------------------------------------------------------------------
int ctkDICOMSeriesTableView::selectedCount() const
{
  // Return count of unique selected series (not all selected indexes)
  return this->selectedSeriesInstanceUIDs().size();
}

//------------------------------------------------------------------------------
QString ctkDICOMSeriesTableView::seriesInstanceUID(const QModelIndex& index) const
{
  if (!index.isValid())
  {
    return QString();
  }
  
  ctkDICOMSeriesModel* seriesModel = this->seriesModel();
  if (seriesModel)
  {
    return seriesModel->seriesInstanceUID(index);
  }
  
  // Fallback to direct model access
  return this->model()->data(index, ctkDICOMSeriesModel::SeriesInstanceUIDRole).toString();
}

//------------------------------------------------------------------------------
QModelIndex ctkDICOMSeriesTableView::indexForSeriesInstanceUID(const QString& seriesInstanceUID) const
{
  ctkDICOMSeriesModel* seriesModel = this->seriesModel();
  if (seriesModel)
  {
    return seriesModel->indexForSeriesInstanceUID(seriesInstanceUID);
  }
  
  // Fallback: search through all rows
  for (int row = 0; row < this->model()->rowCount(); ++row)
  {
    QModelIndex index = this->model()->index(row, 0);
    if (this->seriesInstanceUID(index) == seriesInstanceUID)
    {
      return index;
    }
  }
  
  return QModelIndex();
}

//------------------------------------------------------------------------------
QStringList ctkDICOMSeriesTableView::visibleSeriesInstanceUIDs() const
{
  ctkDICOMSeriesModel* seriesModel = this->seriesModel();
  if (seriesModel)
  {
    // Get all series UIDs from the model
    QStringList seriesUIDs;
    for (int row = 0; row < seriesModel->rowCount(); ++row)
    {
      QModelIndex index = seriesModel->index(row, 0);
      QString seriesUID = seriesModel->seriesInstanceUID(index);
      if (!seriesUID.isEmpty())
      {
        seriesUIDs << seriesUID;
      }
    }
    return seriesUIDs;
  }
  
  // Fallback: get all series UIDs
  QStringList seriesUIDs;
  for (int row = 0; row < this->model()->rowCount(); ++row)
  {
    QModelIndex index = this->model()->index(row, 0);
    QString seriesUID = this->seriesInstanceUID(index);
    if (!seriesUID.isEmpty())
    {
      seriesUIDs.append(seriesUID);
    }
  }
  
  return seriesUIDs;
}

//------------------------------------------------------------------------------
int ctkDICOMSeriesTableView::visibleCount() const
{
  return this->model() ? this->model()->rowCount() : 0;
}

//------------------------------------------------------------------------------
ctkDICOMSeriesModel* ctkDICOMSeriesTableView::seriesModel() const
{
  return qobject_cast<ctkDICOMSeriesModel*>(this->model());
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::scrollToSeriesInstanceUID(const QString& seriesInstanceUID)
{
  QModelIndex index = this->indexForSeriesInstanceUID(seriesInstanceUID);
  if (index.isValid())
  {
    this->scrollTo(index, QAbstractItemView::EnsureVisible);
  }
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::refreshLayout()
{
  this->updateGridLayout();
  this->update();
}

//------------------------------------------------------------------------------
ctkDICOMSeriesDelegate* ctkDICOMSeriesTableView::seriesDelegate() const
{
  return qobject_cast<ctkDICOMSeriesDelegate*>(this->itemDelegate());
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::resizeEvent(QResizeEvent* event)
{
  Superclass::resizeEvent(event);
  
  // Update grid layout when size changes
  this->updateGridLayout();
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::wheelEvent(QWheelEvent* event)
{
  // Handle wheel events for scrolling
  Superclass::wheelEvent(event);
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::keyPressEvent(QKeyEvent* event)
{
  // Handle special key events
  switch (event->key())
  {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    {
      // Activate current item
      QString currentUID = this->currentSeriesInstanceUID();
      if (!currentUID.isEmpty())
      {
        emit seriesActivated(currentUID);
      }
      break;
    }
    case Qt::Key_Escape:
      // Clear selection
      this->clearSelection();
      break;
    default:
      Superclass::keyPressEvent(event);
      break;
  }
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::mousePressEvent(QMouseEvent* event)
{
  QModelIndex index = this->indexAt(event->pos());
  
  // Check if the clicked index has valid series data
  if (index.isValid())
  {
    QString seriesUID = index.data(ctkDICOMSeriesModel::SeriesInstanceUIDRole).toString();
    if (seriesUID.isEmpty())
    {
      // Don't allow selection of empty cells
      event->ignore();
      return;
    }
  }
  
  Superclass::mousePressEvent(event);
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::mouseDoubleClickEvent(QMouseEvent* event)
{
  QModelIndex index = this->indexAt(event->pos());
  if (index.isValid())
  {
    QString seriesUID = this->seriesInstanceUID(index);
    if (!seriesUID.isEmpty())
    {
      emit seriesActivated(seriesUID);
    }
  }
  
  Superclass::mouseDoubleClickEvent(event);
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::contextMenuEvent(QContextMenuEvent* event)
{
  QStringList selectedUIDs = this->selectedSeriesInstanceUIDs();
  emit contextMenuRequested(event->globalPos(), selectedUIDs);
}

//------------------------------------------------------------------------------
bool ctkDICOMSeriesTableView::event(QEvent* event)
{
  return Superclass::event(event);
}

//------------------------------------------------------------------------------
QSize ctkDICOMSeriesTableView::viewportSizeHint() const
{
  Q_D(const ctkDICOMSeriesTableView);
  
  // Get thumbnail size from the model instead of delegate
  QSize itemSize(200, 200); // Default fallback
  const ctkDICOMSeriesModel* seriesModel = qobject_cast<const ctkDICOMSeriesModel*>(this->model());
  if (seriesModel)
  {
    int thumbnailSize = seriesModel->thumbnailSize();
    itemSize = QSize(thumbnailSize, thumbnailSize);
  }
  
  int cols = d->GridColumns > 0 ? d->GridColumns : 2;
  int totalSeries = this->model() ? this->model()->rowCount() : 0;
  int rows = (cols > 0) ? ((totalSeries + cols - 1) / cols) : 0;
  return QSize(itemSize.width() * cols, itemSize.height() * 1.1 * rows);
}

//------------------------------------------------------------------------------
int ctkDICOMSeriesTableView::sizeHintForRow(int row) const
{
  Q_UNUSED(row);
  
  // Get thumbnail size from the model instead of delegate
  const ctkDICOMSeriesModel* seriesModel = qobject_cast<const ctkDICOMSeriesModel*>(this->model());
  if (seriesModel)
  {
    return seriesModel->thumbnailSize();
  }
  
  return 250; // Default fallback
}

//------------------------------------------------------------------------------
int ctkDICOMSeriesTableView::sizeHintForColumn(int column) const
{
  Q_UNUSED(column);
  
  // Get thumbnail size from the model instead of delegate
  const ctkDICOMSeriesModel* seriesModel = qobject_cast<const ctkDICOMSeriesModel*>(this->model());
  if (seriesModel)
  {
    return seriesModel->thumbnailSize();
  }
  
  return 200; // Default fallback
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::updateGridLayout()
{
  Q_D(ctkDICOMSeriesTableView);
  
  if (d->LayoutUpdatePending)
  {
    return;
  }
  
  d->LayoutUpdatePending = true;
  
  // Get series model
  ctkDICOMSeriesModel* seriesModel = this->seriesModel();
  if (!seriesModel)
  {
    d->LayoutUpdatePending = false;
    return;
  }
  
  // Get cell size from the model's thumbnail size
  int thumbnailSize = seriesModel->thumbnailSize();
  QSize cellSize = QSize(thumbnailSize, thumbnailSize);
  
  // Configure table dimensions based on the model's grid
  int columns = seriesModel->columnCount();
  int rows = seriesModel->rowCount();
  
  // Set uniform column widths and row heights
  for (int col = 0; col < columns; ++col)
  {
    this->setColumnWidth(col, cellSize.width());
  }
  
  for (int row = 0; row < rows; ++row)
  {
    this->setRowHeight(row, cellSize.height());
  }
  
  // Force table to exactly fit content with no extra space
  this->resizeRowsToContents();
  this->resizeColumnsToContents();

  // Thumbnail size is now managed by the model, not the delegate
  
  d->LayoutUpdatePending = false;
  
  emit layoutChanged();
}

//------------------------------------------------------------------------------
int ctkDICOMSeriesTableView::calculateOptimalColumns() const
{
  int viewWidth = this->viewport()->width() - this->verticalScrollBar()->width();
  
  // Get item size from the model's thumbnail size
  QSize itemSize(200, 200); // Default fallback
  ctkDICOMSeriesModel* seriesModel = this->seriesModel();
  if (seriesModel)
  {
    int thumbnailSize = seriesModel->thumbnailSize();
    itemSize = QSize(thumbnailSize, thumbnailSize);
  }
  
  if (itemSize.width() <= 0)
  {
    return 1;
  }
  
  int itemWidthWithSpacing = itemSize.width();
  int columns = qMax(1, viewWidth / itemWidthWithSpacing);
  
  return columns;
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::onSelectionChanged()
{
  this->viewport()->update();

  QStringList selectedUIDs = this->selectedSeriesInstanceUIDs();
  QString currentUID = this->currentSeriesInstanceUID();

  emit seriesSelectionChanged(selectedUIDs);
  emit currentSeriesChanged(currentUID);
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::onDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
{
  Q_UNUSED(topLeft);
  Q_UNUSED(bottomRight);
  Q_UNUSED(roles);

  // Update view if needed
  this->update();
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::onRowsInserted(const QModelIndex& parent, int first, int last)
{
  Q_UNUSED(parent);
  Q_UNUSED(first);
  Q_UNUSED(last);
  
  // Update layout when items are added
  this->updateGridLayout();
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::onRowsRemoved(const QModelIndex& parent, int first, int last)
{
  Q_UNUSED(parent);
  Q_UNUSED(first);
  Q_UNUSED(last);
  
  // Update layout when items are removed
  this->updateGridLayout();
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::onModelReset()
{
  // Update layout when model is reset
  this->updateGridLayout();
}
