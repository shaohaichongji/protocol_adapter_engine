#pragma once

#include <QAbstractTableModel>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include "document_session.h"

namespace pae::protocol_lab_ui {

enum class FieldPresentationAction {
  ENCODE,
  INSPECT,
};

class FieldTableModel final : public QAbstractTableModel {
 public:
  enum Column {
    ID = 0,
    DISPLAY_NAME,
    TYPE,
    SOURCE,
    VALUE,
    RAW_RESULT,
    LOGICAL_RESULT,
    PHYSICAL_LOCATION,
    COLUMN_COUNT,
  };

  enum Role {
    ValueTypeRole = Qt::UserRole + 1,
    EncodeSourceRole,
    ByteWidthRole,
    EnumIdsRole,
    EnumNamesRole,
    FieldIndexRole,
    FieldDescriptionRole,
    FieldSourceRefRole,
    ReadOnlyAnnotationRole,
    HasConversionRole,
    EditorCapacityRejectedRole,
    ByteRepresentationRole,
  };

  using DraftChanged =
      std::function<bool(std::size_t field_index, TypedDraft value, QString& error)>;
  using DraftInvalidated = std::function<void(std::size_t field_index, std::u16string text,
                                              std::string validation_error)>;

  explicit FieldTableModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

  void Reset(const MessageDescriptor* message, DraftChanged draft_changed,
             DraftInvalidated draft_invalidated = {}, bool editable = true,
             ByteRepresentation representation = ByteRepresentation::HEX,
             FieldPresentationAction action = FieldPresentationAction::ENCODE);
  void ApplyDrafts(const std::unordered_map<std::size_t, TypedDraft>& drafts);
  void ApplyInvalidDrafts(const std::unordered_map<std::size_t, InvalidDraftState>& invalid_drafts);
  void ClearResults();
  void ApplyResults(const std::vector<UiFieldResult>& results);
  void SetActualFrameSize(std::optional<std::size_t> frame_size);
  void SetFailedField(std::optional<std::size_t> field_index);
  const FieldDescriptor* FieldAt(int row) const noexcept;
  QString ValidationError(int row) const;

 private:
  struct RowState {
    QString draft_text;
    std::optional<std::size_t> enum_entry_index;
    bool bool_value = false;
    bool has_bool_value = false;
    std::string raw_result;
    std::string logical_result;
    QString validation_error;
    std::optional<ByteRange> actual_range;
  };

  bool ParseDraft(int row, const QVariant& value, int role, TypedDraft& output, QString& canonical,
                  QString& error) const;
  void EmitValueChanged(int row);

  const MessageDescriptor* message_ = nullptr;
  std::vector<RowState> rows_;
  DraftChanged draft_changed_;
  DraftInvalidated draft_invalidated_;
  std::optional<std::size_t> failed_field_index_;
  bool editable_ = true;
  std::optional<std::size_t> actual_frame_size_;
  ByteRepresentation representation_ = ByteRepresentation::HEX;
  FieldPresentationAction action_ = FieldPresentationAction::ENCODE;
};

}  // namespace pae::protocol_lab_ui
