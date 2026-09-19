#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ascii_host_types.h"
#include "owned_presentation_types.h"
#include "pae/compiler.h"
#include "../protocol_lab_ascii/public_ascii_offline_adapter.h"

namespace pae::protocol_lab_ui {

class AsciiHostBackend {
 public:
  virtual ~AsciiHostBackend() = default;
  virtual std::size_t AccountedBytes() const noexcept = 0;
  virtual AsciiExecutionResult Inspect(std::size_t binding, std::size_t stream,
                                       AsciiExecutionIdentity identity,
                                       const std::vector<std::uint8_t>& frame) = 0;
  virtual AsciiExecutionResult Encode(std::size_t binding, AsciiExecutionIdentity identity,
                                      const std::vector<AsciiInputField>& fields) = 0;
  virtual AsciiStreamStepResult Submit(std::size_t binding, std::size_t stream,
                                       AsciiExecutionIdentity identity,
                                       const std::vector<std::uint8_t>& chunk) = 0;
  virtual AsciiStreamStepResult Continue(std::size_t binding, std::size_t stream,
                                         AsciiExecutionIdentity identity) = 0;
  virtual bool Reset(std::size_t binding, std::size_t stream) = 0;
  virtual std::optional<AsciiStreamObservation> Observe(std::size_t binding,
                                                        std::size_t stream) const noexcept = 0;
  virtual bool HasDiscardableState(const std::vector<AsciiHostBinding>& bindings) const noexcept = 0;
};

AsciiExecutionResult ConvertPublicAsciiResult(
    protocol_lab_ascii::public_offline::OperationResult source,
    AsciiExecutionIdentity identity, AsciiOperation operation);

// Lab-owned facade. Its public implementation has no dependency on the old private ASCII adapter;
// a compatibility backend is supplied by a separate target when that path is enabled.
class AsciiHostAdapter final {
 public:
  static std::unique_ptr<AsciiHostAdapter> CreatePublic(
      CompiledProtocol compiled, std::vector<AsciiHostBinding> bindings,
      std::size_t previous_instance_bytes, std::string& error);
  static std::unique_ptr<AsciiHostAdapter> CreatePublicDirect(CompiledProtocol compiled,
                                                              std::string& error);
  static std::unique_ptr<AsciiHostAdapter> AdoptBackend(
      std::unique_ptr<AsciiHostBackend> backend, DocumentDescription description,
      std::vector<AsciiHostBinding> bindings, bool public_complete, bool public_stream);

  const DocumentDescription& Description() const noexcept { return description_; }
  const std::vector<AsciiHostBinding>& Bindings() const noexcept { return bindings_; }
  std::size_t FlowCount(std::size_t binding) const noexcept;
  std::size_t AccountedBytes() const noexcept;
  bool IsPublicCompleteRecord() const noexcept { return public_complete_; }
  bool IsPublicStream() const noexcept { return public_stream_; }
  std::optional<std::size_t> FindBinding(std::size_t pipeline_index,
                                         AsciiHostAction action) const noexcept;

  AsciiExecutionResult Inspect(std::size_t binding, std::size_t stream,
                               AsciiExecutionIdentity identity,
                               const std::vector<std::uint8_t>& frame);
  AsciiExecutionResult Encode(std::size_t binding, AsciiExecutionIdentity identity,
                              const std::vector<AsciiInputField>& fields);
  AsciiStreamStepResult Submit(std::size_t binding, std::size_t stream,
                               AsciiExecutionIdentity identity,
                               const std::vector<std::uint8_t>& chunk);
  AsciiStreamStepResult Continue(std::size_t binding, std::size_t stream,
                                 AsciiExecutionIdentity identity);
  bool Reset(std::size_t binding, std::size_t stream);
  std::optional<AsciiStreamObservation> Observe(std::size_t binding,
                                                std::size_t stream) const noexcept;
  bool HasDiscardableState() const noexcept;

 private:
  AsciiHostAdapter(std::unique_ptr<AsciiHostBackend> backend, DocumentDescription description,
                   std::vector<AsciiHostBinding> bindings, bool public_complete,
                   bool public_stream) noexcept;

  std::unique_ptr<AsciiHostBackend> backend_;
  DocumentDescription description_;
  std::vector<AsciiHostBinding> bindings_;
  bool public_complete_ = false;
  bool public_stream_ = false;
};

}  // namespace pae::protocol_lab_ui
