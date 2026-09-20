#include <QApplication>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QThread>
#include <QTimer>

#include <cstdio>
#include <utility>

#include "compile_worker.h"
#include "document_tab.h"

namespace {
bool Expect(bool condition, const QString& detail) {
  if (!condition)
    std::fprintf(stderr, "BINARY_STREAM_QT_SMOKE_FAIL %s\n", detail.toUtf8().constData());
  return condition;
}
}  // namespace

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  bool unexpected_dialog = false;
  QTimer dialog_guard;
  QObject::connect(&dialog_guard, &QTimer::timeout, [&] {
    if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
      unexpected_dialog = true;
      std::fprintf(stderr, "BINARY_STREAM_QT_SMOKE_FAIL unexpected modal dialog: %s\n",
                   dialog->text().toUtf8().constData());
      dialog->reject();
    }
  });
  dialog_guard.start(20);
  pae::protocol_lab_ui::CompileWorker worker;
  pae::protocol_lab_ui::DocumentTab first(101U, worker);
  pae::protocol_lab_ui::DocumentTab second(202U, worker);
  first.LoadPath(QString::fromWCharArray(PAE_BINARY_STREAM_CONFIG));
  second.LoadPath(QString::fromWCharArray(PAE_BINARY_STREAM_CONFIG));
  QElapsedTimer timer;
  timer.start();
  while ((first.state() == pae::protocol_lab_ui::DocumentState::LOADING ||
          second.state() == pae::protocol_lab_ui::DocumentState::LOADING) &&
         timer.elapsed() < 10000) {
    for (const auto ticket : worker.DrainReadyTickets()) {
      auto completion = worker.TakeResult(ticket);
      if (!completion) continue;
      if (completion->document_id == first.document_id())
        first.AcceptCompletion(std::move(completion));
      else if (completion->document_id == second.document_id())
        second.AcceptCompletion(std::move(completion));
    }
    QApplication::processEvents();
    QThread::msleep(5);
  }
  QString error;
  if (!Expect(first.state() == pae::protocol_lab_ui::DocumentState::READY &&
                  second.state() == pae::protocol_lab_ui::DocumentState::READY,
              QStringLiteral("two Binary stream documents did not load")) ||
      !Expect(!first.IsBinaryStreamForSmoke() && !second.IsBinaryStreamForSmoke(),
              QStringLiteral("stream execution was enabled before explicit binding")) ||
      !Expect(first.VerifyBinaryStreamForSmoke(error), error) ||
      !Expect(!unexpected_dialog, QStringLiteral("unexpected dialog during Tab1 smoke"))) {
    return 1;
  }
  const QString first_signature = first.BinaryStateSignatureForSmoke();
  if (!Expect(!second.IsBinaryStreamForSmoke(),
              QStringLiteral("publishing Tab1 changed Tab2")) ||
      !Expect(second.VerifyBinaryStreamForSmoke(error), error) ||
      !Expect(!unexpected_dialog, QStringLiteral("unexpected dialog during Tab2 smoke")) ||
      !Expect(first.BinaryStateSignatureForSmoke() == first_signature,
              QStringLiteral("executing Tab2 changed Tab1 state"))) {
    return 1;
  }
  first.CloseDocument(false);
  second.CloseDocument(false);
  std::fputs("BINARY_STREAM_QT_SMOKE_PASS\n", stdout);
  return 0;
}
