#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "complete_record_codec.h"
#include "lab_types.h"

namespace pae::protocol_lab {

std::string JsonEscape(std::string_view input);
std::string Quoted(std::string_view value);
std::string HexUpper(const std::uint8_t* data, std::size_t size);
std::string HexUpper(const std::vector<std::uint8_t>& data);
std::string CodecStatusName(protocol_core::CodecStatus status);
void FinalizeFingerprint(OperationResult& result);
std::string SerializeFields(const std::vector<FieldResult>& fields, int indent);
std::string SerializeFieldsCompact(const std::vector<FieldResult>& fields);
std::string SerializeResult(const OperationResult& result);
std::string FieldsCanonical(const OperationResult& result);
std::vector<std::string> CompareStoredRuns(const StoredRun& left, const StoredRun& right);
StoredRun ToStoredRun(const OperationResult& result);

}  // namespace pae::protocol_lab
