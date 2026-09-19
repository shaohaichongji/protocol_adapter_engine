#include "application_window.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include "document_tab.h"
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
#include "ascii_smoke_diagnostic.h"
#endif

namespace pae::protocol_lab_ui {
namespace {

QString UiText(const char* text) { return QCoreApplication::translate("PaeLabUi", text); }

struct SmokeExpectation {
  std::vector<std::uint8_t> frame;
  std::vector<PhysicalBitMask> highlights;
};

std::optional<SmokeExpectation> ExpectedPublicFixture(const QString& path) {
  const QString name = QFileInfo(path).fileName();
  if (name == QStringLiteral("synthetic_ui_v05.pae.json")) {
    return SmokeExpectation{{0x80U, 0x08U, 0x03U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x5AU},
                            {{1U, 0x01U}}};
  }
  if (name == QStringLiteral("synthetic_ui_v06.pae.json")) {
    return SmokeExpectation{
        {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x18U, 0x72U, 0xAAU},
        {{0U, 0xFFU},
         {1U, 0xFFU},
         {2U, 0xFFU},
         {3U, 0xFFU},
         {4U, 0xFFU},
         {5U, 0xFFU},
         {6U, 0xFFU},
         {7U, 0xFFU},
         {8U, 0xFFU}}};
  }
  if (name == QStringLiteral("synthetic_ui_v07.pae.json")) {
    return SmokeExpectation{{0xAAU, 0x00U, 0x06U, 0x00U, 0x00U, 0x55U}, {{1U, 0xFFU}, {2U, 0xFFU}}};
  }
  if (name == QStringLiteral("synthetic_ui_v08.pae.json")) {
    return SmokeExpectation{{0xA5U, 0x06U, 0x00U, 0x00U, 0x00U, 0xABU}, {{1U, 0xFFU}}};
  }
  if (name == QStringLiteral("synthetic_ui_max.pae.json")) {
    return SmokeExpectation{std::vector<std::uint8_t>(65536U, 0U), {{0U, 0xFFU}}};
  }
  return std::nullopt;
}

bool MatchesExpectation(const DocumentTab& document, const SmokeExpectation& expected) {
  if (document.PreviewFrameForSmoke() != expected.frame ||
      document.HighlightedCellCountForSmoke() != expected.highlights.size()) {
    return false;
  }
  return std::all_of(expected.highlights.begin(), expected.highlights.end(),
                     [&document](const PhysicalBitMask& item) {
                       return document.HighlightMaskForSmoke(item.frame_byte_index) ==
                              item.uint8_mask;
                     });
}

QString SpacedLowerHex(const std::vector<std::uint8_t>& frame) {
  QString output;
  for (std::size_t index = 0U; index < frame.size(); ++index) {
    if (index != 0U) output += index % 4U == 0U ? QLatin1Char('\n') : QLatin1Char(' ');
    output += QStringLiteral("%1")
                  .arg(static_cast<unsigned int>(frame[index]), 2, 16, QLatin1Char('0'))
                  .toLower();
  }
  return output;
}

std::array<qint64, 6> TimingValues(const EncodeTimingSnapshot& timing) {
  return {timing.exact_input_ns,    timing.main_codec_ns,  timing.review_decode_ns,
          timing.result_mapping_ns, timing.hex_replace_ns, timing.first_repaint_total_ns};
}

void PrintPerformanceSummary(std::size_t document_index,
                             const std::vector<std::array<qint64, 6>>& samples, int warmup_count) {
  static const char* const phase_names[] = {"typed_materialization", "core_encode",
                                            "review_decode",         "result_materialization",
                                            "hex_replace",           "first_repaint_total"};
  for (std::size_t phase = 0U; phase < std::size(phase_names); ++phase) {
    std::vector<qint64> values;
    values.reserve(samples.size());
    for (const auto& sample : samples) values.push_back(sample[phase]);
    std::sort(values.begin(), values.end());
    const std::size_t median_index = (values.size() - 1U) / 2U;
    const std::size_t p95_index =
        static_cast<std::size_t>(std::ceil(static_cast<double>(values.size()) * 0.95)) - 1U;
    std::fprintf(stdout,
                 "UI_PERF document=%zu phase=%s warmup=%d samples=%zu min_ns=%lld "
                 "median_ns=%lld p95_ns=%lld max_ns=%lld\n",
                 document_index + 1U, phase_names[phase], warmup_count, values.size(),
                 static_cast<long long>(values.front()),
                 static_cast<long long>(values[median_index]),
                 static_cast<long long>(values[p95_index]), static_cast<long long>(values.back()));
  }
}

}  // namespace

ApplicationWindow::ApplicationWindow(QWidget* parent) : QMainWindow(parent) {
  setObjectName(QStringLiteral("paeLabMainWindow"));
  setWindowTitle(UiText("PAE 协议实验室"));
  resize(1280, 820);
  tabs_ = new QTabWidget(this);
  tabs_->setObjectName(QStringLiteral("documentTabs"));
  tabs_->setTabsClosable(true);
  tabs_->setMovable(false);
  setCentralWidget(tabs_);

  auto* file_menu = menuBar()->addMenu(UiText("文件(&F)"));
  file_menu->setObjectName(QStringLiteral("fileMenu"));
  new_action_ = file_menu->addAction(UiText("新建文档(&N)"));
  new_action_->setObjectName(QStringLiteral("newDocument"));
  open_action_ = file_menu->addAction(UiText("打开配置(&O)..."));
  open_action_->setObjectName(QStringLiteral("openConfiguration"));
  auto* quit_action = file_menu->addAction(UiText("退出(&X)"));
  quit_action->setObjectName(QStringLiteral("quitApplication"));

  connect(new_action_, &QAction::triggered, this, [this] { AddDocument(); });
  connect(open_action_, &QAction::triggered, this, [this] {
    const auto path =
        QFileDialog::getOpenFileName(this, UiText("打开 PAE 配置"), {},
                                     UiText("JSON 文件 (*.json);;所有文件 (*)"));
    if (!path.isEmpty()) {
      AddDocument(path);
    }
  });
  connect(quit_action, &QAction::triggered, this, &QWidget::close);
  connect(tabs_, &QTabWidget::tabCloseRequested, this, [this](int index) { CloseTab(index); });

  result_timer_ = new QTimer(this);
  result_timer_->setInterval(15);
  connect(result_timer_, &QTimer::timeout, this, [this] {
    PollCompileResults();
    if (smoke_running_) {
      AdvanceSmoke();
    }
  });
  result_timer_->start();
  statusBar()->setObjectName(QStringLiteral("globalStatus"));
  statusBar()->showMessage(UiText("仅离线分析：不执行通信，也不输出 Evidence"));
  AddDocument();
}

ApplicationWindow::~ApplicationWindow() {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("window_destructor_begin", this);
#endif
  for (int index = 0; index < tabs_->count(); ++index) {
    if (auto* document = dynamic_cast<DocumentTab*>(tabs_->widget(index))) {
      document->CloseDocument(false);
    }
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("window_destructor_end", this);
#endif
}

void ApplicationWindow::closeEvent(QCloseEvent* event) {
  for (int index = 0; index < tabs_->count(); ++index) {
    if (auto* document = dynamic_cast<DocumentTab*>(tabs_->widget(index));
        document != nullptr && !document->ConfirmClose()) {
      event->ignore();
      return;
    }
  }
  for (int index = 0; index < tabs_->count(); ++index) {
    if (auto* document = dynamic_cast<DocumentTab*>(tabs_->widget(index))) {
      document->CloseDocument(false);
    }
  }
  event->accept();
}

DocumentTab* ApplicationWindow::AddDocument(const QString& config_path) {
  if (tabs_->count() >= 2) {
    statusBar()->showMessage(UiText("最多只能同时打开两个文档"), 4000);
    return nullptr;
  }
  auto* document = new DocumentTab(next_document_id_++, worker_, tabs_);
  const int index = tabs_->addTab(document, UiText("未命名"));
  tabs_->setCurrentIndex(index);
  if (!config_path.isEmpty()) {
    document->LoadPath(config_path);
    tabs_->setTabText(index, document->Title());
  }
  new_action_->setEnabled(tabs_->count() < 2);
  open_action_->setEnabled(tabs_->count() < 2);
  return document;
}

int ApplicationWindow::DocumentCount() const noexcept { return tabs_->count(); }

void ApplicationWindow::StartUiSmoke(QStringList config_paths) {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  if (QApplication::arguments().contains(QStringLiteral("--ui-smoke"))) {
    ascii_smoke_diagnostic::Begin();
  }
#endif
  if (smoke_running_) {
    return;
  }
  if (config_paths.isEmpty() || config_paths.size() > 2) {
    FinishSmoke(false, QStringLiteral("--ui-smoke requires one or two config paths"));
    return;
  }
  while (tabs_->count() > 0) {
    CloseTab(0);
  }
  smoke_paths_ = std::move(config_paths);
  smoke_documents_.clear();
  smoke_snapshot_written_ = false;
  for (const auto& path : smoke_paths_) {
    auto* document = AddDocument(path);
    if (document == nullptr) {
      FinishSmoke(false, QStringLiteral("could not create smoke document"));
      return;
    }
    smoke_documents_.push_back(document);
  }
  smoke_running_ = true;
  performance_mode_ = false;
  smoke_drafts_populated_ = false;
  smoke_wait_ticks_ = 0;
}

void ApplicationWindow::StartUiPerformance(QStringList config_paths, int warmup_count,
                                           int sample_count) {
  if (warmup_count < 0 || sample_count <= 0) {
    FinishSmoke(false, QStringLiteral("performance counts must be non-negative/positive"));
    return;
  }
  StartUiSmoke(std::move(config_paths));
  if (!smoke_running_) return;
  performance_mode_ = true;
  performance_warmup_count_ = warmup_count;
  performance_sample_count_ = sample_count;
  performance_iteration_ = 0;
  performance_samples_.assign(smoke_documents_.size(), {});
}

void ApplicationWindow::CloseTab(int index) {
  auto* document = dynamic_cast<DocumentTab*>(tabs_->widget(index));
  if (document == nullptr) {
    return;
  }
  if (!document->CloseDocument()) return;
  tabs_->removeTab(index);
  document->deleteLater();
  new_action_->setEnabled(tabs_->count() < 2);
  open_action_->setEnabled(tabs_->count() < 2);
}

void ApplicationWindow::PollCompileResults() {
  for (const auto ticket : worker_.DrainReadyTickets()) {
    auto completion = worker_.TakeResult(ticket);
    if (completion == nullptr) {
      continue;
    }
    auto* document = FindDocument(completion->document_id);
    if (document != nullptr) {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
      ascii_smoke_diagnostic::Trace("compile_completion_before_publish", document);
#endif
      document->AcceptCompletion(std::move(completion));
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
      ascii_smoke_diagnostic::Trace("compile_completion_after_publish", document);
#endif
      const int index = tabs_->indexOf(document);
      if (index >= 0) {
        tabs_->setTabText(index, document->Title());
      }
    }
  }
}

DocumentTab* ApplicationWindow::FindDocument(DocumentId document_id) const noexcept {
  for (int index = 0; index < tabs_->count(); ++index) {
    auto* document = dynamic_cast<DocumentTab*>(tabs_->widget(index));
    if (document != nullptr && document->document_id() == document_id) {
      return document;
    }
  }
  return nullptr;
}

void ApplicationWindow::AdvanceSmoke() {
  ++smoke_wait_ticks_;
  if (smoke_wait_ticks_ > 4000) {
    FinishSmoke(false, QStringLiteral("compile timeout"));
    return;
  }
  for (auto* document : smoke_documents_) {
    if (document->state() == DocumentState::LOADING) {
      return;
    }
    if (document->state() != DocumentState::READY &&
        document->state() != DocumentState::PREVIEW_VALID) {
      FinishSmoke(false, QStringLiteral("load failed for %1").arg(document->ConfigPath()));
      return;
    }
  }

  QString error;
  if (!smoke_drafts_populated_) {
    for (std::size_t index = 0; index < smoke_documents_.size(); ++index) {
      if (!smoke_documents_[index]->VerifyLocalizationAnchorsForSmoke(error) ||
          !smoke_documents_[index]->PopulateCanonicalDraftsForSmoke(error)) {
        FinishSmoke(
            false,
            QStringLiteral("document %1: %2").arg(static_cast<qulonglong>(index + 1U)).arg(error));
        return;
      }
    }
    smoke_drafts_populated_ = true;
  }
  for (std::size_t index = 0; index < smoke_documents_.size(); ++index) {
    auto* document = smoke_documents_[index];
    tabs_->setCurrentWidget(document);
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
    if (document->IsBinaryHostForSmoke()) {
      if (performance_mode_ || !document->VerifyBinaryHostStage1ForSmoke(error)) {
        FinishSmoke(false, QStringLiteral("Binary Host document %1: %2")
                               .arg(static_cast<qulonglong>(index + 1U))
                               .arg(error));
        return;
      }
      if (!CaptureSmokeSnapshot(error)) {
        FinishSmoke(false, QStringLiteral("Binary Host snapshot: %1").arg(error));
        return;
      }
      std::fprintf(stdout, "UI_BINARY_HOST_SMOKE_DOCUMENT index=%zu fields=%d\n", index + 1U,
                   document->InspectFieldCountForSmoke());
      continue;
    }
#endif
    if (document->IsAsciiForSmoke()) {
      if (performance_mode_) {
        FinishSmoke(false, QStringLiteral("ASCII UI smoke is not a performance benchmark"));
        return;
      }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
      ascii_smoke_diagnostic::Trace("ascii_verify_before", document);
#endif
      const bool ascii_ok = document->IsAsciiStreamForSmoke()
                                ? document->VerifyAsciiStreamForSmoke(error)
                                : document->VerifyAsciiForSmoke(error);
#else
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
      ascii_smoke_diagnostic::Trace("ascii_verify_before", document);
#endif
      const bool ascii_ok = document->VerifyAsciiForSmoke(error);
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
      ascii_smoke_diagnostic::Trace("ascii_verify_after", document);
      if (ascii_smoke_diagnostic::EditorLost()) {
        FinishSmoke(false, QStringLiteral("ASCII_DIAG_EDITOR_LOST during ASCII verification"));
        return;
      }
#endif
      if (!ascii_ok) {
        FinishSmoke(false, QStringLiteral("ASCII document %1: %2")
                               .arg(static_cast<qulonglong>(index + 1U))
                               .arg(error));
        return;
      }
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
      ascii_smoke_diagnostic::Trace("host_verify_before", document);
#endif
      if (!document->VerifyHostForSmoke(error)) {
        FinishSmoke(false, QStringLiteral("Host observer: %1").arg(error));
        return;
      }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
      ascii_smoke_diagnostic::Trace("host_verify_after", document);
      if (ascii_smoke_diagnostic::EditorLost()) {
        FinishSmoke(false, QStringLiteral("ASCII_DIAG_EDITOR_LOST during Host verification"));
        return;
      }
#endif
#endif
      std::fprintf(stdout, "UI_ASCII_SMOKE_DOCUMENT index=%zu frame_bytes=%zu fields=%d\n",
                   index + 1U, document->PreviewFrameSizeForSmoke(),
                   document->InspectFieldCountForSmoke());
      continue;
    }
    if (!document->EncodeForSmoke(error) || document->PreviewFrameSizeForSmoke() == 0U ||
        !document->SelectFirstMappableFieldForSmoke(error) ||
        document->HighlightedCellCountForSmoke() == 0U) {
      FinishSmoke(
          false,
          QStringLiteral("document %1: %2").arg(static_cast<qulonglong>(index + 1U)).arg(error));
      return;
    }
    const auto expected = ExpectedPublicFixture(document->ConfigPath());
    if (expected.has_value() && !MatchesExpectation(*document, *expected)) {
      FinishSmoke(false, QStringLiteral("document %1: frame or exact highlight mask differs from "
                                        "the independent public-fixture expectation")
                             .arg(static_cast<qulonglong>(index + 1U)));
      return;
    }
    const std::size_t verified_frame_bytes = document->PreviewFrameSizeForSmoke();
    const std::size_t verified_highlighted_cells = document->HighlightedCellCountForSmoke();
    if (!performance_mode_ && expected.has_value()) {
      if (!document->InspectTextForSmoke(SpacedLowerHex(expected->frame), error) ||
          document->InspectMatchedMessageForSmoke().isEmpty() ||
          document->InspectFieldCountForSmoke() <= 0 ||
          document->InspectRawValueForSmoke(0).isEmpty() ||
          document->InspectLogicalValueForSmoke(0).isEmpty() ||
          !document->SelectFirstMappableFieldForSmoke(error) ||
          document->HighlightedCellCountForSmoke() == 0U ||
          !MatchesExpectation(*document, *expected)) {
        FinishSmoke(false,
                    QStringLiteral("document %1: Inspect complete-record result, values or exact "
                                   "highlight mask differs from independent expectation: %2")
                        .arg(static_cast<qulonglong>(index + 1U))
                        .arg(error));
        return;
      }
      if (!document->VerifyInspectFailureForSmoke(QStringLiteral("AA:"), InspectFailureStage::INPUT,
                                                  QString{}, 2U, QString{}, error) ||
          document->PreviewFrameSizeForSmoke() != 0U ||
          !document->InspectTextForSmoke(SpacedLowerHex(expected->frame), error)) {
        FinishSmoke(false, QStringLiteral("document %1: Inspect lexical failure offset or recovery "
                                          "differs: %2")
                               .arg(static_cast<qulonglong>(index + 1U))
                               .arg(error));
        return;
      }
      const QString fixture_name = QFileInfo(document->ConfigPath()).fileName();
      if (fixture_name == QStringLiteral("synthetic_ui_v05.pae.json") &&
          (!document->VerifyInspectFailureForSmoke(
               QStringLiteral("AA"), InspectFailureStage::STRUCTURAL_QUERY,
               QStringLiteral("UNKNOWN_MESSAGE"), std::nullopt, QString{}, error) ||
           document->PreviewFrameSizeForSmoke() != 1U)) {
        FinishSmoke(false,
                    QStringLiteral("document %1: structural failure presentation differs: %2")
                        .arg(static_cast<qulonglong>(index + 1U))
                        .arg(error));
        return;
      }
      if (fixture_name == QStringLiteral("synthetic_ui_v05.pae.json") &&
          !document->VerifyPipelineSwitchClearsInspectForSmoke(error)) {
        FinishSmoke(false, QStringLiteral("document %1: Pipeline switch cleanup differs: %2")
                               .arg(static_cast<qulonglong>(index + 1U))
                               .arg(error));
        return;
      }
      if (fixture_name == QStringLiteral("synthetic_ui_v06.pae.json")) {
        auto bad_crc = expected->frame;
        bad_crc[9] ^= 0x01U;
        if (!document->VerifyInspectFailureForSmoke(
                SpacedLowerHex(bad_crc), InspectFailureStage::CODEC,
                QStringLiteral("INTEGRITY_FAILED"), std::nullopt, QString{}, error) ||
            document->HighlightedCellCountForSmoke() != 2U ||
            document->HighlightMaskForSmoke(9U) != 0xFFU ||
            document->HighlightMaskForSmoke(10U) != 0xFFU) {
          FinishSmoke(false, QStringLiteral("document %1: integrity failure region differs: %2")
                                 .arg(static_cast<qulonglong>(index + 1U))
                                 .arg(error));
          return;
        }
        if (!document->SelectFirstMappableFieldForSmoke(error) ||
            document->HighlightedCellCountForSmoke() != 2U ||
            document->HighlightMaskForSmoke(9U) != 0xFFU ||
            document->HighlightMaskForSmoke(10U) != 0xFFU) {
          if (error.isEmpty()) {
            error = QStringLiteral("integrity storage highlight changed after field selection");
          }
          FinishSmoke(false,
                      QStringLiteral("document %1: integrity failure interaction differs: %2")
                          .arg(static_cast<qulonglong>(index + 1U))
                          .arg(error));
          return;
        }
      }
      if (fixture_name == QStringLiteral("synthetic_ui_v07.pae.json") &&
          (!document->VerifyInspectFailureForSmoke(QStringLiteral("AA 00 05 00 00 55"),
                                                   InspectFailureStage::CODEC,
                                                   QStringLiteral("LENGTH_MISMATCH"), std::nullopt,
                                                   QStringLiteral("record_length"), error) ||
           document->HighlightedCellCountForSmoke() != 2U ||
           document->HighlightMaskForSmoke(1U) != 0xFFU ||
           document->HighlightMaskForSmoke(2U) != 0xFFU)) {
        FinishSmoke(false, QStringLiteral("document %1: field-level length failure differs: %2")
                               .arg(static_cast<qulonglong>(index + 1U))
                               .arg(error));
        return;
      }
      if (fixture_name == QStringLiteral("synthetic_ui_v08.pae.json") &&
          !document->VerifyBoundedV08ForSmoke(error)) {
        FinishSmoke(false, QStringLiteral("document %1: bounded Schema 0.8 UI state differs: %2")
                               .arg(static_cast<qulonglong>(index + 1U))
                               .arg(error));
        return;
      }
    }
    const auto timing = document->LastTiming();
    if (performance_mode_) {
      if (performance_iteration_ >= performance_warmup_count_) {
        performance_samples_[index].push_back(TimingValues(timing));
      }
    } else {
      std::fprintf(stdout,
                   "UI_SMOKE_DOCUMENT index=%zu frame_bytes=%zu highlighted_cells=%zu "
                   "input_ns=%lld core_ns=%lld review_ns=%lld map_ns=%lld hex_ns=%lld "
                   "repaint_total_ns=%lld\n",
                   index + 1U, verified_frame_bytes, verified_highlighted_cells,
                   static_cast<long long>(timing.exact_input_ns),
                   static_cast<long long>(timing.main_codec_ns),
                   static_cast<long long>(timing.review_decode_ns),
                   static_cast<long long>(timing.result_mapping_ns),
                   static_cast<long long>(timing.hex_replace_ns),
                   static_cast<long long>(timing.first_repaint_total_ns));
    }
  }
  if (performance_mode_ &&
      ++performance_iteration_ < performance_warmup_count_ + performance_sample_count_) {
    return;
  }
  if (performance_mode_) {
    for (std::size_t index = 0U; index < performance_samples_.size(); ++index) {
      PrintPerformanceSummary(index, performance_samples_[index], performance_warmup_count_);
    }
  }
  if (!performance_mode_) {
    for (std::size_t index = 0; index < smoke_documents_.size(); ++index) {
      if (smoke_documents_[index]->IsAsciiForSmoke()
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
          || smoke_documents_[index]->IsBinaryHostForSmoke()
#endif
      )
        continue;
      if (!smoke_documents_[index]->VerifyInvalidDraftRetentionForSmoke(error)) {
        FinishSmoke(
            false,
            QStringLiteral("document %1: %2").arg(static_cast<qulonglong>(index + 1U)).arg(error));
        return;
      }
    }
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  if (!performance_mode_ && smoke_documents_.size() == 2U &&
      smoke_documents_[0]->IsAsciiStreamForSmoke() &&
      smoke_documents_[1]->IsAsciiStreamForSmoke()) {
    if (!smoke_documents_[0]->PrepareStreamHalfFrameForSmoke(error) ||
        !smoke_documents_[1]->PrepareStreamHalfFrameForSmoke(error)) {
      FinishSmoke(false, QStringLiteral("close confirmation setup failed: %1").arg(error));
      return;
    }
    const QString first_before = smoke_documents_[0]->StreamStateSignatureForSmoke();
    const QString second_before = smoke_documents_[1]->StreamStateSignatureForSmoke();
    QTimer::singleShot(0, qApp, [] {
      if (auto* first = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        first->done(QMessageBox::Yes);
      }
      QTimer::singleShot(0, qApp, [] {
        if (auto* second = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
          second->done(QMessageBox::No);
        }
      });
    });
    const bool closed = close();
    if (closed || !isVisible() ||
        smoke_documents_[0]->StreamStateSignatureForSmoke() != first_before ||
        smoke_documents_[1]->StreamStateSignatureForSmoke() != second_before) {
      FinishSmoke(false,
                  QStringLiteral("two-Tab close Yes-then-No mutated state or closed the window"));
      return;
    }
    smoke_documents_[0]->ResetStreamForSmoke();
    smoke_documents_[1]->ResetStreamForSmoke();
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  if (!performance_mode_ && smoke_documents_.size() == 2U &&
      smoke_documents_[0]->IsBinaryHostForSmoke() && smoke_documents_[1]->IsBinaryHostForSmoke()) {
    const QString first_before = smoke_documents_[0]->BinaryStateSignatureForSmoke();
    const QString second_before = smoke_documents_[1]->BinaryStateSignatureForSmoke();
    QTimer::singleShot(0, qApp, [] {
      if (auto* first = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
        first->done(QMessageBox::Yes);
      QTimer::singleShot(0, qApp, [] {
        if (auto* second = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
          second->done(QMessageBox::No);
      });
    });
    const bool closed = close();
    if (closed || !isVisible() ||
        smoke_documents_[0]->BinaryStateSignatureForSmoke() != first_before ||
        smoke_documents_[1]->BinaryStateSignatureForSmoke() != second_before) {
      FinishSmoke(false,
                  QStringLiteral("Binary two-Tab close Yes-then-No mutated state or closed"));
      return;
    }
  }
  if (!performance_mode_) {
    for (auto* document : smoke_documents_) {
      if (document->IsBinaryHostForSmoke() && !document->VerifyBinaryReloadFailureForSmoke(error)) {
        FinishSmoke(false, QStringLiteral("Binary reload failure semantics differ: %1").arg(error));
        return;
      }
    }
  }
#endif
  for (auto* document : smoke_documents_) {
    document->InvalidatePreviewForSmoke();
    if (document->PreviewFrameSizeForSmoke() != 0U) {
      FinishSmoke(false, QStringLiteral("input invalidation retained stale preview"));
      return;
    }
  }
  FinishSmoke(true, QStringLiteral("%1 document(s)").arg(smoke_documents_.size()));
}

bool ApplicationWindow::CaptureSmokeSnapshot(QString& error) {
  if (smoke_snapshot_written_) return true;
  const QString path = qEnvironmentVariable("PAE_LAB_UI_SNAPSHOT_PATH");
  if (path.isEmpty()) return true;
  bool width_ok = false;
  bool height_ok = false;
  const int width = qEnvironmentVariableIntValue("PAE_LAB_UI_SNAPSHOT_WIDTH", &width_ok);
  const int height = qEnvironmentVariableIntValue("PAE_LAB_UI_SNAPSHOT_HEIGHT", &height_ok);
  if (!width_ok || !height_ok || width < 960 || height < 640) {
    error = QStringLiteral("snapshot dimensions must be integers at least 960x640");
    return false;
  }
  const auto log_size_hints = [this](const char* phase) {
    const auto print_widget = [phase](const QWidget* widget, const char* name) {
      if (widget == nullptr) return;
      const QSize minimum_hint = widget->minimumSizeHint();
      const QSize preferred_hint = widget->sizeHint();
      std::fprintf(stdout,
                   "UI_SNAPSHOT_SIZE_HINT phase=%s widget=%s minimum=%dx%d "
                   "minimum_hint=%dx%d size_hint=%dx%d actual=%dx%d\n",
                   phase, name, widget->minimumWidth(), widget->minimumHeight(),
                   minimum_hint.width(), minimum_hint.height(), preferred_hint.width(),
                   preferred_hint.height(), widget->width(), widget->height());
    };
    print_widget(this, "window");
    print_widget(centralWidget(), "central");
    for (const char* name : {"documentConfigGroup", "workbenchSplitter", "controlTabs",
                             "operationGroup", "hostActiveContext", "resultWorkspace",
                             "resultWorkspaceSplitter", "fieldTable"})
      print_widget(findChild<QWidget*>(QLatin1String(name)), name);
  };
  log_size_hints("before_resize");
  resize(width, height);
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  log_size_hints("after_resize");
  if (size() != QSize(width, height)) {
    error = QStringLiteral("snapshot size mismatch: requested=%1x%2 actual=%3x%4")
                .arg(width)
                .arg(height)
                .arg(this->width())
                .arg(this->height());
    return false;
  }
  QPixmap image(size());
  image.fill(Qt::transparent);
  render(&image);
  if (!image.save(path, "PNG")) {
    error = QStringLiteral("could not save %1").arg(path);
    return false;
  }
  smoke_snapshot_written_ = true;
  const auto path_utf8 = path.toUtf8();
  std::fprintf(stdout,
               "UI_SNAPSHOT_PASS path=%s requested=%dx%d actual=%dx%d\n",
               path_utf8.constData(), width, height, this->width(), this->height());
  std::fflush(stdout);
  return true;
}

void ApplicationWindow::FinishSmoke(bool success, const QString& detail) {
  smoke_running_ = false;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace(success ? "smoke_finish_pass" : "smoke_finish_fail", this);
#endif
  const auto utf8 = detail.toUtf8();
  std::fprintf(success ? stdout : stderr, "%s detail=%s\n",
               success ? "UI_SMOKE_PASS" : "UI_SMOKE_FAIL", utf8.constData());
  std::fflush(success ? stdout : stderr);
  QTimer::singleShot(0, qApp, [success] {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
    ascii_smoke_diagnostic::Trace("smoke_exit_event");
#endif
    qApp->exit(success ? 0 : 2);
  });
}

}  // namespace pae::protocol_lab_ui
