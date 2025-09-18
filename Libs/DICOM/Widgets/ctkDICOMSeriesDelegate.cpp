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
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QPixmap>
#include <QFont>
#include <QFontMetrics>
#include <QPalette>
#include <QRect>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QCheckBox>
#include <QIcon>

// CTK includes
#include "ctkDICOMSeriesDelegate.h"
#include "ctkDICOMSeriesModel.h"

// STD includes
#include <cmath>

//------------------------------------------------------------------------------
class ctkDICOMSeriesDelegatePrivate
{
public:
  ctkDICOMSeriesDelegatePrivate();

  int Spacing;
  int CornerRadius;
};

//------------------------------------------------------------------------------
ctkDICOMSeriesDelegatePrivate::ctkDICOMSeriesDelegatePrivate()
{
  this->Spacing = 8;
  this->CornerRadius = 8;
}

//------------------------------------------------------------------------------
ctkDICOMSeriesDelegate::ctkDICOMSeriesDelegate(QObject* parent)
  : Superclass(parent)
  , d_ptr(new ctkDICOMSeriesDelegatePrivate)
{
}

//------------------------------------------------------------------------------
ctkDICOMSeriesDelegate::~ctkDICOMSeriesDelegate()
{
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  Q_D(const ctkDICOMSeriesDelegate);
  
  if (!index.isValid())
  {
    return;
  }

  // Check if this cell has actual series data
  QString seriesUID = index.data(ctkDICOMSeriesModel::SeriesInstanceUIDRole).toString();
  if (seriesUID.isEmpty())
  {
    // Empty cell - don't draw anything
    return;
  }

  painter->setRenderHint(QPainter::Antialiasing, true);

  // Get item rect
  QRect itemRect = option.rect;
  
  // Calculate content areas
  QRect thumbRect = this->thumbnailRect(itemRect, index);
  QRect textRect = this->textRect(itemRect, index);
  
  // Create a tighter content area around thumbnail and text
  QRect contentRect = thumbRect.united(textRect);
  // Use minimal margins for a tighter selection rectangle
  int verticalMargin = (d->Spacing) / 2;
  int horizontalMargin = (d->Spacing) / 8;
  contentRect = contentRect.marginsAdded(QMargins(horizontalMargin, verticalMargin, horizontalMargin, verticalMargin));

  // Draw selection/hover around content area only
  this->paintSelection(painter, contentRect, option);

  // Draw thumbnail
  this->paintThumbnail(painter, thumbRect, index);
  
  // Draw thumbnail overlay with series info
  this->paintThumbnailOverlay(painter, thumbRect, index);
  
  // Draw progress indicator if loading
  this->paintProgress(painter, thumbRect, index);
  
  // Draw text information
  this->paintText(painter, textRect, index, option);
}

//------------------------------------------------------------------------------
QSize ctkDICOMSeriesDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  Q_UNUSED(option);
  
  // Query the model for thumbnail size using the role
  if (index.model())
  {
    QVariant sizeData = index.model()->data(index, ctkDICOMSeriesModel::ThumbnailSizeRole);
    if (sizeData.isValid())
    {
      return sizeData.toSize();
    }
  }
  
  // Fallback size if model doesn't provide one
  return QSize(256, 256);
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesDelegate::paintThumbnail(QPainter* painter, const QRect& rect, const QModelIndex& index) const
{
  Q_D(const ctkDICOMSeriesDelegate);
  
  // Get thumbnail data
  QVariant thumbnailData = index.data(ctkDICOMSeriesModel::ThumbnailRole);
  QPixmap thumbnail;
  
  if (thumbnailData.isValid() && !thumbnailData.isNull())
  {
    thumbnail = thumbnailData.value<QPixmap>();
  }
  
  // Draw thumbnail background
  painter->save();
  
  // Add subtle drop shadow for depth
  if (!thumbnail.isNull())
  {
    QRect shadowRect = rect.adjusted(2, 2, 2, 2);
    QPainterPath shadowPath;
    shadowPath.addRoundedRect(shadowRect, d->CornerRadius, d->CornerRadius);
    painter->fillPath(shadowPath, QColor(0, 0, 0, 30));
  }
  
  // Draw thumbnail
  if (!thumbnail.isNull())
  {
    // Scale thumbnail to fit while maintaining aspect ratio
    QPixmap scaledThumbnail = thumbnail.scaled(rect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    // Center the thumbnail
    QRect thumbRect = rect;
    thumbRect.setSize(scaledThumbnail.size());
    thumbRect.moveCenter(rect.center());
    
    // Create rounded clipping path
    QPainterPath path;
    path.addRoundedRect(thumbRect, d->CornerRadius, d->CornerRadius);
    painter->setClipPath(path);
    painter->setRenderHint(QPainter::Antialiasing, true);
    
    painter->drawPixmap(thumbRect, scaledThumbnail);
  }
  
  painter->restore();
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesDelegate::paintText(QPainter* painter, const QRect& rect, const QModelIndex& index, const QStyleOptionViewItem& option) const
{
  painter->save();
  
  // Get series description only
  QString seriesDescription = index.data(ctkDICOMSeriesModel::SeriesDescriptionRole).toString();
  if (seriesDescription.isEmpty())
  {
    seriesDescription = tr("Unknown Series");
  }
  
  // Set up smaller font for series description
  QFont titleFont = option.font;
  titleFont.setBold(false);
  titleFont.setPointSize(titleFont.pointSize() - 2); // Smaller text
  
  QFontMetrics titleMetrics(titleFont);
  
  // Create a much smaller rect for the text (minimize vertical space)
  QRect textRect = rect;
  int textHeight = titleMetrics.height();
  int reducedHeight = textHeight + 4; // Just add small padding above/below text
  
  textRect.setHeight(reducedHeight);
  textRect.moveTop(rect.top() + 2); // Position near top with minimal margin
  
  // Draw series description centered with ellipsis if needed
  painter->setFont(titleFont);
  painter->setRenderHint(QPainter::Antialiasing, true);
  
  QColor textColor = option.palette.color(QPalette::Text);
  painter->setPen(textColor);
  
  QString elidedTitle = titleMetrics.elidedText(seriesDescription, Qt::ElideRight, textRect.width());
  painter->drawText(textRect, Qt::AlignCenter, elidedTitle);
  
  painter->restore();
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesDelegate::paintThumbnailOverlay(QPainter* painter, const QRect& rect, const QModelIndex& index) const
{
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  
  // Get series information
  int instanceCount = index.data(ctkDICOMSeriesModel::InstanceCountRole).toInt();
  QString seriesNumber = index.data(ctkDICOMSeriesModel::SeriesNumberRole).toString();
  QString modality = index.data(ctkDICOMSeriesModel::ModalityRole).toString();
  
  // Get dimensions from DICOM model roles
  int rows = index.data(ctkDICOMSeriesModel::RowsRole).toInt();
  int cols = index.data(ctkDICOMSeriesModel::ColumnsRole).toInt();
  QString dimensions = QString("%1x%2x%3").arg(cols).arg(rows).arg(instanceCount);
  
  if (seriesNumber.isEmpty())
  {
    painter->restore();
    return;
  }
  
  // Set up font for overlay - larger and bold to match reference image
  QFont overlayFont = painter->font();
  overlayFont.setPointSize(overlayFont.pointSize() + 1); // Larger text
  overlayFont.setBold(true);
  
  QFontMetrics metrics(overlayFont);
  int lineHeight = metrics.height();
  int padding = 4;
  
  // Set up text drawing with blue color
  painter->setFont(overlayFont);
  QColor textColor = QColor(0, 120, 215); // Blue color
  QColor shadowColor = QColor(128, 128, 128, 180); // Grey shadow
  
  // Draw "Series:" in top-left corner
  QString seriesText = QString("Series: %1").arg(seriesNumber);
  QRect seriesRect(rect.left() + padding, rect.top() + padding, rect.width() - (padding * 2), lineHeight);
  
  // Draw shadow first (offset by 1 pixel)
  painter->setPen(shadowColor);
  QRect seriesShadowRect = seriesRect.adjusted(1, 1, 1, 1);
  painter->drawText(seriesShadowRect, Qt::AlignLeft | Qt::AlignTop, seriesText);
  
  // Draw main text
  painter->setPen(textColor);
  painter->drawText(seriesRect, Qt::AlignLeft | Qt::AlignTop, seriesText);
  
  // Draw modality on the second line (top-left area)
  if (!modality.isEmpty())
  {
    QRect modalityRect(rect.left() + padding, rect.top() + padding + lineHeight, rect.width() - (padding * 2), lineHeight);
    
    // Draw shadow first
    painter->setPen(shadowColor);
    QRect modalityShadowRect = modalityRect.adjusted(1, 1, 1, 1);
    painter->drawText(modalityShadowRect, Qt::AlignLeft | Qt::AlignTop, modality);
    
    // Draw main text
    painter->setPen(textColor);
    painter->drawText(modalityRect, Qt::AlignLeft | Qt::AlignTop, modality);
  }
  
  // Draw dimensions at bottom-left
  QRect dimensionsRect(rect.left() + padding, rect.bottom() - lineHeight - padding, rect.width() - (padding * 2), lineHeight);
  
  // Draw shadow first
  painter->setPen(shadowColor);
  QRect dimensionsShadowRect = dimensionsRect.adjusted(1, 1, 1, 1);
  painter->drawText(dimensionsShadowRect, Qt::AlignLeft | Qt::AlignTop, dimensions);
  
  // Draw main text
  painter->setPen(textColor);
  painter->drawText(dimensionsRect, Qt::AlignLeft | Qt::AlignTop, dimensions);
  
  painter->restore();
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesDelegate::paintSelection(QPainter* painter, const QRect& rect, const QStyleOptionViewItem& option) const
{
  Q_D(const ctkDICOMSeriesDelegate);
  if (!(option.state & QStyle::State_Selected))
  {
    return;
  }

  painter->save();

  QColor backgroundColor = option.palette.color(QPalette::Highlight);
  backgroundColor.setAlpha(80);
  QColor borderColor = option.palette.color(QPalette::Highlight);
  borderColor.setAlpha(150);

  painter->setBrush(QBrush(backgroundColor));
  painter->setPen(QPen(borderColor, 2));
  painter->drawRoundedRect(rect, d->CornerRadius, d->CornerRadius);

  painter->restore();
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesDelegate::paintProgress(QPainter* painter, const QRect& rect, const QModelIndex& index) const
{
  // Check if series is currently loading
  QVariant statusData = index.data(ctkDICOMSeriesModel::StatusRole);
  if (!statusData.isValid())
  {
    return;
  }
  
  int status = statusData.toInt();
  if (status != ctkDICOMSeriesModel::LoadingThumbnail)
  {
    return;
  }
  
  painter->save();
  
  // Draw semi-transparent overlay
  painter->setBrush(QBrush(QColor(0, 0, 0, 100)));
  painter->setPen(Qt::NoPen);
  painter->drawRoundedRect(rect, 8, 8);
  
  // Draw spinning progress indicator
  painter->setPen(QPen(Qt::white, 3));
  painter->setBrush(Qt::NoBrush);
  
  QRect progressRect = rect.adjusted(rect.width()/4, rect.height()/4, -rect.width()/4, -rect.height()/4);
  
  // Simple rotating arc
  static int rotation = 0;
  rotation = (rotation + 30) % 360;
  
  painter->translate(progressRect.center());
  painter->rotate(rotation);
  painter->drawArc(QRect(-15, -15, 30, 30), 0, 120 * 16);
  
  painter->restore();
}

//------------------------------------------------------------------------------
QRect ctkDICOMSeriesDelegate::thumbnailRect(const QRect& itemRect, const QModelIndex& index) const
{
  Q_D(const ctkDICOMSeriesDelegate);
  
  // Query the model for thumbnail size using the role
  QSize thumbnailSize;
  if (index.model())
  {
    QVariant sizeData = index.model()->data(index, ctkDICOMSeriesModel::ThumbnailSizeRole);
    if (sizeData.isValid())
    {
      thumbnailSize = sizeData.toSize();
    }
    else
    {
      thumbnailSize = QSize(256, 256); // Fallback
    }
  }
  else
  {
    thumbnailSize = QSize(256, 256); // Fallback
  }
  
  QRect thumbRect;
  thumbRect.setSize(thumbnailSize);
  
  // Reduce margins for tighter layout
  int margin = d->Spacing * 1.5;
  
  // Ensure thumbnail fits within the available space with margins
  int availableWidth = itemRect.width() - margin * 2;
  int availableHeight = itemRect.height() - margin * 2;

  // Scale down thumbnail if it doesn't fit with margins
  QSize actualThumbnailSize = thumbnailSize;
  if (actualThumbnailSize.width() > availableWidth || actualThumbnailSize.height() > availableHeight)
  {
    actualThumbnailSize = actualThumbnailSize.scaled(availableWidth, availableHeight, Qt::KeepAspectRatioByExpanding);
  }
  
  thumbRect.setSize(actualThumbnailSize);
  
  // Create the available area with margins
  QRect availableArea(itemRect.left() + margin, 
                      itemRect.top() + margin,
                      availableWidth, 
                      availableHeight);
  
  // Center the thumbnail within the available area
  thumbRect.moveCenter(availableArea.center());
  
  return thumbRect;
}

//------------------------------------------------------------------------------
QRect ctkDICOMSeriesDelegate::textRect(const QRect& itemRect, const QModelIndex& index) const
{
  Q_D(const ctkDICOMSeriesDelegate);
  
  QRect thumbRect = this->thumbnailRect(itemRect, index);
  
  // Create a much smaller text area - just enough for one line of text
  QFont font; // Use default font to calculate height
  font.setPointSize(font.pointSize() - 2); // Match the font size used in paintText
  QFontMetrics metrics(font);
  int textHeight = metrics.height();
  
  QRect textRect;
  textRect.setLeft(itemRect.left() + d->Spacing);
  textRect.setRight(itemRect.right() - d->Spacing);
  textRect.setTop(thumbRect.bottom() + 2); // Minimal spacing after thumbnail
  textRect.setHeight(textHeight + 6); // Just enough for text + small padding
  
  return textRect;
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesDelegate::setSpacing(int spacing)
{
  Q_D(ctkDICOMSeriesDelegate);
  d->Spacing = spacing;
}

//------------------------------------------------------------------------------
int ctkDICOMSeriesDelegate::spacing() const
{
  Q_D(const ctkDICOMSeriesDelegate);
  return d->Spacing;
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesDelegate::setCornerRadius(int radius)
{
  Q_D(ctkDICOMSeriesDelegate);
  d->CornerRadius = radius;
}

//------------------------------------------------------------------------------
int ctkDICOMSeriesDelegate::cornerRadius() const
{
  Q_D(const ctkDICOMSeriesDelegate);
  return d->CornerRadius;
}
