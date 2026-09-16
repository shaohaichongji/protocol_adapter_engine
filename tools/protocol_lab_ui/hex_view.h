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

  bool PrepareCapacity(std::size_t frame_capacity) noexcept;
  void FailNextCapacityPreparationForTest() noexcept;
  std::size_t AccountedCapacityBytes() const noexcept;
  void SetFrame(const std::vector<std::uint8_t>& frame,
                const std::vector<PhysicalBitMask>& highlights);
  void ClearFrame();

  std::size_t FrameSize() const noexcept;
  std::size_t HighlightedCellCount() const noexcept;
  std::uint8_t HighlightMaskAt(std::size_t frame_byte_index) const noexcept;

 private:
  class Model;
  Model* model_ = nullptr;
  bool fail_next_capacity_preparation_ = false;
};

}  // namespace pae::protocol_lab_ui
