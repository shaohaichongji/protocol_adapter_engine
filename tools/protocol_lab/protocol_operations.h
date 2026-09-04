#pragma once

#include <yyjson.h>

#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "lab_types.h"
#include "plan_bundle.h"

namespace pae::protocol_lab {

struct DocumentDeleter {
  void operator()(yyjson_doc* document) const noexcept;
};
using DocumentPtr = std::unique_ptr<yyjson_doc, DocumentDeleter>;

bool ReadFile(const std::filesystem::path& path, std::vector<std::uint8_t>& output,
              std::string& error);
bool ReadText(const std::filesystem::path& path, std::string& output, std::string& error);
bool ParseFrameHex(std::string_view text, std::vector<std::uint8_t>& output, std::string& error);
bool ParseDocument(std::string& text, DocumentPtr& output, std::string& error);
bool ValidateObject(yyjson_val* object, const std::set<std::string_view>& allowed,
                    const std::set<std::string_view>& required, std::string_view pointer,
                    std::string& error);
bool ReadJsonString(yyjson_val* object, const char* name, std::string& output, std::string& error);
bool ParseValues(std::string& text, ParsedValues& output, std::string& error);
OperationResult InspectFrame(const protocol_plan::PlanBundle& plan,
                             const std::vector<std::uint8_t>& frame);
OperationResult EncodeValues(const protocol_plan::PlanBundle& plan, const ParsedValues& parsed,
                             std::string& error);
bool LoadFrameArgument(const std::filesystem::path& binary, const std::filesystem::path& hex,
                       std::vector<std::uint8_t>& output, std::string& error);
bool CompileConfig(const std::filesystem::path& path, std::string& text,
                   protocol_plan::PlanOwner& plan, OperationResult& result, std::string& error);

}  // namespace pae::protocol_lab
