#pragma once

#include <QList>

namespace TaskListTransaction {

// Changes the in-memory list only when its corresponding disk save succeeds.
// Existing records already modified by a caller must be restored by that caller
// on failure; this utility controls list membership and original order only.
template <typename T, typename Save>
bool persist(QList<T *> &items, T *item, Save save) {
  if (!item)
    return false;
  const bool added = !items.contains(item);
  if (added)
    items.append(item);
  if (save())
    return true;
  if (added)
    items.removeOne(item);
  return false;
}

template <typename T, typename Save>
bool forget(QList<T *> &items, T *item, Save save) {
  const auto index = items.indexOf(item);
  if (!item || index < 0)
    return false;
  items.removeAt(index);
  if (save())
    return true;
  items.insert(index, item);
  return false;
}

} // namespace TaskListTransaction
