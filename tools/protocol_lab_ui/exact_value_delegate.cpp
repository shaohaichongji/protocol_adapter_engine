#include "exact_value_delegate.h"
#include "smoke_editor_target.h"
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
#include "ascii_smoke_diagnostic.h"
#endif

#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolTip>
#include <QValidator>
#include <limits>
#include <optional>
#include <utility>

#include "field_table_model.h"

namespace pae::protocol_lab_ui {
namespace {

QString UiText(const char* text) { return QCoreApplication::translate("PaeLabUi", text); }

class DecimalEditor final : public QWidget {
 public:
  explicit DecimalEditor(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    coefficient = new QLineEdit(this);
    coefficient->setPlaceholderText(UiText("系数"));
    scale = new QSpinBox(this);
    scale->setRange(0, 18);
    scale->setPrefix(UiText("小数位 "));
    layout->addWidget(coefficient, 1);
    layout->addWidget(scale);
  }

  QLineEdit* coefficient = nullptr;
  QSpinBox* scale = nullptr;
};

class EditCapacityValidator final : public QValidator {
 public:
  EditCapacityValidator(int capacity, QObject* parent) : QValidator(parent), capacity_(capacity) {}

  State validate(QString& input, int&) const override {
    return input.size() <= capacity_ ? Acceptable : Invalid;
  }

 private:
  int capacity_ = 0;
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
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
    ascii_smoke_diagnostic::RegisterEditor(editor);
#endif
    return editor;
  }
  const auto value_type =
      static_cast<FieldValueType>(index.data(FieldTableModel::ValueTypeRole).toInt());
  if (value_type == FieldValueType::BOOL) {
    return nullptr;
  }
  if (value_type == FieldValueType::ENUM) {
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
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
    ascii_smoke_diagnostic::RegisterEditor(combo);
#endif
    return combo;
  }
  auto* line_edit = new QLineEdit(parent);
  smoke_editor_target::Stamp(*line_edit, index);
  const auto field_index = index.data(FieldTableModel::FieldIndexRole).toULongLong();
  if (value_type == FieldValueType::BYTES) {
    const auto width = index.data(FieldTableModel::ByteWidthRole).toULongLong();
    const auto representation = static_cast<ByteRepresentation>(
        index.data(FieldTableModel::ByteRepresentationRole).toInt());
    const auto capacity_value = ByteEditorCapacity(static_cast<std::size_t>(width), representation);
    if (!capacity_value.has_value() ||
        *capacity_value > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
      delete line_edit;
      return nullptr;
    }
    const int capacity = static_cast<int>(*capacity_value);
    line_edit->setMaxLength((std::numeric_limits<int>::max)());
    line_edit->setValidator(new EditCapacityValidator(capacity, line_edit));
    line_edit->setProperty("paeEditCapacity", capacity);
    line_edit->setProperty("paeByteRepresentation", static_cast<int>(representation));
    line_edit->setProperty("paeCapacityRejected", false);
    line_edit->setPlaceholderText(representation == ByteRepresentation::ASCII_ESCAPED
                                      ? QStringLiteral("ASCII (escaped)")
                                      : UiText("大写 Hex"));
    const QPersistentModelIndex persistent_index{index};
    QObject::connect(
        line_edit, &QLineEdit::inputRejected, line_edit,
        [line_edit, persistent_index, capacity, representation] {
          const QString feedback = UiText("输入超过编辑器容量上限（%1 个 %2 字符）；本次编辑已整体拒绝")
                                       .arg(capacity)
                                       .arg(representation == ByteRepresentation::ASCII_ESCAPED
                                                ? QStringLiteral("ASCII escaped")
                                                : QStringLiteral("Hex"));
          line_edit->setProperty("paeCapacityRejected", true);
          line_edit->setToolTip(feedback);
          line_edit->setStyleSheet(QStringLiteral("QLineEdit { background-color: #FFE4E4; }"));
          QToolTip::showText(line_edit->mapToGlobal(line_edit->rect().bottomLeft()), feedback,
                             line_edit);
          if (persistent_index.isValid()) {
            auto* model = const_cast<QAbstractItemModel*>(persistent_index.model());
            model->setData(persistent_index, feedback, FieldTableModel::EditorCapacityRejectedRole);
          }
        });
    QObject::connect(line_edit, &QLineEdit::textEdited, line_edit, [line_edit](const QString&) {
      line_edit->setProperty("paeCapacityRejected", false);
      line_edit->setToolTip(QString{});
      line_edit->setStyleSheet(QString{});
    });
  } else {
    line_edit->setPlaceholderText(value_type == FieldValueType::UINT64
                                      ? UiText("规范无符号十进制")
                                      : UiText("规范有符号十进制"));
  }
  QObject::connect(line_edit, &QLineEdit::textChanged, line_edit,
                   [callback = editor_changed_, field_index](const QString&) {
                     if (callback) callback(static_cast<std::size_t>(field_index));
                   });
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC)
  ascii_smoke_diagnostic::RegisterEditor(line_edit);
#endif
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
    if (line_edit->property("paeCapacityRejected").toBool()) return;
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
