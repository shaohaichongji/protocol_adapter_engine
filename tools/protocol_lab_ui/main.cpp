#include <QApplication>
#include <QCoreApplication>
#include <QTimer>

#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#if defined(_MSC_VER)
#include <cstdlib>
#if defined(_DEBUG)
#include <crtdbg.h>
#endif
#endif
#endif

#include "application_window.h"

namespace {

#if defined(_WIN32) && defined(_MSC_VER) && defined(_DEBUG)
int __cdecl UiSmokeCrtFailFastReportHook(int report_type, wchar_t* message,
                                        int*) noexcept {
  if (report_type != _CRT_ASSERT && report_type != _CRT_ERROR) return FALSE;

  std::fputs("UI_SMOKE_CRT_FAIL_FAST ", stderr);
  if (message != nullptr) {
    char utf8_message[4096]{};
    const int converted = WideCharToMultiByte(CP_UTF8, 0, message, -1, utf8_message,
                                               sizeof(utf8_message), nullptr, nullptr);
    if (converted > 0) std::fputs(utf8_message, stderr);
  }
  std::fputc('\n', stderr);
  std::fflush(stderr);
  std::_Exit(EXIT_FAILURE);
}
#endif

bool HasArgument(int argc, char* argv[], const char* expected) noexcept {
  for (int index = 1; index < argc; ++index) {
    if (argv[index] != nullptr && std::strcmp(argv[index], expected) == 0) return true;
  }
  return false;
}

void ConfigureUiSmokeDiagnostics() noexcept {
#if defined(_WIN32)
  SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#if defined(_MSC_VER)
  _set_error_mode(_OUT_TO_STDERR);
  _set_abort_behavior(_WRITE_ABORT_MSG, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#if defined(_DEBUG)
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _CrtSetReportHookW2(_CRT_RPTHOOK_INSTALL, UiSmokeCrtFailFastReportHook);
#endif
#endif
#endif
  std::fprintf(stderr, "UI_SMOKE_DIAGNOSTICS stderr=enabled dialogs=disabled\n");
  std::fflush(stderr);
}

}  // namespace

int main(int argc, char* argv[]) {
  const bool ui_smoke = HasArgument(argc, argv, "--ui-smoke");
  if (ui_smoke) ConfigureUiSmokeDiagnostics();

#if defined(_WIN32) && defined(_MSC_VER) && defined(_DEBUG)
  if (ui_smoke && HasArgument(argc, argv, "--ui-smoke-crt-assert-probe")) {
    _ASSERTE(false && "PAE_UI_SMOKE_CRT_ASSERT_PROBE");
    std::fputs("UI_SMOKE_CRT_ASSERT_CONTINUED\n", stderr);
    std::fflush(stderr);
    return EXIT_FAILURE;
  }
#endif

  QApplication application(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("PAE Offline Encode Inspector"));
  QCoreApplication::setOrganizationName(QStringLiteral("PAE"));

  const auto arguments = QCoreApplication::arguments();
  pae::protocol_lab_ui::ApplicationWindow window;
  if (ui_smoke || !qEnvironmentVariableIsEmpty("PAE_LAB_UI_SNAPSHOT_PATH"))
    window.setAttribute(Qt::WA_DontShowOnScreen, true);
  window.show();

  const int smoke_index = arguments.indexOf(QStringLiteral("--ui-smoke"));
  if (smoke_index >= 0) {
    const auto paths = arguments.mid(smoke_index + 1);
    QTimer::singleShot(0, &window, [&window, paths] { window.StartUiSmoke(paths); });
  } else {
    const int performance_index = arguments.indexOf(QStringLiteral("--ui-performance"));
    if (performance_index >= 0) {
      const auto paths = arguments.mid(performance_index + 1);
      QTimer::singleShot(0, &window, [&window, paths] { window.StartUiPerformance(paths, 2, 10); });
    }
  }
  return application.exec();
}
