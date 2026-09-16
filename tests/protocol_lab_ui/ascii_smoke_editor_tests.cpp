#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPointer>
#include <QStandardItemModel>
#include <QTableView>
#include <QWidget>

#include <cstdio>

#include "smoke_editor_target.h"

namespace {
bool Expect(bool condition, const char* detail) {
  if (!condition) std::fprintf(stderr, "ASCII_SMOKE_EDITOR_TEST_FAIL %s\n", detail);
  return condition;
}
}  // namespace

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QStandardItemModel model(2, 2);
  QTableView table;
  table.setModel(&model);
  table.show();
  const QModelIndex target = model.index(0, 1);
  table.setCurrentIndex(target);
  auto* editor = new QLineEdit(table.viewport());
  pae::protocol_lab_ui::smoke_editor_target::Stamp(*editor, target);
  editor->show();
  QWidget other_focus;
  other_focus.show();
  editor->clearFocus();
  other_focus.setFocus(Qt::OtherFocusReason);
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  QString error;

  // A live target editor is found even when global focus is absent or elsewhere.
  if (!Expect(qobject_cast<QLineEdit*>(QApplication::focusWidget()) != editor,
              "lost/wrong global focus negative setup") ||
      !Expect(pae::protocol_lab_ui::smoke_editor_target::Find(table, target, error) == editor,
              "lost-focus target lookup")) {
    return 1;
  }
  table.setCurrentIndex(model.index(1, 1));
  error.clear();
  if (!Expect(pae::protocol_lab_ui::smoke_editor_target::Find(table, target, error) == editor,
              "navigation state does not replace editor identity")) {
    return 1;
  }
  table.setCurrentIndex(target);
  QStandardItemModel foreign_model(1, 2);
  error.clear();
  if (!Expect(!pae::protocol_lab_ui::smoke_editor_target::Find(
                  table, foreign_model.index(0, 1), error),
              "foreign model rejected")) {
    return 1;
  }
  auto* wrong_editor = new QLineEdit(table.viewport());
  pae::protocol_lab_ui::smoke_editor_target::Stamp(*wrong_editor, model.index(1, 1));
  wrong_editor->show();
  error.clear();
  if (!Expect(pae::protocol_lab_ui::smoke_editor_target::Find(table, target, error) == editor,
              "foreign cell editor not selected")) {
    return 1;
  }
  editor->hide();
  error.clear();
  if (!Expect(!pae::protocol_lab_ui::smoke_editor_target::Find(table, target, error) &&
                  error.contains(QStringLiteral("no live editor")),
              "wrong cell editor cannot impersonate hidden target")) {
    return 1;
  }
  editor->show();
  auto* duplicate = new QLineEdit(table.viewport());
  pae::protocol_lab_ui::smoke_editor_target::Stamp(*duplicate, target);
  duplicate->show();
  error.clear();
  if (!Expect(!pae::protocol_lab_ui::smoke_editor_target::Find(table, target, error) &&
                  error.contains(QStringLiteral("multiple")),
              "duplicate target editors rejected")) {
    return 1;
  }
  delete duplicate;
  delete wrong_editor;
  QPointer<QLineEdit> guarded(editor);
  delete editor;
  QKeyEvent forbidden_press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier);
  error.clear();
  if (!Expect(!guarded, "destroyed editor pointer cleared") ||
      !Expect(!pae::protocol_lab_ui::smoke_editor_target::SendIfLive(guarded, forbidden_press),
              "no key event dispatched to destroyed editor") ||
      !Expect(!pae::protocol_lab_ui::smoke_editor_target::Find(table, target, error) &&
                  error.contains(QStringLiteral("no live editor")),
              "destroyed target editor rejected")) {
    return 1;
  }
  std::fputs("ASCII_SMOKE_EDITOR_TEST_PASS\n", stdout);
  return 0;
}
