#pragma once

#include <QApplication>
#include <QEvent>
#include <QLineEdit>
#include <QModelIndex>
#include <QPointer>
#include <QTableView>
#include <QString>

namespace pae::protocol_lab_ui::smoke_editor_target {

inline void Stamp(QLineEdit& editor, const QModelIndex& index) {
  editor.setProperty("paeSmokeEditorModel", static_cast<qulonglong>(
                                                reinterpret_cast<quintptr>(index.model())));
  editor.setProperty("paeSmokeEditorRow", index.row());
  editor.setProperty("paeSmokeEditorColumn", index.column());
}

inline bool SendIfLive(QPointer<QLineEdit> editor, QEvent& event) {
  if (!editor) return false;
  QApplication::sendEvent(editor.data(), &event);
  return editor != nullptr;
}

inline QPointer<QLineEdit> Find(QTableView& table, const QModelIndex& target, QString& error) {
  // currentIndex is navigation state and may be cleared while the delegate editor lives.
  if (!target.isValid() || target.model() != table.model()) {
    error = QStringLiteral("ASCII smoke target table/model identity mismatch");
    return {};
  }
  QPointer<QLineEdit> found;
  for (auto* candidate : table.viewport()->findChildren<QLineEdit*>()) {
    if (candidate->isHidden() ||
        candidate->property("paeSmokeEditorModel").toULongLong() !=
            static_cast<qulonglong>(reinterpret_cast<quintptr>(target.model())) ||
        candidate->property("paeSmokeEditorRow").toInt() != target.row() ||
        candidate->property("paeSmokeEditorColumn").toInt() != target.column()) {
      continue;
    }
    if (found) {
      error = QStringLiteral("ASCII smoke target cell has multiple live editors");
      return {};
    }
    found = candidate;
  }
  if (!found) error = QStringLiteral("ASCII smoke target cell has no live editor");
  return found;
}

}  // namespace pae::protocol_lab_ui::smoke_editor_target
