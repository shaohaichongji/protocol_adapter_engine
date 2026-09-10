#include "exact_value_delegate.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <limits>
#include <utility>

#include "field_table_model.h"

namespace pae::protocol_lab_ui {
namespace {

class DecimalEditor final : public QWidget {
 public:
  explicit DecimalEditor(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    coefficient = new QLineEdit(this);
    coefficient->setPlaceholderText(QStringLiteral("coefficient"));
    scale = new QSpinBox(this);
    scale->setRange(0, 18);
    scale->setPrefix(QStringLiteral("scale "));
    layout->addWidget(coefficient, 1);
    layout->addWidget(scale);
  }

  QLineEdit* coefficient = nullptr;
  QSpinBox* scale = nullptr;
};

}  // namespace

ExactValueDelegate::ExactValueDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QWidget* ExactValueDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&,
                                          const QModelIndex& index) const {
  if (index.column() != FieldTableModel::VALUE) {
    return nullptr;
  }
  if (index.data(FieldTableModel::HasConversionRole).toBool()) {
    auto* editor = new DecimalEditor(parent);
    const auto field_index = index.data(FieldTableModel::FieldIndexRole).toULongLong();
    QObject::connect(editor->coefficient, &QLineEdit::textChanged, editor,
                     [callback = editor_changed_, field_index](const QString&) {
                       if (callback) callback(static_cast<std::size_t>(field_index));
                     });
    QObject::connect(editor->scale, qOverload<int>(&QSpinBox::valueChanged), editor,
                     [callback = editor_changed_, field_index](int) {
                       if (callback) callback(static_cast<std::size_t>(field_index));
                     });
    return editor;
  }
  const auto value_type =
      static_cast<protocol_plan::ValueType>(index.data(FieldTableModel::ValueTypeRole).toInt());
  if (value_type == protocol_plan::ValueType::BOOL) {
    return nullptr;
  }
  if (value_type == protocol_plan::ValueType::ENUM) {
    auto* combo = new QComboBox(parent);
    const auto field_index = index.data(FieldTableModel::FieldIndexRole).toULongLong();
    const auto names = index.data(FieldTableModel::EnumNamesRole).toStringList();
    const auto ids = index.data(FieldTableModel::EnumIdsRole).toStringList();
    for (int item = 0; item < names.size(); ++item) {
      combo->addItem(names[item], item < ids.size() ? ids[item] : QString{});
    }
    QObject::connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), combo,
                     [callback = editor_changed_, field_index](int) {
                       if (callback) callback(static_cast<std::size_t>(field_index));
                     });
    return combo;
  }
  auto* line_edit = new QLineEdit(parent);
  const auto field_index = index.data(FieldTableModel::FieldIndexRole).toULongLong();
  if (value_type == protocol_plan::ValueType::BYTES) {
    const auto width = index.data(FieldTableModel::ByteWidthRole).toULongLong();
    if (width <= static_cast<qulonglong>(std::numeric_limits<int>::max() / 2)) {
      line_edit->setMaxLength(static_cast<int>(width * 2U));
    }
    line_edit->setPlaceholderText(QStringLiteral("uppercase Hex"));
  } else {
    line_edit->setPlaceholderText(value_type == protocol_plan::ValueType::UINT64
                                      ? QStringLiteral("canonical unsigned decimal")
                                      : QStringLiteral("canonical signed decimal"));
  }
  QObject::connect(line_edit, &QLineEdit::textChanged, line_edit,
                   [callback = editor_changed_, field_index](const QString&) {
                     if (callback) callback(static_cast<std::size_t>(field_index));
                   });
  return line_edit;
}

void ExactValueDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
  if (auto* decimal = dynamic_cast<DecimalEditor*>(editor)) {
    const auto parts = index.data(Qt::EditRole).toString().split(QLatin1Char('@'));
    const QSignalBlocker coefficient_blocker(decimal->coefficient);
    const QSignalBlocker scale_blocker(decimal->scale);
    decimal->coefficient->setText(parts.value(0));
    decimal->scale->setValue(parts.size() == 2 ? parts[1].toInt() : 0);
    return;
  }
  if (auto* combo = qobject_cast<QComboBox*>(editor)) {
    const QSignalBlocker blocker(combo);
    combo->setCurrentIndex(index.data(Qt::EditRole).toInt());
    return;
  }
  if (auto* line_edit = qobject_cast<QLineEdit*>(editor)) {
    const QSignalBlocker blocker(line_edit);
    line_edit->setText(index.data(Qt::EditRole).toString());
  }
}

void ExactValueDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                      const QModelIndex& index) const {
  if (auto* decimal = dynamic_cast<DecimalEditor*>(editor)) {
    model->setData(index, decimal->coefficient->text() + QLatin1Char('@') +
                              QString::number(decimal->scale->value()));
    return;
  }
  if (auto* combo = qobject_cast<QComboBox*>(editor)) {
    model->setData(index, combo->currentIndex());
    return;
  }
  if (auto* line_edit = qobject_cast<QLineEdit*>(editor)) {
    model->setData(index, line_edit->text());
  }
}

void ExactValueDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                              const QModelIndex&) const {
  editor->setGeometry(option.rect);
}

void ExactValueDelegate::SetEditorChangedCallback(std::function<void(std::size_t)> callback) {
  editor_changed_ = std::move(callback);
}

}  // namespace pae::protocol_lab_ui
