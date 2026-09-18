#include "document_tab.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTableView>
#include <QTemporaryFile>
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
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

QString FromUtf8(const std::string& value) {
  return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
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
QString StreamPhaseName(protocol_framing::StreamFramingPhase value) {
  switch (value) {
    case protocol_framing::StreamFramingPhase::COLLECTING:
      return QStringLiteral("COLLECTING");
    case protocol_framing::StreamFramingPhase::DELIVERY_PENDING:
      return QStringLiteral("DELIVERY_PENDING");
    case protocol_framing::StreamFramingPhase::DISCARDING_UNTIL_CRLF:
      return QStringLiteral("DISCARDING_UNTIL_CRLF");
  }
  return QStringLiteral("UNKNOWN");
}

QString StopReasonName(protocol_framing::SubmitStopReason value) {
  switch (value) {
    case protocol_framing::SubmitStopReason::INPUT_EXHAUSTED:
      return QStringLiteral("INPUT_EXHAUSTED");
    case protocol_framing::SubmitStopReason::NEED_MORE:
      return QStringLiteral("NEED_MORE");
    case protocol_framing::SubmitStopReason::WORK_BUDGET_REACHED:
      return QStringLiteral("WORK_BUDGET_REACHED");
    case protocol_framing::SubmitStopReason::SINK_STOP:
      return QStringLiteral("SINK_STOP");
  }
  return QStringLiteral("UNKNOWN");
}

QString SubmitApiStatusName(protocol_framing::SubmitApiStatus value) {
  switch (value) {
    case protocol_framing::SubmitApiStatus::OK:
      return QStringLiteral("OK");
    case protocol_framing::SubmitApiStatus::INVALID_ARGUMENT:
      return QStringLiteral("INVALID_ARGUMENT");
    case protocol_framing::SubmitApiStatus::INVALID_PLAN:
      return QStringLiteral("INVALID_PLAN");
    case protocol_framing::SubmitApiStatus::WORKSPACE_PLAN_MISMATCH:
      return QStringLiteral("WORKSPACE_PLAN_MISMATCH");
    case protocol_framing::SubmitApiStatus::WORKSPACE_BUSY:
      return QStringLiteral("WORKSPACE_BUSY");
    case protocol_framing::SubmitApiStatus::REENTRANT_CALL:
      return QStringLiteral("REENTRANT_CALL");
    case protocol_framing::SubmitApiStatus::LIMIT_EXCEEDED:
      return QStringLiteral("LIMIT_EXCEEDED");
    case protocol_framing::SubmitApiStatus::INTERNAL_ERROR:
      return QStringLiteral("INTERNAL_ERROR");
  }
  return QStringLiteral("UNKNOWN");
}

QString FramingIssueName(protocol_framing::FramingIssue value) {
  switch (value) {
    case protocol_framing::FramingIssue::NONE:
      return QStringLiteral("NONE");
    case protocol_framing::FramingIssue::MALFORMED_LENGTH:
      return QStringLiteral("MALFORMED_LENGTH");
    case protocol_framing::FramingIssue::RECORD_TOO_LONG:
      return QStringLiteral("RECORD_TOO_LONG");
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

class TimingObserver final : public protocol_lab::v06::ExecutionObserver {
 public:
  explicit TimingObserver(EncodeTimingSnapshot& output) : output_(output) {}

  void PhaseStarted(protocol_lab::v06::ExecutionPhase phase) override {
    active_phase_ = phase;
    timer_.restart();
  }

  void PhaseFinished(protocol_lab::v06::ExecutionPhase phase, std::string_view) override {
    if (!active_phase_.has_value() || *active_phase_ != phase) {
      return;
    }
    const auto elapsed = timer_.nsecsElapsed();
    switch (phase) {
      case protocol_lab::v06::ExecutionPhase::MAIN_CODEC:
        output_.main_codec_ns += elapsed;
        break;
      case protocol_lab::v06::ExecutionPhase::REVIEW_DECODE:
        output_.review_decode_ns += elapsed;
        break;
      case protocol_lab::v06::ExecutionPhase::RESULT_MAPPING:
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
  std::optional<protocol_lab::v06::ExecutionPhase> active_phase_;
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
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
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
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
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
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  if (require_confirmation && !ConfirmStreamDiscardOnly(QStringLiteral("close this document"))) {
    return false;
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI) && defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  if (require_confirmation && (session_.BinaryHasDiscardableState() ||
                               (session_.IsBinaryHostDocument() && host_pending_revision_))) {
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Close Binary Host document"),
        QStringLiteral(
            "Closing will discard Binary flow drafts/results or pending preparation. Continue?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
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
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  if (!ConfirmStreamDiscardOnly(QStringLiteral("close the application"))) return false;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI) && defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  if (session_.BinaryHasDiscardableState() ||
      (session_.IsBinaryHostDocument() && host_pending_revision_)) {
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Close Binary Host document"),
        QStringLiteral(
            "Closing will discard Binary flow drafts/results or pending preparation. Continue?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
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
    if (field == nullptr || field->encode_source != protocol_plan::EncodeSource::INPUT) {
      continue;
    }
    const auto index = field_model_->index(row, FieldTableModel::VALUE);
    QVariant value;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
    if (field->conversion.has_value()) {
      value = QStringLiteral("0@0");
    } else
#endif
        if (field->value_type == protocol_plan::ValueType::UINT64 ||
            field->value_type == protocol_plan::ValueType::INT64) {
      value = QStringLiteral("0");
    } else if (field->value_type == protocol_plan::ValueType::BYTES) {
      const std::size_t byte_count = field->ascii_text && field->byte_length_bounds.has_value()
                                         ? field->byte_length_bounds->minimum
                                         : field->byte_width;
      value = field->ascii_text ? QStringLiteral("41").repeated(static_cast<int>(byte_count))
                                : QString(static_cast<int>(byte_count * 2U), QLatin1Char('0'));
    } else if (field->value_type == protocol_plan::ValueType::ENUM) {
      if (field->enum_entries.empty()) {
        error = QStringLiteral("enum field has no configured entries");
        return false;
      }
      value = 0;
    } else if (field->value_type == protocol_plan::ValueType::BOOL) {
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
      field_table_->selectRow(row);
      RefreshFieldDetails(row);
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
        field->encode_source != protocol_plan::EncodeSource::INPUT ||
        field->value_type != protocol_plan::ValueType::UINT64
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
        || field->conversion.has_value()
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
  encode_button_->setFocus(Qt::OtherFocusReason);
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  if (field_model_->data(payload_index, Qt::EditRole).toString() != QStringLiteral("010203")) {
    error = QStringLiteral("focus-out did not commit corrected legal BYTES");
    return false;
  }
  EncodeCurrent();
  if (!session_.preview().has_value()) {
    error = QStringLiteral("focus-out correction did not recover a valid Encode result");
    return false;
  }

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
      !field_model_->ValidationError(payload_row)
           .contains(QStringLiteral("editor capacity (8 Hex characters)"))) {
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
      !details_view_->toPlainText().contains(QStringLiteral("Actual byte range: 2 + 0")) ||
      !details_view_->toPlainText().contains(QStringLiteral("Integrity storage: 2 + 1"))) {
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
      !details_view_->toPlainText().contains(QStringLiteral("Actual byte range: 2 + 2")) ||
      !details_view_->toPlainText().contains(QStringLiteral("Integrity storage: 4 + 1"))) {
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
      !details_view_->toPlainText().contains(QStringLiteral("Actual byte range: 2 + 3")) ||
      !details_view_->toPlainText().contains(QStringLiteral("Integrity storage: 5 + 1"))) {
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
      details_view_->toPlainText().contains(QStringLiteral("Actual byte range:"))) {
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
      !details_view_->toPlainText().contains(QStringLiteral("Integrity storage: 4 + 1"))) {
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
  if (!inspect_button_->isEnabled() || inspect_button_->text() != QStringLiteral("Submit chunk") ||
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
          protocol_framing::StreamFramingPhase::DISCARDING_UNTIL_CRLF) {
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
  ascii_smoke_diagnostic::Trace("editor_first_before_focus_commit", editor.data(),
                                ascii_smoke_diagnostic::EditorId(editor.data()));
#endif
  encode_button_->setFocus(Qt::OtherFocusReason);
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  EncodeCurrent();
  const std::vector<std::uint8_t> expected{'T', 'X', ' ', 'A', 'L',  'I',
                                           'C', 'E', '!', 'A', '\r', '\n'};
  if (!session_.preview().has_value() || session_.preview()->encoded_frame != expected ||
      !session_.preview()->tx_template_review) {
    error = QStringLiteral("ASCII Enter commit did not produce independent TX template bytes");
    return false;
  }
  if (!timing_label_->text().contains(QStringLiteral("review kind TX_TEMPLATE"))) {
    error = QStringLiteral("successful ASCII Encode did not publish TX_TEMPLATE timing context");
    return false;
  }
  if (field_model_->data(field_model_->index(1, FieldTableModel::SOURCE)).toString() !=
          QStringLiteral("not referenced") ||
      field_model_->data(field_model_->index(1, FieldTableModel::VALUE)).toString() !=
          QStringLiteral("not referenced by Encode action") ||
      field_model_->data(field_model_->index(2, FieldTableModel::SOURCE)).toString() !=
          QStringLiteral("input")) {
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
  if (field_model_->data(rx_source).toString() != QStringLiteral("decoded") ||
      !field_model_->data(rx_value).toString().isEmpty() ||
      field_model_->data(rx_raw).toString() != QStringLiteral("4F4B") ||
      field_model_->data(rx_logical).toString() != QStringLiteral("OK") ||
      field_model_->data(rx_physical).toString() != QStringLiteral("9 + 2") ||
      field_model_->data(tx_source).toString() != QStringLiteral("not referenced") ||
      field_model_->data(tx_value).toString() !=
          QStringLiteral("not referenced by Decode action") ||
      !field_model_->data(tx_raw).toString().isEmpty()) {
    error = QStringLiteral("ASCII Inspect action/source/result annotations are inconsistent");
    return false;
  }
  field_table_->selectRow(1);
  RefreshFieldDetails(1);
  const QString rx_details = details_view_->toPlainText();
  if (!rx_details.contains(QStringLiteral("Actual byte range: 9 + 2")) ||
      !rx_details.contains(
          QStringLiteral("Actual physical bytes (zero-based): [9, 11), full-byte range")) ||
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

#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
bool DocumentTab::VerifyBinaryHostStage1ForSmoke(QString& error) {
  if (!session_.IsBinaryHostDocument() || session_.BinaryHostActive() ||
      session_.InspectAvailable() || !host_apply_->isEnabled()) {
    error = QStringLiteral("Schema 0.9 did not start unbound");
    return false;
  }
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

void DocumentTab::BuildHostUi(QVBoxLayout* root) {
  host_panel_ = new QWidget(this);
  auto* layout = new QVBoxLayout(host_panel_);
  layout->setContentsMargins(0, 0, 0, 0);
  host_draft_ = new QTableWidget(0, 3, host_panel_);
  host_draft_->setObjectName(QStringLiteral("hostBindingDraft"));
  host_draft_->setHorizontalHeaderLabels({QStringLiteral("Endpoint (draft)"),
                                          QStringLiteral("Action"), QStringLiteral("Pipeline ID")});
  host_draft_->horizontalHeader()->setStretchLastSection(true);
  host_draft_->setMaximumHeight(110);
  layout->addWidget(host_draft_);
  auto* row = new QHBoxLayout;
  auto* add = new QPushButton(QStringLiteral("Add binding"), host_panel_);
  auto* remove = new QPushButton(QStringLiteral("Remove selected"), host_panel_);
  host_apply_ = new QPushButton(QStringLiteral("Apply binding table"), host_panel_);
  host_apply_->setObjectName(QStringLiteral("hostApply"));
  host_binding_combo_ = new QComboBox(host_panel_);
  host_binding_combo_->setObjectName(QStringLiteral("hostBinding"));
  host_flow_combo_ = new QComboBox(host_panel_);
  host_flow_combo_->setObjectName(QStringLiteral("hostFlow"));
  host_flow_combo_->addItems({QStringLiteral("Flow 0"), QStringLiteral("Flow 1")});
  row->addWidget(add);
  row->addWidget(remove);
  row->addWidget(host_apply_);
  row->addWidget(new QLabel(QStringLiteral("Active binding"), host_panel_));
  row->addWidget(host_binding_combo_, 1);
  row->addWidget(host_flow_combo_);
  layout->addLayout(row);
  host_status_ = new QLabel(host_panel_);
  host_status_->setObjectName(QStringLiteral("hostStatus"));
  host_status_->setWordWrap(true);
  layout->addWidget(host_status_);
  root->addWidget(host_panel_);
  connect(add, &QPushButton::clicked, this, [this] {
    if (!host_pending_revision_) AddHostDraftRow();
  });
  connect(remove, &QPushButton::clicked, this, [this] {
    if (!host_pending_revision_ && host_draft_->currentRow() >= 0)
      host_draft_->removeRow(host_draft_->currentRow());
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
    action->addItem(QStringLiteral("Decode"));
  } else
#endif
    action->addItems({QStringLiteral("Decode"), QStringLiteral("Encode")});
  auto* pipeline = new QComboBox(host_draft_);
  for (const auto& item : session_.description()->pipelines)
    pipeline->addItem(FromUtf8(item.id),
                      QVariant::fromValue(static_cast<qulonglong>(item.pipeline_index)));
  host_draft_->setCellWidget(row, 0, endpoint);
  host_draft_->setCellWidget(row, 1, action);
  host_draft_->setCellWidget(row, 2, pipeline);
}
void DocumentTab::InitializeHostDraft() {
  rebuilding_selectors_ = true;
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
      host_status_->setText(QStringLiteral(
          "Schema 0.9 is unbound. Edit the draft and Apply explicitly; Decode remains disabled "
          "until publication succeeds."));
      rebuilding_selectors_ = false;
      return;
    }
#endif
    AddHostDraftRow();
    if (auto* action = qobject_cast<QComboBox*>(host_draft_->cellWidget(1, 1)))
      action->setCurrentIndex(1);
    host_status_->setText(
        QStringLiteral("Draft only. Apply explicitly to activate Host Session; current execution "
                       "is legacy offline."));
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
  if (!binary) {
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
      host_status_->setText(QStringLiteral(
          "Active document exceeds Host transition admission limit; no candidate created."));
      return;
    }
  }
  const auto high_bit = Revision{1} << 63U;
  if (host_request_sequence_ == high_bit - 1U) {
    host_status_->setText(QStringLiteral("Binding request sequence exhausted; reopen document."));
    return;
  }
  std::vector<protocol_lab::ascii::HostBinding> bindings;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  std::vector<BinaryHostBinding> binary_bindings;
#endif
  for (int row = 0; row < host_draft_->rowCount(); ++row) {
    const auto* endpoint = qobject_cast<QLineEdit*>(host_draft_->cellWidget(row, 0));
    const auto* action = qobject_cast<QComboBox*>(host_draft_->cellWidget(row, 1));
    const auto* pipeline = qobject_cast<QComboBox*>(host_draft_->cellWidget(row, 2));
    if (!endpoint || !action || !pipeline || pipeline->currentIndex() < 0 ||
        endpoint->text().isEmpty() || endpoint->text().toUtf8().size() > 256) {
      host_status_->setText(QStringLiteral("Invalid binding draft; active Session unchanged."));
      return;
    }
    const auto pipeline_index = static_cast<std::size_t>(pipeline->currentData().toULongLong());
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
    if (binary) {
      if (action->currentIndex() != 0 ||
          pipeline_index >= session_.description()->pipelines.size()) {
        host_status_->setText(
            QStringLiteral("Stage 1 accepts Decode bindings only; active Session unchanged."));
        return;
      }
      binary_bindings.push_back({Utf8(endpoint->text()),
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
                                 pae::HostAction::DECODE,
#else
                                 host_endpoint::Action::DECODE,
#endif
                                 session_.description()->pipelines[pipeline_index].id, 2U});
    } else
#endif
      bindings.push_back({Utf8(endpoint->text()),
                          action->currentIndex() == 0 ? host_endpoint::Action::DECODE
                                                      : host_endpoint::Action::ENCODE,
                          pipeline_index});
  }
  if (bindings.empty()
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
      && binary_bindings.empty()
#endif
  ) {
    host_status_->setText(QStringLiteral("At least one binding required."));
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
    host_pending_bindings_ = std::move(bindings);
  if (worker_.Submit(session_.id(), *host_pending_revision_, host_config_text_) !=
      SubmitStatus::ACCEPTED) {
    host_pending_revision_.reset();
    host_pending_bindings_.clear();
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
    binary_host_pending_bindings_.clear();
    binary_host_pending_identity_.reset();
#endif
    host_status_->setText(
        QStringLiteral("Preparation scheduler rejected request; active Session unchanged."));
  } else
    host_status_->setText(QStringLiteral(
        "Preparing candidate Session. Active state retained until confirmed publication."));
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
                                                      QStringLiteral(" / Decode / ") +
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
          QStringLiteral("Preparation failed; active Session and retry draft retained: %1")
              .arg(FromUtf8(error)));
      RefreshState();
      return;
    }
    auto publication =
        session_.PrepareBinaryHostPublication(std::move(candidate), completion->load_revision);
    if (!publication) {
      delete selector_model;
      delete pipeline_model;
      delete message_model;
      host_status_->setText(QStringLiteral(
          "Candidate publication preparation failed; active Session and retry draft retained."));
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
      host_status_->setText(QStringLiteral(
          "Selector preparation allocation failed; active Session and retry draft retained."));
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
      host_status_->setText(QStringLiteral(
          "UI view preparation allocation failed; active Session and retry draft retained."));
      RefreshState();
      return;
    }
    if (!publication->adapter->SetPresentationRetainedBytes(row_bytes + highlight_bytes)) {
      delete selector_model;
      delete pipeline_model;
      delete message_model;
      host_status_->setText(QStringLiteral(
          "UI presentation accounting failed; active Session and retry draft retained."));
      RefreshState();
      return;
    }
    if (session_.BinaryHasDiscardableState()) {
      const auto answer = QMessageBox::question(
          this, QStringLiteral("Replace Binary Host Session"),
          QStringLiteral("Publishing will discard active Binary flow drafts/results. Continue?"),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (answer != QMessageBox::Yes) {
        delete selector_model;
        delete pipeline_model;
        delete message_model;
        host_status_->setText(
            QStringLiteral("Publication cancelled; active Session and retry draft retained."));
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
    field_model_->Reset(CurrentMessage(), {}, {}, false, session_.representation(),
                        FieldPresentationAction::INSPECT);
    RefreshInspect();
    RefreshModePresentation();
    RefreshState();
    host_status_->setText(QStringLiteral("Binary Host Session published."));
    return;
  }
#endif
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
    } else if (completion->route == SchemaDispatchStatus::PRIVATE_ASCII && completion->artifacts &&
               !completion->diagnostic) {
      candidate = AsciiHostAdapter::CreatePrivate(std::move(*completion->artifacts),
                                                  std::move(host_pending_bindings_), error);
    }
  }
#else
  std::unique_ptr<protocol_lab::ascii::HostObserverAdapter> candidate;
  if (completion->artifacts && !completion->diagnostic && session_.prepared() &&
      completion->config_sha256 == session_.prepared()->config_sha256)
    candidate = protocol_lab::ascii::HostObserverAdapter::Create(
        std::move(*completion->artifacts), std::move(host_pending_bindings_), error);
#endif
  host_pending_bindings_.clear();
  if (!candidate) {
    host_status_->setText(
        QStringLiteral("Preparation failed; active state unchanged: %1").arg(FromUtf8(error)));
    RefreshState();
    return;
  }
  // Each active/candidate adapter has an admission cap; both may coexist only during preparation.
  if (session_.HostActive() &&
      candidate->AccountedBytes() >
          2U * protocol_lab::ascii::HostObserverAdapter::kMaximumAccountedBytes -
              session_.prepared()->host_adapter->AccountedBytes()) {
    host_status_->setText(
        QStringLiteral("Rebinding peak admission limit; active state unchanged."));
    RefreshState();
    return;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  if (session_.HostActive() && session_.prepared()->host_adapter->IsPublicCompleteRecord() &&
      session_.HostHasDiscardableState()) {
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Replace ASCII Host Session"),
        QStringLiteral("Publishing will discard active ASCII flow drafts/results. Continue?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
      host_status_->setText(
          QStringLiteral("Publication cancelled; active Session and retry draft retained."));
      RefreshState();
      return;
    }
  }
#endif
  if (!ConfirmStreamDiscardOnly(QStringLiteral("publish replacement binding table"))) {
    host_status_->setText(QStringLiteral("Publication cancelled; all active flows preserved."));
    RefreshState();
    return;
  }
  try {
    if (!session_.ApplyHostAdapter(std::move(candidate))) {
      host_status_->setText(QStringLiteral("Candidate publication rejected."));
      RefreshState();
      return;
    }
  } catch (const std::exception&) {
    host_status_->setText(
        QStringLiteral("Publication preparation allocation failed; active Session unchanged."));
    RefreshState();
    return;
  }
  rebuilding_selectors_ = true;
  host_binding_combo_->clear();
  for (const auto& binding : session_.prepared()->host_adapter->Bindings())
    host_binding_combo_->addItem(
        FromUtf8(binding.endpoint) +
        (binding.action == host_endpoint::Action::DECODE ? QStringLiteral(" / Decode / ")
                                                         : QStringLiteral(" / Encode / ")) +
        FromUtf8(session_.description()->pipelines[binding.pipeline_index].id));
  host_binding_combo_->setCurrentIndex(0);
  host_flow_combo_->setCurrentIndex(0);
  rebuilding_selectors_ = false;
  SelectHostView();
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
    if (!session_.HostActive()) return;
    if (!session_.SelectHostFlow(static_cast<std::size_t>(host_binding_combo_->currentIndex()),
                                 static_cast<std::size_t>(host_flow_combo_->currentIndex())))
      return;
  }
  rebuilding_selectors_ = true;
  mode_combo_->setCurrentIndex(mode_combo_->findData(static_cast<int>(session_.mode())));
  representation_combo_->setCurrentIndex(
      representation_combo_->findData(static_cast<int>(session_.representation())));
  if (!session_.BinaryHostActive()) {
    const auto& text = session_.inspect_draft_utf16();
    inspect_input_->setPlainText(QString::fromUtf16(reinterpret_cast<const ushort*>(text.data()),
                                                    static_cast<int>(text.size())));
    accepted_inspect_text_ = inspect_input_->toPlainText();
  } else {
    pipeline_combo_->setCurrentIndex(
        pipeline_combo_->findData(static_cast<qulonglong>(*session_.selected_pipeline_index())));
    message_combo_->setCurrentIndex(
        message_combo_->findData(static_cast<qulonglong>(session_.selection()->message_index)));
  }
  rebuilding_selectors_ = false;
  timing_label_->clear();
  if (session_.BinaryHostActive()) {
    field_model_->Reset(CurrentMessage(), {}, {}, false, session_.representation(),
                        FieldPresentationAction::INSPECT);
  } else {
    RebuildSelectorsAndModel();
  }
  RefreshInspect();
  RefreshPreview();
  RefreshModePresentation();
  RefreshState();
  host_status_->setText(
      QStringLiteral("Host Session active | Tab=%1 load=%2 session=%3 binding=%4 flow=%5. Draft "
                     "edits require Apply; view switches preserve flows.")
          .arg(session_.id())
          .arg(session_.load_revision())
          .arg(session_.plan_generation())
          .arg(
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
              session_.BinaryHostActive() ? session_.BinaryHostBindingIndex() :
#endif
                                          session_.HostBindingIndex())
          .arg(
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
              session_.BinaryHostActive() ? session_.BinaryHostFlowIndex() :
#endif
                                          session_.HostStreamIndex()));
}
#endif

void DocumentTab::BuildUi() {
  auto* root = new QVBoxLayout(this);
  auto* path_row = new QHBoxLayout;
  path_edit_ = new QLineEdit(this);
  path_edit_->setPlaceholderText(QStringLiteral("PAE configuration JSON path"));
  browse_button_ = new QPushButton(QStringLiteral("Browse..."), this);
  load_button_ = new QPushButton(QStringLiteral("Load / Reload"), this);
  path_row->addWidget(path_edit_, 1);
  path_row->addWidget(browse_button_);
  path_row->addWidget(load_button_);
  root->addLayout(path_row);

  identity_label_ = new QLabel(this);
  identity_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  root->addWidget(identity_label_);
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  BuildHostUi(root);
#endif

  auto* selection_row = new QHBoxLayout;
  pipeline_combo_ = new QComboBox(this);
  mode_combo_ = new QComboBox(this);
  mode_combo_->addItem(QStringLiteral("Encode"), static_cast<int>(OperationMode::ENCODE));
  mode_combo_->addItem(QStringLiteral("Inspect"), static_cast<int>(OperationMode::INSPECT));
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  mode_combo_->addItem(QStringLiteral("Stream Inspect"),
                       static_cast<int>(OperationMode::STREAM_INSPECT));
#endif
  message_combo_ = new QComboBox(this);
  representation_combo_ = new QComboBox(this);
  representation_combo_->setToolTip(QStringLiteral(
      "切换表示会按当前格式解析并转换已有草稿，不会重新解释输入。\n"
      "转换失败时保留当前格式与草稿；请修正草稿，或先复制/清空，再切换格式并输入。"));
  representation_combo_->addItem(QStringLiteral("Hex"), static_cast<int>(ByteRepresentation::HEX));
  representation_combo_->addItem(QStringLiteral("ASCII (escaped)"),
                                 static_cast<int>(ByteRepresentation::ASCII_ESCAPED));
  encode_button_ = new QPushButton(QStringLiteral("Encode"), this);
  inspect_button_ = new QPushButton(QStringLiteral("Inspect complete record"), this);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  continue_button_ = new QPushButton(QStringLiteral("Continue"), this);
  reset_stream_button_ = new QPushButton(QStringLiteral("Reset stream"), this);
#endif
  selection_row->addWidget(new QLabel(QStringLiteral("Mode"), this));
  selection_row->addWidget(mode_combo_);
  selection_row->addWidget(new QLabel(QStringLiteral("Pipeline"), this));
  selection_row->addWidget(pipeline_combo_, 1);
  selection_row->addWidget(new QLabel(QStringLiteral("Encode Message"), this));
  selection_row->addWidget(message_combo_, 1);
  selection_row->addWidget(new QLabel(QStringLiteral("Representation"), this));
  selection_row->addWidget(representation_combo_);
  selection_row->addWidget(encode_button_);
  selection_row->addWidget(inspect_button_);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  selection_row->addWidget(continue_button_);
  selection_row->addWidget(reset_stream_button_);
#endif
  root->addLayout(selection_row);

  inspect_input_label_ =
      new QLabel(QStringLiteral("Raw input / 原始输入（Hex；允许大小写及 SP/HT/CR/LF）"), this);
  inspect_input_ = new QPlainTextEdit(this);
  inspect_input_->setPlaceholderText(
      QStringLiteral("Paste one complete record, for example: AA 00 06 00 00 55"));
  inspect_input_->setMaximumHeight(92);
  root->addWidget(inspect_input_label_);
  root->addWidget(inspect_input_);

  result_kind_label_ = new QLabel(this);
  result_kind_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  root->addWidget(result_kind_label_);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  stream_status_label_ = new QLabel(this);
  stream_status_label_->setWordWrap(true);
  stream_status_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  root->addWidget(stream_status_label_);
#endif

  auto* vertical_splitter = new QSplitter(Qt::Vertical, this);
  auto* upper_splitter = new QSplitter(Qt::Horizontal, vertical_splitter);
  field_table_ = new QTableView(upper_splitter);
  field_model_ = new FieldTableModel(field_table_);
  value_delegate_ = new ExactValueDelegate(field_table_);
  field_table_->setModel(field_model_);
  field_table_->setItemDelegateForColumn(FieldTableModel::VALUE, value_delegate_);
  field_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  field_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  field_table_->setAlternatingRowColors(true);
  field_table_->horizontalHeader()->setStretchLastSection(true);
  field_table_->verticalHeader()->setVisible(false);
  details_view_ = new QTextBrowser(upper_splitter);
  details_view_->setOpenExternalLinks(false);
  details_view_->setPlaceholderText(
      QStringLiteral("Description, source, conversion and integrity annotations"));
  upper_splitter->addWidget(field_table_);
  upper_splitter->addWidget(details_view_);
  upper_splitter->setStretchFactor(0, 3);
  upper_splitter->setStretchFactor(1, 1);

  hex_view_ = new HexView(vertical_splitter);
  vertical_splitter->addWidget(upper_splitter);
  vertical_splitter->addWidget(hex_view_);
  vertical_splitter->setStretchFactor(0, 3);
  vertical_splitter->setStretchFactor(1, 2);
  root->addWidget(vertical_splitter, 1);

  diagnostic_label_ = new QLabel(this);
  diagnostic_label_->setWordWrap(true);
  diagnostic_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  timing_label_ = new QLabel(this);
  timing_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  root->addWidget(diagnostic_label_);
  root->addWidget(timing_label_);

  connect(browse_button_, &QPushButton::clicked, this, [this] {
    const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("Open PAE configuration"),
                                                   path_edit_->text(),
                                                   QStringLiteral("JSON (*.json);;All files (*)"));
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
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
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
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
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
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  if (!discard_confirmed && !ConfirmStreamDiscard(QStringLiteral("reload this configuration")))
    return;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI) && defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  if (!discard_confirmed &&
      (session_.BinaryHasDiscardableState() ||
       (session_.IsBinaryHostDocument() && host_pending_revision_)) &&
      QMessageBox::question(this, QStringLiteral("Reload Binary Host document"),
                            QStringLiteral("Reloading will discard Binary flow drafts/results or "
                                           "pending preparation. Continue?"),
                            QMessageBox::Yes | QMessageBox::No,
                            QMessageBox::No) != QMessageBox::Yes)
    return;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  host_pending_revision_.reset();
  host_pending_bindings_.clear();
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
    completion->diagnostic = config_compiler::CompileDiagnostic{
        config_compiler::CompileStage::INPUT_PROFILE, config_compiler::CompileError::EMPTY_INPUT,
        "", std::nullopt, std::string("cannot read config: ") + Utf8(file.errorString())};
    AcceptCompletion(std::move(completion));
    return;
  }
  const auto bytes = file.read(static_cast<qint64>(kMaximumConfigBytes + 1U));
  if (bytes.size() > static_cast<qint64>(kMaximumConfigBytes)) {
    auto completion = std::make_unique<CompileCompletion>();
    completion->document_id = session_.id();
    completion->load_revision = revision;
    completion->diagnostic =
        config_compiler::CompileDiagnostic{config_compiler::CompileStage::INPUT_PROFILE,
                                           config_compiler::CompileError::INPUT_LIMIT_EXCEEDED, "",
                                           std::nullopt, "UI config file exceeds 4 MiB precheck"};
    AcceptCompletion(std::move(completion));
    return;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  host_config_text_.assign(bytes.constData(), static_cast<std::size_t>(bytes.size()));
#endif
  const auto status =
      worker_.Submit(session_.id(), revision,
                     std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
  if (status != SubmitStatus::ACCEPTED) {
    auto completion = std::make_unique<CompileCompletion>();
    completion->document_id = session_.id();
    completion->load_revision = revision;
    completion->diagnostic = config_compiler::CompileDiagnostic{
        config_compiler::CompileStage::INPUT_PROFILE,
        config_compiler::CompileError::INPUT_LIMIT_EXCEEDED, "", std::nullopt,
        "compile request was rejected by bounded scheduler"};
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
  for (const auto message_index : description->pipelines[pipeline_index].message_indices) {
    if (message_index >= description->messages.size()) {
      continue;
    }
    const auto& message = description->messages[message_index];
    const auto& base_label = message.display_name.empty() ? message.id : message.display_name;
    QString label = FromUtf8(base_label);
    if (description->layout == DocumentLayout::ASCII_TEXT) {
      label +=
          QStringLiteral(" [%1%2]")
              .arg(message.encode_available ? QStringLiteral("Encode") : QStringLiteral(""))
              .arg(message.decode_available ? (message.encode_available ? QStringLiteral("/Decode")
                                                                        : QStringLiteral("Decode"))
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
  if (description != nullptr) {
    identity_label_->setText(QStringLiteral("Schema %1 | Protocol %2 %3")
                                 .arg(FromUtf8(description->schema_version),
                                      FromUtf8(description->protocol_id),
                                      FromUtf8(description->protocol_version)));
  } else {
    identity_label_->setText(QStringLiteral("No compiled document"));
  }
  const bool loading = session_.state() == DocumentState::LOADING;
  pipeline_combo_->setEnabled(description != nullptr && !loading);
  mode_combo_->setEnabled(description != nullptr && !loading);
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  const bool host_document = session_.IsAsciiDocument()
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
                             || session_.IsBinaryHostDocument()
#endif
      ;
  host_panel_->setVisible(host_document);
  host_apply_->setEnabled(host_document && !loading && !host_pending_revision_);
  host_draft_->setEnabled(!host_pending_revision_);
  const bool active_host = session_.HostActive()
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
                           || session_.BinaryHostActive()
#endif
      ;
  host_binding_combo_->setEnabled(active_host);
  host_flow_combo_->setEnabled(
      active_host &&
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
      (session_.BinaryHostActive() ||
#endif
       (session_.HostActive() &&
        session_.prepared()->host_adapter->Bindings()[session_.HostBindingIndex()].action ==
            host_endpoint::Action::DECODE)
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
           )
#endif
  );
  if (active_host) {
    pipeline_combo_->setEnabled(false);
    mode_combo_->setEnabled(false);
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
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
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  encode_button_->setVisible(!session_.IsBinaryHostDocument());
#endif
  const bool encode_mode = session_.mode() == OperationMode::ENCODE;
  const bool inspect_mode = session_.mode() == OperationMode::INSPECT;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
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
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
                                            : session_.StreamInspectAvailable()
#else
                                            : false
#endif
                                   ) &&
                              !loading && !encode_mode
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
                              && !(stream_mode && session_.StreamContinueAvailable())
#endif
  );
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  inspect_button_->setText(stream_mode ? QStringLiteral("Submit chunk")
                                       : QStringLiteral("Inspect complete record"));
  continue_button_->setVisible(stream_mode);
  reset_stream_button_->setVisible(stream_mode);
  stream_status_label_->setVisible(stream_mode);
  continue_button_->setEnabled(stream_mode && !loading && session_.StreamContinueAvailable());
  reset_stream_button_->setEnabled(stream_mode && !loading && session_.StreamInspectAvailable());
  inspect_input_->setReadOnly(stream_mode && session_.StreamContinueAvailable());
  representation_combo_->setEnabled(session_.IsAsciiDocument() && !loading);
  if (stream_mode) {
    const auto observation = session_.StreamObservation();
    if (observation.has_value()) {
      QString text =
          QStringLiteral(
              "phase=%1 | buffered=%2 | internal=%3 | C=%4 | work=%5 | "
              "frozen=%6/%7 | generation=%8 | step=%9 | candidates=%10 | "
              "decode_ok=%11 | discarded=%12 | malformed=%13 | reset_required=%14")
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
              .arg(observation->reset_required ? QStringLiteral("true") : QStringLiteral("false"));
      if (session_.stream_step().has_value()) {
        const auto& step = *session_.stream_step();
        text += QStringLiteral(
                    "\nlast step: api=%1 | stop=%2 | consumed=%3 | frames=%4 | "
                    "discarded=%5 | malformed=%6 | issue=%7 | work=%8")
                    .arg(SubmitApiStatusName(step.framing.api_status))
                    .arg(StopReasonName(step.framing.stop_reason))
                    .arg(static_cast<qulonglong>(step.framing.bytes_consumed))
                    .arg(static_cast<qulonglong>(step.framing.frames_delivered))
                    .arg(static_cast<qulonglong>(step.framing.bytes_discarded))
                    .arg(static_cast<qulonglong>(step.framing.malformed_candidates))
                    .arg(FramingIssueName(step.framing.last_framing_issue))
                    .arg(static_cast<qulonglong>(step.framing.work_units_used));
      }
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
      if (session_.HostActive())
        text += QStringLiteral(" | observed=%1 | business=%2")
                    .arg(observation->total_observed_candidates)
                    .arg(observation->total_business_outputs);
#endif
      stream_status_label_->setText(text);
    } else {
      stream_status_label_->setText(QStringLiteral("Stream observer unavailable"));
    }
  }
#endif
  if (session_.diagnostic_id().empty() && session_.diagnostic_detail().empty()) {
    diagnostic_label_->clear();
  } else {
    diagnostic_label_->setText(QStringLiteral("%1: %2").arg(
        FromUtf8(session_.diagnostic_id()), FromUtf8(session_.diagnostic_detail())));
  }
}

void DocumentTab::RefreshModePresentation() {
  const bool inspect_mode = session_.mode() != OperationMode::ENCODE;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  const bool stream_mode = session_.mode() == OperationMode::STREAM_INSPECT;
#else
  const bool stream_mode = false;
#endif
  inspect_input_label_->setText(
      session_.IsAsciiDocument() && session_.representation() == ByteRepresentation::ASCII_ESCAPED
          ? QStringLiteral("Raw input / 原始输入（ASCII escaped；实际控制字符和非ASCII被拒绝）")
          : QStringLiteral("Raw input / 原始输入（Hex；允许大小写及 SP/HT/CR/LF）"));
  if (stream_mode) {
    inspect_input_label_->setText(QStringLiteral(
        "Stream chunk / 流输入块（容量按 C=min(65536,effective max_submit_bytes)）"));
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
            ? QStringLiteral("ASCII Encode output / 待执行")
            : QStringLiteral("Valid encoded output / 有效编码输出（仅 Encode OK 时）"));
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
        QStringLiteral(
            "Valid decoded result / 有效解码结果 | matched Message: %1 | 成功，%2 个字段")
            .arg(FromUtf8(session_.inspect_result()->message_id).toHtmlEscaped())
            .arg(session_.inspect_result()->fields.size()));
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
    result_kind_label_->setText(QStringLiteral("Failure location / 失败定位（非有效结果）"));
    field_model_->SetFailedField(failure.failed_field_index);
    FillInspectFailureHighlights(message, highlights);
    if (message != nullptr && failure.failed_field_index.has_value() &&
        *failure.failed_field_index < message->fields.size()) {
      failed_detail_row = static_cast<int>(*failure.failed_field_index);
    }
  } else {
    result_kind_label_->setText(
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
        session_.mode() == OperationMode::STREAM_INSPECT && session_.stream_step().has_value()
            ? QStringLiteral("Current step / 本步无候选")
            :
#endif
            QStringLiteral("Raw input / 原始输入（尚无有效解码结果）"));
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
        QStringLiteral("Valid ASCII Encode / 成功，%1 个字段 | review kind: %2")
            .arg(session_.preview()->fields.size())
            .arg(session_.preview()->tx_template_review ? QStringLiteral("TX_TEMPLATE")
                                                        : QStringLiteral("NOT_APPLICABLE")));
  }
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
        QStringLiteral("<b>Message</b>: %1").arg(FromUtf8(message_name).toHtmlEscaped()));
    if (!message->description.empty()) {
      details.push_back(FromUtf8(message->description).toHtmlEscaped());
    }
    if (!message->source_ref.empty()) {
      details.push_back(QStringLiteral("<b>Message source</b>: %1")
                            .arg(FromUtf8(message->source_ref).toHtmlEscaped()));
    }
    if (message->integrity_storage.has_value()) {
      const auto actual_storage = ActualFrameSize().has_value()
                                      ? ResolveActualIntegrityStorage(*message, *ActualFrameSize())
                                      : std::optional<ByteRange>{};
      if (message->integrity_storage_at_payload_end && !actual_storage.has_value()) {
        details.push_back(QStringLiteral(
            "<b>Integrity storage</b>: dynamic at payload end; current range unavailable"));
      } else {
        const auto& storage =
            actual_storage.has_value() ? *actual_storage : *message->integrity_storage;
        details.push_back(QStringLiteral("<b>Integrity storage</b>: %1 + %2")
                              .arg(static_cast<qulonglong>(storage.offset))
                              .arg(static_cast<qulonglong>(storage.length)));
      }
      if (message->integrity_range_ends_at_payload) {
        details.push_back(QStringLiteral(
            "<b>Integrity coverage</b>: configured range ends at actual payload end"));
      }
    }
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    if (message->computed_length_storage.has_value()) {
      details.push_back(
          QStringLiteral("<b>Computed length storage</b>: %1 + %2")
              .arg(static_cast<qulonglong>(message->computed_length_storage->offset))
              .arg(static_cast<qulonglong>(message->computed_length_storage->length)));
    }
#endif
  }
  details.push_back(QStringLiteral("<b>%1</b>")
                        .arg(FromUtf8(field->display_name.empty() ? field->id : field->display_name)
                                 .toHtmlEscaped()));
  if (!field->description.empty()) {
    details.push_back(FromUtf8(field->description).toHtmlEscaped());
  }
  if (!field->source_ref.empty()) {
    details.push_back(
        QStringLiteral("<b>Source</b>: %1").arg(FromUtf8(field->source_ref).toHtmlEscaped()));
  }
  if (field->ascii_text) {
    details.push_back(QStringLiteral("<b>Action participation</b>: Decode %1; Encode %2")
                          .arg(field->decode_referenced ? QStringLiteral("referenced")
                                                        : QStringLiteral("not referenced"),
                               field->encode_referenced ? QStringLiteral("referenced")
                                                        : QStringLiteral("not referenced")));
  } else if (!field->read_only_annotation.empty()) {
    details.push_back(QStringLiteral("<b>Storage / integrity / conversion</b>: %1")
                          .arg(FromUtf8(field->read_only_annotation).toHtmlEscaped()));
  }
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (field->conversion.has_value() || field->decode_decimal64) {
    details.push_back(
        field->decode_decimal64
            ? QStringLiteral("<b>Conversion</b>: logical Decimal64 result; Core performs "
                             "logical/raw representability checks")
            : QStringLiteral("<b>Conversion</b>: logical Decimal64 input; Core performs "
                             "logical/raw representability checks"));
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
    details.push_back(QStringLiteral("<b>Payload length bounds</b>: %1..%2 bytes")
                          .arg(static_cast<qulonglong>(field->byte_length_bounds->minimum))
                          .arg(static_cast<qulonglong>(field->byte_length_bounds->maximum)));
    if (actual_result_range.has_value()) {
      details.push_back(QStringLiteral("<b>Actual byte range</b>: %1 + %2")
                            .arg(static_cast<qulonglong>(actual_result_range->offset))
                            .arg(static_cast<qulonglong>(actual_result_range->length)));
    } else if (!session_.IsAsciiDocument() && ActualFrameSize().has_value()) {
      const auto range = ResolveActualFieldRange(*message, *field, *ActualFrameSize());
      if (range.has_value()) {
        details.push_back(QStringLiteral("<b>Actual byte range</b>: %1 + %2")
                              .arg(static_cast<qulonglong>(range->offset))
                              .arg(static_cast<qulonglong>(range->length)));
      }
    }
  } else if (field->byte_range.has_value()) {
    details.push_back(QStringLiteral("<b>Byte range</b>: %1 + %2")
                          .arg(static_cast<qulonglong>(field->byte_range->offset))
                          .arg(static_cast<qulonglong>(field->byte_range->length)));
  }
  if (!field->physical_bits.empty()) {
    details.push_back(QStringLiteral("<b>Physical bit cells</b>: %1")
                          .arg(static_cast<qulonglong>(field->physical_bits.size())));
  }
  if (session_.IsAsciiDocument()) {
    if (actual_result_range.has_value()) {
      details.push_back(QStringLiteral("<b>Actual physical bytes (zero-based)</b>: [%1, %2), "
                                       "full-byte range")
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
      details.push_back(QStringLiteral("<b>Physical byte / bit / mask (zero-based, LSB0)</b>: %1")
                            .arg(FromUtf8(physical).toHtmlEscaped()));
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
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  if (session_.selected_pipeline_index().has_value() &&
      requested != *session_.selected_pipeline_index() &&
      !ConfirmStreamDiscard(QStringLiteral("switch Pipeline"))) {
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
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
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
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  if (session_.mode() == OperationMode::STREAM_INSPECT && mode != OperationMode::STREAM_INSPECT &&
      !ConfirmStreamDiscard(QStringLiteral("leave Stream Inspect"))) {
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
  }
  RefreshState();
  hex_view_->viewport()->repaint();
  timing_.first_repaint_total_ns = total.nsecsElapsed();
  if (session_.IsAsciiDocument()) {
    timing_label_->setText(
        session_.preview().has_value() && session_.preview()->tx_template_review
            ? QStringLiteral("input %1 ms | adapter + UI total %2 ms | review kind TX_TEMPLATE; no "
                             "independent RX Decode timing")
                  .arg(static_cast<double>(timing_.exact_input_ns) / 1000000.0, 0, 'f', 3)
                  .arg(static_cast<double>(timing_.first_repaint_total_ns) / 1000000.0, 0, 'f', 3)
            : QStringLiteral("ASCII Encode failed; no successful review result"));
  } else {
    timing_label_->setText(TimingText(timing_));
  }
}

void DocumentTab::InspectCurrent() {
  timing_label_->clear();
  field_table_->clearFocus();
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  session_.SetInspectDraftUtf16(Utf16(inspect_input_->toPlainText()));
  TimingObserver observer(timing_);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
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

#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
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
  const auto answer =
      QMessageBox::question(this, QStringLiteral("Discard stream state?"),
                            QStringLiteral("%1 will discard affected stream state (including "
                                           "non-selected flows) and any frozen suffix. Continue?")
                                .arg(action),
                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
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
  if (session_.mode() == OperationMode::INSPECT && session_.inspect_result().has_value()) {
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
