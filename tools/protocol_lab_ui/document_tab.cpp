#include "document_tab.h"

#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QTextBrowser>
#include <QTextCursor>
#include <QVBoxLayout>
#include <algorithm>
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
      value = QString(static_cast<int>(field->byte_width * 2U), QLatin1Char('0'));
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
  encode_button_ = new QPushButton(QStringLiteral("Encode"), this);
  inspect_button_ = new QPushButton(QStringLiteral("Inspect complete record"), this);
  selection_row->addWidget(new QLabel(QStringLiteral("Mode"), this));
  selection_row->addWidget(mode_combo_);
  selection_row->addWidget(new QLabel(QStringLiteral("Pipeline"), this));
  selection_row->addWidget(pipeline_combo_, 1);
  selection_row->addWidget(new QLabel(QStringLiteral("Encode Message"), this));
  selection_row->addWidget(message_combo_, 1);
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
  connect(inspect_input_, &QPlainTextEdit::textChanged, this, [this] {
    if (rebuilding_selectors_) return;
    QString text = inspect_input_->toPlainText();
    const std::size_t frame_budget = session_.InspectFrameBudget();
    const int maximum_utf16_units =
        static_cast<int>((std::min)(std::size_t{196609U},
                                    frame_budget == 0U ? std::size_t{1U} : frame_budget * 3U + 1U));
    if (text.size() > maximum_utf16_units) {
      text.truncate(maximum_utf16_units);
      rebuilding_selectors_ = true;
      inspect_input_->setPlainText(text);
      rebuilding_selectors_ = false;
    }
    session_.SetInspectDraft(Utf8(text));
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
  rebuilding_selectors_ = false;
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
    const auto& label = message.display_name.empty() ? message.id : message.display_name;
    message_combo_->addItem(FromUtf8(label),
                            QVariant::fromValue(static_cast<qulonglong>(message_index)));
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
      [this](std::size_t field_index, std::string text, std::string validation_error) {
        session_.SetInvalidDraft(field_index, std::move(text), std::move(validation_error));
        RefreshPreview();
        RefreshState();
      });
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
  const bool encode_mode = session_.mode() == OperationMode::ENCODE;
  message_combo_->setEnabled(description != nullptr && !loading && encode_mode);
  encode_button_->setEnabled(description != nullptr && session_.selection().has_value() &&
                             !loading && encode_mode);
  inspect_button_->setEnabled(description != nullptr &&
                              session_.selected_pipeline_index().has_value() && !loading &&
                              !encode_mode);
  if (session_.diagnostic_id().empty() && session_.diagnostic_detail().empty()) {
    diagnostic_label_->clear();
  } else {
    diagnostic_label_->setText(QStringLiteral("%1: %2").arg(
        FromUtf8(session_.diagnostic_id()), FromUtf8(session_.diagnostic_detail())));
  }
}

void DocumentTab::RefreshModePresentation() {
  const bool inspect_mode = session_.mode() == OperationMode::INSPECT;
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
        [this](std::size_t field_index, std::string text, std::string validation_error) {
          session_.SetInvalidDraft(field_index, std::move(text), std::move(validation_error));
          RefreshPreview();
          RefreshState();
        });
    field_model_->ApplyDrafts(session_.drafts());
    field_model_->ApplyInvalidDrafts(session_.invalid_drafts());
    result_kind_label_->setText(
        QStringLiteral("Valid encoded output / 有效编码输出（仅 Encode OK 时）"));
    RefreshPreview();
  }
  RefreshState();
}

void DocumentTab::RefreshInspect() {
  const auto* message = DisplayedMessage();
  field_model_->Reset(message, {}, {}, false);
  std::vector<std::uint8_t> frame;
  std::vector<PhysicalBitMask> highlights;
  std::optional<int> failed_detail_row;
  if (session_.inspect_result().has_value()) {
    frame = session_.inspect_result()->input_frame;
    field_model_->SetActualFrameSize(frame.size());
    field_model_->ApplyResults(session_.inspect_result()->fields);
    result_kind_label_->setText(
        QStringLiteral("Valid decoded result / 有效解码结果 | matched Message: %1")
            .arg(FromUtf8(session_.inspect_result()->message_id).toHtmlEscaped()));
    const auto* field = field_model_->FieldAt(field_table_->currentIndex().row());
    highlights = FieldHighlights(message, field, frame.size());
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
      FieldHighlights(CurrentMessage(), field, session_.preview()->encoded_frame.size()));
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
  if (!field->read_only_annotation.empty()) {
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
  if (field->byte_length_bounds.has_value()) {
    details.push_back(QStringLiteral("<b>Payload length bounds</b>: %1..%2 bytes")
                          .arg(static_cast<qulonglong>(field->byte_length_bounds->minimum))
                          .arg(static_cast<qulonglong>(field->byte_length_bounds->maximum)));
    if (ActualFrameSize().has_value()) {
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
  const auto physical = message == nullptr
                            ? FormatPhysicalLocation(*field)
                            : FormatPhysicalLocation(*message, *field, ActualFrameSize());
  if (!physical.empty()) {
    details.push_back(QStringLiteral("<b>Physical byte / bit / mask (zero-based, LSB0)</b>: %1")
                          .arg(FromUtf8(physical).toHtmlEscaped()));
  }
  details_view_->setHtml(details.join(QStringLiteral("<br/>")));
  if (!refresh_frame) return;
  if (session_.mode() == OperationMode::INSPECT) {
    std::vector<std::uint8_t> frame;
    std::vector<PhysicalBitMask> highlights;
    if (session_.inspect_result().has_value()) {
      frame = session_.inspect_result()->input_frame;
      highlights = FieldHighlights(message, field, frame.size());
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
    field_model_->Reset(nullptr, {});
    hex_view_->ClearFrame();
    rebuilding_selectors_ = true;
    inspect_input_->clear();
    rebuilding_selectors_ = false;
    RebuildMessageSelector();
  }
  RefreshState();
}

void DocumentTab::SelectMode(int combo_index) {
  if (rebuilding_selectors_ || combo_index < 0) return;
  const auto mode = static_cast<OperationMode>(mode_combo_->itemData(combo_index).toInt());
  if (session_.SetMode(mode)) RefreshModePresentation();
}

void DocumentTab::SelectMessage(int combo_index) {
  if (rebuilding_selectors_ || combo_index < 0) {
    return;
  }
  if (session_.SelectMessage(
          static_cast<std::size_t>(message_combo_->itemData(combo_index).toULongLong()))) {
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
        [this](std::size_t field_index, std::string text, std::string validation_error) {
          session_.SetInvalidDraft(field_index, std::move(text), std::move(validation_error));
          RefreshPreview();
          RefreshState();
        });
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
  timing_label_->setText(TimingText(timing_));
}

void DocumentTab::InspectCurrent() {
  field_table_->clearFocus();
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  session_.SetInspectDraft(Utf8(inspect_input_->toPlainText()));
  TimingObserver observer(timing_);
  session_.Inspect(&observer);
  if (session_.inspect_failure().has_value() &&
      session_.inspect_failure()->input_offset.has_value()) {
    const QString text = inspect_input_->toPlainText();
    const QByteArray utf8 = text.toUtf8();
    const auto bounded_offset = (std::min)(*session_.inspect_failure()->input_offset,
                                           static_cast<std::size_t>(utf8.size()));
    const int utf16_offset =
        QString::fromUtf8(utf8.constData(), static_cast<int>(bounded_offset)).size();
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
