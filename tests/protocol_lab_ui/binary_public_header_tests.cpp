#include "../../tools/protocol_lab_ui/binary_host_adapter_public.h"
#include "../../tools/protocol_lab_ui/public_binary_description.h"

int main() {
  pae::protocol_lab_ui::BinaryUiDecodeView view;
  return view.ok ? 1 : 0;
}
