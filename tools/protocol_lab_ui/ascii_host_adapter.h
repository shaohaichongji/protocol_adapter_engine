#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../protocol_lab_ascii/host_observer_adapter.h"
#include "../protocol_lab_ascii/public_ascii_host_adapter.h"
#include "description_mapping.h"

namespace pae::protocol_lab_ui {

protocol_lab::ascii::ExecutionResult ConvertPublicAsciiResult(
    protocol_lab_ascii::public_offline::OperationResult source,
    protocol_lab::ascii::ExecutionIdentity identity, protocol_lab::ascii::Operation operation);

// Runtime facade that keeps Schema 0.11 on the established private stream observer while routing
// Schema 0.10 A2 through public PAE. The UI/session code sees one stable complete/stream surface.
class AsciiHostAdapter final {
 public:
  static std::unique_ptr<AsciiHostAdapter> CreatePublic(
      CompiledProtocol compiled, std::vector<protocol_lab::ascii::HostBinding> bindings,
      std::size_t previous_instance_bytes, std::string& error);
  static std::unique_ptr<AsciiHostAdapter> CreatePublicDirect(CompiledProtocol compiled,
                                                              std::string& error);
  static std::unique_ptr<AsciiHostAdapter> CreatePrivate(
      config_compiler::CompiledUiArtifacts artifacts,
      std::vector<protocol_lab::ascii::HostBinding> bindings, std::string& error);

  const DocumentDescription& Description() const noexcept { return description_; }
  const std::vector<protocol_lab::ascii::HostBinding>& Bindings() const noexcept {
    return bindings_;
  }
  std::size_t FlowCount(std::size_t binding) const noexcept;
  std::size_t AccountedBytes() const noexcept;
  bool IsPublicCompleteRecord() const noexcept { return public_ && !public_stream_; }
  bool IsPublicStream() const noexcept { return public_ && public_stream_; }
  std::optional<std::size_t> FindBinding(std::size_t pipeline_index,
                                         host_endpoint::Action action) const noexcept;

  protocol_lab::ascii::ExecutionResult Inspect(std::size_t binding, std::size_t stream,
                                               protocol_lab::ascii::ExecutionIdentity identity,
                                               const std::vector<std::uint8_t>& frame);
  protocol_lab::ascii::ExecutionResult Encode(
      std::size_t binding, protocol_lab::ascii::ExecutionIdentity identity,
      const std::vector<protocol_lab::ascii::InputField>& fields);
  protocol_lab::ascii::StreamStepResult Submit(std::size_t binding, std::size_t stream,
                                               protocol_lab::ascii::ExecutionIdentity identity,
                                               const std::vector<std::uint8_t>& chunk);
  protocol_lab::ascii::StreamStepResult Continue(std::size_t binding, std::size_t stream,
                                                 protocol_lab::ascii::ExecutionIdentity identity);
  bool Reset(std::size_t binding, std::size_t stream);
  std::optional<protocol_lab::ascii::StreamObservation> Observe(std::size_t binding,
                                                                std::size_t stream) const noexcept;
  bool HasDiscardableState() const noexcept;

 private:
  DocumentDescription description_;
  std::vector<protocol_lab::ascii::HostBinding> bindings_;
  std::unique_ptr<protocol_lab_ascii::public_offline::HostAdapter> public_;
  std::unique_ptr<protocol_lab::ascii::HostObserverAdapter> private_;
  bool public_stream_ = false;
};

}  // namespace pae::protocol_lab_ui
