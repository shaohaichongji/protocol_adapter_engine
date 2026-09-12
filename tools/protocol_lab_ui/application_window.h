#pragma once

#include <QMainWindow>
#include <QStringList>
#include <array>
#include <cstdint>
#include <vector>

#include "compile_worker.h"

class QAction;
class QTabWidget;
class QTimer;
class QCloseEvent;

namespace pae::protocol_lab_ui {

class DocumentTab;

class ApplicationWindow final : public QMainWindow {
 public:
  explicit ApplicationWindow(QWidget* parent = nullptr);
  ~ApplicationWindow() override;

  DocumentTab* AddDocument(const QString& config_path = {});
  int DocumentCount() const noexcept;
  void StartUiSmoke(QStringList config_paths);
  void StartUiPerformance(QStringList config_paths, int warmup_count, int sample_count);

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void CloseTab(int index);
  void PollCompileResults();
  DocumentTab* FindDocument(DocumentId document_id) const noexcept;
  void AdvanceSmoke();
  void FinishSmoke(bool success, const QString& detail);

  CompileWorker worker_;
  QTabWidget* tabs_ = nullptr;
  QTimer* result_timer_ = nullptr;
  QAction* new_action_ = nullptr;
  QAction* open_action_ = nullptr;
  DocumentId next_document_id_ = 1U;

  QStringList smoke_paths_;
  std::vector<DocumentTab*> smoke_documents_;
  std::vector<std::vector<std::array<qint64, 6>>> performance_samples_;
  bool smoke_running_ = false;
  bool performance_mode_ = false;
  bool smoke_drafts_populated_ = false;
  int smoke_wait_ticks_ = 0;
  int performance_warmup_count_ = 0;
  int performance_sample_count_ = 0;
  int performance_iteration_ = 0;
};

}  // namespace pae::protocol_lab_ui
