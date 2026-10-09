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

// GUI 线程的窗口协调器：拥有共享编译 worker，按文档 ID 向 Tab 移交编译结果。
// Qt parent 链管理控件生命周期；窗口不执行协议通信，也不代替 Session 管理文档状态。
class ApplicationWindow final : public QMainWindow {
 public:
  explicit ApplicationWindow(QWidget* parent = nullptr);
  ~ApplicationWindow() override;

  // 最多两个文档；返回借用指针，达到上限返回 nullptr，调用者不负责 delete。
  DocumentTab* AddDocument(const QString& config_path = {});
  int DocumentCount() const noexcept;
  // 验证入口复用真实 Tab；其运行记录不等于真实设备或现场验收。
  void StartUiSmoke(QStringList config_paths);
  void StartUiPerformance(QStringList config_paths, int warmup_count, int sample_count);

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void CloseTab(int index);
  void PollCompileResults();
  DocumentTab* FindDocument(DocumentId document_id) const noexcept;
  void AdvanceSmoke();
  bool CaptureSmokeSnapshot(QString& error);
  void FinishSmoke(bool success, const QString& detail);

  // 值成员拥有后台线程；窗口析构体先关闭文档，之后 worker 析构等待线程退出。
  CompileWorker worker_;
  // 以下裸指针借用 Qt parent 所有的对象，不另行释放。
  QTabWidget* tabs_ = nullptr;
  QTimer* result_timer_ = nullptr;
  QAction* new_action_ = nullptr;
  QAction* open_action_ = nullptr;
  DocumentId next_document_id_ = 1U;

  QStringList smoke_paths_;
  // smoke 期间借用当前 Tab，不能延长被关闭文档的生命周期。
  std::vector<DocumentTab*> smoke_documents_;
  std::vector<std::vector<std::array<qint64, 6>>> performance_samples_;
  bool smoke_running_ = false;
  bool performance_mode_ = false;
  bool smoke_drafts_populated_ = false;
  bool smoke_snapshot_written_ = false;
  int smoke_wait_ticks_ = 0;
  int performance_warmup_count_ = 0;
  int performance_sample_count_ = 0;
  int performance_iteration_ = 0;
};

}  // namespace pae::protocol_lab_ui
