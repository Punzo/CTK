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

#ifndef __ctkDICOMSeriesDelegate_h
#define __ctkDICOMSeriesDelegate_h

// Qt includes
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QPixmap>
#include <QRect>

// CTK includes
#include "ctkDICOMWidgetsExport.h"

class ctkDICOMSeriesDelegatePrivate;

/// \ingroup DICOM_Widgets
/// \brief Custom delegate for rendering DICOM series in list/grid views
///
/// This delegate provides custom rendering for DICOM series items including:
/// - Thumbnail images with fallback icons
/// - Series description and metadata
/// - Progress indicators for loading operations
/// - Selection states and hover effects
/// - Modality-specific styling
///
/// The delegate is designed to work with ctkDICOMSeriesModel and provides
/// a rich, modern UI for browsing DICOM series.
class CTK_DICOM_WIDGETS_EXPORT ctkDICOMSeriesDelegate : public QStyledItemDelegate
{
  Q_OBJECT

public:
  typedef QStyledItemDelegate Superclass;
  explicit ctkDICOMSeriesDelegate(QObject* parent = nullptr);
  virtual ~ctkDICOMSeriesDelegate();

  /// Reimplemented from QStyledItemDelegate
  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

  /// Set spacing between elements
  /// \param spacing Spacing in pixels
  void setSpacing(int spacing);
  int spacing() const;

  /// Set corner radius for rounded rectangles
  /// \param radius Corner radius in pixels
  void setCornerRadius(int radius);
  int cornerRadius() const;

protected:
  /// Paint the thumbnail area
  void paintThumbnail(QPainter* painter, const QRect& rect, const QModelIndex& index) const;
  
  /// Paint the text information
  void paintText(QPainter* painter, const QRect& rect, const QModelIndex& index, const QStyleOptionViewItem& option) const;
  
  /// Paint overlay information on thumbnail
  void paintThumbnailOverlay(QPainter* painter, const QRect& rect, const QModelIndex& index) const;
  
  /// Paint the selection state
  void paintSelection(QPainter* painter, const QRect& rect, const QStyleOptionViewItem& option) const;
  
  /// Paint the progress indicator
  void paintProgress(QPainter* painter, const QRect& rect, const QModelIndex& index) const;
  
  /// Get the thumbnail rect within the item rect
  QRect thumbnailRect(const QRect& itemRect, const QModelIndex& index) const;
  
  /// Get the text rect within the item rect
  QRect textRect(const QRect& itemRect, const QModelIndex& index) const;

private:
  Q_DECLARE_PRIVATE(ctkDICOMSeriesDelegate);
  Q_DISABLE_COPY(ctkDICOMSeriesDelegate);
  
protected:
  QScopedPointer<ctkDICOMSeriesDelegatePrivate> d_ptr;
};

#endif
