#include <QApplication>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QThread>
#include <QTimer>

#include <cstdio>
#include <limits>
#include <string>
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
  const QString first_diagnostic_before = first.DiagnosticTextForSmoke();
  const QString second_diagnostic_before = second.DiagnosticTextForSmoke();

  QTemporaryFile invalid_config;
  if (!Expect(invalid_config.open(), QStringLiteral("could not create invalid config")) ||
      !Expect(invalid_config.write("{\"schema_version\":\"0.9\"}") > 0,
              QStringLiteral("could not write invalid config")) ||
      !Expect(invalid_config.flush(), QStringLiteral("could not flush invalid config"))) {
    return 1;
  }
  pae::protocol_lab_ui::DocumentTab invalid_document(303U, worker);
  invalid_document.LoadPath(invalid_config.fileName());
  timer.restart();
  while (invalid_document.state() == pae::protocol_lab_ui::DocumentState::LOADING &&
         timer.elapsed() < 10000) {
    for (const auto ticket : worker.DrainReadyTickets()) {
      auto completion = worker.TakeResult(ticket);
      if (completion && completion->document_id == invalid_document.document_id())
        invalid_document.AcceptCompletion(std::move(completion));
    }
    QApplication::processEvents();
    QThread::msleep(5);
  }
  const QString invalid_text = invalid_document.DiagnosticTextForSmoke();
  if (!Expect(invalid_document.state() == pae::protocol_lab_ui::DocumentState::CONFIG_ERROR,
              QStringLiteral("real invalid Binary config did not fail compilation")) ||
      !Expect(invalid_document.DiagnosticUsesPlainTextForSmoke(),
              QStringLiteral("diagnostic widget is not forced to plain text")) ||
      !Expect(invalid_text.contains(
                  QStringLiteral("配置编译失败（UI_PUBLIC_BINARY_COMPILE_FAILED）")) &&
                  invalid_text.contains(QStringLiteral("阶段：")) &&
                  invalid_text.contains(QStringLiteral("错误码：")) &&
                  invalid_text.contains(QStringLiteral("JSON 位置：")) &&
                  invalid_text.contains(QStringLiteral("字节偏移：")) &&
                  invalid_text.contains(QStringLiteral("资源类型：")) &&
                  invalid_text.contains(QStringLiteral("需要 / 限制：")) &&
                  invalid_text.contains(QStringLiteral("资源配置：")) &&
                  invalid_text.contains(QStringLiteral("技术详情：")),
              QStringLiteral("real compiler fields did not reach the diagnostic UI"))) {
    return 1;
  }

  QTemporaryFile pending_config;
  if (!Expect(pending_config.open(), QStringLiteral("could not create pending config")) ||
      !Expect(pending_config.write("{\"schema_version\":\"0.9\"}") > 0,
              QStringLiteral("could not write pending config")) ||
      !Expect(pending_config.flush(), QStringLiteral("could not flush pending config"))) {
    return 1;
  }
  pae::protocol_lab_ui::DocumentTab special_document(404U, worker);
  special_document.LoadPath(pending_config.fileName());
  pae::CompileDiagnostic source;
  source.stage = pae::CompileStage::RESOURCE_BUDGET;
  source.code = pae::CompileError::RESOURCE_LIMIT_EXCEEDED;
  source.json_pointer.clear();
  source.byte_offset = 0U;
  source.detail = "<b attr=\"unsafe\">& raw detail</b>\n" + std::string(4096U, 'z');
  source.resource_kind = pae::ResourceKind::METADATA_ACCOUNTED_MEMORY;
  source.required_bytes = 0U;
  source.limit_bytes = (std::numeric_limits<std::size_t>::max)();
  source.resource_profile = pae::ResourceProfile::CONSTRAINED;
  auto manual = std::make_unique<pae::protocol_lab_ui::CompileCompletion>();
  manual->document_id = special_document.document_id();
  manual->load_revision = special_document.LoadRevisionForSmoke();
  manual->route = pae::protocol_lab_ui::SchemaDispatchStatus::BINARY_PUBLIC;
  manual->public_diagnostic = source;
  manual->structured_compile_diagnostic = pae::protocol_lab_ui::ProjectCompileDiagnostic(source);
  special_document.AcceptCompletion(std::move(manual));
  const QString special_text = special_document.DiagnosticTextForSmoke();
  if (!Expect(special_document.state() == pae::protocol_lab_ui::DocumentState::CONFIG_ERROR,
              QStringLiteral("manual structured diagnostic was not published")) ||
      !Expect(special_document.DiagnosticUsesPlainTextForSmoke(),
              QStringLiteral("special diagnostic is not plain text")) ||
      !Expect(special_text.contains(QStringLiteral("JSON 位置：根（空 pointer）")) &&
                  special_text.contains(QStringLiteral("字节偏移：0")) &&
                  special_text.contains(QStringLiteral("资源类型：METADATA_ACCOUNTED_MEMORY")) &&
                  special_text.contains(QStringLiteral("需要 / 限制：0 / ")) &&
                  special_text.contains(QStringLiteral("资源配置：CONSTRAINED")) &&
                  special_text.contains(QStringLiteral("<b attr=\"unsafe\">& raw detail</b>")) &&
                  special_text.count(QLatin1Char('z')) >= 4096,
              QStringLiteral("zero/root/resource/special or long detail formatting differs")) ||
      !Expect(first.DiagnosticTextForSmoke() == first_diagnostic_before &&
                  second.DiagnosticTextForSmoke() == second_diagnostic_before,
              QStringLiteral("compile diagnostic crossed document boundaries"))) {
    return 1;
  }
  invalid_document.CloseDocument(false);
  special_document.CloseDocument(false);
  first.CloseDocument(false);
  second.CloseDocument(false);
  std::fputs("BINARY_STREAM_QT_SMOKE_PASS\n", stdout);
  return 0;
}
