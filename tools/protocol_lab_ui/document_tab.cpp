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
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QTextBrowser>
#include <QTextCursor>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

#include "exact_value_delegate.h"
#include "field_table_model.h"
#include "hex_view.h"

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

std::vector<PhysicalBitMask> FieldHighlights(const MessageDescriptor* message,
                                             const FieldDescriptor* field,
                                             std::optional<std::size_t> actual_frame_size) {
  if (message == nullptr || field == nullptr) {
    return {};
  }
  if (!field->physical_bits.empty()) {
    return field->physical_bits;
  }
  std::vector<PhysicalBitMask> output;
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

void ReplaceEditorTextByKeyboard(QLineEdit& editor, const QString& text) {
  editor.selectAll();
  for (const QChar character : text) {
    QKeyEvent event{QEvent::KeyPress, static_cast<int>(character.unicode()), Qt::NoModifier,
                    QString{character}};
    QApplication::sendEvent(&editor, &event);
  }
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ReplaceEditorTextByPaste(QLineEdit& editor, const QString& text) {
  QApplication::clipboard()->setText(text);
  editor.selectAll();
  QKeyEvent event{QEvent::KeyPress, Qt::Key_V, Qt::ControlModifier};
  QApplication::sendEvent(&editor, &event);
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void CommitEditorByKey(QLineEdit& editor, int key) {
  QKeyEvent press{QEvent::KeyPress, key, Qt::NoModifier};
  QApplication::sendEvent(&editor, &press);
  QKeyEvent release{QEvent::KeyRelease, key, Qt::NoModifier};
  QApplication::sendEvent(&editor, &release);
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

}  // namespace

DocumentTab::DocumentTab(DocumentId document_id, CompileWorker& worker, QWidget* parent)
    : QWidget(parent), worker_(worker), session_(document_id) {
  BuildUi();
  RefreshState();
}

DocumentTab::~DocumentTab() { CloseDocument(); }

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
  if (closed_ || completion == nullptr || completion->document_id != session_.id()) {
    return;
  }
  const bool published = session_.ApplyCompileCompletion(std::move(completion));
  ResetVisibleDocument();
  if (published) {
    RebuildSelectorsAndModel();
  }
  RefreshState();
}

void DocumentTab::CloseDocument() {
  if (closed_) {
    return;
  }
  closed_ = true;
  worker_.CloseDocument(session_.id());
  field_model_->Reset(nullptr, {});
  hex_view_->ClearFrame();
  session_.Close();
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

  auto* editor = open_editor();
  if (editor == nullptr || editor->property("paeEditCapacity").toInt() != 8) {
    error = QStringLiteral("bounded BYTES editor capacity is not the expected 8 Hex characters");
    return false;
  }
  ReplaceEditorTextByKeyboard(*editor, QStringLiteral("1020"));
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
  ReplaceEditorTextByKeyboard(*editor, QStringLiteral("01020304"));
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
  ReplaceEditorTextByPaste(*editor, QStringLiteral("01020304"));
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
  ReplaceEditorTextByKeyboard(*editor, QStringLiteral("010203"));
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
  ReplaceEditorTextByPaste(*editor, QStringLiteral("0102030405"));
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
  ReplaceEditorTextByKeyboard(*editor, QStringLiteral("1020"));
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

bool DocumentTab::VerifyAsciiForSmoke(QString& error) {
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
  field_table_->edit(name_index);
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
  if (editor == nullptr || editor->property("paeEditCapacity").toInt() != 36 ||
      editor->property("paeByteRepresentation").toInt() !=
          static_cast<int>(ByteRepresentation::ASCII_ESCAPED)) {
    error = QStringLiteral("ASCII table editor did not expose expected escaped capacity=36");
    return false;
  }
  ReplaceEditorTextByKeyboard(*editor, QStringLiteral("ALICE"));
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

  field_table_->edit(name_index);
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
  if (editor == nullptr) {
    error = QStringLiteral("failed to reopen ASCII table editor");
    return false;
  }
  ReplaceEditorTextByPaste(*editor, QStringLiteral("ABCDEFGHI"));
  CommitEditorByKey(*editor, Qt::Key_Tab);
  if (!field_model_->ValidationError(name_row).contains(QStringLiteral("1..8 bytes")) ||
      session_.preview().has_value()) {
    error = QStringLiteral("ASCII over-protocol pasted draft was not retained as invalid");
    return false;
  }
  field_table_->edit(name_index);
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
  if (editor == nullptr) return false;
  ReplaceEditorTextByPaste(*editor, QString(37, QLatin1Char('A')));
  if (editor->text() != QStringLiteral("ABCDEFGHI") ||
      !editor->property("paeCapacityRejected").toBool()) {
    error = QStringLiteral("ASCII over-capacity paste was not wholly rejected");
    return false;
  }
  ReplaceEditorTextByKeyboard(*editor, QStringLiteral("ALICE"));
  CommitEditorByKey(*editor, Qt::Key_Return);
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

  auto* selection_row = new QHBoxLayout;
  pipeline_combo_ = new QComboBox(this);
  mode_combo_ = new QComboBox(this);
  mode_combo_->addItem(QStringLiteral("Encode"), static_cast<int>(OperationMode::ENCODE));
  mode_combo_->addItem(QStringLiteral("Inspect"), static_cast<int>(OperationMode::INSPECT));
  message_combo_ = new QComboBox(this);
  representation_combo_ = new QComboBox(this);
  representation_combo_->addItem(QStringLiteral("Hex"), static_cast<int>(ByteRepresentation::HEX));
  representation_combo_->addItem(QStringLiteral("ASCII (escaped)"),
                                 static_cast<int>(ByteRepresentation::ASCII_ESCAPED));
  encode_button_ = new QPushButton(QStringLiteral("Encode"), this);
  inspect_button_ = new QPushButton(QStringLiteral("Inspect complete record"), this);
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
  connect(mode_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int index) { SelectMode(index); });
  connect(representation_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int index) { SelectRepresentation(index); });
  connect(inspect_input_, &QPlainTextEdit::textChanged, this, [this] {
    if (rebuilding_selectors_) return;
    timing_label_->clear();
    const QString text = inspect_input_->toPlainText();
    const auto capacity =
        InspectEditorCapacity(session_.InspectFrameBudget(), session_.representation());
    if (!capacity.has_value() || text.size() > *capacity) {
      rebuilding_selectors_ = true;
      inspect_input_->setPlainText(accepted_inspect_text_);
      rebuilding_selectors_ = false;
      session_.RejectInspectCapacity(capacity.value_or(0));
      RefreshInspect();
      RefreshState();
      return;
    }
    accepted_inspect_text_ = text;
    session_.SetInspectDraftUtf16(Utf16(text));
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

void DocumentTab::BeginLoadFromPath() {
  if (closed_) {
    return;
  }
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
  if (session_.selection().has_value()) {
    for (int index = 0; index < pipeline_combo_->count(); ++index) {
      if (pipeline_combo_->itemData(index).toULongLong() == session_.selection()->pipeline_index) {
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
  representation_combo_->setVisible(session_.IsAsciiDocument());
  representation_combo_->setEnabled(session_.IsAsciiDocument() && !loading);
  const bool encode_mode = session_.mode() == OperationMode::ENCODE;
  message_combo_->setEnabled(description != nullptr && !loading && encode_mode);
  encode_button_->setEnabled(description != nullptr && session_.selection().has_value() &&
                             session_.EncodeAvailable() && !loading && encode_mode);
  inspect_button_->setEnabled(description != nullptr &&
                              session_.selected_pipeline_index().has_value() &&
                              session_.InspectAvailable() && !loading && !encode_mode);
  if (session_.diagnostic_id().empty() && session_.diagnostic_detail().empty()) {
    diagnostic_label_->clear();
  } else {
    diagnostic_label_->setText(QStringLiteral("%1: %2").arg(
        FromUtf8(session_.diagnostic_id()), FromUtf8(session_.diagnostic_detail())));
  }
}

void DocumentTab::RefreshModePresentation() {
  const bool inspect_mode = session_.mode() == OperationMode::INSPECT;
  inspect_input_label_->setText(
      session_.IsAsciiDocument() && session_.representation() == ByteRepresentation::ASCII_ESCAPED
          ? QStringLiteral("Raw input / 原始输入（ASCII escaped；实际控制字符和非ASCII被拒绝）")
          : QStringLiteral("Raw input / 原始输入（Hex；允许大小写及 SP/HT/CR/LF）"));
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
  std::vector<std::uint8_t> frame;
  std::vector<PhysicalBitMask> highlights;
  std::optional<int> failed_detail_row;
  if (session_.inspect_result().has_value()) {
    frame = session_.inspect_result()->input_frame;
    field_model_->SetActualFrameSize(frame.size());
    field_model_->ApplyResults(session_.inspect_result()->fields);
    result_kind_label_->setText(
        QStringLiteral(
            "Valid decoded result / 有效解码结果 | matched Message: %1 | 成功，%2 个字段")
            .arg(FromUtf8(session_.inspect_result()->message_id).toHtmlEscaped())
            .arg(session_.inspect_result()->fields.size()));
    const auto* field = field_model_->FieldAt(field_table_->currentIndex().row());
    highlights = session_.IsAsciiDocument()
                     ? ActualFieldHighlights(session_.inspect_result()->fields, field)
                     : FieldHighlights(message, field, frame.size());
  } else if (session_.inspect_failure().has_value()) {
    const auto& failure = *session_.inspect_failure();
    frame = failure.input_frame;
    result_kind_label_->setText(QStringLiteral("Failure location / 失败定位（非有效结果）"));
    field_model_->SetFailedField(failure.failed_field_index);
    highlights = InspectFailureHighlights(message);
    if (message != nullptr && failure.failed_field_index.has_value() &&
        *failure.failed_field_index < message->fields.size()) {
      failed_detail_row = static_cast<int>(*failure.failed_field_index);
    }
  } else {
    result_kind_label_->setText(QStringLiteral("Raw input / 原始输入（尚无有效解码结果）"));
  }
  if (frame.empty()) {
    hex_view_->ClearFrame();
  } else {
    hex_view_->SetFrame(std::move(frame), highlights);
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
    if (session_.mode() == OperationMode::INSPECT) {
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
  if (field->conversion.has_value()) {
    details.push_back(
        QStringLiteral("<b>Conversion</b>: logical Decimal64 input; Core performs "
                       "logical/raw representability checks"));
  }
#endif
  std::optional<ByteRange> ascii_actual_range;
  if (session_.IsAsciiDocument()) {
    const std::vector<UiFieldResult>* results = nullptr;
    if (session_.mode() == OperationMode::ENCODE && session_.preview().has_value())
      results = &session_.preview()->fields;
    if (session_.mode() == OperationMode::INSPECT && session_.inspect_result().has_value())
      results = &session_.inspect_result()->fields;
    if (results != nullptr) {
      const auto found = std::find_if(results->begin(), results->end(), [&](const auto& result) {
        return result.field_index == field->field_index && result.id == field->id;
      });
      if (found != results->end()) ascii_actual_range = found->actual_range;
    }
  }
  if (field->byte_length_bounds.has_value()) {
    details.push_back(QStringLiteral("<b>Payload length bounds</b>: %1..%2 bytes")
                          .arg(static_cast<qulonglong>(field->byte_length_bounds->minimum))
                          .arg(static_cast<qulonglong>(field->byte_length_bounds->maximum)));
    if (ascii_actual_range.has_value()) {
      details.push_back(QStringLiteral("<b>Actual byte range</b>: %1 + %2")
                            .arg(static_cast<qulonglong>(ascii_actual_range->offset))
                            .arg(static_cast<qulonglong>(ascii_actual_range->length)));
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
    if (ascii_actual_range.has_value()) {
      details.push_back(QStringLiteral("<b>Actual physical bytes (zero-based)</b>: [%1, %2), "
                                       "full-byte range")
                            .arg(static_cast<qulonglong>(ascii_actual_range->offset))
                            .arg(static_cast<qulonglong>(ascii_actual_range->offset +
                                                         ascii_actual_range->length)));
    }
  } else {
    const auto physical = message == nullptr
                              ? FormatPhysicalLocation(*field)
                              : FormatPhysicalLocation(*message, *field, ActualFrameSize());
    if (!physical.empty()) {
      details.push_back(QStringLiteral("<b>Physical byte / bit / mask (zero-based, LSB0)</b>: %1")
                            .arg(FromUtf8(physical).toHtmlEscaped()));
    }
  }
  details_view_->setHtml(details.join(QStringLiteral("<br/>")));
  if (!refresh_frame) return;
  if (session_.mode() == OperationMode::INSPECT) {
    std::vector<std::uint8_t> frame;
    std::vector<PhysicalBitMask> highlights;
    if (session_.inspect_result().has_value()) {
      frame = session_.inspect_result()->input_frame;
      highlights = session_.IsAsciiDocument()
                       ? ActualFieldHighlights(session_.inspect_result()->fields, field)
                       : FieldHighlights(message, field, frame.size());
    } else if (session_.inspect_failure().has_value()) {
      frame = session_.inspect_failure()->input_frame;
      highlights = InspectFailureHighlights(message);
    }
    hex_view_->SetFrame(std::move(frame), highlights);
  } else {
    RefreshPreview();
  }
}

void DocumentTab::SelectPipeline(int combo_index) {
  if (rebuilding_selectors_ || combo_index < 0) {
    return;
  }
  if (session_.SelectPipeline(
          static_cast<std::size_t>(pipeline_combo_->itemData(combo_index).toULongLong()))) {
    timing_label_->clear();
    field_model_->Reset(nullptr, {});
    hex_view_->ClearFrame();
    rebuilding_selectors_ = true;
    inspect_input_->clear();
    rebuilding_selectors_ = false;
    accepted_inspect_text_.clear();
    RebuildMessageSelector();
  }
  RefreshState();
}

void DocumentTab::SelectMode(int combo_index) {
  if (rebuilding_selectors_ || combo_index < 0) return;
  const auto mode = static_cast<OperationMode>(mode_combo_->itemData(combo_index).toInt());
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
  session_.Inspect(&observer);
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

std::vector<PhysicalBitMask> DocumentTab::InspectFailureHighlights(
    const MessageDescriptor* message) const {
  std::vector<PhysicalBitMask> highlights;
  if (message == nullptr || !session_.inspect_failure().has_value()) return highlights;
  if (session_.IsAsciiDocument()) return highlights;
  const auto& failure = *session_.inspect_failure();
  if (failure.failed_field_index.has_value() &&
      *failure.failed_field_index < message->fields.size()) {
    return FieldHighlights(message, &message->fields[*failure.failed_field_index], std::nullopt);
  }
  if (failure.status == "INTEGRITY_FAILED" && message->integrity_storage.has_value() &&
      !message->integrity_storage_at_payload_end) {
    highlights.reserve(message->integrity_storage->length);
    for (std::size_t index = 0U; index < message->integrity_storage->length; ++index) {
      highlights.push_back(PhysicalBitMask{message->integrity_storage->offset + index, 0xFFU});
    }
  }
  return highlights;
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
