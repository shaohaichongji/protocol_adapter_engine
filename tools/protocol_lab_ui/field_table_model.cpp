#include "field_table_model.h"

#include <QBrush>
#include <QColor>
#include <QStringList>
#include <limits>
#include <type_traits>
#include <utility>

#include "../protocol_lab/exact_value_text_internal.h"

namespace pae::protocol_lab_ui {
namespace {

QString ValueTypeName(protocol_plan::ValueType value_type) {
  switch (value_type) {
    case protocol_plan::ValueType::UINT64:
      return QStringLiteral("UINT64");
    case protocol_plan::ValueType::INT64:
      return QStringLiteral("INT64");
    case protocol_plan::ValueType::BYTES:
      return QStringLiteral("BYTES");
    case protocol_plan::ValueType::ENUM:
      return QStringLiteral("ENUM");
    case protocol_plan::ValueType::BOOL:
      return QStringLiteral("BOOL");
  }
  return QStringLiteral("UNKNOWN");
}

QString SourceName(protocol_plan::EncodeSource source) {
  switch (source) {
    case protocol_plan::EncodeSource::INPUT:
      return QStringLiteral("input");
    case protocol_plan::EncodeSource::CONSTANT:
      return QStringLiteral("constant");
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    case protocol_plan::EncodeSource::COMPUTED:
      return QStringLiteral("computed");
#endif
  }
  return QStringLiteral("unknown");
}

std::string Utf8(const QString& value) {
  const auto bytes = value.toUtf8();
  return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

}  // namespace

FieldTableModel::FieldTableModel(QObject* parent) : QAbstractTableModel(parent) {}

int FieldTableModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int FieldTableModel::columnCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : COLUMN_COUNT;
}

QVariant FieldTableModel::data(const QModelIndex& index, int role) const {
  const auto* field = FieldAt(index.row());
  if (!index.isValid() || field == nullptr) {
    return {};
  }
  const auto& row = rows_[static_cast<std::size_t>(index.row())];
  if (role == ValueTypeRole) {
    return static_cast<int>(field->value_type);
  }
  if (role == EncodeSourceRole) {
    return static_cast<int>(field->encode_source);
  }
  if (role == ByteWidthRole) {
    return static_cast<qulonglong>(field->byte_width);
  }
  if (role == FieldIndexRole) {
    return static_cast<qulonglong>(field->field_index);
  }
  if (role == FieldDescriptionRole) {
    return QString::fromUtf8(field->description.data(),
                             static_cast<int>(field->description.size()));
  }
  if (role == FieldSourceRefRole) {
    return QString::fromUtf8(field->source_ref.data(), static_cast<int>(field->source_ref.size()));
  }
  if (role == ReadOnlyAnnotationRole) {
    return QString::fromUtf8(field->read_only_annotation.data(),
                             static_cast<int>(field->read_only_annotation.size()));
  }
  if (role == HasConversionRole) {
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
    return field->conversion.has_value();
#else
    return false;
#endif
  }
  if (role == EnumIdsRole || role == EnumNamesRole) {
    QStringList values;
    for (const auto& entry : field->enum_entries) {
      const auto& source = role == EnumIdsRole
                               ? entry.id
                               : (entry.display_name.empty() ? entry.id : entry.display_name);
      values.push_back(QString::fromUtf8(source.data(), static_cast<int>(source.size())));
    }
    return values;
  }
  if (role == Qt::BackgroundRole && failed_field_index_.has_value() &&
      *failed_field_index_ == field->field_index) {
    return QBrush(QColor(255, 205, 205));
  }
  if (role == Qt::ToolTipRole) {
    if (!row.validation_error.isEmpty()) {
      return row.validation_error;
    }
    QStringList lines;
    if (!field->description.empty()) {
      lines.push_back(QString::fromUtf8(field->description.data(),
                                        static_cast<int>(field->description.size())));
    }
    if (!field->source_ref.empty()) {
      lines.push_back(QStringLiteral("source: %1")
                          .arg(QString::fromUtf8(field->source_ref.data(),
                                                 static_cast<int>(field->source_ref.size()))));
    }
    if (!field->read_only_annotation.empty()) {
      lines.push_back(QString::fromUtf8(field->read_only_annotation.data(),
                                        static_cast<int>(field->read_only_annotation.size())));
    }
    return lines.join(QLatin1Char('\n'));
  }
  if (role == Qt::CheckStateRole && index.column() == VALUE &&
      field->value_type == protocol_plan::ValueType::BOOL &&
      field->encode_source == protocol_plan::EncodeSource::INPUT) {
    return row.has_bool_value ? (row.bool_value ? Qt::Checked : Qt::Unchecked)
                              : Qt::PartiallyChecked;
  }
  if (role != Qt::DisplayRole && role != Qt::EditRole) {
    return {};
  }
  switch (index.column()) {
    case ID:
      return QString::fromUtf8(field->id.data(), static_cast<int>(field->id.size()));
    case DISPLAY_NAME: {
      const auto& name = field->display_name.empty() ? field->id : field->display_name;
      return QString::fromUtf8(name.data(), static_cast<int>(name.size()));
    }
    case TYPE:
      return ValueTypeName(field->value_type);
    case SOURCE:
      return SourceName(field->encode_source);
    case VALUE:
      if (field->encode_source != protocol_plan::EncodeSource::INPUT) {
        return QString::fromUtf8(field->read_only_annotation.data(),
                                 static_cast<int>(field->read_only_annotation.size()));
      }
      if (field->value_type == protocol_plan::ValueType::ENUM && row.enum_entry_index.has_value() &&
          *row.enum_entry_index < field->enum_entries.size()) {
        const auto& entry = field->enum_entries[*row.enum_entry_index];
        const auto& name = entry.display_name.empty() ? entry.id : entry.display_name;
        return role == Qt::EditRole
                   ? QVariant::fromValue(static_cast<int>(*row.enum_entry_index))
                   : QVariant(QString::fromUtf8(name.data(), static_cast<int>(name.size())));
      }
      return QString::fromUtf8(row.draft_text.data(), static_cast<int>(row.draft_text.size()));
    case RAW_RESULT:
      return QString::fromUtf8(row.raw_result.data(), static_cast<int>(row.raw_result.size()));
    case LOGICAL_RESULT:
      return QString::fromUtf8(row.logical_result.data(),
                               static_cast<int>(row.logical_result.size()));
    case PHYSICAL_LOCATION: {
      const auto location = FormatPhysicalLocation(*field);
      return QString::fromUtf8(location.data(), static_cast<int>(location.size()));
    }
    default:
      return {};
  }
}

QVariant FieldTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
    return {};
  }
  static const char* const labels[] = {"Id",    "Name", "Type",    "Source",
                                       "Value", "Raw",  "Logical", "Physical byte / bit / mask"};
  return section >= 0 && section < COLUMN_COUNT ? QString::fromLatin1(labels[section]) : QVariant{};
}

Qt::ItemFlags FieldTableModel::flags(const QModelIndex& index) const {
  auto result = QAbstractTableModel::flags(index);
  const auto* field = FieldAt(index.row());
  if (!editable_ || field == nullptr || index.column() != VALUE ||
      field->encode_source != protocol_plan::EncodeSource::INPUT) {
    return result;
  }
  result |= Qt::ItemIsEditable;
  if (field->value_type == protocol_plan::ValueType::BOOL) {
    result |= Qt::ItemIsUserCheckable;
  }
  return result;
}

bool FieldTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!editable_ || !index.isValid() || index.column() != VALUE) {
    return false;
  }
  TypedDraft draft;
  QString canonical;
  QString error;
  if (!ParseDraft(index.row(), value, role, draft, canonical, error)) {
    auto& row = rows_[static_cast<std::size_t>(index.row())];
    const auto* field = FieldAt(index.row());
    if (field != nullptr && field->value_type != protocol_plan::ValueType::BOOL &&
        field->value_type != protocol_plan::ValueType::ENUM) {
      row.draft_text = Utf8(value.toString());
    }
    row.validation_error = error;
    if (draft_invalidated_ && field != nullptr) {
      draft_invalidated_(field->field_index, row.draft_text, Utf8(error));
    }
    EmitValueChanged(index.row());
    return false;
  }
  if (draft_changed_ &&
      !draft_changed_(FieldAt(index.row())->field_index, std::move(draft), error)) {
    auto& row = rows_[static_cast<std::size_t>(index.row())];
    const auto* field = FieldAt(index.row());
    if (field != nullptr && field->value_type != protocol_plan::ValueType::BOOL &&
        field->value_type != protocol_plan::ValueType::ENUM) {
      row.draft_text = Utf8(value.toString());
    }
    row.validation_error = error;
    if (draft_invalidated_ && field != nullptr) {
      draft_invalidated_(field->field_index, row.draft_text, Utf8(error));
    }
    EmitValueChanged(index.row());
    return false;
  }
  auto& row = rows_[static_cast<std::size_t>(index.row())];
  row.validation_error.clear();
  const auto* field = FieldAt(index.row());
  if (field->value_type == protocol_plan::ValueType::BOOL) {
    row.bool_value = role == Qt::CheckStateRole ? value.toInt() == Qt::Checked : value.toBool();
    row.has_bool_value = true;
    row.draft_text = row.bool_value ? "true" : "false";
  } else if (field->value_type == protocol_plan::ValueType::ENUM) {
    row.enum_entry_index = static_cast<std::size_t>(value.toInt());
    row.draft_text = Utf8(canonical);
  } else {
    row.draft_text = Utf8(canonical);
  }
  EmitValueChanged(index.row());
  return true;
}

void FieldTableModel::Reset(const MessageDescriptor* message, DraftChanged draft_changed,
                            DraftInvalidated draft_invalidated, bool editable) {
  beginResetModel();
  message_ = message;
  rows_.assign(message_ == nullptr ? 0U : message_->fields.size(), RowState{});
  draft_changed_ = std::move(draft_changed);
  draft_invalidated_ = std::move(draft_invalidated);
  editable_ = editable;
  failed_field_index_.reset();
  endResetModel();
}

void FieldTableModel::ApplyDrafts(const std::unordered_map<std::size_t, TypedDraft>& drafts) {
  if (message_ == nullptr) return;
  for (std::size_t row_index = 0U; row_index < message_->fields.size(); ++row_index) {
    const auto found = drafts.find(message_->fields[row_index].field_index);
    if (found == drafts.end()) continue;
    auto& row = rows_[row_index];
    std::visit(
        [&row](const auto& item) {
          using T = std::decay_t<decltype(item)>;
          if constexpr (std::is_same_v<T, std::uint64_t> || std::is_same_v<T, std::int64_t>) {
            row.draft_text = std::to_string(item);
          } else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>>) {
            static const char digits[] = "0123456789ABCDEF";
            for (const auto value : item) {
              row.draft_text.push_back(digits[value >> 4U]);
              row.draft_text.push_back(digits[value & 0x0FU]);
            }
          } else if constexpr (std::is_same_v<T, EnumSelection>) {
            row.enum_entry_index = item.entry_index;
            row.draft_text = item.entry_id;
          } else if constexpr (std::is_same_v<T, bool>) {
            row.bool_value = item;
            row.has_bool_value = true;
            row.draft_text = item ? "true" : "false";
          } else if constexpr (std::is_same_v<T, protocol_lab::v06::Decimal64>) {
            row.draft_text = std::to_string(item.coefficient) + "@" + std::to_string(item.scale);
          }
        },
        found->second);
  }
  if (!rows_.empty()) emit dataChanged(index(0, VALUE), index(rowCount() - 1, VALUE));
}

void FieldTableModel::ApplyInvalidDrafts(
    const std::unordered_map<std::size_t, InvalidDraftState>& invalid_drafts) {
  if (message_ == nullptr) return;
  for (std::size_t row_index = 0U; row_index < message_->fields.size(); ++row_index) {
    const auto found = invalid_drafts.find(message_->fields[row_index].field_index);
    if (found == invalid_drafts.end()) continue;
    rows_[row_index].draft_text = found->second.text;
    rows_[row_index].validation_error =
        QString::fromUtf8(found->second.validation_error.data(),
                          static_cast<int>(found->second.validation_error.size()));
  }
  if (!rows_.empty()) {
    emit dataChanged(index(0, VALUE), index(rowCount() - 1, VALUE),
                     {Qt::DisplayRole, Qt::EditRole, Qt::ToolTipRole});
  }
}

void FieldTableModel::ClearResults() {
  if (rows_.empty()) {
    return;
  }
  for (auto& row : rows_) {
    row.raw_result.clear();
    row.logical_result.clear();
  }
  emit dataChanged(index(0, RAW_RESULT), index(rowCount() - 1, LOGICAL_RESULT));
}

void FieldTableModel::ApplyResults(const std::vector<protocol_lab::v06::FieldResult>& results) {
  ClearResults();
  if (message_ == nullptr) {
    return;
  }
  for (const auto& result : results) {
    for (std::size_t row_index = 0; row_index < message_->fields.size(); ++row_index) {
      if (message_->fields[row_index].id == result.id) {
        rows_[row_index].raw_result = result.raw_value;
        rows_[row_index].logical_result = result.logical_value;
        break;
      }
    }
  }
  if (!rows_.empty()) {
    emit dataChanged(index(0, RAW_RESULT), index(rowCount() - 1, LOGICAL_RESULT));
  }
}

void FieldTableModel::SetFailedField(std::optional<std::size_t> field_index) {
  failed_field_index_ = field_index;
  if (!rows_.empty()) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, COLUMN_COUNT - 1), {Qt::BackgroundRole});
  }
}

const FieldDescriptor* FieldTableModel::FieldAt(int row) const noexcept {
  return message_ != nullptr && row >= 0 && static_cast<std::size_t>(row) < message_->fields.size()
             ? &message_->fields[static_cast<std::size_t>(row)]
             : nullptr;
}

QString FieldTableModel::ValidationError(int row) const {
  return row >= 0 && static_cast<std::size_t>(row) < rows_.size()
             ? rows_[static_cast<std::size_t>(row)].validation_error
             : QString{};
}

bool FieldTableModel::ParseDraft(int row, const QVariant& value, int role, TypedDraft& output,
                                 QString& canonical, QString& error) const {
  const auto* field = FieldAt(row);
  if (field == nullptr || field->encode_source != protocol_plan::EncodeSource::INPUT) {
    error = QStringLiteral("Field is read-only");
    return false;
  }
  if (field->value_type == protocol_plan::ValueType::BOOL) {
    const bool checked = role == Qt::CheckStateRole ? value.toInt() == Qt::Checked : value.toBool();
    output = checked;
    canonical = checked ? QStringLiteral("true") : QStringLiteral("false");
    return true;
  }
  if (field->value_type == protocol_plan::ValueType::ENUM) {
    bool ok = false;
    const int entry_index = value.toInt(&ok);
    if (!ok || entry_index < 0 ||
        static_cast<std::size_t>(entry_index) >= field->enum_entries.size()) {
      error = QStringLiteral("Select a configured enum entry");
      return false;
    }
    const auto& entry = field->enum_entries[static_cast<std::size_t>(entry_index)];
    output = EnumSelection{entry.entry_index, entry.id};
    canonical = QString::fromUtf8(entry.id.data(), static_cast<int>(entry.id.size()));
    return true;
  }

  canonical = value.toString();
  const auto text = Utf8(canonical);
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (field->conversion.has_value()) {
    const auto separator = text.find('@');
    std::int64_t coefficient = 0;
    std::uint64_t scale = 0U;
    if (separator == std::string::npos ||
        !protocol_lab::v06::internal::ParseCanonicalInt64Text(
            std::string_view(text).substr(0U, separator), coefficient) ||
        !protocol_lab::v06::internal::ParseCanonicalUint64Text(
            std::string_view(text).substr(separator + 1U), scale) ||
        scale > 18U) {
      error = QStringLiteral("Expected Decimal64 coefficient@scale with scale 0..18");
      return false;
    }
    output = protocol_lab::v06::NormalizeDecimal64(
        protocol_lab::v06::Decimal64{coefficient, static_cast<std::int32_t>(scale)});
    return true;
  }
#endif
  if (field->value_type == protocol_plan::ValueType::UINT64) {
    std::uint64_t parsed = 0U;
    if (!protocol_lab::v06::internal::ParseCanonicalUint64Text(text, parsed)) {
      error = QStringLiteral("Expected canonical UINT64 decimal");
      return false;
    }
    output = parsed;
    return true;
  }
  if (field->value_type == protocol_plan::ValueType::INT64) {
    std::int64_t parsed = 0;
    if (!protocol_lab::v06::internal::ParseCanonicalInt64Text(text, parsed)) {
      error = QStringLiteral("Expected canonical INT64 decimal");
      return false;
    }
    output = parsed;
    return true;
  }
  if (field->value_type == protocol_plan::ValueType::BYTES) {
    if (field->byte_width > std::numeric_limits<std::size_t>::max() / 2U ||
        text.size() != field->byte_width * 2U) {
      error = QStringLiteral("Expected exactly %1 uppercase Hex characters")
                  .arg(static_cast<qulonglong>(field->byte_width * 2U));
      return false;
    }
    std::vector<std::uint8_t> bytes;
    if (!protocol_lab::v06::internal::ParseCanonicalUpperHexText(text, bytes)) {
      error = QStringLiteral("Expected uppercase Hex without separators");
      return false;
    }
    output = std::move(bytes);
    return true;
  }

  error = QStringLiteral("Unsupported input type");
  return false;
}

void FieldTableModel::EmitValueChanged(int row) {
  emit dataChanged(index(row, VALUE), index(row, VALUE),
                   {Qt::DisplayRole, Qt::EditRole, Qt::CheckStateRole, Qt::ToolTipRole});
}

}  // namespace pae::protocol_lab_ui
