#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFile>
#include <QTemporaryFile>
#include <QThread>

#include "../../tools/protocol_lab_ui/document_tab.h"
#include "test_support.h"

namespace {

using namespace pae::protocol_lab_ui;

void AwaitLoad(DocumentTab& tab, CompileWorker& worker) {
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 10000 && tab.state() == DocumentState::LOADING) {
    QCoreApplication::processEvents();
    for (const auto ticket : worker.DrainReadyTickets())
      tab.AcceptCompletion(worker.TakeResult(ticket));
    QThread::msleep(1);
  }
  assert(tab.state() == DocumentState::READY || tab.state() == DocumentState::PREVIEW_VALID);
}

}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  CompileWorker worker;
  DocumentTab tab(700U, worker);
  tab.setAttribute(Qt::WA_DontShowOnScreen, true);
  const auto path = QString::fromWCharArray(PAE_TEST_YAML_PATH);
  tab.LoadPath(path);
  AwaitLoad(tab, worker);
  assert(tab.ConfigPath() == path);
  assert(tab.Title() == QFileInfo(path).fileName());
  assert(tab.YamlGeneratedJsonMatchesForSmoke());
  assert(tab.DiagnosticUsesPlainTextForSmoke());
  const auto first_revision = tab.LoadRevisionForSmoke();
  tab.LoadPath(path);
  assert(tab.LoadRevisionForSmoke() > first_revision);
  AwaitLoad(tab, worker);
  assert(tab.YamlGeneratedJsonMatchesForSmoke());

  QFile source(path);
  assert(source.open(QIODevice::ReadOnly));
  QTemporaryFile upper_case(QDir::tempPath() + QStringLiteral("/pae-yaml-XXXXXX.YML"));
  assert(upper_case.open());
  assert(upper_case.write(source.readAll()) > 0);
  assert(upper_case.flush());
  tab.LoadPath(upper_case.fileName());
  AwaitLoad(tab, worker);
  assert(tab.ConfigPath() == upper_case.fileName());
  assert(tab.YamlGeneratedJsonMatchesForSmoke());

  QTemporaryFile unknown_suffix(QDir::tempPath() + QStringLiteral("/pae-unknown-XXXXXX.txt"));
  assert(unknown_suffix.open());
  assert(unknown_suffix.write("schema_version: \"0.10\"\n") > 0);
  assert(unknown_suffix.flush());
  tab.LoadPath(unknown_suffix.fileName());
  QElapsedTimer json_failure_timer;
  json_failure_timer.start();
  while (json_failure_timer.elapsed() < 10000 && tab.state() == DocumentState::LOADING) {
    QCoreApplication::processEvents();
    for (const auto ticket : worker.DrainReadyTickets())
      tab.AcceptCompletion(worker.TakeResult(ticket));
    QThread::msleep(1);
  }
  assert(tab.state() == DocumentState::CONFIG_ERROR);
  assert(!tab.YamlGeneratedJsonMatchesForSmoke());

  QTemporaryFile invalid(QDir::tempPath() + QStringLiteral("/pae-invalid-XXXXXX.yaml"));
  assert(invalid.open());
  assert(invalid.write("a: [") == 4);
  assert(invalid.flush());
  tab.LoadPath(invalid.fileName());
  QElapsedTimer failure_timer;
  failure_timer.start();
  while (failure_timer.elapsed() < 10000 && tab.state() == DocumentState::LOADING) {
    QCoreApplication::processEvents();
    for (const auto ticket : worker.DrainReadyTickets())
      tab.AcceptCompletion(worker.TakeResult(ticket));
    QThread::msleep(1);
  }
  assert(tab.state() == DocumentState::CONFIG_ERROR);
  assert(!tab.YamlGeneratedJsonMatchesForSmoke());
  assert(tab.DiagnosticTextForSmoke().contains(QStringLiteral("未提供")));

  QTemporaryFile oversized(QDir::tempPath() + QStringLiteral("/pae-large-XXXXXX.yaml"));
  assert(oversized.open());
  assert(oversized.write(QByteArray(16 * 1024 + 1, 'x')) == 16 * 1024 + 1);
  assert(oversized.flush());
  tab.LoadPath(oversized.fileName());
  assert(tab.state() == DocumentState::CONFIG_ERROR);
  assert(tab.DiagnosticTextForSmoke().contains(QStringLiteral("input precheck")));

  const auto json_path = QString::fromWCharArray(PAE_TEST_JSON_PATH);
  tab.LoadPath(json_path);
  AwaitLoad(tab, worker);
  assert(tab.ConfigPath() == json_path);
  assert(!tab.YamlGeneratedJsonMatchesForSmoke());
  assert(tab.CloseDocument(false));
  assert(worker.Submit(tab.document_id(), 99U, "{}") == SubmitStatus::DOCUMENT_CLOSED);
  std::puts("LAB_YAML_UI_SMOKE_PASS");
}
