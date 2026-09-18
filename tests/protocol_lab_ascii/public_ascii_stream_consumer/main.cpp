#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "public_ascii_host_adapter.h"

using namespace pae::protocol_lab_ascii::public_offline;

namespace {

std::vector<std::uint8_t> Bytes(std::string_view text) {
  return {reinterpret_cast<const std::uint8_t*>(text.data()),
          reinterpret_cast<const std::uint8_t*>(text.data()) + text.size()};
}

std::string_view Text(const std::vector<std::uint8_t>& bytes) {
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::ifstream input(argv[1], std::ios::binary);
  const std::string json(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
  auto compiled = pae::CompileProtocolJson(json);
  if (!compiled.Succeeded()) return 3;

  HostBinding binding{"stream", pae::HostAction::DECODE, 0U};
  auto prepared = HostAdapter::Create(std::move(compiled).TakeCompiled(), {binding});
  if (prepared.status != LocalStatus::OK || prepared.host_status != pae::HostStatus::OK ||
      !prepared.adapter) {
    return 4;
  }
  auto& adapter = *prepared.adapter;
  const auto description = adapter.ObserveStream(0U, 0U);
  if (!description || description->maximum_candidate_frame_bytes != 12U ||
      adapter.StreamChunkCapacity(0U, 0U) == 0U) {
    return 5;
  }

  const auto split = adapter.SubmitStreamChunk(0U, 0U, Bytes("RX SDK!OK\r"));
  if (split.status != LocalStatus::OK || split.candidate) return 6;
  const auto completed = adapter.SubmitStreamChunk(0U, 0U, Bytes("\n"));
  if (completed.status != LocalStatus::OK || !completed.candidate ||
      completed.candidate->local_status != LocalStatus::OK ||
      Text(completed.candidate->frame) != "RX SDK!OK\r\n" || completed.host.candidates != 1U ||
      completed.host.decode_successes != 1U) {
    return 7;
  }

  const auto glued = adapter.SubmitStreamChunk(0U, 0U, Bytes("ONLY\r\nBAD\r\nONLY\r\n"));
  if (!glued.candidate || glued.after.frozen_cursor != 6U) return 8;
  const auto failed = adapter.ContinueStream(0U, 0U);
  if (!failed.candidate || failed.candidate->local_status != LocalStatus::CODEC_FAILED ||
      failed.after.reset_required) {
    return 9;
  }
  const auto recovered = adapter.ContinueStream(0U, 0U);
  if (!recovered.candidate || recovered.candidate->local_status != LocalStatus::OK ||
      recovered.after.frozen_input_bytes != 0U || recovered.after.total_candidates != 4U) {
    return 10;
  }

  const auto other = adapter.SubmitStreamChunk(0U, 1U, Bytes("ON"));
  if (other.status != LocalStatus::OK || !adapter.Reset(0U, 0U)) return 11;
  const auto other_completed = adapter.SubmitStreamChunk(0U, 1U, Bytes("LY\r\n"));
  if (!other_completed.candidate || other_completed.candidate->local_status != LocalStatus::OK) {
    return 12;
  }

  std::cout << "PUBLIC_ASCII_STREAM_PACKAGE_CONSUMER_PASS\n";
  return 0;
}
