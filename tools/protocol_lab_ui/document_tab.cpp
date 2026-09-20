#include "document_tab.h"

#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY) && \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2) && \
    defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
#include "ascii_host_adapter_compat.h"
#endif

#include <QApplication>
#include <QAbstractButton>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTableView>
#include <QTabWidget>
#include <QTemporaryFile>
#if defined(PAE_BUILD_PROTOCOL_LAB_BINDING_UI)
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#endif
#include <QTextBrowser>
#include <QTextCursor>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>
#include <new>
#include <optional>
#include <utility>

#include "exact_value_delegate.h"
#include "field_table_model.h"
#include "hex_view.h"
#include "smoke_editor_target.h"
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
#include "ascii_smoke_diagnostic.h"
#endif

namespace pae::protocol_lab_ui {
namespace {

QString UiText(const char* text) { return QCoreApplication::translate("PaeLabUi", text); }

QMessageBox::StandardButton AskChineseQuestion(QWidget* parent, const QString& title,
                                               const QString& text) {
  QMessageBox box(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::No, parent);
  box.setDefaultButton(QMessageBox::No);
  if (auto* yes = box.button(QMessageBox::Yes)) yes->setText(UiText("是"));
  if (auto* no = box.button(QMessageBox::No)) no->setText(UiText("否"));
  return static_cast<QMessageBox::StandardButton>(box.exec());
}

#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
bool IsDecodeHostAction(
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
    AsciiHostAction action
#else
    host_endpoint::Action action
#endif
) noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  return action == AsciiHostAction::DECODE;
#else
  return action == host_endpoint::Action::DECODE;
#endif
}
#endif

QString FromUtf8(const std::string& value) {
  return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

QString HtmlPreservingLines(const std::string& value) {
  QString escaped = FromUtf8(value).toHtmlEscaped();
  escaped.replace(QStringLiteral("\r\n"), QStringLiteral("<br/>"));
  escaped.replace(QLatin1Char('\r'), QStringLiteral("<br/>"));
  escaped.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
  return escaped;
}

std::string Utf8(const QString& value) {
  const auto bytes = value.toUtf8();
  return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

std::u16string Utf16(const QString& value) {
  const auto* begin = reinterpret_cast<const char16_t*>(value.utf16());
  return std::u16string(begin, begin + value.size());
}

QString FromUtf16(const std::u16string& value) {
  return QString::fromUtf16(reinterpret_cast<const ushort*>(value.data()),
                            static_cast<int>(value.size()));
}

#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
QString StreamPhaseName(StreamFramingPhase value) {
  switch (value) {
    case StreamFramingPhase::COLLECTING:
      return QStringLiteral("COLLECTING");
    case StreamFramingPhase::DELIVERY_PENDING:
      return QStringLiteral("DELIVERY_PENDING");
    case StreamFramingPhase::DISCARDING_UNTIL_CRLF:
      return QStringLiteral("DISCARDING_UNTIL_CRLF");
  }
  return QStringLiteral("UNKNOWN");
}

QString StopReasonName(StreamFramerStopReason value) {
  switch (value) {
    case StreamFramerStopReason::INPUT_EXHAUSTED:
      return QStringLiteral("INPUT_EXHAUSTED");
    case StreamFramerStopReason::NEED_MORE:
      return QStringLiteral("NEED_MORE");
    case StreamFramerStopReason::WORK_BUDGET_REACHED:
      return QStringLiteral("WORK_BUDGET_REACHED");
    case StreamFramerStopReason::SINK_STOP:
      return QStringLiteral("SINK_STOP");
  }
  return QStringLiteral("UNKNOWN");
}

QString SubmitApiStatusName(StreamFramerStatus value) {
  switch (value) {
    case StreamFramerStatus::OK:
      return QStringLiteral("OK");
    case StreamFramerStatus::INVALID_ARGUMENT:
      return QStringLiteral("INVALID_ARGUMENT");
    case StreamFramerStatus::INVALID_COMPILED_PROTOCOL:
      return QStringLiteral("INVALID_COMPILED_PROTOCOL");
    case StreamFramerStatus::PIPELINE_OUT_OF_RANGE:
      return QStringLiteral("PIPELINE_OUT_OF_RANGE");
    case StreamFramerStatus::INPUT_KIND_NOT_STREAM:
      return QStringLiteral("INPUT_KIND_NOT_STREAM");
    case StreamFramerStatus::RESOURCE_LIMIT_EXCEEDED:
      return QStringLiteral("RESOURCE_LIMIT_EXCEEDED");
    case StreamFramerStatus::ALLOCATION_FAILED:
      return QStringLiteral("ALLOCATION_FAILED");
    case StreamFramerStatus::WORKSPACE_BUSY:
      return QStringLiteral("WORKSPACE_BUSY");
    case StreamFramerStatus::REENTRANT_CALL:
      return QStringLiteral("REENTRANT_CALL");
    case StreamFramerStatus::INTERNAL_ERROR:
      return QStringLiteral("INTERNAL_ERROR");
  }
  return QStringLiteral("UNKNOWN");
}

QString FramingIssueName(StreamFramingIssue value) {
  switch (value) {
    case StreamFramingIssue::NONE:
      return QStringLiteral("NONE");
    case StreamFramingIssue::MALFORMED_LENGTH:
      return QStringLiteral("MALFORMED_LENGTH");
    case StreamFramingIssue::RECORD_TOO_LONG:
      return QStringLiteral("RECORD_TOO_LONG");
  }
  return QStringLiteral("UNKNOWN");
}
#endif

#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
QString StreamPhaseName(StreamRuntimePhase value) {
  switch (value) {
    case StreamRuntimePhase::COLLECTING: return QStringLiteral("COLLECTING");
    case StreamRuntimePhase::DELIVERY_PENDING: return QStringLiteral("DELIVERY_PENDING");
    case StreamRuntimePhase::DISCARDING_UNTIL_CRLF:
      return QStringLiteral("DISCARDING_UNTIL_CRLF");
  }
  return QStringLiteral("UNKNOWN");
}
#endif

void FillFieldHighlights(const MessageDescriptor* message, const FieldDescriptor* field,
                         std::optional<std::size_t> actual_frame_size,
                         std::vector<PhysicalBitMask>& output) {
  output.clear();
  if (message == nullptr || field == nullptr) {
    return;
  }
  if (!field->physical_bits.empty()) {
    output.assign(field->physical_bits.begin(), field->physical_bits.end());
    return;
  }
  auto range = field->byte_range;
  if (field->byte_length_bounds.has_value()) {
    range = actual_frame_size.has_value()
                ? ResolveActualFieldRange(*message, *field, *actual_frame_size)
                : std::nullopt;
  }
  if (range.has_value()) {
    output.reserve(range->length);
    for (std::size_t index = 0; index < range->length; ++index) {
      output.push_back(PhysicalBitMask{range->offset + index, 0xFFU});
    }
  }
}

std::vector<PhysicalBitMask> FieldHighlights(const MessageDescriptor* message,
                                             const FieldDescriptor* field,
                                             std::optional<std::size_t> actual_frame_size) {
  std::vector<PhysicalBitMask> output;
  FillFieldHighlights(message, field, actual_frame_size, output);
  return output;
}

std::vector<PhysicalBitMask> ActualFieldHighlights(const std::vector<UiFieldResult>& results,
                                                   const FieldDescriptor* field) {
  if (field == nullptr) return {};
  const auto found = std::find_if(results.begin(), results.end(), [&](const auto& result) {
    return result.field_index == field->field_index && result.id == field->id;
  });
  if (found == results.end() || !found->actual_range.has_value() ||
      found->actual_range->length == 0U) {
    return {};
  }
  std::vector<PhysicalBitMask> highlights;
  highlights.reserve(found->actual_range->length);
  for (std::size_t index = 0U; index < found->actual_range->length; ++index) {
    highlights.push_back({found->actual_range->offset + index, 0xFFU});
  }
  return highlights;
}

std::optional<int> InspectEditorCapacity(std::size_t maximum_bytes,
                                         ByteRepresentation representation) {
  const std::size_t multiplier = representation == ByteRepresentation::ASCII_ESCAPED ? 4U : 3U;
  const std::size_t headroom = representation == ByteRepresentation::ASCII_ESCAPED ? 4U : 1U;
  if (maximum_bytes > ((std::numeric_limits<std::size_t>::max)() - headroom) / multiplier) {
    return std::nullopt;
  }
  const std::size_t capacity = maximum_bytes * multiplier + headroom;
  if (capacity > static_cast<std::size_t>((std::numeric_limits<int>::max)())) return std::nullopt;
  return static_cast<int>(capacity);
}

QString TimingText(const EncodeTimingSnapshot& timing) {
  auto milliseconds = [](qint64 nanoseconds) {
    return static_cast<double>(nanoseconds) / 1000000.0;
  };
  return QStringLiteral(
             "input %1 ms | core %2 ms | review %3 ms | map %4 ms | hex %5 ms | "
             "repaint-total %6 ms")
      .arg(milliseconds(timing.exact_input_ns), 0, 'f', 3)
      .arg(milliseconds(timing.main_codec_ns), 0, 'f', 3)
      .arg(milliseconds(timing.review_decode_ns), 0, 'f', 3)
      .arg(milliseconds(timing.result_mapping_ns), 0, 'f', 3)
      .arg(milliseconds(timing.hex_replace_ns), 0, 'f', 3)
      .arg(milliseconds(timing.first_repaint_total_ns), 0, 'f', 3);
}

class TimingObserver final : public LabExecutionObserver {
 public:
  explicit TimingObserver(EncodeTimingSnapshot& output) : output_(output) {}

  void PhaseStarted(LabExecutionPhase phase) override {
    active_phase_ = phase;
    timer_.restart();
  }

  void PhaseFinished(LabExecutionPhase phase, std::string_view) override {
    if (!active_phase_.has_value() || *active_phase_ != phase) {
      return;
    }
    const auto elapsed = timer_.nsecsElapsed();
    switch (phase) {
      case LabExecutionPhase::MAIN_CODEC:
        output_.main_codec_ns += elapsed;
        break;
      case LabExecutionPhase::REVIEW_DECODE:
        output_.review_decode_ns += elapsed;
        break;
      case LabExecutionPhase::RESULT_MAPPING:
        output_.result_mapping_ns += elapsed;
        break;
      default:
        break;
    }
    active_phase_.reset();
  }

 private:
  EncodeTimingSnapshot& output_;
  QElapsedTimer timer_;
  std::optional<LabExecutionPhase> active_phase_;
};

class QtInputMaterializationTimer final : public InputMaterializationTimer {
 public:
  void Start() noexcept override { timer_.start(); }
  std::int64_t ElapsedNanoseconds() const noexcept override { return timer_.nsecsElapsed(); }

 private:
  QElapsedTimer timer_;
};

class ClipboardMimeGuard final {
 public:
  ClipboardMimeGuard() {
    const auto* original = QApplication::clipboard()->mimeData();
    if (original == nullptr) return;
    for (const auto& format : original->formats()) {
      original_data_.push_back({format, original->data(format)});
    }
  }
  ~ClipboardMimeGuard() {
    auto* restored = new QMimeData;
    for (const auto& item : original_data_) restored->setData(item.first, item.second);
    QApplication::clipboard()->setMimeData(restored);
  }

 private:
  std::vector<std::pair<QString, QByteArray>> original_data_;
};

bool ReplaceEditorTextByKeyboard(QLineEdit& editor, const QString& text) {
  QPointer<QLineEdit> guarded(&editor);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  const auto editor_id = ascii_smoke_diagnostic::EditorId(guarded.data());
  ascii_smoke_diagnostic::Trace("keyboard_before_select", guarded.data(), editor_id);
#endif
  guarded->selectAll();
  for (const QChar character : text) {
    if (!guarded) {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
      ascii_smoke_diagnostic::MarkEditorLost("keyboard_editor_lost", editor_id);
#endif
      return false;
    }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
    ascii_smoke_diagnostic::Trace("keyboard_before_key", guarded.data(), editor_id);
#endif
    QKeyEvent event{QEvent::KeyPress, static_cast<int>(character.unicode()), Qt::NoModifier,
                    QString{character}};
    if (!smoke_editor_target::SendIfLive(guarded, event)) {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
      ascii_smoke_diagnostic::MarkEditorLost("keyboard_editor_lost_after_key", editor_id);
#endif
      return false;
    }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
    ascii_smoke_diagnostic::Trace("keyboard_after_key", guarded.data(), editor_id);
#endif
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("keyboard_before_pump", guarded.data(), editor_id);
#endif
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("keyboard_after_pump", guarded.data(), editor_id);
  if (!guarded) ascii_smoke_diagnostic::MarkEditorLost("keyboard_editor_lost", editor_id);
#endif
  return guarded != nullptr;
}

bool ReplaceEditorTextByPaste(QLineEdit& editor, const QString& text) {
  QPointer<QLineEdit> guarded(&editor);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  const auto editor_id = ascii_smoke_diagnostic::EditorId(guarded.data());
  ascii_smoke_diagnostic::Trace("paste_before_clipboard", guarded.data(), editor_id);
#endif
  QApplication::clipboard()->setText(text);
  if (!guarded) return false;
  guarded->selectAll();
  if (!guarded) return false;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("paste_before_key", guarded.data(), editor_id);
#endif
  QKeyEvent event{QEvent::KeyPress, Qt::Key_V, Qt::ControlModifier};
  if (!smoke_editor_target::SendIfLive(guarded, event)) {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
    ascii_smoke_diagnostic::MarkEditorLost("paste_editor_lost_after_key", editor_id);
#endif
    return false;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("paste_after_key", guarded.data(), editor_id);
  ascii_smoke_diagnostic::Trace("paste_before_pump", guarded.data(), editor_id);
#endif
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("paste_after_pump", guarded.data(), editor_id);
  if (!guarded) ascii_smoke_diagnostic::MarkEditorLost("paste_editor_lost", editor_id);
#endif
  return guarded != nullptr;
}

void CommitEditorByKey(QLineEdit& editor, int key) {
  QPointer<QLineEdit> guarded(&editor);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  const auto editor_id = ascii_smoke_diagnostic::EditorId(guarded.data());
  ascii_smoke_diagnostic::Trace("commit_before_press", guarded.data(), editor_id);
#endif
  QKeyEvent press{QEvent::KeyPress, key, Qt::NoModifier};
  smoke_editor_target::SendIfLive(guarded, press);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("commit_after_press", guarded.data(), editor_id);
#endif
  // A successful delegate commit may close its editor during KeyPress.
  if (guarded) {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
    ascii_smoke_diagnostic::Trace("commit_before_release", guarded.data(), editor_id);
#endif
    QKeyEvent release{QEvent::KeyRelease, key, Qt::NoModifier};
    smoke_editor_target::SendIfLive(guarded, release);
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("commit_after_release", guarded.data(), editor_id);
  ascii_smoke_diagnostic::Trace("commit_before_pump", guarded.data(), editor_id);
#endif
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("commit_after_pump", guarded.data(), editor_id);
#endif
  QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("commit_after_deferred_delete", guarded.data(), editor_id);
#endif
  // Callers verify the model/result; editor closure itself is permitted.
}

}  // namespace

DocumentTab::DocumentTab(DocumentId document_id, CompileWorker& worker, QWidget* parent)
    : QWidget(parent), worker_(worker), session_(document_id) {
  BuildUi();
  RefreshState();
}

DocumentTab::~DocumentTab() { CloseDocument(false); }

QString DocumentTab::ConfigPath() const { return path_edit_->text(); }

QString DocumentTab::Title() const {
  const QFileInfo info(path_edit_->text());
  return info.fileName().isEmpty() ? QStringLiteral("Untitled") : info.fileName();
}

void DocumentTab::LoadPath(const QString& path) {
  path_edit_->setText(path);
  BeginLoadFromPath();
}

void DocumentTab::AcceptCompletion(std::unique_ptr<CompileCompletion> completion) {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("document_completion_enter", this);
#endif
  if (closed_ || completion == nullptr || completion->document_id != session_.id()) {
    return;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_BINDING_UI)
  if (host_pending_revision_ && completion->load_revision == *host_pending_revision_) {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
    ascii_smoke_diagnostic::Trace("host_completion_before_publish", this);
#endif
    AcceptHostCompletion(std::move(completion));
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
    ascii_smoke_diagnostic::Trace("host_completion_after_publish", this);
#endif
    return;
  }
  if (completion->load_revision != session_.load_revision()) return;
#endif
  const bool published = session_.ApplyCompileCompletion(std::move(completion));
  ResetVisibleDocument();
  if (published) {
    RebuildSelectorsAndModel();
#if defined(PAE_BUILD_PROTOCOL_LAB_BINDING_UI)
    InitializeHostDraft();
#endif
  }
  RefreshState();
}

bool DocumentTab::CloseDocument(bool require_confirmation) {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("document_close_enter", this);
#endif
  if (closed_) {
    return true;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  const bool stream_discardable = session_.StreamHasDiscardableState();
  if (require_confirmation && stream_discardable &&
      !ConfirmStreamDiscardOnly(UiText("关闭此文档"))) {
    return false;
  }
#else
  constexpr bool stream_discardable = false;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI) && defined(PAE_BUILD_PROTOCOL_LAB_BINDING_UI)
  if (require_confirmation && ((!stream_discardable && session_.BinaryHasDiscardableState()) ||
                               (session_.IsBinaryHostDocument() && host_pending_revision_))) {
    const auto answer = AskChineseQuestion(
        this, UiText("关闭 Binary Host 文档"),
        UiText("关闭将丢弃 Binary 流草稿、结果或等待中的准备任务。是否继续？"));
    if (answer != QMessageBox::Yes) return false;
  }
#endif
  closed_ = true;
  worker_.CloseDocument(session_.id());
  field_model_->Reset(nullptr, {});
  hex_view_->ClearFrame();
  session_.Close();
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("document_close_end", this);
#endif
  return true;
}

bool DocumentTab::ConfirmClose() {
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  const bool stream_discardable = session_.StreamHasDiscardableState();
  if (stream_discardable && !ConfirmStreamDiscardOnly(UiText("关闭应用程序"))) return false;
#else
  constexpr bool stream_discardable = false;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI) && defined(PAE_BUILD_PROTOCOL_LAB_BINDING_UI)
  if ((!stream_discardable && session_.BinaryHasDiscardableState()) ||
      (session_.IsBinaryHostDocument() && host_pending_revision_)) {
    const auto answer = AskChineseQuestion(
        this, UiText("关闭 Binary Host 文档"),
        UiText("关闭将丢弃 Binary 流草稿、结果或等待中的准备任务。是否继续？"));
    if (answer != QMessageBox::Yes) return false;
  }
#endif
  return true;
}

bool DocumentTab::PopulateCanonicalDraftsForSmoke(QString& error) {
  const auto* message = CurrentMessage();
  if (message == nullptr) {
    error = QStringLiteral("no selected message");
    return false;
  }
  for (int row = 0; row < field_model_->rowCount(); ++row) {
    const auto* field = field_model_->FieldAt(row);
    if (field == nullptr || field->encode_source != FieldEncodeSource::INPUT) {
      continue;
    }
    const auto index = field_model_->index(row, FieldTableModel::VALUE);
    QVariant value;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
    if (field->decimal_conversion) {
      value = QStringLiteral("0@0");
    } else
#endif
        if (field->value_type == FieldValueType::UINT64 ||
            field->value_type == FieldValueType::INT64) {
      value = QStringLiteral("0");
    } else if (field->value_type == FieldValueType::BYTES) {
      const std::size_t byte_count = field->ascii_text && field->byte_length_bounds.has_value()
                                         ? field->byte_length_bounds->minimum
                                         : field->byte_width;
      value = field->ascii_text ? QStringLiteral("41").repeated(static_cast<int>(byte_count))
                                : QString(static_cast<int>(byte_count * 2U), QLatin1Char('0'));
    } else if (field->value_type == FieldValueType::ENUM) {
      if (field->enum_entries.empty()) {
        error = QStringLiteral("enum field has no configured entries");
        return false;
      }
      value = 0;
    } else if (field->value_type == FieldValueType::BOOL) {
      if (!field_model_->setData(index, Qt::Unchecked, Qt::CheckStateRole)) {
        error = field_model_->ValidationError(row);
        return false;
      }
      continue;
    }
    if (!field_model_->setData(index, value, Qt::EditRole)) {
      error = field_model_->ValidationError(row);
      return false;
    }
  }
  return true;
}

bool DocumentTab::EncodeForSmoke(QString& error) {
  mode_combo_->setCurrentIndex(0);
  EncodeCurrent();
  if (session_.state() != DocumentState::PREVIEW_VALID) {
    error = FromUtf8(session_.diagnostic_id()) + QStringLiteral(": ") +
            FromUtf8(session_.diagnostic_detail());
    return false;
  }
  return true;
}

bool DocumentTab::InspectTextForSmoke(const QString& text, QString& error) {
  mode_combo_->setCurrentIndex(1);
  inspect_input_->setPlainText(text);
  InspectCurrent();
  if (!session_.inspect_result().has_value()) {
    error = FromUtf8(session_.diagnostic_id()) + QStringLiteral(": ") +
            FromUtf8(session_.diagnostic_detail());
    return false;
  }
  return true;
}

bool DocumentTab::VerifyInspectFailureForSmoke(const QString& text, InspectFailureStage stage,
                                               const QString& status,
                                               std::optional<std::size_t> input_offset,
                                               const QString& failed_field_id, QString& error) {
  mode_combo_->setCurrentIndex(1);
  inspect_input_->setPlainText(text);
  InspectCurrent();
  if (!session_.inspect_failure().has_value() || session_.inspect_result().has_value()) {
    error = QStringLiteral("Inspect did not publish the expected failure-only state");
    return false;
  }
  const auto& failure = *session_.inspect_failure();
  if (failure.stage != stage || FromUtf8(failure.status) != status ||
      failure.input_offset != input_offset ||
      (failed_field_id.isEmpty() ? failure.failed_field_id.has_value()
                                 : (!failure.failed_field_id.has_value() ||
                                    FromUtf8(*failure.failed_field_id) != failed_field_id))) {
    error = QStringLiteral("Inspect failure stage/status/offset/field identity differs");
    return false;
  }
  if (result_kind_label_->property("paeResultState").toString() !=
      QStringLiteral("failure")) {
    error = QStringLiteral("Inspect failure lost stable result state");
    return false;
  }
  if (!diagnostic_label_->text().contains(UiText("操作失败")) ||
      (!session_.diagnostic_id().empty() &&
       !diagnostic_label_->text().contains(FromUtf8(session_.diagnostic_id()))) ||
      (!session_.diagnostic_detail().empty() &&
       !diagnostic_label_->text().contains(FromUtf8(session_.diagnostic_detail())))) {
    error = QStringLiteral("localized diagnostic summary, stable code, or technical detail missing");
    return false;
  }
  if (failed_field_id.isEmpty() && (field_table_->currentIndex().isValid() ||
                                    !field_table_->selectionModel()->selectedRows().isEmpty() ||
                                    !details_view_->toPlainText().trimmed().isEmpty())) {
    error =
        QStringLiteral("Inspect failure without field identity retained stale selection/details");
    return false;
  }
  if (!failed_field_id.isEmpty()) {
    const auto* selected_field = field_model_->FieldAt(field_table_->currentIndex().row());
    if (selected_field == nullptr || FromUtf8(selected_field->id) != failed_field_id ||
        details_view_->toPlainText().trimmed().isEmpty()) {
      error = QStringLiteral("Inspect field failure did not present its current field details");
      return false;
    }
  }
  return true;
}

QString DocumentTab::InspectMatchedMessageForSmoke() const {
  return session_.inspect_result().has_value() ? FromUtf8(session_.inspect_result()->message_id)
                                               : QString{};
}

int DocumentTab::InspectFieldCountForSmoke() const noexcept { return field_model_->rowCount(); }

QString DocumentTab::InspectRawValueForSmoke(int row) const {
  return field_model_->data(field_model_->index(row, FieldTableModel::RAW_RESULT)).toString();
}

QString DocumentTab::InspectLogicalValueForSmoke(int row) const {
  return field_model_->data(field_model_->index(row, FieldTableModel::LOGICAL_RESULT)).toString();
}

bool DocumentTab::SelectFirstMappableFieldForSmoke(QString& error) {
  for (int row = 0; row < field_model_->rowCount(); ++row) {
    const auto* field = field_model_->FieldAt(row);
    if (field != nullptr && (!field->physical_bits.empty() ||
                             (field->byte_range.has_value() && field->byte_range->length != 0U))) {
      const int tab_before = right_tabs_->currentIndex();
      right_tabs_->setCurrentIndex(1);
      field_table_->selectRow(row);
      RefreshFieldDetails(row);
      if (right_tabs_->currentIndex() != 1) {
        error = QStringLiteral("field selection forced a result detail tab change");
        right_tabs_->setCurrentIndex(tab_before);
        return false;
      }
      right_tabs_->setCurrentIndex(tab_before);
      return true;
    }
  }
  error = QStringLiteral("selected message has no physical field mapping");
  return false;
}

bool DocumentTab::VerifyInvalidDraftRetentionForSmoke(QString& error) {
  mode_combo_->setCurrentIndex(0);
  for (int row = 0; row < field_model_->rowCount(); ++row) {
    const auto* field = field_model_->FieldAt(row);
    if (field == nullptr || field->id != "count" ||
        field->encode_source != FieldEncodeSource::INPUT ||
        field->value_type != FieldValueType::UINT64
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
        || field->decimal_conversion
#endif
    ) {
      continue;
    }
    const auto index = field_model_->index(row, FieldTableModel::VALUE);
    if (field_model_->setData(index, QStringLiteral("01"), Qt::EditRole) ||
        field_model_->data(index, Qt::EditRole).toString() != QStringLiteral("01") ||
        !field_model_->ValidationError(row).contains(QStringLiteral("canonical UINT64")) ||
        session_.preview().has_value()) {
      error = QStringLiteral("Count=01 did not become a retained invalid editor state");
      return false;
    }
    mode_combo_->setCurrentIndex(1);
    mode_combo_->setCurrentIndex(0);
    const auto restored = field_model_->index(row, FieldTableModel::VALUE);
    if (field_model_->data(restored, Qt::EditRole).toString() != QStringLiteral("01") ||
        !field_model_->ValidationError(row).contains(QStringLiteral("canonical UINT64"))) {
      error = QStringLiteral("Count=01 or its reason was lost across Encode/Inspect/Encode");
      return false;
    }
    EncodeCurrent();
    if (session_.preview().has_value() ||
        !FromUtf8(session_.diagnostic_detail()).contains(QStringLiteral("field=count")) ||
        !FromUtf8(session_.diagnostic_detail()).contains(QStringLiteral("canonical UINT64")) ||
        !diagnostic_label_->text().contains(QStringLiteral("field=count")) ||
        !diagnostic_label_->text().contains(QStringLiteral("canonical UINT64"))) {
      error = QStringLiteral("Encode did not surface Count and the canonical UINT64 reason");
      return false;
    }
    if (!field_model_->setData(restored, QStringLiteral("1"), Qt::EditRole)) {
      error = QStringLiteral("corrected Count draft was rejected");
      return false;
    }
    EncodeCurrent();
    if (!session_.preview().has_value() || !session_.diagnostic_id().empty() ||
        !diagnostic_label_->text().isEmpty()) {
      error = QStringLiteral("correcting Count did not produce a fresh valid Encode result");
      return false;
    }
    return true;
  }
  return true;
}

bool DocumentTab::VerifyBoundedV08ForSmoke(QString& error) {
  const auto* message = CurrentMessage();
  if (message == nullptr || !message->bounded_payload.has_value()) return true;
  const int payload_row = static_cast<int>(message->bounded_payload->payload_field_index);
  const auto payload_index = field_model_->index(payload_row, FieldTableModel::VALUE);
  const auto physical_index = field_model_->index(payload_row, FieldTableModel::PHYSICAL_LOCATION);
  mode_combo_->setCurrentIndex(0);
  field_table_->selectRow(payload_row);
  RefreshFieldDetails(payload_row);

  ClipboardMimeGuard clipboard_guard;
  const auto open_editor = [&]() -> QLineEdit* {
    field_table_->setCurrentIndex(payload_index);
    field_table_->edit(payload_index);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    auto* line_edit = qobject_cast<QLineEdit*>(QApplication::focusWidget());
    if (line_edit != nullptr && field_table_->isAncestorOf(line_edit)) return line_edit;
    for (auto* candidate : field_table_->viewport()->findChildren<QLineEdit*>()) {
      if (candidate->isVisible()) return candidate;
    }
    return nullptr;
  };

  QPointer<QLineEdit> editor = open_editor();
  if (editor == nullptr || editor->property("paeEditCapacity").toInt() != 8) {
    error = QStringLiteral("bounded BYTES editor capacity is not the expected 8 Hex characters");
    return false;
  }
  if (!ReplaceEditorTextByKeyboard(*editor, QStringLiteral("1020"))) {
    error = QStringLiteral("bounded BYTES editor vanished during keyboard entry");
    return false;
  }
  CommitEditorByKey(*editor, Qt::Key_Return);
  if (field_model_->data(payload_index, Qt::EditRole).toString() != QStringLiteral("1020")) {
    error = QStringLiteral("Enter did not commit legal BYTES through the table delegate");
    return false;
  }

  EncodeCurrent();
  if (!session_.preview().has_value() ||
      session_.preview()->encoded_frame !=
          std::vector<std::uint8_t>({0xA5U, 0x05U, 0x10U, 0x20U, 0xDAU})) {
    error = QStringLiteral("Enter-committed legal BYTES did not Encode correctly");
    return false;
  }

  editor = open_editor();
  if (editor == nullptr) {
    error = QStringLiteral("failed to reopen table editor for Tab submission");
    return false;
  }
  if (!ReplaceEditorTextByKeyboard(*editor, QStringLiteral("01020304"))) {
    error = QStringLiteral("bounded BYTES editor vanished during keyboard entry");
    return false;
  }
  if (editor->text() != QStringLiteral("01020304") || session_.preview().has_value()) {
    error = QStringLiteral("keyboard over-protocol draft was truncated or retained old output");
    return false;
  }
  CommitEditorByKey(*editor, Qt::Key_Tab);
  if (!field_model_->ValidationError(payload_row).contains(QStringLiteral("0..3 bytes")) ||
      field_model_->data(payload_index, Qt::EditRole).toString() != QStringLiteral("01020304") ||
      session_.preview().has_value()) {
    error = QStringLiteral("Tab did not submit the complete over-protocol keyboard draft");
    return false;
  }

  editor = open_editor();
  if (editor == nullptr) {
    error = QStringLiteral("failed to reopen table editor for paste submission");
    return false;
  }
  if (!ReplaceEditorTextByPaste(*editor, QStringLiteral("01020304"))) {
    error = QStringLiteral("bounded BYTES editor vanished during paste");
    return false;
  }
  if (editor->text() != QStringLiteral("01020304")) {
    error = QStringLiteral("paste over-protocol draft was truncated before submission");
    return false;
  }
  CommitEditorByKey(*editor, Qt::Key_Return);
  if (!field_model_->ValidationError(payload_row).contains(QStringLiteral("0..3 bytes")) ||
      field_model_->data(payload_index, Qt::EditRole).toString() != QStringLiteral("01020304")) {
    error = QStringLiteral("Enter did not submit the complete over-protocol pasted draft");
    return false;
  }

  editor = open_editor();
  if (editor == nullptr) {
    error = QStringLiteral("failed to reopen table editor for focus-out recovery");
    return false;
  }
  if (!ReplaceEditorTextByKeyboard(*editor, QStringLiteral("010203"))) {
    error = QStringLiteral("bounded BYTES editor vanished during keyboard entry");
    return false;
  }
  QPointer<QLineEdit> focus_editor = editor;
  QWidget* top_level = window();
  if (top_level == nullptr) {
    error = QStringLiteral("focus-out smoke has no top-level window");
    return false;
  }
  top_level->activateWindow();
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  if (!focus_editor) {
    error = QStringLiteral("bounded BYTES editor vanished before focus-out precondition");
    return false;
  }
  focus_editor->setFocus(Qt::OtherFocusReason);
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  if (!focus_editor || QApplication::focusWidget() != focus_editor.data()) {
    error = QStringLiteral("hidden focus-out smoke could not establish editor focus");
    return false;
  }
  std::fputs("UI_SMOKE_EDITOR_COMMIT case=v08 method=focus_out precondition=focused\n", stdout);
  std::fflush(stdout);
  encode_button_->setFocus(Qt::OtherFocusReason);
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  if (field_model_->data(payload_index, Qt::EditRole).toString() != QStringLiteral("010203")) {
    error = QStringLiteral("focus-out did not commit corrected legal BYTES");
    return false;
  }
  std::fputs("UI_SMOKE_EDITOR_COMMIT case=v08 method=focus_out model=accepted\n", stdout);
  std::fflush(stdout);
  EncodeCurrent();
  if (!session_.preview().has_value()) {
    error = QStringLiteral("focus-out correction did not recover a valid Encode result");
    return false;
  }
  std::fputs("UI_SMOKE_EDITOR_COMMIT case=v08 method=focus_out output=accepted\n", stdout);
  std::fflush(stdout);

  editor = open_editor();
  if (editor == nullptr) {
    error = QStringLiteral("failed to open BYTES editor for capacity rejection");
    return false;
  }
  if (!ReplaceEditorTextByPaste(*editor, QStringLiteral("0102030405"))) {
    error = QStringLiteral("bounded BYTES editor vanished during paste");
    return false;
  }
  if (editor->text() != QStringLiteral("010203") ||
      !editor->property("paeCapacityRejected").toBool() || editor->toolTip().isEmpty() ||
      session_.preview().has_value() ||
      field_model_->ValidationError(payload_row) != editor->toolTip()) {
    error = QStringLiteral("over-capacity paste was not wholly rejected with visible feedback");
    return false;
  }
  CommitEditorByKey(*editor, Qt::Key_Return);
  if (session_.preview().has_value()) {
    error = QStringLiteral("rejected capacity operation was committed as a new success");
    return false;
  }

  editor = open_editor();
  if (editor == nullptr) {
    error = QStringLiteral("failed to reopen table editor after capacity rejection");
    return false;
  }
  if (!ReplaceEditorTextByKeyboard(*editor, QStringLiteral("1020"))) {
    error = QStringLiteral("bounded BYTES editor vanished during keyboard entry");
    return false;
  }
  CommitEditorByKey(*editor, Qt::Key_Return);
  EncodeCurrent();
  if (!session_.preview().has_value() ||
      session_.preview()->encoded_frame !=
          std::vector<std::uint8_t>({0xA5U, 0x05U, 0x10U, 0x20U, 0xDAU})) {
    error = QStringLiteral("capacity rejection correction did not recover expected Encode");
    return false;
  }

  if (!field_model_->setData(payload_index, QString{}, Qt::EditRole)) {
    error = QStringLiteral("legal empty bounded payload was rejected: %1")
                .arg(field_model_->ValidationError(payload_row));
    return false;
  }
  EncodeCurrent();
  const std::vector<std::uint8_t> empty_expected{0xA5U, 0x03U, 0xA8U};
  if (PreviewFrameForSmoke() != empty_expected || HighlightedCellCountForSmoke() != 0U ||
      !field_model_->data(physical_index).toString().contains(QStringLiteral("length=0")) ||
      !details_view_->toPlainText().contains(QStringLiteral("实际字节范围：2 + 0")) ||
      !details_view_->toPlainText().contains(QStringLiteral("完整性校验存储位置：2 + 1"))) {
    error = QStringLiteral("empty payload frame, zero-length range, or dynamic trailer differs");
    return false;
  }

  if (!field_model_->setData(payload_index, QStringLiteral("1020"), Qt::EditRole)) {
    error = QStringLiteral("two-byte bounded payload was rejected");
    return false;
  }
  EncodeCurrent();
  const std::vector<std::uint8_t> two_expected{0xA5U, 0x05U, 0x10U, 0x20U, 0xDAU};
  if (PreviewFrameForSmoke() != two_expected || HighlightedCellCountForSmoke() != 2U ||
      HighlightMaskForSmoke(2U) != 0xFFU || HighlightMaskForSmoke(3U) != 0xFFU ||
      !details_view_->toPlainText().contains(QStringLiteral("实际字节范围：2 + 2")) ||
      !details_view_->toPlainText().contains(QStringLiteral("完整性校验存储位置：4 + 1"))) {
    error = QStringLiteral("two-byte actual payload range or dynamic trailer differs");
    return false;
  }

  if (!field_model_->setData(payload_index, QStringLiteral("00FF01"), Qt::EditRole)) {
    error = QStringLiteral("three-byte bounded payload was rejected");
    return false;
  }
  EncodeCurrent();
  const std::vector<std::uint8_t> three_expected{0xA5U, 0x06U, 0x00U, 0xFFU, 0x01U, 0xABU};
  if (PreviewFrameForSmoke() != three_expected || HighlightedCellCountForSmoke() != 3U ||
      HighlightMaskForSmoke(2U) != 0xFFU || HighlightMaskForSmoke(3U) != 0xFFU ||
      HighlightMaskForSmoke(4U) != 0xFFU ||
      !details_view_->toPlainText().contains(QStringLiteral("实际字节范围：2 + 3")) ||
      !details_view_->toPlainText().contains(QStringLiteral("完整性校验存储位置：5 + 1"))) {
    error = QStringLiteral("three-byte actual payload range or dynamic trailer differs");
    return false;
  }

  if (field_model_->setData(payload_index, QStringLiteral("GG"), Qt::EditRole) ||
      !field_model_->ValidationError(payload_row).contains(QStringLiteral("uppercase Hex")) ||
      session_.preview().has_value() || HighlightedCellCountForSmoke() != 0U ||
      !field_model_->data(physical_index)
           .toString()
           .contains(QStringLiteral("current range unavailable")) ||
      !details_view_->toPlainText().contains(QStringLiteral("current range unavailable")) ||
      details_view_->toPlainText().contains(QStringLiteral("实际字节范围："))) {
    error = QStringLiteral("lexical BYTES error was not distinct or cleared stale output");
    return false;
  }
  if (field_model_->setData(payload_index, QStringLiteral("01020304"), Qt::EditRole) ||
      !field_model_->ValidationError(payload_row).contains(QStringLiteral("0..3 bytes")) ||
      session_.preview().has_value() || HighlightedCellCountForSmoke() != 0U ||
      !field_model_->data(physical_index)
           .toString()
           .contains(QStringLiteral("current range unavailable"))) {
    error = QStringLiteral("bounded payload overflow was not reported as a length violation");
    return false;
  }
  if (!field_model_->setData(payload_index, QStringLiteral("1020"), Qt::EditRole)) {
    error = QStringLiteral("corrected bounded payload was rejected");
    return false;
  }
  EncodeCurrent();
  if (PreviewFrameForSmoke() != two_expected || HighlightedCellCountForSmoke() != 2U ||
      HighlightMaskForSmoke(2U) != 0xFFU || HighlightMaskForSmoke(3U) != 0xFFU ||
      !details_view_->toPlainText().contains(QStringLiteral("完整性校验存储位置：4 + 1"))) {
    error = QStringLiteral("two-byte actual payload range or dynamic trailer differs");
    return false;
  }

  if (!VerifyInspectFailureForSmoke(QStringLiteral("A5 05 10 20 DB"), InspectFailureStage::CODEC,
                                    QStringLiteral("INTEGRITY_FAILED"), std::nullopt, QString{},
                                    error) ||
      HighlightedCellCountForSmoke() != 0U) {
    if (error.isEmpty()) {
      error = QStringLiteral("dynamic trailer failure used an unproven current location");
    }
    return false;
  }
  if (!InspectTextForSmoke(QStringLiteral("A5 05 10 20 DA"), error)) return false;
  field_table_->selectRow(payload_row);
  RefreshFieldDetails(payload_row);
  if (HighlightedCellCountForSmoke() != 2U || HighlightMaskForSmoke(2U) != 0xFFU ||
      HighlightMaskForSmoke(3U) != 0xFFU) {
    error = QStringLiteral("valid Inspect recovery did not restore the actual payload highlight");
    return false;
  }
  return true;
}

#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
bool DocumentTab::VerifyAsciiStreamForSmoke(QString& error) {
  if (!session_.StreamInspectAvailable()) {
    error = QStringLiteral("selected document is not an ASCII CRLF stream Pipeline");
    return false;
  }
  mode_combo_->setCurrentIndex(
      mode_combo_->findData(static_cast<int>(OperationMode::STREAM_INSPECT)));
  representation_combo_->setCurrentIndex(
      representation_combo_->findData(static_cast<int>(ByteRepresentation::ASCII_ESCAPED)));
  if (!inspect_button_->isEnabled() || inspect_button_->objectName() != QStringLiteral("primaryAction") ||
      mode_combo_->currentData().toInt() != static_cast<int>(OperationMode::STREAM_INSPECT) ||
      session_.StreamChunkBudget() !=
          (std::min)(std::size_t{65536U},
                     session_.StreamObservation()->effective_max_submit_bytes)) {
    error = QStringLiteral("Stream Inspect mode or effective chunk capacity differs");
    return false;
  }
  inspect_input_->setPlainText(QStringLiteral("RX A!"));
  InspectCurrent();
  const auto half = session_.StreamObservation();
  if (!half.has_value() || half->buffered_bytes != 5U || session_.inspect_result().has_value() ||
      !result_kind_label_->text().contains(QStringLiteral("本步无候选"))) {
    error = QStringLiteral("half-frame Submit presentation differs");
    return false;
  }
  const auto step_before_repeat = half->step_sequence;
  InspectCurrent();
  if (session_.diagnostic_id() != "UI_STREAM_CHUNK_ALREADY_SUBMITTED" ||
      session_.StreamObservation()->step_sequence != step_before_repeat ||
      session_.StreamObservation()->buffered_bytes != half->buffered_bytes) {
    error = QStringLiteral("unchanged submitted draft was pushed more than once");
    return false;
  }
  inspect_input_->setPlainText(QStringLiteral("RX A!OK\\q"));
  InspectCurrent();
  if (session_.StreamObservation()->buffered_bytes != half->buffered_bytes ||
      !session_.inspect_failure().has_value() || session_.inspect_failure()->input_offset != 7U ||
      !session_.inspect_failure()->input_offset_is_utf16) {
    error = QStringLiteral("invalid stream draft changed state or lost UTF-16 offset");
    return false;
  }
  inspect_input_->setPlainText(QStringLiteral("OK\\r\\nONLY\\r\\n"));
  InspectCurrent();
  if (!session_.inspect_result().has_value() ||
      session_.inspect_result()->message_id != "greeting" || !continue_button_->isEnabled()) {
    error = QStringLiteral("candidate STOP or frozen suffix presentation differs");
    return false;
  }
  ContinueStream();
  if (!session_.inspect_result().has_value() ||
      session_.inspect_result()->message_id != "decode_only" || continue_button_->isEnabled()) {
    error = QStringLiteral("Continue did not expose exactly the next candidate");
    return false;
  }
  inspect_input_->setPlainText(QStringLiteral("1234567890123"));
  InspectCurrent();
  if (!session_.StreamHasDiscardableState() ||
      session_.StreamObservation()->phase !=
          StreamFramingPhase::DISCARDING_UNTIL_CRLF) {
    error = QStringLiteral("overlong candidate did not expose discard state");
    return false;
  }
  ResetStream();
  if (session_.StreamHasDiscardableState() ||
      session_.StreamObservation()->total_candidates != 0U) {
    error = QStringLiteral("Reset did not clear stream state and counters");
    return false;
  }
  return true;
}

bool DocumentTab::PrepareStreamHalfFrameForSmoke(QString& error) {
  if (!session_.StreamInspectAvailable()) {
    error = QStringLiteral("stream Pipeline is unavailable");
    return false;
  }
  mode_combo_->setCurrentIndex(
      mode_combo_->findData(static_cast<int>(OperationMode::STREAM_INSPECT)));
  representation_combo_->setCurrentIndex(
      representation_combo_->findData(static_cast<int>(ByteRepresentation::ASCII_ESCAPED)));
  inspect_input_->setPlainText(QStringLiteral("RX A!"));
  InspectCurrent();
  const auto observation = session_.StreamObservation();
  if (!observation.has_value() || observation->buffered_bytes != 5U) {
    error = QStringLiteral("failed to prepare a five-byte half-frame");
    return false;
  }
  return true;
}

QString DocumentTab::StreamStateSignatureForSmoke() const {
  const auto observation = session_.StreamObservation();
  if (!observation.has_value()) return QStringLiteral("unavailable");
  return QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11")
      .arg(FromUtf16(session_.inspect_draft_utf16()))
      .arg(static_cast<int>(observation->phase))
      .arg(static_cast<qulonglong>(observation->buffered_bytes))
      .arg(observation->has_internal_work ? 1 : 0)
      .arg(static_cast<qulonglong>(observation->frozen_input_bytes))
      .arg(static_cast<qulonglong>(observation->frozen_cursor))
      .arg(static_cast<qulonglong>(observation->generation))
      .arg(static_cast<qulonglong>(observation->step_sequence))
      .arg(static_cast<qulonglong>(observation->total_candidates))
      .arg(static_cast<qulonglong>(observation->total_decode_successes))
      .arg(session_.stream_step().has_value() ? 1 : 0);
}

void DocumentTab::ResetStreamForSmoke() { ResetStream(); }
#endif

bool DocumentTab::VerifyAsciiForSmoke(QString& error) {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("ascii_document_verify_enter", this);
#endif
  if (!session_.IsAsciiDocument()) {
    error = QStringLiteral("document is not Schema 0.10 ASCII");
    return false;
  }
  const auto* message = CurrentMessage();
  if (message == nullptr) {
    error = QStringLiteral("ASCII document has no selected Message");
    return false;
  }

  mode_combo_->setCurrentIndex(0);
  RefreshState();
  if (encode_button_->isEnabled() != session_.EncodeAvailable()) {
    error = QStringLiteral("Encode button availability differs from the action description");
    return false;
  }
  mode_combo_->setCurrentIndex(1);
  RefreshState();
  if (inspect_button_->isEnabled() != session_.InspectAvailable()) {
    error = QStringLiteral("Inspect button availability differs from Pipeline Decode candidates");
    return false;
  }
  if (!message->encode_available) {
    mode_combo_->setCurrentIndex(0);
    if (session_.Encode() || session_.diagnostic_id() != "OPERATION_NOT_SUPPORTED") {
      error = QStringLiteral("Decode-only Message did not enforce session Encode rejection");
      return false;
    }
    representation_combo_->setCurrentIndex(1);
    return InspectTextForSmoke(QStringLiteral("PING\\r\\n"), error);
  }
  if (!session_.InspectAvailable()) {
    if (!EncodeForSmoke(error)) return false;
    mode_combo_->setCurrentIndex(1);
    if (inspect_button_->isEnabled()) {
      error = QStringLiteral("Encode-only Pipeline left Inspect enabled");
      return false;
    }
    session_.SetInspectDraft("50494E470D0A");
    if (session_.Inspect() || !session_.inspect_failure().has_value() ||
        session_.inspect_failure()->status != "OPERATION_NOT_SUPPORTED") {
      error = QStringLiteral("Encode-only Pipeline did not enforce session Inspect rejection");
      return false;
    }
    return true;
  }

  if (message->fields.empty()) {
    if (!EncodeForSmoke(error) || !session_.preview().has_value() ||
        !session_.preview()->zero_field_success ||
        !result_kind_label_->text().contains(QStringLiteral("成功，0 个字段")) ||
        timing_label_->property("paeReviewKind").toString() != QStringLiteral("TX_TEMPLATE") ||
        !timing_label_->text().contains(QStringLiteral("TX_TEMPLATE")) ||
        HighlightedCellCountForSmoke() != 0U) {
      if (error.isEmpty())
        error = QStringLiteral("literal-only Encode did not show 0-field success");
      return false;
    }
    representation_combo_->setCurrentIndex(1);
    if (!InspectTextForSmoke(QStringLiteral("PING\\r\\n"), error) ||
        !session_.inspect_result()->zero_field_success ||
        !result_kind_label_->text().contains(QStringLiteral("成功，0 个字段")) ||
        HighlightedCellCountForSmoke() != 0U) {
      if (error.isEmpty())
        error = QStringLiteral("literal-only Inspect did not show 0-field success");
      return false;
    }
    return true;
  }

  ClipboardMimeGuard clipboard_guard;

  const int name_row = 0;
  representation_combo_->setCurrentIndex(1);
  mode_combo_->setCurrentIndex(0);
  QModelIndex name_index = field_model_->index(name_row, FieldTableModel::VALUE);
  field_table_->setCurrentIndex(name_index);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("editor_first_before_edit", this);
#endif
  field_table_->edit(name_index);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("editor_first_before_pump", this);
#endif
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  QPointer<QLineEdit> editor = smoke_editor_target::Find(*field_table_, name_index, error);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("editor_first_after_pump", editor.data(),
                                ascii_smoke_diagnostic::EditorId(editor.data()));
#endif
  if (!editor) return false;
  if (editor->property("paeEditCapacity").toInt() != 36 ||
      editor->property("paeByteRepresentation").toInt() !=
          static_cast<int>(ByteRepresentation::ASCII_ESCAPED)) {
    error = QStringLiteral("ASCII table editor did not expose expected escaped capacity=36");
    return false;
  }
  if (!ReplaceEditorTextByKeyboard(*editor, QStringLiteral("ALICE"))) {
    error = QStringLiteral("ASCII smoke first editor vanished during keyboard entry");
    return false;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  if (ascii_smoke_diagnostic::EditorLost()) {
    error = QStringLiteral("ASCII_DIAG_EDITOR_LOST during first keyboard entry");
    return false;
  }
  ascii_smoke_diagnostic::Trace("editor_first_before_return_commit", editor.data(),
                                ascii_smoke_diagnostic::EditorId(editor.data()));
#endif
  CommitEditorByKey(*editor, Qt::Key_Return);
  if (field_model_->data(name_index, Qt::EditRole).toString() != QStringLiteral("ALICE")) {
    error = QStringLiteral("ASCII Return did not commit ALICE to the field model");
    return false;
  }
  const auto committed_draft =
      session_.drafts().find(message->fields[static_cast<std::size_t>(name_row)].field_index);
  const std::vector<std::uint8_t> expected_name{'A', 'L', 'I', 'C', 'E'};
  const auto* committed_bytes =
      committed_draft == session_.drafts().end()
          ? nullptr
          : std::get_if<std::vector<std::uint8_t>>(&committed_draft->second);
  if (committed_bytes == nullptr || *committed_bytes != expected_name) {
    error = QStringLiteral("ASCII Return did not commit ALICE to the document session");
    return false;
  }
  std::fputs("UI_SMOKE_EDITOR_COMMIT case=ascii method=return model=accepted session=accepted\n",
             stdout);
  std::fflush(stdout);
  EncodeCurrent();
  const std::vector<std::uint8_t> expected{'T', 'X', ' ', 'A', 'L',  'I',
                                           'C', 'E', '!', 'A', '\r', '\n'};
  if (!session_.preview().has_value()) {
    error = QStringLiteral("ASCII Return-committed ALICE did not produce an Encode preview: %1")
                .arg(FromUtf8(session_.diagnostic_id()));
    return false;
  }
  if (session_.preview()->encoded_frame != expected) {
    error = QStringLiteral("ASCII Return-committed ALICE produced unexpected TX template bytes");
    return false;
  }
  if (!session_.preview()->tx_template_review) {
    error = QStringLiteral("ASCII Return-committed ALICE lost TX_TEMPLATE review identity");
    return false;
  }
  std::fputs("UI_SMOKE_EDITOR_COMMIT case=ascii method=return output=accepted\n", stdout);
  std::fflush(stdout);
  if (timing_label_->property("paeReviewKind").toString() != QStringLiteral("TX_TEMPLATE") ||
      !timing_label_->text().contains(QStringLiteral("TX_TEMPLATE")) ||
      result_kind_label_->property("paeResultState").toString() != QStringLiteral("success")) {
    error = QStringLiteral("successful ASCII Encode lost TX_TEMPLATE timing semantics");
    return false;
  }
  if (field_model_->data(field_model_->index(1, FieldTableModel::SOURCE),
                         FieldTableModel::ActionReferencedRole)
          .toBool() ||
      !field_model_->data(field_model_->index(2, FieldTableModel::SOURCE),
                          FieldTableModel::ActionReferencedRole)
           .toBool() ||
      field_model_->data(field_model_->index(2, FieldTableModel::SOURCE),
                         FieldTableModel::EncodeSourceRole)
              .toInt() != static_cast<int>(FieldEncodeSource::INPUT)) {
    error = QStringLiteral("ASCII Encode action/source annotations are inconsistent");
    return false;
  }
  field_table_->selectRow(name_row);
  RefreshFieldDetails(name_row);
  if (HighlightedCellCountForSmoke() != 5U || HighlightMaskForSmoke(3U) != 0xFFU ||
      HighlightMaskForSmoke(7U) != 0xFFU) {
    error = QStringLiteral("ASCII Encode actual field range was not highlighted");
    return false;
  }
  representation_combo_->setCurrentIndex(0);
  if (!timing_label_->text().isEmpty() || session_.preview().has_value()) {
    error = QStringLiteral("representation switch retained stale ASCII Encode timing/result");
    return false;
  }
  representation_combo_->setCurrentIndex(1);
  name_index = field_model_->index(name_row, FieldTableModel::VALUE);
  EncodeCurrent();
  if (!session_.preview().has_value()) {
    error =
        QStringLiteral("ASCII Encode did not recover after byte-equivalent representation switch");
    return false;
  }

#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("editor_second_before_edit", this);
#endif
  field_table_->edit(name_index);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("editor_second_before_pump", this);
#endif
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  editor = smoke_editor_target::Find(*field_table_, name_index, error);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("editor_second_after_pump", editor.data(),
                                ascii_smoke_diagnostic::EditorId(editor.data()));
#endif
  if (!editor) return false;
  if (!ReplaceEditorTextByPaste(*editor, QStringLiteral("ABCDEFGHI"))) {
    error = QStringLiteral("ASCII smoke second editor vanished during paste");
    return false;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  if (ascii_smoke_diagnostic::EditorLost()) {
    error = QStringLiteral("ASCII_DIAG_EDITOR_LOST during second editor paste");
    return false;
  }
#endif
  CommitEditorByKey(*editor, Qt::Key_Tab);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  if (ascii_smoke_diagnostic::EditorLost()) {
    error = QStringLiteral("ASCII_DIAG_EDITOR_LOST before second editor key release");
    return false;
  }
#endif
  if (!field_model_->ValidationError(name_row).contains(QStringLiteral("1..8 bytes")) ||
      session_.preview().has_value()) {
    error = QStringLiteral("ASCII over-protocol pasted draft was not retained as invalid");
    return false;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("editor_third_before_edit", this);
#endif
  field_table_->edit(name_index);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("editor_third_before_pump", this);
#endif
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  editor = smoke_editor_target::Find(*field_table_, name_index, error);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::Trace("editor_third_after_pump", editor.data(),
                                ascii_smoke_diagnostic::EditorId(editor.data()));
#endif
  if (!editor) return false;
  if (!ReplaceEditorTextByPaste(*editor, QString(37, QLatin1Char('A')))) {
    error = QStringLiteral("ASCII smoke third editor vanished during paste");
    return false;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  if (ascii_smoke_diagnostic::EditorLost()) {
    error = QStringLiteral("ASCII_DIAG_EDITOR_LOST during third editor paste");
    return false;
  }
#endif
  if (editor->text() != QStringLiteral("ABCDEFGHI") ||
      !editor->property("paeCapacityRejected").toBool()) {
    error = QStringLiteral("ASCII over-capacity paste was not wholly rejected");
    return false;
  }
  if (!ReplaceEditorTextByKeyboard(*editor, QStringLiteral("ALICE"))) {
    error = QStringLiteral("ASCII smoke third editor vanished during keyboard entry");
    return false;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  if (ascii_smoke_diagnostic::EditorLost()) {
    error = QStringLiteral("ASCII_DIAG_EDITOR_LOST during third editor keyboard entry");
    return false;
  }
#endif
  CommitEditorByKey(*editor, Qt::Key_Return);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  if (ascii_smoke_diagnostic::EditorLost()) {
    error = QStringLiteral("ASCII_DIAG_EDITOR_LOST before third editor key release");
    return false;
  }
#endif
  EncodeCurrent();
  if (!session_.preview().has_value()) {
    error = QStringLiteral("ASCII field did not recover after invalid/capacity input");
    return false;
  }

  if (!InspectTextForSmoke(QStringLiteral("RX ALICE!OK\\r\\n"), error)) return false;
  field_table_->selectRow(name_row);
  RefreshFieldDetails(name_row);
  if (session_.inspect_result()->message_id != "greeting" ||
      session_.inspect_result()->fields.size() != 2U || HighlightedCellCountForSmoke() != 5U ||
      !timing_label_->text().isEmpty()) {
    error = QStringLiteral("ASCII Inspect did not publish Core match, fields and actual range");
    return false;
  }
  const QModelIndex rx_source = field_model_->index(1, FieldTableModel::SOURCE);
  const QModelIndex rx_value = field_model_->index(1, FieldTableModel::VALUE);
  const QModelIndex rx_raw = field_model_->index(1, FieldTableModel::RAW_RESULT);
  const QModelIndex rx_logical = field_model_->index(1, FieldTableModel::LOGICAL_RESULT);
  const QModelIndex rx_physical = field_model_->index(1, FieldTableModel::PHYSICAL_LOCATION);
  const QModelIndex tx_source = field_model_->index(2, FieldTableModel::SOURCE);
  const QModelIndex tx_value = field_model_->index(2, FieldTableModel::VALUE);
  const QModelIndex tx_raw = field_model_->index(2, FieldTableModel::RAW_RESULT);
  const auto expect_inspect_display = [&](const QModelIndex& index, const char* field_id,
                                          const char* column, const QString& expected) {
    const QString actual = field_model_->data(index).toString();
    if (actual == expected) return true;
    const auto visible = [](const QString& value) {
      return value.isEmpty() ? QStringLiteral("<empty>") : value;
    };
    error = QStringLiteral("ASCII Inspect display mismatch: field=%1; column=%2; expected=%3; "
                           "actual=%4")
                .arg(QString::fromLatin1(field_id), QString::fromLatin1(column), visible(expected),
                     visible(actual));
    return false;
  };
  if (!expect_inspect_display(rx_source, "rx_code", "SOURCE", QStringLiteral("解析结果")) ||
      !expect_inspect_display(rx_value, "rx_code", "VALUE", QString{}) ||
      !expect_inspect_display(rx_raw, "rx_code", "RAW_RESULT", QStringLiteral("4F4B")) ||
      !expect_inspect_display(rx_logical, "rx_code", "LOGICAL_RESULT", QStringLiteral("OK")) ||
      !expect_inspect_display(rx_physical, "rx_code", "PHYSICAL_LOCATION",
                              QStringLiteral("9 + 2")) ||
      !expect_inspect_display(tx_source, "tx_tag", "SOURCE", QStringLiteral("未引用")) ||
      !expect_inspect_display(tx_value, "tx_tag", "VALUE",
                              QStringLiteral("未被 Decode 动作引用")) ||
      !expect_inspect_display(tx_raw, "tx_tag", "RAW_RESULT", QString{}))
    return false;
  field_table_->selectRow(1);
  RefreshFieldDetails(1);
  const QString rx_details = details_view_->toPlainText();
  if (!rx_details.contains(QStringLiteral("实际字节范围：9 + 2")) ||
      !rx_details.contains(
          QStringLiteral("实际物理字节（从 0 起）：[9, 11)，整字节范围")) ||
      rx_details.contains(QStringLiteral("current range unavailable"))) {
    error = QStringLiteral("ASCII Inspect details contradict the adapter actual byte range");
    return false;
  }
  if (!VerifyInspectFailureForSmoke(QStringLiteral("RX ALICE!OK\\q"), InspectFailureStage::INPUT,
                                    QString{}, 11U, QString{}, error) ||
      !diagnostic_label_->text().contains(
          QStringLiteral("input_utf16_code_unit_offset=11 (zero-based)")) ||
      !timing_label_->text().isEmpty()) {
    if (error.isEmpty()) {
      error = QStringLiteral("ASCII escape failure omitted UTF-16 offset or retained timing");
    }
    return false;
  }
  if (!VerifyInspectFailureForSmoke(QStringLiteral("RX A\n!OK\\r\\n"), InspectFailureStage::INPUT,
                                    QString{}, 4U, QString{}, error) ||
      session_.diagnostic_id() != "UI_ASCII_INPUT_INVALID") {
    if (error.isEmpty()) error = QStringLiteral("ASCII actual-control failure differs");
    return false;
  }
  if (!InspectTextForSmoke(QStringLiteral("RX ALICE!OK\\r\\n"), error)) return false;
  representation_combo_->setCurrentIndex(0);
  const QString converted_hex = inspect_input_->toPlainText();
  if (converted_hex != QStringLiteral("525820414C494345214F4B0D0A") ||
      session_.inspect_result().has_value()) {
    error = QStringLiteral("ASCII-to-Hex switch did not preserve bytes and clear result");
    return false;
  }
  inspect_input_->setPlainText(QStringLiteral("80"));
  representation_combo_->setCurrentIndex(1);
  if (session_.representation() != ByteRepresentation::HEX ||
      inspect_input_->toPlainText() != QStringLiteral("80")) {
    error = QStringLiteral("non-ASCII Hex incorrectly switched representation or lost draft");
    return false;
  }
  return true;
}

bool DocumentTab::VerifyPipelineSwitchClearsInspectForSmoke(QString& error) {
  if (pipeline_combo_->count() < 2) {
    error = QStringLiteral("Pipeline reset smoke requires two real Pipeline choices");
    return false;
  }
  mode_combo_->setCurrentIndex(1);
  inspect_input_->setPlainText(QStringLiteral("AA:00"));
  InspectCurrent();
  if (!session_.inspect_failure().has_value() || session_.diagnostic_id().empty() ||
      session_.inspect_draft().empty()) {
    error = QStringLiteral("precondition Inspect failure was not visible");
    return false;
  }
  pipeline_combo_->setCurrentIndex(1);
  if (!session_.inspect_draft().empty() || session_.inspect_result().has_value() ||
      session_.inspect_failure().has_value() || !session_.diagnostic_id().empty() ||
      !session_.diagnostic_detail().empty() || !inspect_input_->toPlainText().isEmpty() ||
      !diagnostic_label_->text().isEmpty() || hex_view_->FrameSize() != 0U) {
    error =
        QStringLiteral("Pipeline switch retained Inspect input/result/failure/diagnostic state");
    return false;
  }
  pipeline_combo_->setCurrentIndex(0);
  mode_combo_->setCurrentIndex(0);
  return PopulateCanonicalDraftsForSmoke(error);
}

void DocumentTab::InvalidatePreviewForSmoke() { InvalidateEditedPreview(); }

std::size_t DocumentTab::PreviewFrameSizeForSmoke() const noexcept {
  return hex_view_->FrameSize();
}

const std::vector<std::uint8_t>& DocumentTab::PreviewFrameForSmoke() const noexcept {
  static const std::vector<std::uint8_t> empty;
  return session_.preview().has_value() ? session_.preview()->encoded_frame : empty;
}

std::size_t DocumentTab::HighlightedCellCountForSmoke() const noexcept {
  return hex_view_->HighlightedCellCount();
}

std::uint8_t DocumentTab::HighlightMaskForSmoke(std::size_t frame_byte_index) const noexcept {
  return hex_view_->HighlightMaskAt(frame_byte_index);
}

#if defined(PAE_BUILD_PROTOCOL_LAB_BINDING_UI)
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
bool DocumentTab::VerifyHostForSmoke(QString& error) {
  if (!session_.IsAsciiDocument()) return true;
  const bool stream = session_.StreamInspectAvailable();
  const auto& pipeline = session_.description()->pipelines.front();
  bool has_encode = false;
  for (const auto index : pipeline.message_indices)
    has_encode = has_encode || session_.description()->messages[index].encode_available;
  if (pipeline.decode_message_indices.empty() || !has_encode) return true;
  if (stream) ResetStream();
  InitializeHostDraft();
  auto apply = [this] {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
    ascii_smoke_diagnostic::Trace("host_apply_before_click", this);
#endif
    host_apply_->click();
    QElapsedTimer timer;
    timer.start();
    while (host_pending_revision_ && timer.elapsed() < 10000) {
      for (const auto ticket : worker_.DrainReadyTickets())
        AcceptCompletion(worker_.TakeResult(ticket));
      if (host_pending_revision_) QThread::msleep(5);
    }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
    ascii_smoke_diagnostic::Trace(
        host_pending_revision_ ? "host_apply_pending_timeout" : "host_apply_after_completion",
        this);
#endif
    return !host_pending_revision_;
  };
  auto check = [&](bool condition, const char* why) {
    if (!condition) error = QString::fromLatin1(why);
    return condition;
  };
  if (!check(apply() && session_.HostActive(), "Host Apply publication")) return false;
  representation_combo_->setCurrentIndex(
      representation_combo_->findData(static_cast<int>(ByteRepresentation::ASCII_ESCAPED)));
  if (!stream) {
    const bool literal = CurrentMessage()->fields.empty();
    inspect_input_->setPlainText(literal ? QStringLiteral("PING\\r\\n")
                                         : QStringLiteral("RX ALICE!OK\\r\\n"));
    inspect_button_->click();
    if (!check(session_.inspect_result().has_value(), "Host complete record Decode")) return false;
    host_binding_combo_->setCurrentIndex(1);
    representation_combo_->setCurrentIndex(
        representation_combo_->findData(static_cast<int>(ByteRepresentation::ASCII_ESCAPED)));
    if (!literal && (!field_model_->setData(field_model_->index(0, FieldTableModel::VALUE),
                                            QStringLiteral("ALICE")) ||
                     !field_model_->setData(field_model_->index(2, FieldTableModel::VALUE),
                                            QStringLiteral("Z"))))
      return check(false, "Host complete Encode inputs");
    encode_button_->click();
    const std::string expected = literal ? "PONG\r\n" : "TX ALICE!Z\r\n";
    if (!check(
            session_.preview() && session_.preview()->encoded_frame ==
                                      std::vector<std::uint8_t>(expected.begin(), expected.end()),
            "Host complete Encode bytes"))
      return false;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
    const auto generation = session_.plan_generation();
    const auto retained = session_.preview()->encoded_frame;
    QTimer cancel_timer;
    connect(&cancel_timer, &QTimer::timeout, this, [] {
      if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
        box->done(QMessageBox::No);
    });
    cancel_timer.start(10);
    if (!check(apply() && session_.plan_generation() == generation && session_.preview() &&
                   session_.preview()->encoded_frame == retained,
               "Host complete Apply cancellation preserves state"))
      return false;
#endif
    return true;
  }
  inspect_input_->setPlainText(QStringLiteral("RX A!"));
  inspect_button_->click();
  if (!check(session_.StreamObservation()->buffered_bytes == 5, "Host flow0 half")) return false;
  host_flow_combo_->setCurrentIndex(1);
  inspect_input_->setPlainText(QStringLiteral("ON"));
  representation_combo_->setCurrentIndex(
      representation_combo_->findData(static_cast<int>(ByteRepresentation::ASCII_ESCAPED)));
  if (!check(session_.representation() == ByteRepresentation::HEX &&
                 representation_combo_->currentData().toInt() ==
                     static_cast<int>(ByteRepresentation::HEX) &&
                 inspect_input_->toPlainText() == QStringLiteral("ON") &&
                 diagnostic_label_->text().contains(QStringLiteral("Hex -> ASCII (escaped)")) &&
                 diagnostic_label_->text().contains(QStringLiteral("clear")) &&
                 session_.StreamObservation()->buffered_bytes == 0,
             "Rejected representation switch preserves draft and explains recovery"))
    return false;
  inspect_input_->clear();
  representation_combo_->setCurrentIndex(
      representation_combo_->findData(static_cast<int>(ByteRepresentation::ASCII_ESCAPED)));
  inspect_input_->setPlainText(QStringLiteral("ON"));
  inspect_button_->click();
  host_binding_combo_->setCurrentIndex(1);
  if (!check(session_.mode() == OperationMode::ENCODE && session_.StreamHasDiscardableState(),
             "Host binding switch"))
    return false;
  const auto generation = session_.plan_generation();
  auto* action = qobject_cast<QComboBox*>(host_draft_->cellWidget(1, 1));
  action->setCurrentIndex(0);
  if (!check(apply() && session_.plan_generation() == generation && session_.HostActive(),
             "Invalid Apply preserves Session"))
    return false;
  action->setCurrentIndex(1);
  QTimer cancel_timer;
  connect(&cancel_timer, &QTimer::timeout, this, [] {
    if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
      box->done(QMessageBox::No);
  });
  cancel_timer.start(10);
  if (!check(apply() && session_.plan_generation() == generation, "Cancel Apply preserves Session"))
    return false;
  cancel_timer.stop();
  host_binding_combo_->setCurrentIndex(0);
  if (!check(session_.StreamObservation()->buffered_bytes == 5 &&
                 inspect_input_->toPlainText() == QStringLiteral("RX A!"),
             "Restore flow0 draft"))
    return false;
  inspect_input_->setPlainText(QStringLiteral("OK\\r\\nBAD\\r\\nONLY\\r\\n"));
  inspect_button_->click();
  if (!check(session_.inspect_result() && session_.inspect_result()->fields[0].logical_value == "A",
             "Host success candidate"))
    return false;
  continue_button_->click();
  if (!check(session_.inspect_failure() && !session_.inspect_result(), "Host failed candidate"))
    return false;
  continue_button_->click();
  if (!check(session_.inspect_result() && session_.inspect_result()->zero_field_success,
             "Host zero field success"))
    return false;
  reset_stream_button_->click();
  host_flow_combo_->setCurrentIndex(1);
  if (!check(session_.StreamObservation()->buffered_bytes == 2, "Reset selected flow only"))
    return false;
  inspect_input_->setPlainText(QStringLiteral("LY\\r\\n"));
  inspect_button_->click();
  if (!check(session_.inspect_result() && session_.inspect_result()->zero_field_success,
             "Flow1 continuation"))
    return false;
  ResetStream();
  host_flow_combo_->setCurrentIndex(0);
  return true;
}
#endif

#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
bool DocumentTab::VerifyBinaryStreamForSmoke(QString& error) {
  if (!session_.IsBinaryHostDocument() || session_.BinaryHostActive() ||
      host_draft_->rowCount() != 1) {
    error = QStringLiteral("Binary stream smoke did not start from one unpublished binding");
    return false;
  }
  AddHostDraftRow();
  AddHostDraftRow();
  for (int row = 0; row < 3; ++row) {
    auto* endpoint = qobject_cast<QLineEdit*>(host_draft_->cellWidget(row, 0));
    auto* action = qobject_cast<QComboBox*>(host_draft_->cellWidget(row, 1));
    auto* pipeline = qobject_cast<QComboBox*>(host_draft_->cellWidget(row, 2));
    const int pipeline_item =
        pipeline == nullptr ? -1 : pipeline->findData(static_cast<qulonglong>(row));
    if (!endpoint || !action || pipeline_item < 0) {
      error = QStringLiteral("Binary stream binding draft controls are incomplete");
      return false;
    }
    endpoint->setText(QStringLiteral("stream-%1").arg(row));
    action->setCurrentIndex(0);
    pipeline->setCurrentIndex(pipeline_item);
  }
  host_apply_->click();
  QElapsedTimer timer;
  timer.start();
  while (host_pending_revision_ && timer.elapsed() < 10000) {
    for (const auto ticket : worker_.DrainReadyTickets())
      AcceptCompletion(worker_.TakeResult(ticket));
    if (host_pending_revision_) QThread::msleep(5);
  }
  auto check = [&](bool condition, const char* detail) {
    if (!condition) error = QString::fromLatin1(detail);
    return condition;
  };
  if (!check(!host_pending_revision_ && session_.BinaryHostActive() &&
                 host_binding_combo_->count() == 3 && session_.mode() == OperationMode::STREAM_INSPECT &&
                 inspect_button_->isEnabled() && reset_stream_button_->isEnabled(),
             "Binary stream binding publication or controls"))
    return false;
  auto submit = [&](const QString& text) {
    inspect_input_->setPlainText(text);
    inspect_button_->click();
    QApplication::processEvents();
  };
  auto select = [&](int binding, int flow) {
    host_binding_combo_->setCurrentIndex(binding);
    QApplication::processEvents();
    host_flow_combo_->setCurrentIndex(flow);
    QApplication::processEvents();
    return host_binding_combo_->currentIndex() == binding &&
           host_flow_combo_->currentIndex() == flow;
  };
  submit(QStringLiteral("AA"));
  auto observation = session_.BinaryStreamObservation();
  if (!check(observation && observation->buffered_bytes == 1U && !session_.inspect_result() &&
                 !session_.inspect_failure(),
             "fixed split first step"))
    return false;
  submit(QStringLiteral("01 02"));
  if (!check(session_.inspect_result() &&
                 session_.inspect_result()->fields[0].logical_value == "258",
             "fixed split completion"))
    return false;
  submit(QStringLiteral("AA 03 04 AA 05 06"));
  if (!check(session_.inspect_result() &&
                 session_.inspect_result()->fields[0].logical_value == "772" &&
                 continue_button_->isEnabled(),
             "fixed glued first candidate"))
    return false;
  inspect_input_->setPlainText(QStringLiteral("FF"));
  continue_button_->click();
  QApplication::processEvents();
  if (!check(session_.inspect_result() &&
                 session_.inspect_result()->fields[0].logical_value == "1286" &&
                 inspect_input_->toPlainText() == QStringLiteral("FF") &&
                 !continue_button_->isEnabled(),
             "Continue did not use frozen suffix"))
    return false;
  if (!check(select(0, 1), "select fixed Flow1")) return false;
  submit(QStringLiteral("AA 07 08"));
  if (!check(session_.inspect_result() &&
                 session_.inspect_result()->fields[0].logical_value == "1800",
             "fixed Flow1 result"))
    return false;
  const auto flow_one_generation = session_.BinaryStreamObservation()->generation;
  if (!check(select(0, 0) && inspect_input_->toPlainText() == QStringLiteral("FF") &&
                 session_.inspect_result() &&
                 session_.inspect_result()->fields[0].logical_value == "1286",
             "fixed Flow0 restore"))
    return false;
  reset_stream_button_->click();
  QApplication::processEvents();
  if (!check(!session_.inspect_result() && !session_.inspect_failure(),
             "fixed Flow0 reset"))
    return false;
  if (!check(select(0, 1) && session_.inspect_result() &&
                 session_.inspect_result()->fields[0].logical_value == "1800" &&
                 session_.BinaryStreamObservation()->generation == flow_one_generation,
             "Reset changed another flow"))
    return false;
  const auto before_tab_sequence = session_.BinaryStreamObservation()->step_sequence;
  const int control_tab = control_tabs_->currentIndex();
  const int result_tab = right_tabs_->currentIndex();
  control_tabs_->setCurrentIndex((control_tab + 1) % control_tabs_->count());
  right_tabs_->setCurrentIndex((result_tab + 1) % right_tabs_->count());
  control_tabs_->setCurrentIndex(control_tab);
  right_tabs_->setCurrentIndex(result_tab);
  if (!check(session_.BinaryStreamObservation()->step_sequence == before_tab_sequence &&
                 session_.inspect_result() &&
                 session_.inspect_result()->fields[0].logical_value == "1800",
             "workbench Tab switch executed or changed stream state"))
    return false;
  if (!check(select(1, 0), "select synchronized fixed binding")) return false;
  submit(QStringLiteral("00 A5 5A 01 02 A5 5A 03 04"));
  if (!check(session_.inspect_result() &&
                 session_.inspect_result()->fields[0].logical_value == "258" &&
                 session_.BinaryStreamObservation()->total_discarded_bytes == 1U &&
                 continue_button_->isEnabled(),
             "synchronized fixed first candidate"))
    return false;
  continue_button_->click();
  QApplication::processEvents();
  if (!check(session_.inspect_result() &&
                 session_.inspect_result()->fields[0].logical_value == "772",
             "synchronized fixed Continue"))
    return false;
  if (!check(select(2, 0), "select synchronized length binding")) return false;
  submit(QStringLiteral("00 C3 3C 06 01 02 55 C3 3C 06 03 04 55"));
  if (!check(session_.inspect_result() && session_.inspect_result()->fields.size() == 2U &&
                 session_.inspect_result()->fields[1].logical_value == "258" &&
                 continue_button_->isEnabled(),
             "synchronized length first candidate"))
    return false;
  continue_button_->click();
  QApplication::processEvents();
  if (!check(session_.inspect_result() &&
                 session_.inspect_result()->fields[1].logical_value == "772",
             "synchronized length Continue"))
    return false;
  submit(QStringLiteral("C3"));
  if (!check(!session_.inspect_result() && !session_.inspect_failure(),
             "partial candidate reported failure"))
    return false;
  submit(QStringLiteral("C3 3C 06 00 00 00"));
  if (!check(!session_.inspect_result() && session_.inspect_failure(),
             "Decode failure retained stale result"))
    return false;
  return check(!stream_status_label_->isHidden() && !stream_status_label_->text().isEmpty(),
               "Binary stream status is not visible");
}
#endif

bool DocumentTab::VerifyBinaryHostStage1ForSmoke(QString& error) {
  if (!session_.IsBinaryHostDocument() || session_.BinaryHostActive() ||
      session_.InspectAvailable() || !host_apply_->isEnabled()) {
    error = QStringLiteral("Schema 0.9 did not start unbound");
    return false;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  AddHostDraftRow();
  auto* encode_endpoint = qobject_cast<QLineEdit*>(host_draft_->cellWidget(1, 0));
  auto* encode_action = qobject_cast<QComboBox*>(host_draft_->cellWidget(1, 1));
  const int encode_action_index = encode_action == nullptr ? -1 : encode_action->findData(1);
  if (encode_endpoint == nullptr || encode_action_index < 0) {
    error = QStringLiteral("Binary Host Encode action is not exposed by hostBindingDraft");
    return false;
  }
  encode_endpoint->setText(QStringLiteral("device-encode"));
  encode_action->setCurrentIndex(encode_action_index);
  if (encode_action->currentData().toInt() != 1) {
    error = QStringLiteral("Binary Host Encode action itemData differs");
    return false;
  }
#endif
  host_apply_->click();
  QElapsedTimer timer;
  timer.start();
  while (host_pending_revision_ && timer.elapsed() < 10000) {
    for (const auto ticket : worker_.DrainReadyTickets())
      AcceptCompletion(worker_.TakeResult(ticket));
    if (host_pending_revision_) QThread::msleep(5);
  }
  if (host_pending_revision_ || !session_.BinaryHostActive() || !session_.InspectAvailable()) {
    error = QStringLiteral("explicit Apply did not publish complete-record Decode");
    return false;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (host_binding_combo_->count() != 2) {
    error = QStringLiteral("Binary Host draft did not publish Decode and Encode bindings");
    return false;
  }
  host_binding_combo_->setCurrentIndex(1);
  QApplication::processEvents();
  if (host_binding_combo_->currentIndex() != 1 || session_.mode() != OperationMode::ENCODE ||
      !session_.EncodeAvailable() || message_combo_->currentIndex() < 0 ||
      !message_combo_->currentData().isValid()) {
    error = QStringLiteral("Binary Host Encode binding did not select an Encode Message");
    return false;
  }
  if (!PopulateCanonicalDraftsForSmoke(error) || !EncodeForSmoke(error) ||
      !session_.preview().has_value() || session_.preview()->encoded_frame.empty() ||
      result_kind_label_->property("paeResultState").toString() != QStringLiteral("success")) {
    if (error.isEmpty())
      error = QStringLiteral("Binary Host Encode controls did not produce a typed Encode result");
    return false;
  }
  const QString workbench_state = BinaryStateSignatureForSmoke();
  const QString workbench_input = inspect_input_->toPlainText();
  const auto workbench_preview = session_.preview()->encoded_frame;
  const int control_tab = control_tabs_->currentIndex();
  const int result_tab = right_tabs_->currentIndex();
  control_tabs_->setCurrentIndex(1);
  right_tabs_->setCurrentIndex(1);
  right_tabs_->setCurrentIndex(diagnostic_tab_index_);
  control_tabs_->setCurrentIndex(control_tab);
  right_tabs_->setCurrentIndex(result_tab);
  if (BinaryStateSignatureForSmoke() != workbench_state ||
      inspect_input_->toPlainText() != workbench_input || !session_.preview().has_value() ||
      session_.preview()->encoded_frame != workbench_preview ||
      result_kind_label_->property("paeResultState").toString() != QStringLiteral("success")) {
    error = QStringLiteral("workbench tab roundtrip changed Binary Encode draft or result");
    return false;
  }
  host_binding_combo_->setCurrentIndex(0);
  QApplication::processEvents();
  if (host_binding_combo_->currentIndex() != 0 || session_.mode() != OperationMode::INSPECT ||
      !session_.InspectAvailable()) {
    error = QStringLiteral("Binary Host controls did not return to Decode binding");
    return false;
  }
#endif
  const auto selector_model_count = [this] {
    return host_binding_combo_
               ->findChildren<QAbstractItemModel*>(QString{}, Qt::FindDirectChildrenOnly)
               .size() +
           pipeline_combo_->findChildren<QAbstractItemModel*>(QString{}, Qt::FindDirectChildrenOnly)
               .size() +
           message_combo_->findChildren<QAbstractItemModel*>(QString{}, Qt::FindDirectChildrenOnly)
               .size();
  };
  const int initial_selector_model_count = selector_model_count();
  if (!InspectTextForSmoke(QStringLiteral("80 0D 03 00 01 00 CA FE 05 5A"), error) ||
      InspectFieldCountForSmoke() != 9 ||
      InspectRawValueForSmoke(0) != QStringLiteral("未单独提供") ||
      InspectLogicalValueForSmoke(0) != QStringLiteral("true") ||
      InspectRawValueForSmoke(6) != QStringLiteral("CAFE") ||
      InspectRawValueForSmoke(7) != QStringLiteral("5") ||
      InspectLogicalValueForSmoke(7) != QStringLiteral("5@0")) {
    if (error.isEmpty()) error = QStringLiteral("owned typed/raw result differs");
    return false;
  }
  if (!VerifyInspectFailureForSmoke(QStringLiteral("AA:"), InspectFailureStage::INPUT, QString{},
                                    2U, QString{}, error) ||
      session_.inspect_result()) {
    if (error.isEmpty()) error = QStringLiteral("local rejection retained stale result");
    return false;
  }
  if (!InspectTextForSmoke(QStringLiteral("80 0D 03 00 01 00 CA FE 05 5A"), error)) return false;
  host_flow_combo_->setCurrentIndex(1);
  QApplication::processEvents();
  if (!session_.inspect_draft().empty() || session_.inspect_result()
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
      || !inspect_input_->toPlainText().isEmpty() ||
      !session_.prepared()->binary_host_adapter->Draft(0U, 1U).empty()
#endif
  ) {
    error = QStringLiteral("fresh flow inherited another flow view");
    return false;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (!InspectTextForSmoke(QStringLiteral("80 0D 03 00 02 00 CA FE 05 5A"), error)) return false;
  const auto flow_matches = [this](int flow, const QString& text, int count) {
    const auto& result = session_.inspect_result();
    return host_flow_combo_->currentIndex() == flow &&
           session_.BinaryHostFlowIndex() == static_cast<std::size_t>(flow) &&
           inspect_input_->toPlainText() == text &&
           QString::fromStdString(session_.inspect_draft()) == text && result &&
           result->fields.size() == 9U &&
           result->fields[4].logical_value == std::to_string(count) &&
           result->input_frame.size() == 10U &&
           result->input_frame[4] == static_cast<std::uint8_t>(count);
  };
  const QString first_text = QStringLiteral("80 0D 03 00 01 00 CA FE 05 5A");
  const QString second_text = QStringLiteral("80 0D 03 00 02 00 CA FE 05 5A");
  for (int cycle = 0; cycle < 3; ++cycle) {
    host_flow_combo_->setCurrentIndex(0);
    QApplication::processEvents();
    if (!flow_matches(0, first_text, 1)) {
      error = QStringLiteral("Flow0 editor, Session draft and decoded count/frame differ");
      return false;
    }
    host_flow_combo_->setCurrentIndex(1);
    QApplication::processEvents();
    if (!flow_matches(1, second_text, 2)) {
      error = QStringLiteral("Flow1 editor, Session draft and decoded count/frame differ");
      return false;
    }
  }
#else
  if (!InspectTextForSmoke(QStringLiteral("80 0D 03 00 01 00 CA FE 05 5A"), error)) return false;
#endif
  if (session_.prepared()->binary_host_adapter->Current(0U, 1U) == nullptr) {
    error = QStringLiteral("second Binary flow did not retain its result");
    return false;
  }
  host_flow_combo_->setCurrentIndex(0);
  QApplication::processEvents();
  if (!session_.inspect_result() || session_.inspect_result()->fields.size() != 9U
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
      || !flow_matches(0, first_text, 1)
#endif
  ) {
    error = QStringLiteral("flow switch did not restore the owned result");
    return false;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  const QString uninspected_text = QStringLiteral("80 0D 03 00 03 00 CA FE 05 5A");
  inspect_input_->setPlainText(uninspected_text);
  if (session_.inspect_result()) {
    error = QStringLiteral("editing an uninspected Flow0 draft retained current UI result");
    return false;
  }
  host_flow_combo_->setCurrentIndex(1);
  QApplication::processEvents();
  if (!flow_matches(1, second_text, 2)) {
    error = QStringLiteral("uninspected Flow0 draft overwrote Flow1 view");
    return false;
  }
  host_flow_combo_->setCurrentIndex(0);
  QApplication::processEvents();
  if (inspect_input_->toPlainText() != uninspected_text ||
      QString::fromStdString(session_.inspect_draft()) != uninspected_text ||
      session_.BinaryHostFlowIndex() != 0U ||
      session_.prepared()->binary_host_adapter->Draft(0U, 0U) != Utf16(uninspected_text)) {
    error = QStringLiteral("uninspected Flow0 editor draft was lost on return");
    return false;
  }
  if (!InspectTextForSmoke(first_text, error) || !flow_matches(0, first_text, 1)) return false;
#endif
  const QString before_flow_failure = BinaryStateSignatureForSmoke();
  auto* active_adapter = session_.prepared()->binary_host_adapter.get();
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  const std::u16string before_flow0_draft(active_adapter->Draft(0U, 0U));
  const std::u16string before_flow1_draft(active_adapter->Draft(0U, 1U));
#endif
  const auto active_view_bytes = session_.BinaryActiveViewBytes();
  const auto target_upper = active_adapter->CurrentResultCopyUpperBoundBytes(0U, 1U);
  const auto original_presentation = active_adapter->PresentationRetainedBytes();
  const auto mapped_view_budget = active_adapter->UiViewReserveBytes() / 2U;
  if (target_upper == 0U || active_view_bytes > target_upper || target_upper > mapped_view_budget) {
    error = QStringLiteral("could not arrange Binary combined-view minus-one budget");
    return false;
  }
  const auto injected_presentation = mapped_view_budget - target_upper + 1U;
  if (!active_adapter->SetPresentationRetainedBytes(injected_presentation)) {
    error = QStringLiteral("could not arrange Binary combined-view minus-one budget");
    return false;
  }
  host_flow_combo_->setCurrentIndex(1);
  QApplication::processEvents();
  active_adapter->SetPresentationRetainedBytes(original_presentation);
  if (host_flow_combo_->currentIndex() != 0 || BinaryStateSignatureForSmoke() != before_flow_failure
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
      || inspect_input_->toPlainText() != first_text ||
      active_adapter->Draft(0U, 0U) != before_flow0_draft ||
      active_adapter->Draft(0U, 1U) != before_flow1_draft
#endif
  ) {
    error = QStringLiteral("combined-view rejection split Qt and Session state");
    return false;
  }
  const auto generation = session_.plan_generation();
  auto* endpoint = qobject_cast<QLineEdit*>(host_draft_->cellWidget(0, 0));
  if (!endpoint) {
    error = QStringLiteral("Binary binding draft endpoint is missing");
    return false;
  }
  const QString endpoint_text = endpoint->text();
  endpoint->clear();
  host_apply_->click();
  if (host_pending_revision_ || session_.plan_generation() != generation ||
      !session_.inspect_result()) {
    error = QStringLiteral("invalid Apply changed the active Session or view");
    return false;
  }
  endpoint->setText(endpoint_text);
  hex_view_->FailNextCapacityPreparationForTest();
  host_apply_->click();
  timer.restart();
  while (host_pending_revision_ && timer.elapsed() < 10000) {
    for (const auto ticket : worker_.DrainReadyTickets())
      AcceptCompletion(worker_.TakeResult(ticket));
    QApplication::processEvents(QEventLoop::AllEvents, 20);
    if (host_pending_revision_) QThread::msleep(5);
  }
  if (host_pending_revision_ || session_.plan_generation() != generation ||
      !session_.inspect_result()) {
    error = QStringLiteral("UI capacity preparation failure half-published replacement");
    return false;
  }
  QTimer publish_timer;
  connect(&publish_timer, &QTimer::timeout, this, [&publish_timer] {
    if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
      publish_timer.stop();
      box->button(QMessageBox::Yes)->click();
    }
  });
  publish_timer.start(10);
  host_status_->setText(QStringLiteral("Starting successful replacement smoke."));
  host_apply_->click();
  if (!host_pending_revision_) {
    error = QStringLiteral("successful replacement was not submitted: config_bytes=%1 status=%2")
                .arg(host_config_text_.size())
                .arg(host_status_->text());
    return false;
  }
  timer.restart();
  while (host_pending_revision_ && timer.elapsed() < 10000) {
    for (const auto ticket : worker_.DrainReadyTickets())
      AcceptCompletion(worker_.TakeResult(ticket));
    QApplication::processEvents(QEventLoop::AllEvents, 20);
    if (host_pending_revision_) QThread::msleep(5);
  }
  publish_timer.stop();
  if (host_pending_revision_ || session_.plan_generation() != generation + 1U ||
      session_.inspect_result() || selector_model_count() != initial_selector_model_count) {
    error = QStringLiteral(
                "successful replacement mismatch: pending=%1 generation=%2 expected=%3 "
                "result=%4 selector_models=%5 expected_models=%6 apply_enabled=%7")
                .arg(host_pending_revision_.has_value())
                .arg(session_.plan_generation())
                .arg(generation + 1U)
                .arg(session_.inspect_result().has_value())
                .arg(selector_model_count())
                .arg(initial_selector_model_count)
                .arg(host_apply_->isEnabled()) +
            QStringLiteral(" status=%1").arg(host_status_->text());
    return false;
  }
  if (!InspectTextForSmoke(QStringLiteral("80 0D 03 00 01 00 CA FE 05 5A"), error)) return false;
  {
    QTimer cancel_timer;
    connect(&cancel_timer, &QTimer::timeout, this, [&cancel_timer] {
      if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        cancel_timer.stop();
        box->done(QMessageBox::No);
      }
    });
    cancel_timer.start(10);
    host_apply_->click();
    timer.restart();
    while (host_pending_revision_ && timer.elapsed() < 10000) {
      for (const auto ticket : worker_.DrainReadyTickets())
        AcceptCompletion(worker_.TakeResult(ticket));
      QApplication::processEvents(QEventLoop::AllEvents, 20);
      if (host_pending_revision_) QThread::msleep(5);
    }
  }
  if (host_pending_revision_ || session_.plan_generation() != generation + 1U ||
      !session_.inspect_result() || selector_model_count() != initial_selector_model_count) {
    error = QStringLiteral("cancelled replacement changed Session, result, or selector ownership");
    return false;
  }
  const auto load = session_.load_revision();
  QTimer reload_cancel;
  connect(&reload_cancel, &QTimer::timeout, this, [] {
    if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
      box->done(QMessageBox::No);
  });
  reload_cancel.start(10);
  LoadPath(ConfigPath());
  reload_cancel.stop();
  if (session_.load_revision() != load || !session_.inspect_result()) {
    error = QStringLiteral("cancelled reload changed the active Binary document");
    return false;
  }
  QTimer close_cancel;
  connect(&close_cancel, &QTimer::timeout, this, [] {
    if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
      box->done(QMessageBox::No);
  });
  close_cancel.start(10);
  const bool closed = CloseDocument(true);
  close_cancel.stop();
  if (closed || session_.state() == DocumentState::CLOSED || !session_.inspect_result()) {
    error = QStringLiteral("cancelled close changed the active Binary document");
    return false;
  }
  const QModelIndex temperature_type = field_model_->index(7, FieldTableModel::TYPE);
  const QModelIndex temperature_value = field_model_->index(7, FieldTableModel::VALUE);
  const QModelIndex marker_value = field_model_->index(8, FieldTableModel::VALUE);
  const QString temperature_tooltip =
      field_model_->data(temperature_value, Qt::ToolTipRole).toString();
  const QString marker_tooltip = field_model_->data(marker_value, Qt::ToolTipRole).toString();
  field_table_->selectRow(7);
  RefreshFieldDetails(7);
  right_tabs_->setCurrentIndex(0);
  const QString temperature_details = details_view_->toPlainText();
  if (field_model_->data(temperature_type).toString() != QStringLiteral("DECIMAL64") ||
      !temperature_tooltip.contains(
          QStringLiteral("Logical DECIMAL64 with observed signed raw.")) ||
      !marker_tooltip.contains(QStringLiteral("只读原因：当前为 Decode 结果")) ||
      !marker_tooltip.contains(QStringLiteral("Read-only constant.")) ||
      !temperature_details.contains(QStringLiteral("字段：Temperature")) ||
      !temperature_details.contains(
          QStringLiteral("Logical DECIMAL64 with observed signed raw.")) ||
      !temperature_details.contains(QStringLiteral("转换：逻辑 Decimal64 结果")) ||
      !temperature_details.contains(
          QStringLiteral("物理字节 / 位 / 掩码（从 0 起，LSB0）："))) {
    error = QStringLiteral(
        "Binary readability labels, original descriptions, type, or read-only reason differ");
    return false;
  }
  return true;
}

QString DocumentTab::BinaryStateSignatureForSmoke() const {
  return QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8")
      .arg(session_.load_revision())
      .arg(session_.plan_generation())
      .arg(session_.BinarySessionRevision())
      .arg(session_.BinaryHostBindingIndex())
      .arg(session_.BinaryHostFlowIndex())
      .arg(QString::fromStdString(session_.inspect_draft()))
      .arg(session_.inspect_result() ? session_.inspect_result()->fields.size() : 0U)
      .arg(session_.prepared() && session_.prepared()->binary_host_adapter
               ? session_.prepared()->binary_host_adapter->Instance()
               : 0U);
}

bool DocumentTab::VerifyBinaryReloadFailureForSmoke(QString& error) {
  QTemporaryFile invalid;
  if (!invalid.open() || invalid.write("{\"schema_version\":\"0.9\"}") <= 0 || !invalid.flush()) {
    error = QStringLiteral("could not create invalid reload fixture");
    return false;
  }
  path_edit_->setText(invalid.fileName());
  BeginLoadFromPath(true);
  QElapsedTimer timer;
  timer.start();
  while (session_.state() == DocumentState::LOADING && timer.elapsed() < 10000) {
    for (const auto ticket : worker_.DrainReadyTickets())
      AcceptCompletion(worker_.TakeResult(ticket));
    QApplication::processEvents(QEventLoop::AllEvents, 20);
    if (session_.state() == DocumentState::LOADING) QThread::msleep(5);
  }
  if (session_.state() != DocumentState::CONFIG_ERROR || session_.BinaryHostActive()) {
    error = QStringLiteral("confirmed failed reload state=%1 active=%2 load=%3")
                .arg(static_cast<int>(session_.state()))
                .arg(session_.BinaryHostActive())
                .arg(session_.load_revision());
    return false;
  }
  return true;
}
#endif

void DocumentTab::BuildHostUi(QVBoxLayout* operation_root, QVBoxLayout* binding_root) {
  auto* host_group = new QGroupBox(UiText("绑定与上下文 · 绑定草稿·尚未应用"), this);
  host_panel_ = host_group;
  host_panel_->setObjectName(QStringLiteral("hostPanel"));
  auto* layout = new QVBoxLayout(host_panel_);
  layout->setContentsMargins(0, 0, 0, 0);
  host_toggle_ = new QPushButton(UiText("展开绑定设置"), host_panel_);
  host_toggle_->setObjectName(QStringLiteral("hostPanelToggle"));
  host_toggle_->setCheckable(true);
  host_toggle_->setChecked(true);
  layout->addWidget(host_toggle_);
  host_content_ = new QWidget(host_panel_);
  host_content_->setObjectName(QStringLiteral("hostPanelContent"));
  auto* content_layout = new QVBoxLayout(host_content_);
  content_layout->setContentsMargins(8, 4, 8, 8);
  host_draft_ = new QTableWidget(0, 3, host_content_);
  host_draft_->setObjectName(QStringLiteral("hostBindingDraft"));
  host_draft_->setHorizontalHeaderLabels(
      {UiText("端点（草稿）"), UiText("动作"), UiText("处理管线（Pipeline）ID")});
  host_draft_->horizontalHeader()->setStretchLastSection(true);
  host_draft_->setMaximumHeight(110);
  content_layout->addWidget(host_draft_);
  auto* row = new QHBoxLayout;
  auto* add = new QPushButton(UiText("添加绑定"), host_content_);
  add->setObjectName(QStringLiteral("hostAddBinding"));
  auto* remove = new QPushButton(UiText("移除所选绑定"), host_content_);
  remove->setObjectName(QStringLiteral("hostRemoveBinding"));
  host_apply_ = new QPushButton(UiText("应用绑定表"), host_content_);
  host_apply_->setObjectName(QStringLiteral("hostApply"));
  row->addWidget(add);
  row->addWidget(remove);
  row->addWidget(host_apply_);
  content_layout->addLayout(row);
  layout->addWidget(host_content_);
  host_content_->setVisible(true);
  connect(host_toggle_, &QPushButton::toggled, this, [this](bool expanded) {
    host_content_->setVisible(expanded);
    host_toggle_->setText(expanded ? UiText("收起绑定设置") : UiText("展开绑定设置"));
  });
  host_toggle_->setText(UiText("收起绑定设置"));
  binding_root->addWidget(host_panel_);
  binding_root->addStretch(1);

  host_active_panel_ = new QGroupBox(UiText("当前 Host 上下文"), this);
  host_active_panel_->setObjectName(QStringLiteral("hostActiveContext"));
  auto* active_layout = new QFormLayout(host_active_panel_);
  active_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  host_binding_combo_ = new QComboBox(host_active_panel_);
  host_binding_combo_->setObjectName(QStringLiteral("hostBinding"));
  host_binding_combo_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  active_layout->addRow(UiText("当前绑定 / 动作"), host_binding_combo_);
  host_flow_combo_ = new QComboBox(host_active_panel_);
  host_flow_combo_->setObjectName(QStringLiteral("hostFlow"));
  host_flow_combo_->addItem(UiText("流编号（Flow）0"), 0);
  host_flow_combo_->addItem(UiText("流编号（Flow）1"), 1);
  active_layout->addRow(UiText("当前 Flow"), host_flow_combo_);
  host_status_ = new QLabel(host_active_panel_);
  host_status_->setObjectName(QStringLiteral("hostStatus"));
  host_status_->setWordWrap(true);
  host_status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  active_layout->addRow(UiText("状态"), host_status_);
  operation_root->addWidget(host_active_panel_);
  connect(add, &QPushButton::clicked, this, [this] {
    if (!host_pending_revision_) {
      AddHostDraftRow();
      host_draft_dirty_ = true;
      RefreshState();
    }
  });
  connect(remove, &QPushButton::clicked, this, [this] {
    if (!host_pending_revision_ && host_draft_->currentRow() >= 0) {
      host_draft_->removeRow(host_draft_->currentRow());
      host_draft_dirty_ = true;
      RefreshState();
    }
  });
  connect(host_apply_, &QPushButton::clicked, this, [this] { ApplyHostDraft(); });
  connect(host_binding_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
    if (!rebuilding_selectors_) {
      rebuilding_selectors_ = true;
      host_flow_combo_->setCurrentIndex(0);
      rebuilding_selectors_ = false;
      SelectHostView();
    }
  });
  connect(host_flow_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
    if (!rebuilding_selectors_) SelectHostView();
  });
}
void DocumentTab::AddHostDraftRow() {
  if (!session_.description() || host_draft_->rowCount() >= 64) return;
  const int row = host_draft_->rowCount();
  host_draft_->insertRow(row);
  auto* endpoint = new QLineEdit(QStringLiteral("device"), host_draft_);
  endpoint->setMaxLength(256);
  auto* action = new QComboBox(host_draft_);
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  if (session_.IsBinaryHostDocument()) {
    action->addItem(UiText("解析"), 0);
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    action->addItem(UiText("组包"), 1);
#endif
  } else
#endif
  {
    action->addItem(UiText("解析"), 0);
    action->addItem(UiText("组包"), 1);
  }
  auto* pipeline = new QComboBox(host_draft_);
  for (const auto& item : session_.description()->pipelines)
    pipeline->addItem(FromUtf8(item.id),
                      QVariant::fromValue(static_cast<qulonglong>(item.pipeline_index)));
  host_draft_->setCellWidget(row, 0, endpoint);
  host_draft_->setCellWidget(row, 1, action);
  host_draft_->setCellWidget(row, 2, pipeline);
  connect(endpoint, &QLineEdit::textChanged, this, [this] {
    if (!rebuilding_selectors_) {
      host_draft_dirty_ = true;
      RefreshState();
    }
  });
  connect(action, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
    if (!rebuilding_selectors_) {
      host_draft_dirty_ = true;
      RefreshState();
    }
  });
  connect(pipeline, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
    if (!rebuilding_selectors_) {
      host_draft_dirty_ = true;
      RefreshState();
    }
  });
}
void DocumentTab::InitializeHostDraft() {
  rebuilding_selectors_ = true;
  host_draft_dirty_ = true;
  host_draft_->setRowCount(0);
  host_binding_combo_->clear();
  host_flow_combo_->setCurrentIndex(0);
  if (session_.IsAsciiDocument()
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
      || session_.IsBinaryHostDocument()
#endif
  ) {
    AddHostDraftRow();
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
    if (session_.IsBinaryHostDocument()) {
      if (auto* action = qobject_cast<QComboBox*>(host_draft_->cellWidget(0, 1)))
        action->setCurrentIndex(0);
      host_status_->setText(
          UiText("Schema 0.9 尚未绑定。请编辑草稿并显式应用；发布成功前解析保持禁用。"));
      rebuilding_selectors_ = false;
      return;
    }
#endif
    AddHostDraftRow();
    if (auto* action = qobject_cast<QComboBox*>(host_draft_->cellWidget(1, 1)))
      action->setCurrentIndex(1);
    host_status_->setText(
        UiText("当前仅为草稿。请显式应用以启用 Host Session；当前执行仍为旧版离线模式。"));
  }
  rebuilding_selectors_ = false;
}
void DocumentTab::ApplyHostDraft() {
  if ((!session_.IsAsciiDocument()
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
       && !session_.IsBinaryHostDocument()
#endif
           ) ||
      host_pending_revision_ || host_config_text_.empty())
    return;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  const bool binary = session_.IsBinaryHostDocument();
#else
  const bool binary = false;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  if (!binary) {
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
    const auto current_bytes =
        session_.HostActive()
            ? session_.prepared()->host_adapter->AccountedBytes()
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
            : (session_.prepared()->public_ascii_adapter
                   ? session_.prepared()->public_ascii_adapter->InstanceAdmissionBytes()
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
                   : (session_.prepared()->public_ascii_stream_adapter
                          ? session_.prepared()->public_ascii_stream_adapter->AccountedBytes()
                          : session_.prepared()->ascii_adapter->HostTransitionAdmissionBytes()));
#else
                   : session_.prepared()->ascii_adapter->HostTransitionAdmissionBytes());
#endif
#else
            : session_.prepared()->ascii_adapter->HostTransitionAdmissionBytes();
#endif
    if (current_bytes > protocol_lab::ascii::HostObserverAdapter::kMaximumAccountedBytes) {
      host_status_->setText(UiText("当前文档超过 Host 切换准入上限，未创建候选项。"));
      return;
    }
#endif
  }
#else
  if (!binary) return;
#endif
  const auto high_bit = Revision{1} << 63U;
  if (host_request_sequence_ == high_bit - 1U) {
    host_status_->setText(UiText("绑定请求序号已耗尽，请重新打开文档。"));
    return;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  std::vector<AsciiHostBinding> bindings;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  std::vector<BinaryHostBinding> binary_bindings;
#endif
  for (int row = 0; row < host_draft_->rowCount(); ++row) {
    const auto* endpoint = qobject_cast<QLineEdit*>(host_draft_->cellWidget(row, 0));
    const auto* action = qobject_cast<QComboBox*>(host_draft_->cellWidget(row, 1));
    const auto* pipeline = qobject_cast<QComboBox*>(host_draft_->cellWidget(row, 2));
    if (!endpoint || !action || !pipeline || pipeline->currentIndex() < 0 ||
        endpoint->text().isEmpty() || endpoint->text().toUtf8().size() > 256) {
      host_status_->setText(UiText("绑定草稿无效；当前 Session 保持不变。"));
      return;
    }
    const auto pipeline_index = static_cast<std::size_t>(pipeline->currentData().toULongLong());
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
    if (binary) {
      if ((action->currentIndex() != 0 && action->currentIndex() != 1) ||
          pipeline_index >= session_.description()->pipelines.size()) {
        host_status_->setText(
            QStringLiteral("Binary Host 绑定无效；当前 Session 保持不变。"));
        return;
      }
      binary_bindings.push_back({Utf8(endpoint->text()),
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
                                 action->currentIndex() == 0 ? pae::HostAction::DECODE
                                                             : pae::HostAction::ENCODE,
#else
                                 action->currentIndex() == 0 ? host_endpoint::Action::DECODE
                                                             : host_endpoint::Action::ENCODE,
#endif
                                 session_.description()->pipelines[pipeline_index].id,
                                 action->currentIndex() == 0 ? 2U : 1U});
    } else
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
      bindings.push_back({Utf8(endpoint->text()),
                          action->currentIndex() == 0 ? AsciiHostAction::DECODE
                                                      : AsciiHostAction::ENCODE,
                          pipeline_index});
#else
      return;
#endif
  }
  if (
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
      bindings.empty()
#else
      true
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
      && binary_bindings.empty()
#endif
  ) {
    host_status_->setText(UiText("至少需要一条绑定。"));
    return;
  }
  host_pending_revision_ = high_bit | ++host_request_sequence_;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  if (binary) {
    binary_host_pending_bindings_ = std::move(binary_bindings);
    binary_host_pending_identity_ = BinaryPreparationIdentity{
        session_.id(), session_.load_revision(), session_.BinarySessionRevision() + 1U,
        *host_pending_revision_, session_.prepared()->config_sha256};
  } else
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
    host_pending_bindings_ = std::move(bindings);
#else
    return;
#endif
  if (worker_.Submit(session_.id(), *host_pending_revision_, host_config_text_) !=
      SubmitStatus::ACCEPTED) {
    host_pending_revision_.reset();
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
    host_pending_bindings_.clear();
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
    binary_host_pending_bindings_.clear();
    binary_host_pending_identity_.reset();
#endif
    host_status_->setText(UiText("准备调度器拒绝了请求；当前 Session 保持不变。"));
  } else
    host_status_->setText(UiText("正在准备候选 Session；确认发布前保留当前生效状态。"));
  RefreshState();
}
void DocumentTab::AcceptHostCompletion(std::unique_ptr<CompileCompletion> completion) {
  host_pending_revision_.reset();
  std::string error;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  if (binary_host_pending_identity_) {
    auto identity = std::move(*binary_host_pending_identity_);
    binary_host_pending_identity_.reset();
    std::unique_ptr<BinaryHostAdapter> candidate;
    QStandardItemModel* selector_model = nullptr;
    QStandardItemModel* pipeline_model = nullptr;
    QStandardItemModel* message_model = nullptr;
    if (completion->document_id == identity.document &&
        completion->load_revision == identity.request &&
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
        completion->route == SchemaDispatchStatus::BINARY_PUBLIC && completion->public_compiled &&
        !completion->public_diagnostic &&
#else
        completion->artifacts && !completion->diagnostic &&
#endif
        session_.prepared() && completion->config_sha256 == identity.config_sha256 &&
        session_.load_revision() == identity.load &&
        session_.BinarySessionRevision() + 1U == identity.session) {
      const auto* previous =
          session_.BinaryHostActive() ? session_.prepared()->binary_host_adapter.get() : nullptr;
      try {
        selector_model = new QStandardItemModel(host_binding_combo_);
        pipeline_model = new QStandardItemModel(pipeline_combo_);
        message_model = new QStandardItemModel(message_combo_);
        for (const auto& binding : binary_host_pending_bindings_)
          selector_model->appendRow(new QStandardItem(FromUtf8(binding.endpoint) +
                                                      (binding.action == pae::HostAction::DECODE
                                                           ? UiText(" / 解析 / ")
                                                           : UiText(" / 组包 / ")) +
                                                      FromUtf8(binding.pipeline_id)));
        for (const auto& pipeline : session_.description()->pipelines) {
          const auto& label = pipeline.display_name.empty() ? pipeline.id : pipeline.display_name;
          auto* item = new QStandardItem(FromUtf8(label));
          item->setData(QVariant::fromValue(static_cast<qulonglong>(pipeline.pipeline_index)),
                        Qt::UserRole);
          pipeline_model->appendRow(item);
        }
        for (const auto& message : session_.description()->messages) {
          const auto& label = message.display_name.empty() ? message.id : message.display_name;
          auto* item = new QStandardItem(FromUtf8(label));
          item->setData(QVariant::fromValue(static_cast<qulonglong>(message.message_index)),
                        Qt::UserRole);
          message_model->appendRow(item);
        }
        std::size_t retained_ui_bytes = host_config_text_.capacity() + 1U;
        const auto add_qstring = [&](const QString& value) {
          const auto bytes = static_cast<std::size_t>(value.capacity()) * sizeof(QChar);
          if (retained_ui_bytes > (std::numeric_limits<std::size_t>::max)() - bytes)
            throw std::bad_alloc{};
          retained_ui_bytes += bytes;
        };
        for (int row = 0; row < host_draft_->rowCount(); ++row) {
          for (int column = 0; column < host_draft_->columnCount(); ++column) {
            if (auto* edit = qobject_cast<QLineEdit*>(host_draft_->cellWidget(row, column)))
              add_qstring(edit->text());
            if (auto* combo = qobject_cast<QComboBox*>(host_draft_->cellWidget(row, column)))
              add_qstring(combo->currentText());
          }
        }
        for (int row = 0; row < selector_model->rowCount(); ++row)
          add_qstring(selector_model->item(row)->text());
        for (int row = 0; row < pipeline_model->rowCount(); ++row)
          add_qstring(pipeline_model->item(row)->text());
        for (int row = 0; row < message_model->rowCount(); ++row)
          add_qstring(message_model->item(row)->text());
        const auto selector_items = static_cast<std::size_t>(
            selector_model->rowCount() + pipeline_model->rowCount() + message_model->rowCount());
        constexpr auto selector_item_bytes = sizeof(QStandardItem) + sizeof(QStandardItem*);
        if (selector_items >
            ((std::numeric_limits<std::size_t>::max)() - 3U * sizeof(QStandardItemModel)) /
                selector_item_bytes)
          throw std::bad_alloc{};
        const auto selector_objects =
            3U * sizeof(QStandardItemModel) + selector_items * selector_item_bytes;
        if (retained_ui_bytes > (std::numeric_limits<std::size_t>::max)() - selector_objects)
          throw std::bad_alloc{};
        retained_ui_bytes += selector_objects;
        std::size_t preparation_coexisting_bytes =
            previous == nullptr && session_.description()
                ? BinaryHostAdapter::AccountDescriptionBytes(*session_.description())
                : 0U;
        const auto add_coexisting = [&](std::size_t bytes) {
          if (preparation_coexisting_bytes > (std::numeric_limits<std::size_t>::max)() - bytes)
            throw std::bad_alloc{};
          preparation_coexisting_bytes += bytes;
        };
        add_coexisting(static_cast<std::size_t>(inspect_input_->toPlainText().capacity()) *
                       sizeof(QChar));
        add_coexisting(session_.inspect_draft().capacity() + 1U);
        add_coexisting((session_.inspect_draft_utf16().capacity() + 1U) * sizeof(char16_t));
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
        candidate = BinaryHostAdapter::CreatePublic(
            std::move(*completion->public_compiled), std::move(binary_host_pending_bindings_),
            std::move(identity), previous, retained_ui_bytes, preparation_coexisting_bytes, error);
#else
        candidate = BinaryHostAdapter::Create(
            std::move(*completion->artifacts), std::move(binary_host_pending_bindings_),
            std::move(identity), previous, retained_ui_bytes, preparation_coexisting_bytes, error);
#endif
      } catch (const std::exception& exception) {
        error = exception.what();
      }
    }
    binary_host_pending_bindings_.clear();
    if (!candidate) {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
      if (error.empty()) {
        if (completion->route == SchemaDispatchStatus::CLASSIFICATION_FAILED)
          error = "schema classification failed: " + completion->classification_error;
        else if (completion->public_diagnostic)
          error = "public Binary compiler failed: " + completion->public_diagnostic->detail;
        else
          error = "public Binary preparation result was stale or incomplete";
      }
#endif
      delete selector_model;
      delete pipeline_model;
      delete message_model;
      host_status_->setText(
          UiText("准备失败；已保留当前 Session 和重试草稿。\n%1").arg(FromUtf8(error)));
      RefreshState();
      return;
    }
    auto publication =
        session_.PrepareBinaryHostPublication(std::move(candidate), completion->load_revision);
    if (!publication) {
      delete selector_model;
      delete pipeline_model;
      delete message_model;
      host_status_->setText(UiText("候选发布准备失败；已保留当前 Session 和重试草稿。"));
      RefreshState();
      return;
    }
    std::size_t maximum_fields = 0U;
    try {
      for (const auto& message : publication->description.messages)
        maximum_fields = (std::max)(maximum_fields, message.fields.size());
    } catch (const std::exception&) {
      delete selector_model;
      delete pipeline_model;
      delete message_model;
      host_status_->setText(UiText("选择器准备分配失败；已保留当前 Session 和重试草稿。"));
      RefreshState();
      return;
    }
    bool presentation_prepared = false;
    try {
      binary_highlights_.reserve(publication->description.max_frame_bytes);
      presentation_prepared = field_model_->PrepareCapacity(maximum_fields) &&
                              hex_view_->PrepareCapacity(publication->description.max_frame_bytes);
    } catch (const std::exception&) {
      presentation_prepared = false;
    }
    const auto row_bytes = field_model_->AccountedRowCapacityBytes();
    const auto highlight_bytes = binary_highlights_.capacity() * sizeof(PhysicalBitMask);
    const auto presentation_budget = publication->adapter->UiViewReserveBytes() / 2U;
    const auto presentation_within_budget =
        row_bytes <= presentation_budget && highlight_bytes <= presentation_budget - row_bytes;
    if (!presentation_prepared || !presentation_within_budget ||
        hex_view_->AccountedCapacityBytes() > publication->adapter->HexPreviewReserveBytes()) {
      delete selector_model;
      delete pipeline_model;
      delete message_model;
      host_status_->setText(UiText("UI 视图准备分配失败；已保留当前 Session 和重试草稿。"));
      RefreshState();
      return;
    }
    if (!publication->adapter->SetPresentationRetainedBytes(row_bytes + highlight_bytes)) {
      delete selector_model;
      delete pipeline_model;
      delete message_model;
      host_status_->setText(UiText("UI 展示计费失败；已保留当前 Session 和重试草稿。"));
      RefreshState();
      return;
    }
    if (session_.BinaryHasDiscardableState()) {
      const auto answer = AskChineseQuestion(this, UiText("替换 Binary Host Session"),
                                             UiText("发布将丢弃当前 Binary 流草稿和结果。是否继续？"));
      if (answer != QMessageBox::Yes) {
        delete selector_model;
        delete pipeline_model;
        delete message_model;
        host_status_->setText(UiText("已取消发布；已保留当前 Session 和重试草稿。"));
        RefreshState();
        return;
      }
    }
    rebuilding_selectors_ = true;
    QPointer<QAbstractItemModel> old_host_model = host_binding_combo_->model();
    QPointer<QAbstractItemModel> old_pipeline_model = pipeline_combo_->model();
    QPointer<QAbstractItemModel> old_message_model = message_combo_->model();
    host_binding_combo_->setModel(selector_model);
    pipeline_combo_->setModel(pipeline_model);
    message_combo_->setModel(message_model);
    host_binding_combo_->setCurrentIndex(0);
    host_flow_combo_->setCurrentIndex(0);
    pipeline_combo_->setCurrentIndex(
        pipeline_combo_->findData(static_cast<qulonglong>(publication->selection.pipeline_index)));
    message_combo_->setCurrentIndex(
        message_combo_->findData(static_cast<qulonglong>(publication->selection.message_index)));
    mode_combo_->setCurrentIndex(mode_combo_->findData(static_cast<int>(session_.mode())));
    representation_combo_->setCurrentIndex(
        representation_combo_->findData(static_cast<int>(session_.representation())));
    inspect_input_->clear();
    accepted_inspect_text_.clear();
    rebuilding_selectors_ = false;
    if (old_host_model) delete old_host_model.data();
    if (old_pipeline_model) delete old_pipeline_model.data();
    if (old_message_model) delete old_message_model.data();
    session_.PublishBinaryHostPublication(std::move(*publication));
    host_draft_dirty_ = false;
    field_model_->Reset(CurrentMessage(), {}, {}, false, session_.representation(),
                        FieldPresentationAction::INSPECT);
    RefreshInspect();
    RefreshModePresentation();
    RefreshState();
    host_status_->setText(UiText("Binary Host Session 已发布。"));
    return;
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  std::unique_ptr<AsciiHostAdapter> candidate;
  if (session_.prepared() && completion->config_sha256 == session_.prepared()->config_sha256) {
    if (completion->route == SchemaDispatchStatus::ASCII_PUBLIC && completion->public_compiled &&
        !completion->public_diagnostic) {
      const auto previous =
          session_.HostActive()
              ? session_.prepared()->host_adapter->AccountedBytes()
              : (session_.prepared()->public_ascii_adapter
                     ? session_.prepared()->public_ascii_adapter->InstanceAdmissionBytes()
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
                     : session_.prepared()->public_ascii_stream_adapter->AccountedBytes()
#else
                     : 0U
#endif
                );
      candidate =
          AsciiHostAdapter::CreatePublic(std::move(*completion->public_compiled),
                                         std::move(host_pending_bindings_), previous, error);
    }
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
    else if (completion->route == SchemaDispatchStatus::PRIVATE_ASCII && completion->artifacts &&
               !completion->diagnostic) {
      candidate = CreatePrivateAsciiHostAdapter(std::move(*completion->artifacts),
                                                std::move(host_pending_bindings_), error);
    }
#endif
  }
#else
  std::unique_ptr<protocol_lab::ascii::HostObserverAdapter> candidate;
  if (completion->artifacts && !completion->diagnostic && session_.prepared() &&
      completion->config_sha256 == session_.prepared()->config_sha256) {
    std::vector<protocol_lab::ascii::HostBinding> private_bindings;
    private_bindings.reserve(host_pending_bindings_.size());
    for (const auto& binding : host_pending_bindings_)
      private_bindings.push_back(
          {binding.endpoint,
           binding.action == AsciiHostAction::DECODE ? host_endpoint::Action::DECODE
                                                     : host_endpoint::Action::ENCODE,
           binding.pipeline_index});
    candidate = protocol_lab::ascii::HostObserverAdapter::Create(
        std::move(*completion->artifacts), std::move(private_bindings), error);
  }
#endif
  host_pending_bindings_.clear();
  if (!candidate) {
    host_status_->setText(UiText("准备失败；当前状态保持不变。\n%1").arg(FromUtf8(error)));
    RefreshState();
    return;
  }
  // Each active/candidate adapter has an admission cap; both may coexist only during preparation.
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  if (session_.HostActive() &&
      candidate->AccountedBytes() >
          2U * protocol_lab::ascii::HostObserverAdapter::kMaximumAccountedBytes -
              session_.prepared()->host_adapter->AccountedBytes()) {
    host_status_->setText(UiText("重新绑定超过峰值准入上限；当前状态保持不变。"));
    RefreshState();
    return;
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  if (session_.HostActive() && session_.prepared()->host_adapter->IsPublicCompleteRecord() &&
      session_.HostHasDiscardableState()) {
    const auto answer = AskChineseQuestion(this, UiText("替换 ASCII Host Session"),
                                           UiText("发布将丢弃当前 ASCII 流草稿和结果。是否继续？"));
    if (answer != QMessageBox::Yes) {
      host_status_->setText(UiText("已取消发布；已保留当前 Session 和重试草稿。"));
      RefreshState();
      return;
    }
  }
#endif
  if (!ConfirmStreamDiscardOnly(UiText("发布替换后的绑定表"))) {
    host_status_->setText(UiText("已取消发布；所有活动流均已保留。"));
    RefreshState();
    return;
  }
  try {
    if (!session_.ApplyHostAdapter(std::move(candidate))) {
      host_status_->setText(UiText("候选发布被拒绝。"));
      RefreshState();
      return;
    }
  } catch (const std::exception&) {
    host_status_->setText(UiText("发布准备分配失败；当前 Session 保持不变。"));
    RefreshState();
    return;
  }
  host_draft_dirty_ = false;
  rebuilding_selectors_ = true;
  host_binding_combo_->clear();
  for (const auto& binding : session_.prepared()->host_adapter->Bindings())
    host_binding_combo_->addItem(
        FromUtf8(binding.endpoint) +
        (IsDecodeHostAction(binding.action) ? UiText(" / 解析 / ") : UiText(" / 组包 / ")) +
        FromUtf8(session_.description()->pipelines[binding.pipeline_index].id));
  host_binding_combo_->setCurrentIndex(0);
  host_flow_combo_->setCurrentIndex(0);
  rebuilding_selectors_ = false;
  SelectHostView();
#else
  host_status_->setText(UiText("准备结果不属于当前 Binary Host 请求。"));
#endif
}
void DocumentTab::SelectHostView() {
  if (host_binding_combo_->currentIndex() < 0) return;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  if (session_.BinaryHostActive()) {
    const int previous_binding = static_cast<int>(session_.BinaryHostBindingIndex());
    const int previous_flow = static_cast<int>(session_.BinaryHostFlowIndex());
    const QString previous_text = accepted_inspect_text_;
    const auto rollback_binary_ui = [&] {
      rebuilding_selectors_ = true;
      host_binding_combo_->setCurrentIndex(previous_binding);
      host_flow_combo_->setCurrentIndex(previous_flow);
      inspect_input_->setPlainText(previous_text);
      accepted_inspect_text_ = previous_text;
      rebuilding_selectors_ = false;
    };
    auto publication = session_.PrepareBinaryHostFlow(
        static_cast<std::size_t>(host_binding_combo_->currentIndex()),
        static_cast<std::size_t>(host_flow_combo_->currentIndex()));
    if (!publication) {
      rollback_binary_ui();
      return;
    }
    const QString target_text =
        QString::fromUtf16(reinterpret_cast<const ushort*>(publication->inspect_draft_utf16.data()),
                           static_cast<int>(publication->inspect_draft_utf16.size()));
    rebuilding_selectors_ = true;
    inspect_input_->setPlainText(target_text);
    accepted_inspect_text_ = target_text;
    rebuilding_selectors_ = false;
    const bool published = session_.PublishBinaryHostFlow(std::move(*publication));
    if (!published) {
      rollback_binary_ui();
      return;
    }
  } else
#endif
  {
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
    if (!session_.HostActive()) return;
    if (!session_.SelectHostFlow(static_cast<std::size_t>(host_binding_combo_->currentIndex()),
                                 static_cast<std::size_t>(host_flow_combo_->currentIndex())))
      return;
#else
    return;
#endif
  }
  rebuilding_selectors_ = true;
  mode_combo_->setCurrentIndex(mode_combo_->findData(static_cast<int>(session_.mode())));
  representation_combo_->setCurrentIndex(
      representation_combo_->findData(static_cast<int>(session_.representation())));
  if (!session_.BinaryHostActive()) {
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
    const auto& text = session_.inspect_draft_utf16();
    inspect_input_->setPlainText(QString::fromUtf16(reinterpret_cast<const ushort*>(text.data()),
                                                    static_cast<int>(text.size())));
    accepted_inspect_text_ = inspect_input_->toPlainText();
#endif
  } else {
    pipeline_combo_->setCurrentIndex(
        pipeline_combo_->findData(static_cast<qulonglong>(*session_.selected_pipeline_index())));
    message_combo_->setCurrentIndex(
        message_combo_->findData(static_cast<qulonglong>(session_.selection()->message_index)));
  }
  rebuilding_selectors_ = false;
  timing_label_->clear();
  if (session_.BinaryHostActive()) {
    if (session_.mode() == OperationMode::ENCODE)
      RebuildMessageSelector();
    else
      field_model_->Reset(CurrentMessage(), {}, {}, false, session_.representation(),
                          FieldPresentationAction::INSPECT);
  } else {
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
    RebuildSelectorsAndModel();
#endif
  }
  RefreshInspect();
  RefreshPreview();
  RefreshModePresentation();
  RefreshState();
  host_status_->setText(
      UiText("Host Session 已生效 | Tab=%1 加载=%2 Session=%3 绑定=%4 "
             "流编号（Flow）=%5。草稿编辑需应用后生效；切换视图会保留各流状态。")
          .arg(session_.id())
          .arg(session_.load_revision())
          .arg(session_.plan_generation())
          .arg(
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
              session_.BinaryHostActive() ? session_.BinaryHostBindingIndex() :
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
                                          session_.HostBindingIndex()
#else
                                          0U
#endif
              )
          .arg(
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
              session_.BinaryHostActive() ? session_.BinaryHostFlowIndex() :
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
                                          session_.HostStreamIndex()
#else
                                          0U
#endif
              ));
}
#endif

bool DocumentTab::VerifyLocalizationAnchorsForSmoke(QString& error) {
  const auto require_name = [&error](const QObject* object, const char* expected) {
    if (object != nullptr && object->objectName() == QLatin1String(expected)) return true;
    error = QStringLiteral("missing stable UI anchor: %1").arg(QLatin1String(expected));
    return false;
  };
  if (!require_name(path_edit_, "configPath") ||
      !require_name(config_path_toggle_, "configPathToggle") ||
      !require_name(load_button_, "loadConfig") ||
      !require_name(mode_combo_, "operationMode") ||
      !require_name(pipeline_combo_, "pipelineSelector") ||
      !require_name(message_combo_, "messageSelector") ||
      !require_name(representation_combo_, "inputRepresentation") ||
      !require_name(inspect_button_, "primaryAction") ||
      !require_name(result_kind_label_, "resultStatus") ||
      !require_name(diagnostic_label_, "diagnosticView") ||
      !require_name(field_table_, "fieldTable") || !require_name(hex_view_, "hexView") ||
      !require_name(right_tabs_, "resultDetailsTabs") ||
      !require_name(control_tabs_, "controlTabs") ||
      !require_name(workbench_splitter_, "workbenchSplitter") ||
      !require_name(result_splitter_, "resultWorkspaceSplitter"))
    return false;
  if (findChild<QScrollArea*>(QStringLiteral("operationScroll")) == nullptr ||
      findChild<QScrollArea*>(QStringLiteral("bindingScroll")) == nullptr ||
      findChild<QScrollArea*>(QStringLiteral("diagnosticScroll")) == nullptr) {
    error = QStringLiteral("local workbench scroll anchor is missing");
    return false;
  }
  if (workbench_splitter_->orientation() != Qt::Horizontal || workbench_splitter_->count() != 2 ||
      result_splitter_->orientation() != Qt::Vertical || result_splitter_->count() != 2 ||
      control_tabs_->count() != 2 || right_tabs_->count() < 3 ||
      field_table_->horizontalHeader()->length() <= field_table_->viewport()->width()) {
    error = QStringLiteral("workbench splitters, tabs, or field horizontal reachability differ");
    return false;
  }
  if (field_table_->columnWidth(FieldTableModel::TYPE) <
          field_table_->fontMetrics().horizontalAdvance(QStringLiteral("DECIMAL64")) + 24 ||
      !(details_view_->textInteractionFlags() & Qt::TextSelectableByMouse) ||
      !(details_view_->textInteractionFlags() & Qt::TextSelectableByKeyboard) ||
      field_model_->headerData(FieldTableModel::TYPE, Qt::Horizontal, Qt::ToolTipRole)
          .toString()
          .isEmpty()) {
    error = QStringLiteral(
                "field readability anchor differs: type_width=%1 required=%2 detail_flags=%3 "
                "type_header_help=%4")
                .arg(field_table_->columnWidth(FieldTableModel::TYPE))
                .arg(field_table_->fontMetrics().horizontalAdvance(QStringLiteral("DECIMAL64")) +
                     24)
                .arg(static_cast<int>(details_view_->textInteractionFlags()))
                .arg(field_model_
                         ->headerData(FieldTableModel::TYPE, Qt::Horizontal, Qt::ToolTipRole)
                         .toString());
    return false;
  }
  if (mode_combo_->findData(static_cast<int>(OperationMode::ENCODE)) < 0 ||
      mode_combo_->findData(static_cast<int>(OperationMode::INSPECT)) < 0 ||
      representation_combo_->findData(static_cast<int>(ByteRepresentation::HEX)) < 0 ||
      representation_combo_->findData(static_cast<int>(ByteRepresentation::ASCII_ESCAPED)) < 0) {
    error = QStringLiteral("stable mode/representation itemData is missing");
    return false;
  }
  if (field_model_->rowCount() > 0 &&
      !field_model_->index(0, FieldTableModel::ID).data(FieldTableModel::FieldIndexRole).isValid()) {
    error = QStringLiteral("stable field model role is missing");
    return false;
  }
  const auto state_before_tabs = session_.state();
  const auto generation_before_tabs = session_.plan_generation();
  const auto preview_before_tabs = PreviewFrameSizeForSmoke();
  const QString input_before_tabs = inspect_input_->toPlainText();
  const int field_row_before_tabs = field_table_->currentIndex().row();
  const int control_before = control_tabs_->currentIndex();
  const int result_before = right_tabs_->currentIndex();
  for (int index = 0; index < control_tabs_->count(); ++index)
    if (control_tabs_->isTabEnabled(index)) control_tabs_->setCurrentIndex(index);
  for (int index = 0; index < right_tabs_->count(); ++index)
    if (right_tabs_->isTabEnabled(index)) right_tabs_->setCurrentIndex(index);
  control_tabs_->setCurrentIndex(control_before);
  right_tabs_->setCurrentIndex(result_before);
  if (session_.state() != state_before_tabs ||
      session_.plan_generation() != generation_before_tabs ||
      PreviewFrameSizeForSmoke() != preview_before_tabs ||
      inspect_input_->toPlainText() != input_before_tabs ||
      field_table_->currentIndex().row() != field_row_before_tabs) {
    error = QStringLiteral("workbench tab navigation changed execution, draft, result, or selection");
    return false;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_BINDING_UI)
  if (!host_panel_->isHidden()) {
    if (qobject_cast<QGroupBox*>(host_panel_) == nullptr || host_content_ == nullptr ||
        host_toggle_ == nullptr) {
      error = QStringLiteral("collapsible Host panel anchor is missing");
      return false;
    }
    const auto state_before = session_.state();
    const auto generation_before = session_.plan_generation();
    const auto preview_size_before = PreviewFrameSizeForSmoke();
    const bool checked_before = host_toggle_->isChecked();
    host_toggle_->setChecked(!checked_before);
    const bool visibility_matches = host_content_->isHidden() == checked_before;
    host_toggle_->setChecked(checked_before);
    if (!visibility_matches || session_.state() != state_before ||
        session_.plan_generation() != generation_before ||
        PreviewFrameSizeForSmoke() != preview_size_before) {
      error = QStringLiteral("Host panel collapse changed execution state or visibility mapping");
      return false;
    }
    host_toggle_->setChecked(false);
    if (!host_binding_combo_->isVisibleTo(this) ||
        !host_flow_combo_->isVisibleTo(this) || !host_status_->isVisibleTo(this) ||
        session_.state() != state_before || session_.plan_generation() != generation_before ||
        PreviewFrameSizeForSmoke() != preview_size_before) {
      error = QStringLiteral("collapsed Host settings hid active context or changed execution state");
      host_toggle_->setChecked(checked_before);
      return false;
    }
    host_toggle_->setChecked(checked_before);
  }
#endif
  return true;
}

void DocumentTab::BuildUi() {
  setObjectName(QStringLiteral("documentTab"));
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(8, 6, 8, 8);
  auto* config_group = new QGroupBox(UiText("文档与编译"), this);
  config_group->setObjectName(QStringLiteral("documentConfigGroup"));
  auto* config_layout = new QVBoxLayout(config_group);
  config_layout->setContentsMargins(8, 4, 8, 6);
  auto* config_summary = new QHBoxLayout;
  config_name_label_ = new QLabel(UiText("未选择配置"), config_group);
  config_name_label_->setObjectName(QStringLiteral("configFileName"));
  config_name_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  config_name_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  identity_label_ = new QLabel(config_group);
  identity_label_->setObjectName(QStringLiteral("documentIdentity"));
  identity_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  identity_label_->setWordWrap(true);
  identity_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  config_summary->addWidget(config_name_label_, 1);
  browse_button_ = new QPushButton(UiText("浏览..."), config_group);
  browse_button_->setObjectName(QStringLiteral("browseConfig"));
  load_button_ = new QPushButton(UiText("加载 / 重新加载"), config_group);
  load_button_->setObjectName(QStringLiteral("loadConfig"));
  config_path_toggle_ = new QPushButton(UiText("展开完整路径"), config_group);
  config_path_toggle_->setObjectName(QStringLiteral("configPathToggle"));
  config_path_toggle_->setCheckable(true);
  config_summary->addWidget(browse_button_);
  config_summary->addWidget(load_button_);
  config_summary->addWidget(config_path_toggle_);
  config_layout->addLayout(config_summary);
  config_layout->addWidget(identity_label_);

  config_path_details_ = new QWidget(config_group);
  config_path_details_->setObjectName(QStringLiteral("configPathDetails"));
  auto* path_row = new QHBoxLayout(config_path_details_);
  path_row->setContentsMargins(0, 0, 0, 0);
  path_row->addWidget(new QLabel(UiText("完整路径"), config_path_details_));
  path_edit_ = new QLineEdit(config_path_details_);
  path_edit_->setObjectName(QStringLiteral("configPath"));
  path_edit_->setPlaceholderText(UiText("PAE 配置 JSON 路径"));
  path_row->addWidget(path_edit_, 1);
  config_layout->addWidget(config_path_details_);
  config_path_details_->setVisible(false);
  connect(config_path_toggle_, &QPushButton::toggled, this, [this](bool expanded) {
    config_path_details_->setVisible(expanded);
    config_path_toggle_->setText(expanded ? UiText("收起完整路径") : UiText("展开完整路径"));
  });
  root->addWidget(config_group);

  workbench_splitter_ = new QSplitter(Qt::Horizontal, this);
  workbench_splitter_->setObjectName(QStringLiteral("workbenchSplitter"));
  workbench_splitter_->setChildrenCollapsible(false);
  control_tabs_ = new QTabWidget(workbench_splitter_);
  control_tabs_->setObjectName(QStringLiteral("controlTabs"));
  auto* operation_scroll = new QScrollArea(control_tabs_);
  operation_scroll->setObjectName(QStringLiteral("operationScroll"));
  operation_scroll->setWidgetResizable(true);
  operation_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  auto* operation_page = new QWidget(operation_scroll);
  operation_page->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  auto* operation_page_layout = new QVBoxLayout(operation_page);
  operation_page_layout->setContentsMargins(8, 8, 8, 8);
  auto* operation_group = new QGroupBox(UiText("操作与输入"), operation_page);
  operation_group->setObjectName(QStringLiteral("operationGroup"));
  auto* operation_layout = new QVBoxLayout(operation_group);
  auto* selection_form = new QFormLayout;
  selection_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  pipeline_combo_ = new QComboBox(this);
  pipeline_combo_->setObjectName(QStringLiteral("pipelineSelector"));
  pipeline_combo_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  mode_combo_ = new QComboBox(this);
  mode_combo_->setObjectName(QStringLiteral("operationMode"));
  mode_combo_->addItem(UiText("组包"), static_cast<int>(OperationMode::ENCODE));
  mode_combo_->addItem(UiText("解析"), static_cast<int>(OperationMode::INSPECT));
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  mode_combo_->addItem(UiText("流式解析"),
                       static_cast<int>(OperationMode::STREAM_INSPECT));
#endif
  message_combo_ = new QComboBox(this);
  message_combo_->setObjectName(QStringLiteral("messageSelector"));
  message_combo_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  representation_combo_ = new QComboBox(this);
  representation_combo_->setObjectName(QStringLiteral("inputRepresentation"));
  representation_combo_->setToolTip(QStringLiteral(
      "切换表示会按当前格式解析并转换已有草稿，不会重新解释输入。\n"
      "转换失败时保留当前格式与草稿；请修正草稿，或先复制/清空，再切换格式并输入。"));
  representation_combo_->addItem(QStringLiteral("Hex"), static_cast<int>(ByteRepresentation::HEX));
  representation_combo_->addItem(QStringLiteral("ASCII (escaped)"),
                                 static_cast<int>(ByteRepresentation::ASCII_ESCAPED));
  encode_button_ = new QPushButton(QStringLiteral("生成报文（Encode）"), this);
  encode_button_->setObjectName(QStringLiteral("encodeAction"));
  inspect_button_ = new QPushButton(QStringLiteral("解析完整记录（Decode）"), this);
  inspect_button_->setObjectName(QStringLiteral("primaryAction"));
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  continue_button_ = new QPushButton(UiText("继续"), this);
  continue_button_->setObjectName(QStringLiteral("continueStream"));
  reset_stream_button_ = new QPushButton(UiText("重置流"), this);
  reset_stream_button_->setObjectName(QStringLiteral("resetStream"));
#endif
  selection_form->addRow(UiText("当前动作"), mode_combo_);
  selection_form->addRow(UiText("处理管线（Pipeline）"), pipeline_combo_);
  selection_form->addRow(UiText("组包消息"), message_combo_);
  selection_form->addRow(UiText("输入表示"), representation_combo_);
  operation_layout->addLayout(selection_form);
  auto* action_row = new QHBoxLayout;
  action_row->addWidget(encode_button_);
  action_row->addWidget(inspect_button_);
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  action_row->addWidget(continue_button_);
  action_row->addWidget(reset_stream_button_);
#endif
  action_row->addStretch(1);
  operation_layout->addLayout(action_row);

  auto* binding_scroll = new QScrollArea(control_tabs_);
  binding_scroll->setObjectName(QStringLiteral("bindingScroll"));
  binding_scroll->setWidgetResizable(true);
  binding_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  auto* binding_page = new QWidget(binding_scroll);
  auto* binding_layout = new QVBoxLayout(binding_page);
  binding_layout->setContentsMargins(8, 8, 8, 8);
#if defined(PAE_BUILD_PROTOCOL_LAB_BINDING_UI)
  BuildHostUi(operation_page_layout, binding_layout);
#else
  binding_layout->addWidget(new QLabel(UiText("当前构建未启用 Host 绑定观察。"), binding_page));
  binding_layout->addStretch(1);
#endif

  inspect_input_label_ = new QLabel(UiText("原始输入（Hex；允许大小写及 SP/HT/CR/LF）"), this);
  inspect_input_label_->setWordWrap(true);
  inspect_input_ = new QPlainTextEdit(this);
  inspect_input_->setObjectName(QStringLiteral("inputEditor"));
  inspect_input_->setPlaceholderText(UiText("粘贴一条完整记录，例如：AA 00 06 00 00 55"));
  inspect_input_->setMaximumHeight(180);
  operation_layout->addWidget(inspect_input_label_);
  operation_layout->addWidget(inspect_input_);
  operation_page_layout->insertWidget(0, operation_group);
  operation_page_layout->addStretch(1);
  operation_scroll->setWidget(operation_page);
  binding_scroll->setWidget(binding_page);
  control_tabs_->addTab(operation_scroll, UiText("操作"));
  control_tabs_->addTab(binding_scroll, UiText("绑定设置"));

  auto* result_workspace = new QWidget(workbench_splitter_);
  result_workspace->setObjectName(QStringLiteral("resultWorkspace"));
  auto* result_layout = new QVBoxLayout(result_workspace);
  result_layout->setContentsMargins(8, 0, 0, 0);
  result_kind_label_ = new QLabel(result_workspace);
  result_kind_label_->setObjectName(QStringLiteral("resultStatus"));
  result_kind_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  result_layout->addWidget(result_kind_label_);
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  stream_status_label_ = new QLabel(result_workspace);
  stream_status_label_->setObjectName(QStringLiteral("streamStatus"));
  stream_status_label_->setWordWrap(true);
  stream_status_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
#endif

  result_splitter_ = new QSplitter(Qt::Vertical, result_workspace);
  result_splitter_->setObjectName(QStringLiteral("resultWorkspaceSplitter"));
  result_splitter_->setChildrenCollapsible(false);
  field_table_ = new QTableView(result_splitter_);
  field_table_->setObjectName(QStringLiteral("fieldTable"));
  field_model_ = new FieldTableModel(field_table_);
  value_delegate_ = new ExactValueDelegate(field_table_);
  field_table_->setModel(field_model_);
  field_table_->setItemDelegateForColumn(FieldTableModel::VALUE, value_delegate_);
  field_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  field_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  field_table_->setAlternatingRowColors(true);
  field_table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  field_table_->horizontalHeader()->setStretchLastSection(false);
  field_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  field_table_->setColumnWidth(FieldTableModel::ID, 140);
  field_table_->setColumnWidth(FieldTableModel::DISPLAY_NAME, 150);
  field_table_->setColumnWidth(
      FieldTableModel::TYPE,
      (std::max)(140, field_table_->fontMetrics().horizontalAdvance(QStringLiteral("DECIMAL64")) +
                          24));
  field_table_->setColumnWidth(FieldTableModel::SOURCE, 110);
  field_table_->setColumnWidth(FieldTableModel::VALUE, 180);
  field_table_->setColumnWidth(FieldTableModel::RAW_RESULT, 150);
  field_table_->setColumnWidth(FieldTableModel::LOGICAL_RESULT, 150);
  field_table_->setColumnWidth(FieldTableModel::PHYSICAL_LOCATION, 190);
  field_table_->verticalHeader()->setVisible(false);
  right_tabs_ = new QTabWidget(result_splitter_);
  right_tabs_->setObjectName(QStringLiteral("resultDetailsTabs"));
  details_view_ = new QTextBrowser(right_tabs_);
  details_view_->setObjectName(QStringLiteral("detailsView"));
  details_view_->setOpenExternalLinks(false);
  details_view_->setTextInteractionFlags(Qt::TextBrowserInteraction |
                                         Qt::TextSelectableByKeyboard);
  details_view_->setPlaceholderText(UiText("字段说明、来源、转换和完整性注释"));
  right_tabs_->addTab(details_view_, UiText("字段详情"));

  hex_view_ = new HexView(right_tabs_);
  hex_view_->setObjectName(QStringLiteral("hexView"));
  right_tabs_->addTab(hex_view_, UiText("报文字节"));

  auto* diagnostic_scroll = new QScrollArea(right_tabs_);
  diagnostic_scroll->setObjectName(QStringLiteral("diagnosticScroll"));
  diagnostic_scroll->setWidgetResizable(true);
  auto* diagnostic_content = new QWidget(diagnostic_scroll);
  auto* diagnostic_layout = new QVBoxLayout(diagnostic_content);
  diagnostic_label_ = new QLabel(diagnostic_content);
  diagnostic_label_->setObjectName(QStringLiteral("diagnosticView"));
  diagnostic_label_->setWordWrap(true);
  diagnostic_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  diagnostic_label_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  timing_label_ = new QLabel(diagnostic_content);
  timing_label_->setObjectName(QStringLiteral("timingStatus"));
  timing_label_->setWordWrap(true);
  timing_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  timing_label_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  diagnostic_layout->addWidget(diagnostic_label_);
  diagnostic_layout->addWidget(timing_label_);
  diagnostic_layout->addStretch(1);
  diagnostic_scroll->setWidget(diagnostic_content);
  diagnostic_tab_index_ = right_tabs_->addTab(diagnostic_scroll, UiText("诊断与计时"));

#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  auto* stream_scroll = new QScrollArea(right_tabs_);
  stream_scroll->setObjectName(QStringLiteral("streamStatusScroll"));
  stream_scroll->setWidgetResizable(true);
  auto* stream_content = new QWidget(stream_scroll);
  auto* stream_layout = new QVBoxLayout(stream_content);
  stream_status_label_->setParent(stream_content);
  stream_status_label_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  stream_layout->addWidget(stream_status_label_);
  stream_layout->addStretch(1);
  stream_scroll->setWidget(stream_content);
  stream_tab_index_ = right_tabs_->addTab(stream_scroll, UiText("流状态"));
#endif

  result_splitter_->addWidget(field_table_);
  result_splitter_->addWidget(right_tabs_);
  result_splitter_->setStretchFactor(0, 3);
  result_splitter_->setStretchFactor(1, 2);
  result_splitter_->setSizes({480, 260});
  result_layout->addWidget(result_splitter_, 1);
  workbench_splitter_->addWidget(control_tabs_);
  workbench_splitter_->addWidget(result_workspace);
  workbench_splitter_->setStretchFactor(0, 3);
  workbench_splitter_->setStretchFactor(1, 7);
  workbench_splitter_->setSizes({360, 840});
  root->addWidget(workbench_splitter_, 1);

  connect(browse_button_, &QPushButton::clicked, this, [this] {
    const auto path = QFileDialog::getOpenFileName(this, UiText("打开 PAE 配置"), path_edit_->text(),
                                                   UiText("JSON 文件 (*.json);;所有文件 (*)"));
    if (!path.isEmpty()) {
      LoadPath(path);
    }
  });
  connect(load_button_, &QPushButton::clicked, this, [this] { BeginLoadFromPath(); });
  connect(pipeline_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int index) { SelectPipeline(index); });
  connect(message_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int index) { SelectMessage(index); });
  connect(encode_button_, &QPushButton::clicked, this, [this] { EncodeCurrent(); });
  connect(inspect_button_, &QPushButton::clicked, this, [this] { InspectCurrent(); });
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  connect(continue_button_, &QPushButton::clicked, this, [this] { ContinueStream(); });
  connect(reset_stream_button_, &QPushButton::clicked, this, [this] { ResetStream(); });
#endif
  connect(mode_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int index) { SelectMode(index); });
  connect(representation_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int index) { SelectRepresentation(index); });
  connect(inspect_input_, &QPlainTextEdit::textChanged, this, [this] {
    if (rebuilding_selectors_) return;
    timing_label_->clear();
    const QString text = inspect_input_->toPlainText();
    const auto byte_capacity =
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
        session_.mode() == OperationMode::STREAM_INSPECT ? session_.StreamChunkBudget() :
#endif
                                                         session_.InspectFrameBudget();
    const auto capacity = InspectEditorCapacity(byte_capacity, session_.representation());
    if (!capacity.has_value() || text.size() > *capacity) {
      rebuilding_selectors_ = true;
      inspect_input_->setPlainText(accepted_inspect_text_);
      rebuilding_selectors_ = false;
      session_.RejectInspectCapacity(capacity.value_or(0));
      RefreshInspect();
      RefreshState();
      return;
    }
    if (!session_.SetInspectDraftUtf16(Utf16(text))) {
      rebuilding_selectors_ = true;
      inspect_input_->setPlainText(accepted_inspect_text_);
      rebuilding_selectors_ = false;
      RefreshState();
      return;
    }
    accepted_inspect_text_ = text;
    RefreshInspect();
    RefreshState();
  });
  connect(field_table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
          [this](const QModelIndex& current, const QModelIndex&) {
            RefreshFieldDetails(current.row());
          });
  value_delegate_->SetEditorChangedCallback(
      [this](std::size_t field_index) { InvalidateEditedDraft(field_index); });
}

void DocumentTab::BeginLoadFromPath(bool discard_confirmed) {
  if (closed_) {
    return;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  if (!discard_confirmed && !ConfirmStreamDiscard(UiText("重新加载此配置")))
    return;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI) && defined(PAE_BUILD_PROTOCOL_LAB_BINDING_UI)
  if (!discard_confirmed &&
      (session_.BinaryHasDiscardableState() ||
       (session_.IsBinaryHostDocument() && host_pending_revision_)) &&
      AskChineseQuestion(this, UiText("重新加载 Binary Host 文档"),
                         UiText("重新加载将丢弃 Binary 流草稿、结果或等待中的准备任务。是否继续？")) !=
          QMessageBox::Yes)
    return;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINDING_UI)
  host_pending_revision_.reset();
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  host_pending_bindings_.clear();
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  binary_host_pending_bindings_.clear();
  binary_host_pending_identity_.reset();
#endif
  host_config_text_.clear();
#endif
  const auto revision = session_.BeginLoad();
  ResetVisibleDocument();
  RefreshState();

  QFile file(path_edit_->text());
  if (!file.open(QIODevice::ReadOnly)) {
    auto completion = std::make_unique<CompileCompletion>();
    completion->document_id = session_.id();
    completion->load_revision = revision;
    completion->diagnostic = CompileCompletion::Diagnostic{
        std::string("cannot read config: ") + Utf8(file.errorString())};
    AcceptCompletion(std::move(completion));
    return;
  }
  const auto bytes = file.read(static_cast<qint64>(kMaximumConfigBytes + 1U));
  if (bytes.size() > static_cast<qint64>(kMaximumConfigBytes)) {
    auto completion = std::make_unique<CompileCompletion>();
    completion->document_id = session_.id();
    completion->load_revision = revision;
    completion->diagnostic =
        CompileCompletion::Diagnostic{"UI config file exceeds 4 MiB precheck"};
    AcceptCompletion(std::move(completion));
    return;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_BINDING_UI)
  host_config_text_.assign(bytes.constData(), static_cast<std::size_t>(bytes.size()));
#endif
  const auto status =
      worker_.Submit(session_.id(), revision,
                     std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
  if (status != SubmitStatus::ACCEPTED) {
    auto completion = std::make_unique<CompileCompletion>();
    completion->document_id = session_.id();
    completion->load_revision = revision;
    completion->diagnostic =
        CompileCompletion::Diagnostic{"compile request was rejected by bounded scheduler"};
    AcceptCompletion(std::move(completion));
  }
}

void DocumentTab::ResetVisibleDocument() {
  rebuilding_selectors_ = true;
  pipeline_combo_->clear();
  message_combo_->clear();
  rebuilding_selectors_ = false;
  field_model_->Reset(nullptr, {});
  hex_view_->ClearFrame();
  details_view_->clear();
  timing_ = {};
  timing_label_->clear();
  rebuilding_selectors_ = true;
  inspect_input_->clear();
  representation_combo_->setCurrentIndex(0);
  rebuilding_selectors_ = false;
  accepted_inspect_text_.clear();
  result_kind_label_->clear();
}

void DocumentTab::RebuildSelectorsAndModel() {
  const auto* description = session_.description();
  if (description == nullptr) {
    return;
  }
  rebuilding_selectors_ = true;
  pipeline_combo_->clear();
  for (const auto& pipeline : description->pipelines) {
    const auto& label = pipeline.display_name.empty() ? pipeline.id : pipeline.display_name;
    pipeline_combo_->addItem(FromUtf8(label),
                             QVariant::fromValue(static_cast<qulonglong>(pipeline.pipeline_index)));
  }
  if (session_.selected_pipeline_index().has_value()) {
    for (int index = 0; index < pipeline_combo_->count(); ++index) {
      if (pipeline_combo_->itemData(index).toULongLong() == *session_.selected_pipeline_index()) {
        pipeline_combo_->setCurrentIndex(index);
        break;
      }
    }
  }
  rebuilding_selectors_ = false;
  RebuildMessageSelector();
}

void DocumentTab::RebuildMessageSelector() {
  const auto* description = session_.description();
  if (description == nullptr || pipeline_combo_->currentIndex() < 0) {
    return;
  }
  const auto pipeline_index =
      static_cast<std::size_t>(pipeline_combo_->currentData().toULongLong());
  if (pipeline_index >= description->pipelines.size()) {
    return;
  }
  rebuilding_selectors_ = true;
  message_combo_->clear();
  const auto& pipeline = description->pipelines[pipeline_index];
  const auto& selectable = session_.IsBinaryHostDocument() &&
                                   session_.mode() == OperationMode::ENCODE
                               ? pipeline.encode_message_indices
                               : pipeline.message_indices;
  for (const auto message_index : selectable) {
    if (message_index >= description->messages.size()) {
      continue;
    }
    const auto& message = description->messages[message_index];
    const auto& base_label = message.display_name.empty() ? message.id : message.display_name;
    QString label = FromUtf8(base_label);
    if (description->layout == DocumentLayout::ASCII_TEXT) {
      label +=
          QStringLiteral(" [%1%2]")
              .arg(message.encode_available ? UiText("组包") : QStringLiteral(""))
              .arg(message.decode_available ? (message.encode_available ? UiText("/解析")
                                                                        : UiText("解析"))
                                            : QStringLiteral(""));
    }
    message_combo_->addItem(label, QVariant::fromValue(static_cast<qulonglong>(message_index)));
  }
  if (!session_.selection().has_value() && message_combo_->count() > 0) {
    session_.SelectMessage(static_cast<std::size_t>(message_combo_->itemData(0).toULongLong()));
    message_combo_->setCurrentIndex(0);
  }
  if (session_.selection().has_value()) {
    for (int index = 0; index < message_combo_->count(); ++index) {
      if (message_combo_->itemData(index).toULongLong() == session_.selection()->message_index) {
        message_combo_->setCurrentIndex(index);
        break;
      }
    }
  }
  rebuilding_selectors_ = false;
  const auto* message = CurrentMessage();
  field_model_->Reset(
      message,
      [this](std::size_t field_index, TypedDraft value, QString& error) {
        if (!session_.SetDraft(field_index, std::move(value))) {
          error = FromUtf8(session_.diagnostic_detail());
          return false;
        }
        RefreshPreview();
        RefreshState();
        return true;
      },
      [this](std::size_t field_index, std::u16string text, std::string validation_error) {
        session_.SetInvalidDraftUtf16(field_index, std::move(text), std::move(validation_error));
        RefreshPreview();
        RefreshState();
      },
      true, session_.representation());
  RefreshPreview();
  RefreshState();
  RefreshModePresentation();
}

void DocumentTab::RefreshState() {
  const auto* description = session_.description();
  const QString config_name =
      Title() == QStringLiteral("Untitled") ? UiText("未选择配置") : Title();
  config_name_label_->setText(config_name);
  config_name_label_->setToolTip(config_name);
  QString state_text;
  switch (session_.state()) {
    case DocumentState::EMPTY:
      state_text = UiText("未加载");
      break;
    case DocumentState::LOADING:
      state_text = UiText("加载 / 编译中…");
      break;
    case DocumentState::READY:
      state_text = UiText("已编译");
      break;
    case DocumentState::PREVIEW_VALID:
      state_text = UiText("已生成有效结果");
      break;
    case DocumentState::CONFIG_ERROR:
      state_text = UiText("配置错误");
      break;
    case DocumentState::CLOSING:
      state_text = UiText("正在关闭");
      break;
    case DocumentState::CLOSED:
      state_text = UiText("已关闭");
      break;
  }
  if (description != nullptr) {
    identity_label_->setText(UiText("%1 | Schema %2 | 协议 %3 %4")
                                 .arg(state_text,
                                      FromUtf8(description->schema_version),
                                      FromUtf8(description->protocol_id),
                                      FromUtf8(description->protocol_version)));
  } else {
    identity_label_->setText(UiText("%1 | 尚未编译文档").arg(state_text));
  }
  identity_label_->setToolTip(identity_label_->text());
  const bool loading = session_.state() == DocumentState::LOADING;
  pipeline_combo_->setEnabled(description != nullptr && !loading);
  mode_combo_->setEnabled(description != nullptr && !loading);
#if defined(PAE_BUILD_PROTOCOL_LAB_BINDING_UI)
  const bool host_document =
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
      session_.IsAsciiDocument()
#else
      false
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
                             || session_.IsBinaryHostDocument()
#endif
      ;
  host_panel_->setVisible(host_document);
  host_active_panel_->setVisible(host_document);
  if (control_tabs_ != nullptr && control_tabs_->count() > 1)
    control_tabs_->setTabEnabled(1, host_document);
  host_apply_->setEnabled(host_document && !loading && !host_pending_revision_);
  host_draft_->setEnabled(!host_pending_revision_);
  const bool active_host =
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
      session_.HostActive()
#else
      false
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
                           || session_.BinaryHostActive()
#endif
      ;
  if (auto* host_group = qobject_cast<QGroupBox*>(host_panel_)) {
    if (host_pending_revision_) {
      host_group->setTitle(UiText("绑定与上下文 · 正在准备候选会话…"));
    } else if (active_host && host_draft_dirty_) {
      host_group->setTitle(UiText("绑定与上下文 · 绑定已生效 · 草稿尚未应用"));
    } else if (active_host) {
      host_group->setTitle(UiText("绑定与上下文 · 绑定已生效"));
    } else {
      host_group->setTitle(UiText("绑定与上下文 · 绑定草稿·尚未应用"));
    }
  }
  host_binding_combo_->setEnabled(active_host);
  bool flow_selection_available = false;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  flow_selection_available = session_.BinaryHostActive();
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  flow_selection_available =
      flow_selection_available ||
      (session_.HostActive() &&
       IsDecodeHostAction(session_.prepared()
                              ->host_adapter->Bindings()[session_.HostBindingIndex()]
                              .action));
#endif
  host_flow_combo_->setEnabled(active_host && flow_selection_available);
  if (active_host) {
    pipeline_combo_->setEnabled(false);
    mode_combo_->setEnabled(false);
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  if (auto* model = qobject_cast<QStandardItemModel*>(mode_combo_->model())) {
    const int inspect_index = mode_combo_->findData(static_cast<int>(OperationMode::INSPECT));
    const int stream_index = mode_combo_->findData(static_cast<int>(OperationMode::STREAM_INSPECT));
    if (inspect_index >= 0 && model->item(inspect_index) != nullptr)
      model->item(inspect_index)->setEnabled(session_.InspectAvailable());
    if (stream_index >= 0 && model->item(stream_index) != nullptr)
      model->item(stream_index)->setEnabled(session_.StreamInspectAvailable());
  }
#endif
  representation_combo_->setVisible(session_.IsAsciiDocument());
  representation_combo_->setEnabled(session_.IsAsciiDocument() && !loading);
  encode_button_->setVisible(true);
  const bool encode_mode = session_.mode() == OperationMode::ENCODE;
  const bool inspect_mode = session_.mode() == OperationMode::INSPECT;
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  const bool stream_mode = session_.mode() == OperationMode::STREAM_INSPECT;
#else
  const bool stream_mode = false;
#endif
  message_combo_->setEnabled(description != nullptr && !loading && encode_mode);
  encode_button_->setEnabled(description != nullptr && session_.selection().has_value() &&
                             session_.EncodeAvailable() && !loading && encode_mode);
  inspect_button_->setEnabled(description != nullptr &&
                              session_.selected_pipeline_index().has_value() &&
                              (inspect_mode ? session_.InspectAvailable()
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
                                            : session_.StreamInspectAvailable()
#else
                                            : false
#endif
                                   ) &&
                              !loading && !encode_mode
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
                              && !(stream_mode && session_.StreamContinueAvailable())
#endif
  );
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  inspect_button_->setText(stream_mode ? UiText("提交输入块") : UiText("解析完整记录"));
  continue_button_->setVisible(stream_mode);
  reset_stream_button_->setVisible(stream_mode);
  stream_status_label_->setVisible(stream_mode);
  continue_button_->setEnabled(stream_mode && !loading && session_.StreamContinueAvailable());
  reset_stream_button_->setEnabled(stream_mode && !loading && session_.StreamInspectAvailable());
  inspect_input_->setReadOnly(stream_mode && session_.StreamContinueAvailable());
  representation_combo_->setEnabled(session_.IsAsciiDocument() && !loading);
  if (stream_mode) {
    bool stream_status_updated = false;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    if (session_.BinaryHostActive()) {
      const auto observation = session_.BinaryStreamObservation();
      if (observation) {
        QString text =
            UiText("策略=%1 | M=%2 | 阶段=%3 | 缓冲=%4 | 内部工作=%5 | C=%6 | 工作量=%7 | "
                   "冻结=%8/%9 | 代次=%10 | 步骤=%11 | 候选=%12 | "
                   "解析成功=%13 | 解析失败=%14 | 已丢弃=%15 | 异常=%16 | 需要重置=%17")
                .arg(static_cast<int>(observation->strategy))
                .arg(static_cast<qulonglong>(observation->maximum_candidate_frame_bytes))
                .arg(StreamPhaseName(observation->phase))
                .arg(static_cast<qulonglong>(observation->buffered_bytes))
                .arg(observation->has_internal_work ? QStringLiteral("true")
                                                    : QStringLiteral("false"))
                .arg(static_cast<qulonglong>(session_.StreamChunkBudget()))
                .arg(static_cast<qulonglong>(observation->effective_max_work_units))
                .arg(static_cast<qulonglong>(observation->frozen_cursor))
                .arg(static_cast<qulonglong>(observation->frozen_input_bytes))
                .arg(static_cast<qulonglong>(observation->generation))
                .arg(static_cast<qulonglong>(observation->step_sequence))
                .arg(static_cast<qulonglong>(observation->total_candidates))
                .arg(static_cast<qulonglong>(observation->total_decode_successes))
                .arg(static_cast<qulonglong>(observation->total_decode_failures))
                .arg(static_cast<qulonglong>(observation->total_discarded_bytes))
                .arg(static_cast<qulonglong>(observation->total_malformed_candidates))
                .arg(observation->reset_required ? QStringLiteral("true")
                                                 : QStringLiteral("false"));
        if (session_.binary_stream_view()) {
          const auto& step = *session_.binary_stream_view();
          text += UiText("\n上一步：状态=%1 | Host=%2 | 已消费=%3 | 候选=%4 | "
                         "解析尝试=%5 | 观察回调=%6 | 业务输出=%7")
                      .arg(static_cast<int>(step.status))
                      .arg(static_cast<int>(step.public_host.status))
                      .arg(static_cast<qulonglong>(step.public_host.bytes_consumed))
                      .arg(static_cast<qulonglong>(step.public_host.candidates))
                      .arg(static_cast<qulonglong>(step.public_host.decode_attempts))
                      .arg(static_cast<qulonglong>(step.public_host.observer_callbacks_returned))
                      .arg(static_cast<qulonglong>(step.public_host.business_callbacks_returned));
        }
        stream_status_label_->setText(text);
      } else {
        stream_status_label_->setText(UiText("Binary 流观察器不可用"));
      }
      stream_status_updated = true;
    }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
    if (!stream_status_updated) {
      const auto observation = session_.StreamObservation();
      if (observation.has_value()) {
        QString text =
            UiText("阶段=%1 | 缓冲=%2 | 内部工作=%3 | C=%4 | 工作量=%5 | "
                   "冻结=%6/%7 | 代次=%8 | 步骤=%9 | 候选=%10 | "
                   "解析成功=%11 | 已丢弃=%12 | 异常=%13 | 需要重置=%14")
                .arg(StreamPhaseName(observation->phase))
                .arg(static_cast<qulonglong>(observation->buffered_bytes))
                .arg(observation->has_internal_work ? QStringLiteral("true")
                                                    : QStringLiteral("false"))
                .arg(static_cast<qulonglong>(session_.StreamChunkBudget()))
                .arg(static_cast<qulonglong>(observation->effective_max_work_units))
                .arg(static_cast<qulonglong>(observation->frozen_cursor))
                .arg(static_cast<qulonglong>(observation->frozen_input_bytes))
                .arg(static_cast<qulonglong>(observation->generation))
                .arg(static_cast<qulonglong>(observation->step_sequence))
                .arg(static_cast<qulonglong>(observation->total_candidates))
                .arg(static_cast<qulonglong>(observation->total_decode_successes))
                .arg(static_cast<qulonglong>(observation->total_discarded_bytes))
                .arg(static_cast<qulonglong>(observation->total_malformed_candidates))
                .arg(observation->reset_required ? QStringLiteral("true")
                                                 : QStringLiteral("false"));
        if (session_.stream_step().has_value()) {
          const auto& step = *session_.stream_step();
          text += UiText("\n上一步：API=%1 | 停止原因=%2 | 已消费=%3 | 帧=%4 | "
                         "已丢弃=%5 | 异常=%6 | 问题=%7 | 工作量=%8")
                      .arg(SubmitApiStatusName(step.framing.status))
                      .arg(StopReasonName(step.framing.stop_reason))
                      .arg(static_cast<qulonglong>(step.framing.bytes_consumed))
                      .arg(static_cast<qulonglong>(step.framing.candidates_delivered))
                      .arg(static_cast<qulonglong>(step.framing.bytes_discarded))
                      .arg(static_cast<qulonglong>(step.framing.malformed_candidates))
                      .arg(FramingIssueName(step.framing.last_issue))
                      .arg(static_cast<qulonglong>(step.framing.work_units_used));
        }
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
        if (session_.HostActive())
          text += UiText(" | 已观察=%1 | 业务输出=%2")
                      .arg(observation->total_observed_candidates)
                      .arg(observation->total_business_outputs);
#endif
        stream_status_label_->setText(text);
      } else {
        stream_status_label_->setText(UiText("流观察器不可用"));
      }
      stream_status_updated = true;
    }
#endif
    if (!stream_status_updated) stream_status_label_->setText(UiText("流观察器不可用"));
  }
  if (right_tabs_ != nullptr && stream_tab_index_ >= 0) {
    right_tabs_->setTabEnabled(stream_tab_index_, stream_mode);
    bool reset_required = false;
    bool observation_available = false;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    if (stream_mode && session_.BinaryHostActive()) {
      const auto observation = session_.BinaryStreamObservation();
      observation_available = observation.has_value();
      reset_required = observation && observation->reset_required;
    }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
    if (stream_mode && !observation_available) {
      const auto observation = session_.StreamObservation();
      observation_available = observation.has_value();
      reset_required = observation && observation->reset_required;
    }
#endif
    if (stream_mode && observation_available) {
      right_tabs_->setTabText(
          stream_tab_index_, reset_required
                                 ? UiText("流状态 · 需要重置")
                                 : (session_.StreamContinueAvailable()
                                        ? UiText("流状态 · 可继续处理")
                                        : UiText("流状态")));
    } else {
      right_tabs_->setTabText(stream_tab_index_, UiText("流状态"));
    }
  }
#endif
  if (session_.diagnostic_id().empty() && session_.diagnostic_detail().empty()) {
    diagnostic_label_->clear();
  } else {
    diagnostic_label_->setText(UiText("操作失败（%1）\n%2")
                                   .arg(FromUtf8(session_.diagnostic_id()),
                                        FromUtf8(session_.diagnostic_detail())));
  }
  if (right_tabs_ != nullptr && diagnostic_tab_index_ >= 0) {
    right_tabs_->setTabText(
        diagnostic_tab_index_, diagnostic_label_->text().isEmpty() && timing_label_->text().isEmpty()
                                   ? UiText("诊断与计时")
                                   : UiText("诊断与计时 · 有内容"));
  }
}

void DocumentTab::RefreshModePresentation() {
  const bool inspect_mode = session_.mode() != OperationMode::ENCODE;
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  const bool stream_mode = session_.mode() == OperationMode::STREAM_INSPECT;
#else
  const bool stream_mode = false;
#endif
  inspect_input_label_->setText(
      session_.IsAsciiDocument() && session_.representation() == ByteRepresentation::ASCII_ESCAPED
          ? UiText("原始输入（ASCII escaped；实际控制字符和非 ASCII 被拒绝）")
          : UiText("原始输入（Hex；允许大小写及 SP/HT/CR/LF）"));
  if (stream_mode) {
    inspect_input_label_->setText(
        UiText("流输入块（容量按 C=min(65536,effective max_submit_bytes)）"));
  }
  inspect_input_label_->setVisible(inspect_mode);
  inspect_input_->setVisible(inspect_mode);
  inspect_button_->setVisible(inspect_mode);
  encode_button_->setVisible(!inspect_mode);
  if (inspect_mode) {
    RefreshInspect();
  } else {
    const auto* message = CurrentMessage();
    field_model_->Reset(
        message,
        [this](std::size_t field_index, TypedDraft value, QString& error) {
          if (!session_.SetDraft(field_index, std::move(value))) {
            error = FromUtf8(session_.diagnostic_detail());
            return false;
          }
          RefreshPreview();
          RefreshState();
          return true;
        },
        [this](std::size_t field_index, std::u16string text, std::string validation_error) {
          session_.SetInvalidDraftUtf16(field_index, std::move(text), std::move(validation_error));
          RefreshPreview();
          RefreshState();
        },
        true, session_.representation());
    field_model_->ApplyDrafts(session_.drafts());
    field_model_->ApplyInvalidDrafts(session_.invalid_drafts());
    result_kind_label_->setText(
        session_.IsAsciiDocument()
            ? UiText("○ 待组包 | ASCII Encode")
            : UiText("○ 待组包 | 仅组包成功后显示有效输出"));
    result_kind_label_->setProperty("paeResultState", QStringLiteral("waiting"));
    RefreshPreview();
  }
  RefreshState();
}

void DocumentTab::RefreshInspect() {
  const auto* message = DisplayedMessage();
  field_model_->Reset(message, {}, {}, false, session_.representation(),
                      FieldPresentationAction::INSPECT);
  static const std::vector<std::uint8_t> empty_frame;
  const std::vector<std::uint8_t>* frame = &empty_frame;
  std::vector<PhysicalBitMask> local_highlights;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  auto& highlights = session_.BinaryHostActive() ? binary_highlights_ : local_highlights;
#else
  auto& highlights = local_highlights;
#endif
  highlights.clear();
  std::optional<int> failed_detail_row;
  if (session_.inspect_result().has_value()) {
    frame = &session_.inspect_result()->input_frame;
    field_model_->SetActualFrameSize(frame->size());
    field_model_->ApplyResults(session_.inspect_result()->fields);
    result_kind_label_->setText(
        UiText("✓ 解析成功 | Message: %1 | 成功，%2 个字段")
            .arg(FromUtf8(session_.inspect_result()->message_id).toHtmlEscaped())
            .arg(session_.inspect_result()->fields.size()));
    result_kind_label_->setProperty("paeResultState", QStringLiteral("success"));
    const auto* field = field_model_->FieldAt(field_table_->currentIndex().row());
    if (session_.IsAsciiDocument()
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
        || (session_.BinaryHostActive() && field && field->physical_bits.empty() &&
            field->byte_length_bounds.has_value())
#endif
    )
      highlights = ActualFieldHighlights(session_.inspect_result()->fields, field);
    else
      FillFieldHighlights(message, field, frame->size(), highlights);
  } else if (session_.inspect_failure().has_value()) {
    const auto& failure = *session_.inspect_failure();
    frame = &failure.input_frame;
    result_kind_label_->setText(UiText("✕ 解析失败 | 当前内容仅用于失败定位，不是有效结果"));
    result_kind_label_->setProperty("paeResultState", QStringLiteral("failure"));
    field_model_->SetFailedField(failure.failed_field_index);
    FillInspectFailureHighlights(message, highlights);
    if (message != nullptr && failure.failed_field_index.has_value() &&
        *failure.failed_field_index < message->fields.size()) {
      failed_detail_row = static_cast<int>(*failure.failed_field_index);
    }
  } else {
    bool stream_step_observed = false;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    stream_step_observed = session_.BinaryHostActive() && session_.binary_stream_view().has_value();
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
    stream_step_observed = stream_step_observed || session_.stream_step().has_value();
#endif
    result_kind_label_->setText(
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
        session_.mode() == OperationMode::STREAM_INSPECT && stream_step_observed
            ? UiText("◇ 本步无候选记录")
            :
#endif
            UiText("○ 待解析 | 尚无有效解析结果"));
    result_kind_label_->setProperty(
        "paeResultState",
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
        session_.mode() == OperationMode::STREAM_INSPECT && stream_step_observed
            ? QStringLiteral("candidate_empty")
            :
#endif
            QStringLiteral("waiting"));
  }
  if (frame->empty()) {
    hex_view_->ClearFrame();
  } else {
    hex_view_->SetFrame(*frame, highlights);
  }
  if (failed_detail_row.has_value()) {
    field_table_->selectRow(*failed_detail_row);
    RefreshFieldDetails(*failed_detail_row);
  } else {
    field_table_->clearSelection();
    field_table_->setCurrentIndex(QModelIndex{});
    details_view_->clear();
  }
}

void DocumentTab::RefreshPreview() {
  field_model_->SetFailedField(std::nullopt);
  const int current_row = field_table_->currentIndex().row();
  if (!session_.preview().has_value()) {
    field_model_->SetActualFrameSize(std::nullopt);
    field_model_->ClearResults();
    hex_view_->ClearFrame();
    RefreshFieldDetails(current_row, false);
    return;
  }
  field_model_->SetActualFrameSize(session_.preview()->encoded_frame.size());
  field_model_->ApplyResults(session_.preview()->fields);
  const auto* field = field_model_->FieldAt(current_row);
  hex_view_->SetFrame(
      session_.preview()->encoded_frame,
      session_.IsAsciiDocument()
          ? ActualFieldHighlights(session_.preview()->fields, field)
          : FieldHighlights(CurrentMessage(), field, session_.preview()->encoded_frame.size()));
  if (session_.IsAsciiDocument()) {
    result_kind_label_->setText(
        UiText("✓ 组包成功 | 成功，%1 个字段 | review kind: %2")
            .arg(session_.preview()->fields.size())
            .arg(session_.preview()->tx_template_review ? QStringLiteral("TX_TEMPLATE")
                                                        : QStringLiteral("NOT_APPLICABLE")));
  } else {
    const auto* message = CurrentMessage();
    result_kind_label_->setText(
        UiText("✓ 组包成功 | Message: %1 | 输出 %2 字节")
            .arg(message == nullptr ? QStringLiteral("-") : FromUtf8(message->id))
            .arg(session_.preview()->encoded_frame.size()));
  }
  result_kind_label_->setProperty("paeResultState", QStringLiteral("success"));
  RefreshFieldDetails(current_row, false);
}

void DocumentTab::RefreshFieldDetails(int row, bool refresh_frame) {
  const auto* field = field_model_->FieldAt(row);
  const auto* message = DisplayedMessage();
  if (field == nullptr) {
    details_view_->clear();
    if (!refresh_frame) return;
    if (session_.mode() != OperationMode::ENCODE) {
      std::vector<std::uint8_t> frame;
      if (session_.inspect_result().has_value()) frame = session_.inspect_result()->input_frame;
      if (session_.inspect_failure().has_value()) frame = session_.inspect_failure()->input_frame;
      hex_view_->SetFrame(std::move(frame), {});
    } else {
      RefreshPreview();
    }
    return;
  }
  QStringList details;
  if (message != nullptr) {
    const auto& message_name = message->display_name.empty() ? message->id : message->display_name;
    details.push_back(
        QStringLiteral("<b>报文</b>：%1").arg(HtmlPreservingLines(message_name)));
    if (!message->description.empty()) {
      details.push_back(HtmlPreservingLines(message->description));
    }
    if (!message->source_ref.empty()) {
      details.push_back(QStringLiteral("<b>报文来源</b>：%1")
                            .arg(HtmlPreservingLines(message->source_ref)));
    }
    if (message->integrity_storage.has_value()) {
      const auto actual_storage = ActualFrameSize().has_value()
                                      ? ResolveActualIntegrityStorage(*message, *ActualFrameSize())
                                      : std::optional<ByteRange>{};
      if (message->integrity_storage_at_payload_end && !actual_storage.has_value()) {
        details.push_back(QStringLiteral(
            "<b>完整性校验存储位置</b>：位于动态载荷末尾；当前实际范围不可用"));
      } else {
        const auto& storage =
            actual_storage.has_value() ? *actual_storage : *message->integrity_storage;
        details.push_back(QStringLiteral("<b>完整性校验存储位置</b>：%1 + %2")
                              .arg(static_cast<qulonglong>(storage.offset))
                              .arg(static_cast<qulonglong>(storage.length)));
      }
      if (message->integrity_range_ends_at_payload) {
        details.push_back(QStringLiteral(
            "<b>完整性校验覆盖范围</b>：配置范围结束于实际载荷末尾"));
      }
    }
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    if (message->computed_length_storage.has_value()) {
      details.push_back(
          QStringLiteral("<b>计算长度存储位置</b>：%1 + %2")
              .arg(static_cast<qulonglong>(message->computed_length_storage->offset))
              .arg(static_cast<qulonglong>(message->computed_length_storage->length)));
    }
#endif
  }
  details.push_back(QStringLiteral("<b>字段</b>：%1")
                        .arg(HtmlPreservingLines(field->display_name.empty() ? field->id
                                                                           : field->display_name)));
  if (!field->description.empty()) {
    details.push_back(HtmlPreservingLines(field->description));
  }
  if (!field->source_ref.empty()) {
    details.push_back(
        QStringLiteral("<b>来源</b>：%1").arg(HtmlPreservingLines(field->source_ref)));
  }
  if (field->ascii_text) {
    details.push_back(QStringLiteral("<b>动作参与情况</b>：Decode %1；Encode %2")
                          .arg(field->decode_referenced ? UiText("已引用") : UiText("未引用"),
                               field->encode_referenced ? UiText("已引用") : UiText("未引用")));
  } else if (!field->read_only_annotation.empty()) {
    details.push_back(QStringLiteral("<b>存储 / 完整性校验 / 转换</b>：%1")
                          .arg(HtmlPreservingLines(field->read_only_annotation)));
  }
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (field->decimal_conversion || field->decode_decimal64) {
    details.push_back(
        field->decode_decimal64
            ? QStringLiteral("<b>转换</b>：逻辑 Decimal64 结果；Core 执行逻辑值与原始值的可表示性检查")
            : QStringLiteral("<b>转换</b>：逻辑 Decimal64 输入；Core 执行逻辑值与原始值的可表示性检查"));
  }
#endif
  std::optional<ByteRange> actual_result_range;
  if (session_.IsAsciiDocument()
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
      || session_.BinaryHostActive()
#endif
  ) {
    const std::vector<UiFieldResult>* results = nullptr;
    if (session_.mode() == OperationMode::ENCODE && session_.preview().has_value())
      results = &session_.preview()->fields;
    if (session_.mode() != OperationMode::ENCODE && session_.inspect_result().has_value())
      results = &session_.inspect_result()->fields;
    if (results != nullptr) {
      const auto found = std::find_if(results->begin(), results->end(), [&](const auto& result) {
        return result.field_index == field->field_index && result.id == field->id;
      });
      if (found != results->end()) actual_result_range = found->actual_range;
    }
  }
  if (field->byte_length_bounds.has_value()) {
    details.push_back(QStringLiteral("<b>载荷长度边界</b>：%1..%2 字节")
                          .arg(static_cast<qulonglong>(field->byte_length_bounds->minimum))
                          .arg(static_cast<qulonglong>(field->byte_length_bounds->maximum)));
    if (actual_result_range.has_value()) {
      details.push_back(QStringLiteral("<b>实际字节范围</b>：%1 + %2")
                            .arg(static_cast<qulonglong>(actual_result_range->offset))
                            .arg(static_cast<qulonglong>(actual_result_range->length)));
    } else if (!session_.IsAsciiDocument() && ActualFrameSize().has_value()) {
      const auto range = ResolveActualFieldRange(*message, *field, *ActualFrameSize());
      if (range.has_value()) {
        details.push_back(QStringLiteral("<b>实际字节范围</b>：%1 + %2")
                              .arg(static_cast<qulonglong>(range->offset))
                              .arg(static_cast<qulonglong>(range->length)));
      }
    }
  } else if (field->byte_range.has_value()) {
    details.push_back(QStringLiteral("<b>字节范围</b>：%1 + %2")
                          .arg(static_cast<qulonglong>(field->byte_range->offset))
                          .arg(static_cast<qulonglong>(field->byte_range->length)));
  }
  if (!field->physical_bits.empty()) {
    details.push_back(QStringLiteral("<b>物理位单元数</b>：%1")
                          .arg(static_cast<qulonglong>(field->physical_bits.size())));
  }
  if (session_.IsAsciiDocument()) {
    if (actual_result_range.has_value()) {
      details.push_back(QStringLiteral("<b>实际物理字节（从 0 起）</b>：[%1, %2)，整字节范围")
                            .arg(static_cast<qulonglong>(actual_result_range->offset))
                            .arg(static_cast<qulonglong>(actual_result_range->offset +
                                                         actual_result_range->length)));
    }
  } else {
    const auto physical =
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
        session_.BinaryHostActive() && field->byte_length_bounds && actual_result_range
            ? QStringLiteral("byte[%1..%2), full-byte range")
                  .arg(static_cast<qulonglong>(actual_result_range->offset))
                  .arg(static_cast<qulonglong>(actual_result_range->offset +
                                               actual_result_range->length))
                  .toStdString()
        :
#endif
        message == nullptr ? FormatPhysicalLocation(*field)
                           : FormatPhysicalLocation(*message, *field, ActualFrameSize());
    if (!physical.empty()) {
      details.push_back(QStringLiteral("<b>物理字节 / 位 / 掩码（从 0 起，LSB0）</b>：%1")
                            .arg(HtmlPreservingLines(physical)));
    }
  }
  details_view_->setHtml(details.join(QStringLiteral("<br/>")));
  if (!refresh_frame) return;
  if (session_.mode() != OperationMode::ENCODE) {
    static const std::vector<std::uint8_t> empty_frame;
    const std::vector<std::uint8_t>* frame = &empty_frame;
    std::vector<PhysicalBitMask> local_highlights;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
    auto& highlights = session_.BinaryHostActive() ? binary_highlights_ : local_highlights;
#else
    auto& highlights = local_highlights;
#endif
    highlights.clear();
    if (session_.inspect_result().has_value()) {
      frame = &session_.inspect_result()->input_frame;
      if (session_.IsAsciiDocument()
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
          || (session_.BinaryHostActive() && field && field->physical_bits.empty() &&
              field->byte_length_bounds.has_value())
#endif
      )
        highlights = ActualFieldHighlights(session_.inspect_result()->fields, field);
      else
        FillFieldHighlights(message, field, frame->size(), highlights);
    } else if (session_.inspect_failure().has_value()) {
      frame = &session_.inspect_failure()->input_frame;
      FillInspectFailureHighlights(message, highlights);
    }
    hex_view_->SetFrame(*frame, highlights);
  } else {
    RefreshPreview();
  }
}

void DocumentTab::SelectPipeline(int combo_index) {
  if (rebuilding_selectors_ || combo_index < 0) {
    return;
  }
  const auto requested =
      static_cast<std::size_t>(pipeline_combo_->itemData(combo_index).toULongLong());
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  if (session_.selected_pipeline_index().has_value() &&
      requested != *session_.selected_pipeline_index() &&
      !ConfirmStreamDiscard(UiText("切换处理管线（Pipeline）"))) {
    rebuilding_selectors_ = true;
    for (int index = 0; index < pipeline_combo_->count(); ++index) {
      if (pipeline_combo_->itemData(index).toULongLong() ==
          static_cast<qulonglong>(*session_.selected_pipeline_index())) {
        pipeline_combo_->setCurrentIndex(index);
        break;
      }
    }
    rebuilding_selectors_ = false;
    return;
  }
#endif
  if (session_.SelectPipeline(requested)) {
    timing_label_->clear();
    field_model_->Reset(nullptr, {});
    hex_view_->ClearFrame();
    rebuilding_selectors_ = true;
    inspect_input_->clear();
    rebuilding_selectors_ = false;
    accepted_inspect_text_.clear();
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
    if (session_.mode() == OperationMode::STREAM_INSPECT && !session_.StreamInspectAvailable()) {
      session_.SetMode(OperationMode::ENCODE);
      rebuilding_selectors_ = true;
      mode_combo_->setCurrentIndex(mode_combo_->findData(static_cast<int>(OperationMode::ENCODE)));
      rebuilding_selectors_ = false;
    }
#endif
    RebuildMessageSelector();
  }
  RefreshState();
}

void DocumentTab::SelectMode(int combo_index) {
  if (rebuilding_selectors_ || combo_index < 0) return;
  const auto mode = static_cast<OperationMode>(mode_combo_->itemData(combo_index).toInt());
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  if (session_.mode() == OperationMode::STREAM_INSPECT && mode != OperationMode::STREAM_INSPECT &&
      !ConfirmStreamDiscard(UiText("退出流式解析"))) {
    rebuilding_selectors_ = true;
    mode_combo_->setCurrentIndex(
        mode_combo_->findData(static_cast<int>(OperationMode::STREAM_INSPECT)));
    rebuilding_selectors_ = false;
    return;
  }
#endif
  if (session_.SetMode(mode)) {
    timing_label_->clear();
    RefreshModePresentation();
  }
}

void DocumentTab::SelectRepresentation(int combo_index) {
  if (rebuilding_selectors_ || combo_index < 0 || !session_.IsAsciiDocument()) return;
  const auto requested =
      static_cast<ByteRepresentation>(representation_combo_->itemData(combo_index).toInt());
  if (!session_.SetRepresentation(requested)) {
    rebuilding_selectors_ = true;
    representation_combo_->setCurrentIndex(
        representation_combo_->findData(static_cast<int>(session_.representation())));
    rebuilding_selectors_ = false;
    RefreshState();
    return;
  }
  timing_label_->clear();
  rebuilding_selectors_ = true;
  inspect_input_->setPlainText(FromUtf16(session_.inspect_draft_utf16()));
  accepted_inspect_text_ = inspect_input_->toPlainText();
  rebuilding_selectors_ = false;
  RebuildMessageSelector();
  RefreshModePresentation();
}

void DocumentTab::SelectMessage(int combo_index) {
  if (rebuilding_selectors_ || combo_index < 0) {
    return;
  }
  if (session_.SelectMessage(
          static_cast<std::size_t>(message_combo_->itemData(combo_index).toULongLong()))) {
    timing_label_->clear();
    const auto* message = CurrentMessage();
    field_model_->Reset(
        message,
        [this](std::size_t field_index, TypedDraft value, QString& error) {
          if (!session_.SetDraft(field_index, std::move(value))) {
            error = FromUtf8(session_.diagnostic_detail());
            return false;
          }
          RefreshPreview();
          RefreshState();
          return true;
        },
        [this](std::size_t field_index, std::u16string text, std::string validation_error) {
          session_.SetInvalidDraftUtf16(field_index, std::move(text), std::move(validation_error));
          RefreshPreview();
          RefreshState();
        },
        true, session_.representation());
    hex_view_->ClearFrame();
  }
  RefreshState();
}

void DocumentTab::EncodeCurrent() {
  timing_ = {};
  QElapsedTimer total;
  QElapsedTimer phase;
  total.start();
  field_table_->clearFocus();
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  TimingObserver observer(timing_);
  QtInputMaterializationTimer input_materialization_timer;
  std::int64_t input_materialization_ns = 0;
  const bool success =
      session_.Encode(&observer, &input_materialization_timer, &input_materialization_ns);
  timing_.exact_input_ns = static_cast<qint64>(input_materialization_ns);
  field_model_->SetFailedField(std::nullopt);
  phase.restart();
  RefreshPreview();
  timing_.hex_replace_ns = phase.nsecsElapsed();
  if (!success) {
    hex_view_->ClearFrame();
    field_model_->ClearResults();
    result_kind_label_->setText(
        UiText("✕ 组包失败（%1）").arg(FromUtf8(session_.diagnostic_id())));
    result_kind_label_->setProperty("paeResultState", QStringLiteral("failure"));
  }
  RefreshState();
  hex_view_->viewport()->repaint();
  timing_.first_repaint_total_ns = total.nsecsElapsed();
  if (session_.IsAsciiDocument()) {
    const bool tx_template_review =
        session_.preview().has_value() && session_.preview()->tx_template_review;
    timing_label_->setProperty("paeReviewKind",
                               tx_template_review ? QStringLiteral("TX_TEMPLATE") : QString{});
    timing_label_->setText(
        tx_template_review
            ? QStringLiteral("input %1 ms | adapter + UI total %2 ms | review kind TX_TEMPLATE; no "
                             "independent RX Decode timing")
                  .arg(static_cast<double>(timing_.exact_input_ns) / 1000000.0, 0, 'f', 3)
                  .arg(static_cast<double>(timing_.first_repaint_total_ns) / 1000000.0, 0, 'f', 3)
            : QStringLiteral("ASCII Encode failed; no successful review result"));
  } else {
    timing_label_->setProperty("paeReviewKind", QString{});
    timing_label_->setText(TimingText(timing_));
  }
  if (right_tabs_ != nullptr && diagnostic_tab_index_ >= 0)
    right_tabs_->setTabText(diagnostic_tab_index_, UiText("诊断与计时 · 有内容"));
}

void DocumentTab::InspectCurrent() {
  timing_label_->clear();
  timing_label_->setProperty("paeReviewKind", QString{});
  field_table_->clearFocus();
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  session_.SetInspectDraftUtf16(Utf16(inspect_input_->toPlainText()));
  TimingObserver observer(timing_);
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  if (session_.mode() == OperationMode::STREAM_INSPECT) {
    session_.SubmitStream();
  } else
#endif
  {
    session_.Inspect(&observer);
  }
  if (session_.inspect_failure().has_value() &&
      session_.inspect_failure()->input_offset.has_value()) {
    const QString text = inspect_input_->toPlainText();
    int utf16_offset = 0;
    if (session_.inspect_failure()->input_offset_is_utf16) {
      utf16_offset = static_cast<int>((std::min)(*session_.inspect_failure()->input_offset,
                                                 static_cast<std::size_t>(text.size())));
    } else {
      const QByteArray utf8 = text.toUtf8();
      const auto bounded_offset = (std::min)(*session_.inspect_failure()->input_offset,
                                             static_cast<std::size_t>(utf8.size()));
      utf16_offset = QString::fromUtf8(utf8.constData(), static_cast<int>(bounded_offset)).size();
    }
    QTextCursor cursor = inspect_input_->textCursor();
    cursor.setPosition((std::min)(utf16_offset, text.size()));
    if (cursor.position() < text.size())
      cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    inspect_input_->setTextCursor(cursor);
  }
  RefreshInspect();
  RefreshState();
  hex_view_->viewport()->repaint();
}

#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
void DocumentTab::ContinueStream() {
  timing_label_->clear();
  session_.ContinueStream();
  RefreshInspect();
  RefreshState();
  hex_view_->viewport()->repaint();
}

void DocumentTab::ResetStream() {
  if (!session_.ResetStream()) {
    RefreshState();
    return;
  }
  rebuilding_selectors_ = true;
  inspect_input_->clear();
  rebuilding_selectors_ = false;
  accepted_inspect_text_.clear();
  RefreshInspect();
  RefreshState();
}

bool DocumentTab::ConfirmStreamDiscard(const QString& action) {
  if (!ConfirmStreamDiscardOnly(action)) return false;
  if (!session_.StreamHasDiscardableState()) return true;
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  if (session_.HostActive())
    session_.ResetAllHostStreams();
  else
#endif
      if (!session_.ResetStream())
    return false;
  rebuilding_selectors_ = true;
  inspect_input_->clear();
  rebuilding_selectors_ = false;
  accepted_inspect_text_.clear();
  return true;
}

bool DocumentTab::ConfirmStreamDiscardOnly(const QString& action) {
  if (!session_.StreamHasDiscardableState()) return true;
  const auto answer = AskChineseQuestion(
      this, UiText("丢弃流状态？"),
      UiText("%1 将丢弃受影响的流状态（包括未选择的流）及冻结后缀。是否继续？").arg(action));
  return answer == QMessageBox::Yes;
}
#endif

void DocumentTab::InvalidateEditedPreview() {
  session_.InvalidateInput();
  field_model_->ClearResults();
  hex_view_->ClearFrame();
  timing_label_->clear();
  RefreshState();
}

void DocumentTab::InvalidateEditedDraft(std::size_t field_index) {
  session_.InvalidateDraft(field_index);
  field_model_->ClearResults();
  hex_view_->ClearFrame();
  timing_label_->clear();
  RefreshState();
}

void DocumentTab::FillInspectFailureHighlights(const MessageDescriptor* message,
                                               std::vector<PhysicalBitMask>& highlights) const {
  highlights.clear();
  if (message == nullptr || !session_.inspect_failure().has_value()) return;
  if (session_.IsAsciiDocument()) return;
  const auto& failure = *session_.inspect_failure();
  if (failure.failed_field_index.has_value() &&
      *failure.failed_field_index < message->fields.size()) {
    FillFieldHighlights(message, &message->fields[*failure.failed_field_index], std::nullopt,
                        highlights);
    return;
  }
  if (failure.status == "INTEGRITY_FAILED" && message->integrity_storage.has_value() &&
      !message->integrity_storage_at_payload_end) {
    highlights.reserve(message->integrity_storage->length);
    for (std::size_t index = 0U; index < message->integrity_storage->length; ++index) {
      highlights.push_back(PhysicalBitMask{message->integrity_storage->offset + index, 0xFFU});
    }
  }
}

std::optional<std::size_t> DocumentTab::ActualFrameSize() const noexcept {
  if (session_.mode() == OperationMode::ENCODE && session_.preview().has_value()) {
    return session_.preview()->encoded_frame.size();
  }
  if ((session_.mode() == OperationMode::INSPECT
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
       || session_.mode() == OperationMode::STREAM_INSPECT
#endif
       ) && session_.inspect_result().has_value()) {
    return session_.inspect_result()->input_frame.size();
  }
  return std::nullopt;
}

const MessageDescriptor* DocumentTab::CurrentMessage() const noexcept {
  const auto* description = session_.description();
  if (description == nullptr || !session_.selection().has_value() ||
      session_.selection()->message_index >= description->messages.size()) {
    return nullptr;
  }
  return &description->messages[session_.selection()->message_index];
}

const MessageDescriptor* DocumentTab::DisplayedMessage() const noexcept {
  if (session_.mode() == OperationMode::ENCODE) return CurrentMessage();
  const auto* description = session_.description();
  if (description == nullptr) return nullptr;
  if (session_.inspect_result().has_value() &&
      session_.inspect_result()->message_index < description->messages.size()) {
    return &description->messages[session_.inspect_result()->message_index];
  }
  if (session_.inspect_failure().has_value() &&
      session_.inspect_failure()->message_index.has_value() &&
      *session_.inspect_failure()->message_index < description->messages.size()) {
    return &description->messages[*session_.inspect_failure()->message_index];
  }
  return nullptr;
}

}  // namespace pae::protocol_lab_ui
