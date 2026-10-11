#pragma once

#include "pch.h"
#include "theme.h"

// Keeps the built-in tree item rendering (selection, icons, and sorting)
// while adding fine dividers in file-manager Details mode.
class FileManagerColumnDelegate final : public QStyledItemDelegate {
public:
  explicit FileManagerColumnDelegate(int typeColumn, QObject *parent = nullptr)
      : QStyledItemDelegate(parent), mTypeColumn(typeColumn) {}

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyledItemDelegate::paint(painter, option, index);
    painter->save();
    painter->setPen(QPen(Theme::color("divider"), 1));
    const int x = option.rect.right();
    painter->drawLine(x, option.rect.top() + 2,
                      x, option.rect.bottom() - 2);
    painter->restore();
  }

  bool helpEvent(QHelpEvent *event, QAbstractItemView *view,
                 const QStyleOptionViewItem &option,
                 const QModelIndex &index) override {
    if (event && event->type() == QEvent::ToolTip && index.isValid()) {
      const QString value = index.data(Qt::DisplayRole).toString();
      // Type text is deliberately ellipsized. Show the full classification
      // when hovered; offer the same help for other narrow columns.
      const int availableWidth = option.rect.width() - 12;
      const bool shortened = QFontMetrics(option.font).horizontalAdvance(value) >
                             availableWidth;
      if (!value.isEmpty() && (index.column() == mTypeColumn || shortened)) {
        QToolTip::showText(event->globalPos(), value, view);
        return true;
      }
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
  }

private:
  int mTypeColumn;
};
