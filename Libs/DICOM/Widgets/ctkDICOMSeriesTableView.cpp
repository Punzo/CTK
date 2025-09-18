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
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QItemSelectionModel>
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
  QSize GridSize;
  int Spacing;
  bool UniformItemSizes;
  
  // Layout state
  bool LayoutUpdatePending;
};

//------------------------------------------------------------------------------
ctkDICOMSeriesTableViewPrivate::ctkDICOMSeriesTableViewPrivate()
{
  this->GridColumns = -1; // Auto-calculate
  this->GridSize = QSize(); // Invalid size = auto
  this->Spacing = 8;
  this->UniformItemSizes = true;
  this->LayoutUpdatePending = false;
}

//------------------------------------------------------------------------------
ctkDICOMSeriesTableView::ctkDICOMSeriesTableView(QWidget* parent)
  : Superclass(parent)
  , d_ptr(new ctkDICOMSeriesTableViewPrivate)
{
  // Configure view for grid layout
  this->setViewMode(QListView::IconMode);
  this->setMovement(QListView::Static);
  this->setResizeMode(QListView::Adjust);
  this->setSelectionMode(QAbstractItemView::ExtendedSelection);
  this->setDragDropMode(QAbstractItemView::NoDragDrop); // Can be enabled later
  
  // Enable item wrapping and uniform sizes
  this->setWrapping(true);
  this->setUniformItemSizes(true);
  
  // Set default spacing
  this->setSpacing(8);
  
  // Connect signals for layout updates
  connect(this->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &ctkDICOMSeriesTableView::onSelectionChanged);
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
    connect(model, &QAbstractItemModel::dataChanged,
            this, &ctkDICOMSeriesTableView::onDataChanged);
    connect(model, &QAbstractItemModel::rowsInserted,
            this, &ctkDICOMSeriesTableView::onRowsInserted);
    connect(model, &QAbstractItemModel::rowsRemoved,
            this, &ctkDICOMSeriesTableView::onRowsRemoved);
    connect(model, &QAbstractItemModel::modelReset,
            this, &ctkDICOMSeriesTableView::onModelReset);
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
void ctkDICOMSeriesTableView::setGridSize(const QSize& size)
{
  Q_D(ctkDICOMSeriesTableView);
  
  if (d->GridSize == size)
  {
    return;
  }
  
  d->GridSize = size;
  this->updateGridLayout();
  
  emit gridSizeChanged(size);
}

//------------------------------------------------------------------------------
QSize ctkDICOMSeriesTableView::gridSize() const
{
  Q_D(const ctkDICOMSeriesTableView);
  return d->GridSize;
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::setSpacing(int spacing)
{
  Q_D(ctkDICOMSeriesTableView);
  
  if (d->Spacing == spacing)
  {
    return;
  }
  
  d->Spacing = spacing;
  Superclass::setSpacing(spacing);
  
  emit spacingChanged(spacing);
}

//------------------------------------------------------------------------------
int ctkDICOMSeriesTableView::spacing() const
{
  Q_D(const ctkDICOMSeriesTableView);
  return d->Spacing;
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::setUniformItemSizes(bool uniform)
{
  Q_D(ctkDICOMSeriesTableView);
  
  if (d->UniformItemSizes == uniform)
  {
    return;
  }
  
  d->UniformItemSizes = uniform;
  Superclass::setUniformItemSizes(uniform);
}

//------------------------------------------------------------------------------
bool ctkDICOMSeriesTableView::uniformItemSizes() const
{
  Q_D(const ctkDICOMSeriesTableView);
  return d->UniformItemSizes;
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
  return this->selectionModel()->selectedIndexes().size();
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
void ctkDICOMSeriesTableView::updateGridLayout()
{
  Q_D(ctkDICOMSeriesTableView);
  
  if (d->LayoutUpdatePending)
  {
    return;
  }
  
  d->LayoutUpdatePending = true;
  
  // Calculate grid size
  QSize itemSize;
  if (d->GridSize.isValid())
  {
    itemSize = d->GridSize;
  }
  else
  {
    // Get size from delegate
    ctkDICOMSeriesDelegate* delegate = this->seriesDelegate();
    if (delegate)
    {
      itemSize = delegate->itemSize();
    }
    else
    {
      itemSize = QSize(200, 250); // Default fallback
    }
  }
  
  // Set grid size
  this->setGridSize(itemSize);
  
  // Calculate number of columns
  int columns = d->GridColumns;
  if (columns <= 0)
  {
    columns = this->calculateOptimalColumns();
  }
  
  // Update spacing
  this->setSpacing(d->Spacing);
  
  d->LayoutUpdatePending = false;
  
  emit layoutChanged();
}

//------------------------------------------------------------------------------
int ctkDICOMSeriesTableView::calculateOptimalColumns() const
{
  Q_D(const ctkDICOMSeriesTableView);
  
  int viewWidth = this->viewport()->width() - this->verticalScrollBar()->width();
  
  QSize itemSize;
  if (d->GridSize.isValid())
  {
    itemSize = d->GridSize;
  }
  else
  {
    ctkDICOMSeriesDelegate* delegate = this->seriesDelegate();
    if (delegate)
    {
      itemSize = delegate->itemSize();
    }
    else
    {
      itemSize = QSize(200, 250);
    }
  }
  
  if (itemSize.width() <= 0)
  {
    return 1;
  }
  
  int itemWidthWithSpacing = itemSize.width() + d->Spacing;
  int columns = qMax(1, (viewWidth + d->Spacing) / itemWidthWithSpacing);
  
  return columns;
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesTableView::onSelectionChanged()
{
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