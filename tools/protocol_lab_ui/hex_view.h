#pragma once

#include <QTableView>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "description_mapping.h"

namespace pae::protocol_lab_ui {

class HexView final : public QTableView {
 public:
  explicit HexView(QWidget* parent = nullptr);
  ~HexView() override;

  void SetFrame(std::vector<std::uint8_t> frame, const std::vector<PhysicalBitMask>& highlights);
  void ClearFrame();

  std::size_t FrameSize() const noexcept;
  std::size_t HighlightedCellCount() const noexcept;
  std::uint8_t HighlightMaskAt(std::size_t frame_byte_index) const noexcept;

 private:
  class Model;
  Model* model_ = nullptr;
};

}  // namespace pae::protocol_lab_ui
