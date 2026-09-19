#include <QApplication>
#include <QCoreApplication>
#include <QTimer>

#include "application_window.h"

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("PAE Offline Encode Inspector"));
  QCoreApplication::setOrganizationName(QStringLiteral("PAE"));

  const auto arguments = QCoreApplication::arguments();
  pae::protocol_lab_ui::ApplicationWindow window;
  if (!qEnvironmentVariableIsEmpty("PAE_LAB_UI_SNAPSHOT_PATH"))
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
