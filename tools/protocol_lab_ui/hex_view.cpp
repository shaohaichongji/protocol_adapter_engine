#include "hex_view.h"

#include <QAbstractTableModel>
#include <QColor>
#include <QHeaderView>
#include <QString>
#include <algorithm>

namespace pae::protocol_lab_ui {
namespace {

constexpr int kBytesPerRow = 16;

}  // namespace

class HexView::Model final : public QAbstractTableModel {
 public:
  explicit Model(QObject* parent) : QAbstractTableModel(parent) {}

  int rowCount(const QModelIndex& parent = QModelIndex()) const override {
    if (parent.isValid()) {
      return 0;
    }
    return static_cast<int>((frame_.size() + kBytesPerRow - 1U) / kBytesPerRow);
  }

  int columnCount(const QModelIndex& parent = QModelIndex()) const override {
    return parent.isValid() ? 0 : kBytesPerRow + 1;
  }

  QVariant data(const QModelIndex& index, int role) const override {
    if (!index.isValid()) {
      return {};
    }
    if (index.column() == 0) {
      if (role == Qt::DisplayRole) {
        return QStringLiteral("%1")
            .arg(static_cast<qulonglong>(index.row() * kBytesPerRow), 8, 16, QLatin1Char('0'))
            .toUpper();
      }
      return {};
    }

    const auto byte_index =
        static_cast<std::size_t>(index.row() * kBytesPerRow + index.column() - 1);
    if (byte_index >= frame_.size()) {
      return {};
    }
    if (role == Qt::DisplayRole) {
      return QStringLiteral("%1")
          .arg(static_cast<unsigned int>(frame_[byte_index]), 2, 16, QLatin1Char('0'))
          .toUpper();
    }
    if (role == Qt::TextAlignmentRole) {
      return Qt::AlignCenter;
    }
    const auto mask = highlight_masks_[byte_index];
    if (role == Qt::BackgroundRole && mask != 0U) {
      return QColor(255, 224, 128);
    }
    if (role == Qt::ToolTipRole && mask != 0U) {
      return QStringLiteral("byte %1, bit mask 0x%2")
          .arg(static_cast<qulonglong>(byte_index))
          .arg(static_cast<unsigned int>(mask), 2, 16, QLatin1Char('0'))
          .toUpper();
    }
    return {};
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
    if (role != Qt::DisplayRole) {
      return {};
    }
    if (orientation == Qt::Vertical) {
      return {};
    }
    if (section == 0) {
      return QStringLiteral("Offset");
    }
    return QStringLiteral("%1").arg(section - 1, 2, 16, QLatin1Char('0')).toUpper();
  }

  void SetFrame(std::vector<std::uint8_t> frame, const std::vector<PhysicalBitMask>& highlights) {
    beginResetModel();
    frame_ = std::move(frame);
    highlight_masks_.assign(frame_.size(), 0U);
    for (const auto& highlight : highlights) {
      if (highlight.frame_byte_index < highlight_masks_.size()) {
        highlight_masks_[highlight.frame_byte_index] = static_cast<std::uint8_t>(
            highlight_masks_[highlight.frame_byte_index] | highlight.uint8_mask);
      }
    }
    endResetModel();
  }

  void ClearFrame() { SetFrame({}, {}); }

  std::size_t FrameSize() const noexcept { return frame_.size(); }

  std::size_t HighlightedCellCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(highlight_masks_.begin(), highlight_masks_.end(),
                                                  [](std::uint8_t value) { return value != 0U; }));
  }

  std::uint8_t HighlightMaskAt(std::size_t index) const noexcept {
    return index < highlight_masks_.size() ? highlight_masks_[index] : 0U;
  }

 private:
  std::vector<std::uint8_t> frame_;
  std::vector<std::uint8_t> highlight_masks_;
};

HexView::HexView(QWidget* parent) : QTableView(parent), model_(new Model(this)) {
  setModel(model_);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setSelectionMode(QAbstractItemView::NoSelection);
  setAlternatingRowColors(true);
  setWordWrap(false);
  verticalHeader()->hide();
  horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  for (int column = 1; column <= kBytesPerRow; ++column) {
    horizontalHeader()->setSectionResizeMode(column, QHeaderView::Fixed);
    setColumnWidth(column, 36);
  }
}

HexView::~HexView() = default;

void HexView::SetFrame(std::vector<std::uint8_t> frame,
                       const std::vector<PhysicalBitMask>& highlights) {
  model_->SetFrame(std::move(frame), highlights);
}

void HexView::ClearFrame() { model_->ClearFrame(); }

std::size_t HexView::FrameSize() const noexcept { return model_->FrameSize(); }

std::size_t HexView::HighlightedCellCount() const noexcept {
  return model_->HighlightedCellCount();
}

std::uint8_t HexView::HighlightMaskAt(std::size_t frame_byte_index) const noexcept {
  return model_->HighlightMaskAt(frame_byte_index);
}

}  // namespace pae::protocol_lab_ui
