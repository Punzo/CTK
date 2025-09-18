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

  QSize ItemSize;
  QSize ThumbnailSize;
  bool ShowDetailedText;
  int Spacing;
  int CornerRadius;
  bool ShowSelectionCheckbox;
  
  // Cached icons for different modalities
  mutable QHash<QString, QPixmap> ModalityIcons;
  mutable QHash<QString, QColor> ModalityColors;
};

//------------------------------------------------------------------------------
ctkDICOMSeriesDelegatePrivate::ctkDICOMSeriesDelegatePrivate()
{
  this->ItemSize = QSize(200, 250);
  this->ThumbnailSize = QSize(150, 150);
  this->ShowDetailedText = true;
  this->Spacing = 8;
  this->CornerRadius = 8;
  this->ShowSelectionCheckbox = true;
  
  // Initialize modality colors
  this->ModalityColors["CT"] = QColor(100, 149, 237);  // Cornflower blue
  this->ModalityColors["MR"] = QColor(50, 205, 50);    // Lime green
  this->ModalityColors["US"] = QColor(255, 165, 0);    // Orange
  this->ModalityColors["XA"] = QColor(220, 20, 60);    // Crimson
  this->ModalityColors["CR"] = QColor(138, 43, 226);   // Blue violet
  this->ModalityColors["DR"] = QColor(75, 0, 130);     // Indigo
  this->ModalityColors["DX"] = QColor(148, 0, 211);    // Dark violet
  this->ModalityColors["MG"] = QColor(255, 20, 147);   // Deep pink
  this->ModalityColors["NM"] = QColor(255, 215, 0);    // Gold
  this->ModalityColors["PT"] = QColor(255, 69, 0);     // Red orange
  this->ModalityColors["RF"] = QColor(30, 144, 255);   // Dodger blue
  this->ModalityColors["SC"] = QColor(128, 128, 128);  // Gray
  this->ModalityColors[""] = QColor(169, 169, 169);    // Dark gray (unknown)
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

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  // Get item rect
  QRect itemRect = option.rect;
  
  // Draw background with selection state
  this->paintSelection(painter, itemRect, index, option);
  
  // Draw thumbnail
  QRect thumbRect = this->thumbnailRect(itemRect);
  this->paintThumbnail(painter, thumbRect, index);
  
  // Draw modality badge
  this->paintModalityBadge(painter, thumbRect, index);
  
  // Draw progress indicator if loading
  this->paintProgress(painter, thumbRect, index);
  
  // Draw text information
  if (d->ShowDetailedText)
  {
    QRect textRect = this->textRect(itemRect);
    this->paintText(painter, textRect, index, option);
  }

  painter->restore();
}

//------------------------------------------------------------------------------
QSize ctkDICOMSeriesDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  Q_D(const ctkDICOMSeriesDelegate);
  Q_UNUSED(option);
  Q_UNUSED(index);
  
  return d->ItemSize;
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
  
  // If no thumbnail, use fallback icon
  if (thumbnail.isNull())
  {
    QString modality = index.data(ctkDICOMSeriesModel::ModalityRole).toString();
    thumbnail = this->getFallbackIcon(modality);
  }
  
  // Draw thumbnail background
  painter->save();
  painter->setPen(QPen(QColor(200, 200, 200), 1));
  painter->setBrush(QBrush(QColor(250, 250, 250)));
  painter->drawRoundedRect(rect, d->CornerRadius, d->CornerRadius);
  
  // Draw thumbnail
  if (!thumbnail.isNull())
  {
    // Scale thumbnail to fit while maintaining aspect ratio
    QPixmap scaledThumbnail = thumbnail.scaled(rect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    // Center the thumbnail
    QRect thumbRect = rect;
    thumbRect.setSize(scaledThumbnail.size());
    thumbRect.moveCenter(rect.center());
    
    painter->drawPixmap(thumbRect, scaledThumbnail);
  }
  
  painter->restore();
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesDelegate::paintText(QPainter* painter, const QRect& rect, const QModelIndex& index, const QStyleOptionViewItem& option) const
{
  Q_D(const ctkDICOMSeriesDelegate);
  
  painter->save();
  
  // Set up fonts
  QFont titleFont = option.font;
  titleFont.setBold(true);
  titleFont.setPointSize(titleFont.pointSize() + 1);
  
  QFont detailFont = option.font;
  detailFont.setPointSize(detailFont.pointSize() - 1);
  
  // Get text data
  QString seriesDescription = index.data(ctkDICOMSeriesModel::SeriesDescriptionRole).toString();
  QString modality = index.data(ctkDICOMSeriesModel::ModalityRole).toString();
  int instanceCount = index.data(ctkDICOMSeriesModel::InstanceCountRole).toInt();
  QString seriesNumber = index.data(ctkDICOMSeriesModel::SeriesNumberRole).toString();
  
  // Prepare text
  if (seriesDescription.isEmpty())
  {
    seriesDescription = tr("Unknown Series");
  }
  
  QString detailText = QString("%1 - %2 images").arg(modality).arg(instanceCount);
  if (!seriesNumber.isEmpty())
  {
    detailText = QString("Series %1 - %2").arg(seriesNumber, detailText);
  }
  
  // Calculate text layout
  QFontMetrics titleMetrics(titleFont);
  QFontMetrics detailMetrics(detailFont);
  
  int titleHeight = titleMetrics.height();
  int detailHeight = detailMetrics.height();
  int totalTextHeight = titleHeight + detailHeight + d->Spacing;
  
  // Center text vertically in available rect
  QRect textRect = rect;
  if (totalTextHeight < rect.height())
  {
    int offset = (rect.height() - totalTextHeight) / 2;
    textRect.setTop(rect.top() + offset);
    textRect.setHeight(totalTextHeight);
  }
  
  // Draw series description (title)
  QRect titleRect = textRect;
  titleRect.setHeight(titleHeight);
  
  painter->setFont(titleFont);
  painter->setPen(option.palette.color(QPalette::Text));
  
  QString elidedTitle = titleMetrics.elidedText(seriesDescription, Qt::ElideRight, titleRect.width());
  painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignTop, elidedTitle);
  
  // Draw detail text
  QRect detailRect = textRect;
  detailRect.setTop(titleRect.bottom() + d->Spacing);
  detailRect.setHeight(detailHeight);
  
  painter->setFont(detailFont);
  painter->setPen(option.palette.color(QPalette::Mid));
  
  QString elidedDetail = detailMetrics.elidedText(detailText, Qt::ElideRight, detailRect.width());
  painter->drawText(detailRect, Qt::AlignLeft | Qt::AlignTop, elidedDetail);
  
  painter->restore();
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesDelegate::paintSelection(QPainter* painter, const QRect& rect, const QModelIndex& index, const QStyleOptionViewItem& option) const
{
  Q_D(const ctkDICOMSeriesDelegate);
  
  painter->save();
  
  QColor backgroundColor;
  QColor borderColor;
  
  // Determine colors based on state
  if (option.state & QStyle::State_Selected)
  {
    backgroundColor = option.palette.color(QPalette::Highlight);
    backgroundColor.setAlpha(60);
    borderColor = option.palette.color(QPalette::Highlight);
  }
  else if (option.state & QStyle::State_MouseOver)
  {
    backgroundColor = option.palette.color(QPalette::Highlight);
    backgroundColor.setAlpha(20);
    borderColor = option.palette.color(QPalette::Highlight);
    borderColor.setAlpha(80);
  }
  else
  {
    backgroundColor = option.palette.color(QPalette::Base);
    borderColor = option.palette.color(QPalette::Mid);
    borderColor.setAlpha(40);
  }
  
  // Draw background
  painter->setBrush(QBrush(backgroundColor));
  painter->setPen(QPen(borderColor, 2));
  painter->drawRoundedRect(rect.adjusted(1, 1, -1, -1), d->CornerRadius, d->CornerRadius);
  
  // Draw selection checkbox if enabled
  if (d->ShowSelectionCheckbox)
  {
    QRect checkRect = this->selectionRect(rect);
    bool isSelected = index.data(ctkDICOMSeriesModel::IsSelectedRole).toBool();
    
    // Draw checkbox background
    painter->setBrush(QBrush(Qt::white));
    painter->setPen(QPen(Qt::gray, 1));
    painter->drawRoundedRect(checkRect, 3, 3);
    
    // Draw checkmark if selected
    if (isSelected)
    {
      painter->setPen(QPen(option.palette.color(QPalette::Highlight), 2));
      
      // Draw checkmark
      int x = checkRect.left() + 3;
      int y = checkRect.center().y();
      int w = checkRect.width() - 6;
      int h = checkRect.height() - 6;
      
      QPolygon checkmark;
      checkmark << QPoint(x + w/4, y + h/4)
                << QPoint(x + w/2, y + h/2)
                << QPoint(x + 3*w/4, y - h/4);
      
      painter->drawPolyline(checkmark);
    }
  }
  
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
void ctkDICOMSeriesDelegate::paintModalityBadge(QPainter* painter, const QRect& rect, const QModelIndex& index) const
{
  QString modality = index.data(ctkDICOMSeriesModel::ModalityRole).toString();
  if (modality.isEmpty())
  {
    return;
  }
  
  painter->save();
  
  // Get modality color
  QColor modalityColor = this->getModalityColor(modality);
  
  // Badge rect (top-right corner)
  QRect badgeRect(rect.right() - 30, rect.top() + 5, 25, 16);
  
  // Draw badge background
  painter->setBrush(QBrush(modalityColor));
  painter->setPen(QPen(modalityColor.darker(120), 1));
  painter->drawRoundedRect(badgeRect, 3, 3);
  
  // Draw modality text
  painter->setPen(QPen(Qt::white));
  QFont badgeFont = painter->font();
  badgeFont.setPointSize(8);
  badgeFont.setBold(true);
  painter->setFont(badgeFont);
  
  painter->drawText(badgeRect, Qt::AlignCenter, modality);
  
  painter->restore();
}

//------------------------------------------------------------------------------
QRect ctkDICOMSeriesDelegate::thumbnailRect(const QRect& itemRect) const
{
  Q_D(const ctkDICOMSeriesDelegate);
  
  QRect thumbRect;
  thumbRect.setSize(d->ThumbnailSize);
  thumbRect.moveCenter(QPoint(itemRect.center().x(), itemRect.top() + d->Spacing + d->ThumbnailSize.height()/2));
  
  return thumbRect;
}

//------------------------------------------------------------------------------
QRect ctkDICOMSeriesDelegate::textRect(const QRect& itemRect) const
{
  Q_D(const ctkDICOMSeriesDelegate);
  
  QRect thumbRect = this->thumbnailRect(itemRect);
  
  QRect textRect;
  textRect.setLeft(itemRect.left() + d->Spacing);
  textRect.setRight(itemRect.right() - d->Spacing);
  textRect.setTop(thumbRect.bottom() + d->Spacing);
  textRect.setBottom(itemRect.bottom() - d->Spacing);
  
  return textRect;
}

//------------------------------------------------------------------------------
QRect ctkDICOMSeriesDelegate::selectionRect(const QRect& itemRect) const
{
  return QRect(itemRect.left() + 8, itemRect.top() + 8, 16, 16);
}

//------------------------------------------------------------------------------
QPixmap ctkDICOMSeriesDelegate::getFallbackIcon(const QString& modality) const
{
  Q_D(const ctkDICOMSeriesDelegate);
  
  // Check cache first
  if (d->ModalityIcons.contains(modality))
  {
    return d->ModalityIcons[modality];
  }
  
  // Create fallback icon
  QPixmap icon(64, 64);
  icon.fill(Qt::transparent);
  
  QPainter painter(&icon);
  painter.setRenderHint(QPainter::Antialiasing);
  
  // Get modality color
  QColor color = this->getModalityColor(modality);
  
  // Draw icon background
  painter.setBrush(QBrush(color));
  painter.setPen(QPen(color.darker(120), 2));
  painter.drawRoundedRect(8, 8, 48, 48, 8, 8);
  
  // Draw modality text
  painter.setPen(QPen(Qt::white));
  QFont font = painter.font();
  font.setPointSize(14);
  font.setBold(true);
  painter.setFont(font);
  
  QString displayText = modality.isEmpty() ? "?" : modality;
  painter.drawText(QRect(8, 8, 48, 48), Qt::AlignCenter, displayText);
  
  // Cache the icon
  d->ModalityIcons[modality] = icon;
  
  return icon;
}

//------------------------------------------------------------------------------
QColor ctkDICOMSeriesDelegate::getModalityColor(const QString& modality) const
{
  Q_D(const ctkDICOMSeriesDelegate);
  
  if (d->ModalityColors.contains(modality))
  {
    return d->ModalityColors[modality];
  }
  
  // Return default color for unknown modalities
  return d->ModalityColors[""];
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesDelegate::setItemSize(const QSize& size)
{
  Q_D(ctkDICOMSeriesDelegate);
  d->ItemSize = size;
}

//------------------------------------------------------------------------------
QSize ctkDICOMSeriesDelegate::itemSize() const
{
  Q_D(const ctkDICOMSeriesDelegate);
  return d->ItemSize;
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesDelegate::setShowDetailedText(bool show)
{
  Q_D(ctkDICOMSeriesDelegate);
  d->ShowDetailedText = show;
}

//------------------------------------------------------------------------------
bool ctkDICOMSeriesDelegate::showDetailedText() const
{
  Q_D(const ctkDICOMSeriesDelegate);
  return d->ShowDetailedText;
}

//------------------------------------------------------------------------------
void ctkDICOMSeriesDelegate::setThumbnailSize(const QSize& size)
{
  Q_D(ctkDICOMSeriesDelegate);
  d->ThumbnailSize = size;
}

//------------------------------------------------------------------------------
QSize ctkDICOMSeriesDelegate::thumbnailSize() const
{
  Q_D(const ctkDICOMSeriesDelegate);
  return d->ThumbnailSize;
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

//------------------------------------------------------------------------------
void ctkDICOMSeriesDelegate::setShowSelectionCheckbox(bool show)
{
  Q_D(ctkDICOMSeriesDelegate);
  d->ShowSelectionCheckbox = show;
}

//------------------------------------------------------------------------------
bool ctkDICOMSeriesDelegate::showSelectionCheckbox() const
{
  Q_D(const ctkDICOMSeriesDelegate);
  return d->ShowSelectionCheckbox;
}