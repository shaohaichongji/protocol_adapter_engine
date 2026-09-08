#include <filesystem>
#include <iostream>
#include <string>

#include "v07_run_evidence.h"

int main() {
  pae::protocol_lab::v07::StoredRunBundle output;
  output.record.run_id = "must_be_cleared";
  std::string error;
  const bool loaded = pae::protocol_lab::v07::LoadRunBundle(
      std::filesystem::current_path() / "missing_run_bundle", output, error);
  if (loaded || !output.record.run_id.empty() || error.empty()) {
    std::cerr << "Reader isolation preflight did not fail closed\n";
    return 1;
  }
  return 0;
}
