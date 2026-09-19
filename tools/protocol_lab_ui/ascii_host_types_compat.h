#pragma once

#include <vector>

#include "../protocol_lab_ascii/ascii_offline_adapter.h"
#include "ascii_host_types.h"

namespace pae::protocol_lab_ui {

AsciiExecutionResult ConvertPrivateAsciiResult(protocol_lab::ascii::ExecutionResult source);
protocol_lab::ascii::ExecutionIdentity ToPrivateAsciiIdentity(AsciiExecutionIdentity source);
std::vector<protocol_lab::ascii::InputField> ToPrivateAsciiInputs(
    const std::vector<AsciiInputField>& source);
AsciiStreamStepResult ConvertPrivateAsciiStreamStep(protocol_lab::ascii::StreamStepResult source);
AsciiStreamObservation ConvertPrivateAsciiObservation(
    const protocol_lab::ascii::StreamObservation& source) noexcept;

}  // namespace pae::protocol_lab_ui
