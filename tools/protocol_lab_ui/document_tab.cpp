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
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QTextBrowser>
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

std::vector<PhysicalBitMask> FieldHighlights(const FieldDescriptor* field) {
  if (field == nullptr) {
    return {};
  }
  if (!field->physical_bits.empty()) {
    return field->physical_bits;
  }
  std::vector<PhysicalBitMask> output;
  if (field->byte_range.has_value()) {
    output.reserve(field->byte_range->length);
    for (std::size_t index = 0; index < field->byte_range->length; ++index) {
      output.push_back(PhysicalBitMask{field->byte_range->offset + index, 0xFFU});
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
  EncodeCurrent();
  if (session_.state() != DocumentState::PREVIEW_VALID) {
    error = FromUtf8(session_.diagnostic_id()) + QStringLiteral(": ") +
            FromUtf8(session_.diagnostic_detail());
    return false;
  }
  return true;
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
  for (int row = 0; row < field_model_->rowCount(); ++row) {
    const auto* field = field_model_->FieldAt(row);
    if (field == nullptr || field->encode_source != protocol_plan::EncodeSource::INPUT ||
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
        field_model_->ValidationError(row).isEmpty() || session_.preview().has_value() ||
        session_.Encode()) {
      error = QStringLiteral("invalid UINT64 text was not retained as an invalid draft");
      return false;
    }
    return true;
  }
  return true;
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
  message_combo_ = new QComboBox(this);
  encode_button_ = new QPushButton(QStringLiteral("Encode"), this);
  selection_row->addWidget(new QLabel(QStringLiteral("Pipeline"), this));
  selection_row->addWidget(pipeline_combo_, 1);
  selection_row->addWidget(new QLabel(QStringLiteral("Message"), this));
  selection_row->addWidget(message_combo_, 1);
  selection_row->addWidget(encode_button_);
  root->addLayout(selection_row);

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
      [this](std::size_t field_index) { InvalidateEditedDraft(field_index); });
  RefreshPreview();
  RefreshState();
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
  message_combo_->setEnabled(description != nullptr && !loading);
  encode_button_->setEnabled(description != nullptr && session_.selection().has_value() &&
                             !loading);
  if (session_.diagnostic_id().empty() && session_.diagnostic_detail().empty()) {
    diagnostic_label_->clear();
  } else {
    diagnostic_label_->setText(QStringLiteral("%1: %2").arg(
        FromUtf8(session_.diagnostic_id()), FromUtf8(session_.diagnostic_detail())));
  }
}

void DocumentTab::RefreshPreview() {
  field_model_->SetFailedField(std::nullopt);
  if (!session_.preview().has_value()) {
    field_model_->ClearResults();
    hex_view_->ClearFrame();
    return;
  }
  field_model_->ApplyResults(session_.preview()->fields);
  const auto current = field_table_->currentIndex().row();
  const auto* field = field_model_->FieldAt(current);
  hex_view_->SetFrame(session_.preview()->encoded_frame, FieldHighlights(field));
}

void DocumentTab::RefreshFieldDetails(int row) {
  const auto* field = field_model_->FieldAt(row);
  const auto* message = CurrentMessage();
  if (field == nullptr) {
    details_view_->clear();
    RefreshPreview();
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
      details.push_back(QStringLiteral("<b>Integrity storage</b>: %1 + %2")
                            .arg(static_cast<qulonglong>(message->integrity_storage->offset))
                            .arg(static_cast<qulonglong>(message->integrity_storage->length)));
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
  if (field->byte_range.has_value()) {
    details.push_back(QStringLiteral("<b>Byte range</b>: %1 + %2")
                          .arg(static_cast<qulonglong>(field->byte_range->offset))
                          .arg(static_cast<qulonglong>(field->byte_range->length)));
  }
  if (!field->physical_bits.empty()) {
    details.push_back(QStringLiteral("<b>Physical bit cells</b>: %1")
                          .arg(static_cast<qulonglong>(field->physical_bits.size())));
  }
  details_view_->setHtml(details.join(QStringLiteral("<br/>")));
  RefreshPreview();
}

void DocumentTab::SelectPipeline(int combo_index) {
  if (rebuilding_selectors_ || combo_index < 0) {
    return;
  }
  if (session_.SelectPipeline(
          static_cast<std::size_t>(pipeline_combo_->itemData(combo_index).toULongLong()))) {
    field_model_->Reset(nullptr, {});
    hex_view_->ClearFrame();
    RebuildMessageSelector();
  }
  RefreshState();
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
        [this](std::size_t field_index) { InvalidateEditedDraft(field_index); });
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

const MessageDescriptor* DocumentTab::CurrentMessage() const noexcept {
  const auto* description = session_.description();
  if (description == nullptr || !session_.selection().has_value() ||
      session_.selection()->message_index >= description->messages.size()) {
    return nullptr;
  }
  return &description->messages[session_.selection()->message_index];
}

}  // namespace pae::protocol_lab_ui
