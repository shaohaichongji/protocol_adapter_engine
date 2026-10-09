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

// Qt 字段展示与草稿编辑模型，不执行协议；行内编辑状态自有，描述及结果由上层保活。
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
    ActionReferencedRole,
    PresentationActionRole,
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

  // 借用 message 并清旧结果/失败/实际长度；上层释放或替换 owner 前必须解除相应借用。
  void Reset(const MessageDescriptor* message, DraftChanged draft_changed,
             DraftInvalidated draft_invalidated = {}, bool editable = true,
             ByteRepresentation representation = ByteRepresentation::HEX,
             FieldPresentationAction action = FieldPresentationAction::ENCODE);
  bool PrepareCapacity(std::size_t row_capacity) noexcept;
  std::size_t AccountedRowCapacityBytes() const noexcept;
  // 合法类型化草稿与非法原文本分别恢复；都不是本次 Codec 的 raw/logical 结果。
  void ApplyDrafts(const std::unordered_map<std::size_t, TypedDraft>& drafts);
  void ApplyInvalidDrafts(const std::unordered_map<std::size_t, InvalidDraftState>& invalid_drafts);
  void ClearResults();
  // 保存容器指针而非复制快照；调用方须在借用期间保持该容器及其 owner 有效。
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
    QString validation_error;
  };

  bool ParseDraft(int row, const QVariant& value, int role, TypedDraft& output, QString& canonical,
                  QString& error) const;
  void EmitValueChanged(int row);

  // FieldAt 返回同一描述内的借用字段，后续 Reset 或 owner 变更可能使旧引用失效。
  const MessageDescriptor* message_ = nullptr;
  const std::vector<UiFieldResult>* results_ = nullptr;
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
