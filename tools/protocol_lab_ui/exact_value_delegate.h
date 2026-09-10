#pragma once

#include <QStyledItemDelegate>
#include <functional>

namespace pae::protocol_lab_ui {

class ExactValueDelegate final : public QStyledItemDelegate {
 public:
  explicit ExactValueDelegate(QObject* parent = nullptr);

  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                        const QModelIndex& index) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model,
                    const QModelIndex& index) const override;
  void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const override;

  void SetEditorChangedCallback(std::function<void(std::size_t)> callback);

 private:
  std::function<void(std::size_t)> editor_changed_;
};

}  // namespace pae::protocol_lab_ui
