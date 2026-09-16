#include <pae/compiler.h>

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

std::atomic<std::size_t> g_allocation_count{0U};

class Runner final {
 public:
  void Check(bool condition, std::string_view name) {
    if (condition) {
      ++passed_;
      std::cout << "PASS case=" << name << '\n';
    } else {
      ++failed_;
      std::cerr << "FAIL case=" << name << '\n';
    }
  }

  int Finish() const {
    std::cout << "PUBLIC_CONSUMER_METADATA_TEST_SUMMARY passed=" << passed_ << " failed=" << failed_
              << " gate=" << (failed_ == 0U ? "PASS" : "FAIL") << '\n';
    return failed_ == 0U ? 0 : 1;
  }

 private:
  std::size_t passed_ = 0U;
  std::size_t failed_ = 0U;
};

std::string ReadFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

pae::CompiledProtocol Compile(const char* path, Runner& runner, std::string_view name) {
  auto result = pae::CompileProtocolJson(ReadFile(path));
  runner.Check(result.Succeeded(), name);
  return result.Succeeded() ? std::move(result).TakeCompiled() : pae::CompiledProtocol{};
}

std::optional<std::size_t> FindMessage(const pae::CompiledProtocol& compiled,
                                       std::string_view id) noexcept {
  for (std::size_t index = 0U; index < compiled.MessageCount(); ++index) {
    const auto message = compiled.Message(index);
    if (message.has_value() && message->id == id) return index;
  }
  return std::nullopt;
}

std::optional<pae::FieldDescription> FindField(const pae::CompiledProtocol& compiled,
                                               std::size_t message_index,
                                               std::string_view id) noexcept {
  const auto message = compiled.Message(message_index);
  if (!message.has_value()) return std::nullopt;
  for (std::size_t offset = 0U; offset < message->field_count; ++offset) {
    const auto field = compiled.Field(message->field_begin + offset);
    if (field.has_value() && field->id == id) return field;
  }
  return std::nullopt;
}

void CheckBinary(Runner& runner, const char* path) {
  auto compiled = Compile(path, runner, "binary_compile");
  const auto message = FindMessage(compiled, "lab_command");
  const auto action = message.has_value() ? compiled.PipelineMessageExecution(0U, *message)
                                          : std::optional<pae::MessageExecutionDescription>{};
  const auto input = message.has_value() ? FindField(compiled, *message, "record_length")
                                         : std::optional<pae::FieldDescription>{};
  const auto constant = message.has_value() ? FindField(compiled, *message, "operation_code")
                                            : std::optional<pae::FieldDescription>{};
  runner.Check(action.has_value() && action->decode_available && action->encode_available &&
                   action->encode_output_size_kind == pae::EncodeOutputSizeKind::EXACT &&
                   action->encode_output_size == 13U,
               "binary_actions_and_exact_output");
  runner.Check(input.has_value() && input->value_kind == pae::ValueKind::UINT64 &&
                   input->encode_value_source == pae::EncodeValueSource::CALLER_INPUT &&
                   constant.has_value() && constant->value_kind == pae::ValueKind::UINT64 &&
                   constant->encode_value_source == pae::EncodeValueSource::CONSTANT,
               "binary_field_type_and_source");
  runner.Check(message.has_value() && !compiled.PipelineMessageExecution(1U, *message).has_value(),
               "non_member_association_rejected");
}

void CheckDecimal(Runner& runner, const char* path) {
  auto compiled = Compile(path, runner, "decimal_compile");
  const auto logical = FindField(compiled, 0U, "temperature");
  const auto constant = FindField(compiled, 0U, "legacy_constant");
  runner.Check(logical.has_value() && logical->value_kind == pae::ValueKind::DECIMAL64 &&
                   logical->encode_value_source == pae::EncodeValueSource::CALLER_INPUT,
               "conversion_reports_logical_decimal");
  runner.Check(constant.has_value() && constant->value_kind == pae::ValueKind::UINT64 &&
                   constant->encode_value_source == pae::EncodeValueSource::CONSTANT,
               "decimal_message_constant_source");
}

void CheckComputedAndBounded(Runner& runner, const char* length_path, const char* bounded_path) {
  auto length = Compile(length_path, runner, "length_compile");
  const auto computed = FindField(length, 0U, "record_length");
  runner.Check(computed.has_value() && computed->value_kind == pae::ValueKind::UINT64 &&
                   computed->encode_value_source == pae::EncodeValueSource::COMPUTED,
               "computed_source");

  auto bounded = Compile(bounded_path, runner, "bounded_compile");
  const auto action = bounded.PipelineMessageExecution(0U, 0U);
  runner.Check(action.has_value() && action->decode_available && action->encode_available &&
                   action->encode_output_size_kind == pae::EncodeOutputSizeKind::UPPER_BOUND &&
                   action->encode_output_size == 6U,
               "bounded_output_upper_bound");
}

void CheckAscii(Runner& runner, const char* ascii_path, const char* stream_path) {
  auto ascii = Compile(ascii_path, runner, "ascii_compile");
  const auto action = ascii.PipelineMessageExecution(0U, 0U);
  const auto name = FindField(ascii, 0U, "name");
  const auto receive_only = FindField(ascii, 0U, "rx_code");
  const auto transmit_only = FindField(ascii, 0U, "tx_tag");
  runner.Check(action.has_value() && action->decode_available && action->encode_available &&
                   action->encode_output_size_kind == pae::EncodeOutputSizeKind::UPPER_BOUND &&
                   action->encode_output_size == 15U,
               "ascii_actions_and_upper_bound");
  runner.Check(name.has_value() && name->value_kind == pae::ValueKind::BYTES &&
                   name->encode_value_source == pae::EncodeValueSource::CALLER_INPUT &&
                   receive_only.has_value() &&
                   receive_only->encode_value_source == pae::EncodeValueSource::NOT_REFERENCED &&
                   transmit_only.has_value() &&
                   transmit_only->encode_value_source == pae::EncodeValueSource::CALLER_INPUT,
               "ascii_action_reference_sources");

  auto stream = Compile(stream_path, runner, "ascii_stream_compile");
  const auto bidirectional = stream.PipelineMessageExecution(0U, 0U);
  const auto decode_only = stream.PipelineMessageExecution(2U, 1U);
  const auto encode_only = stream.PipelineMessageExecution(1U, 2U);
  runner.Check(
      bidirectional.has_value() && bidirectional->decode_available &&
          bidirectional->encode_available && decode_only.has_value() &&
          decode_only->decode_available && !decode_only->encode_available &&
          decode_only->encode_output_size_kind == pae::EncodeOutputSizeKind::NOT_AVAILABLE &&
          decode_only->encode_output_size == 0U && encode_only.has_value() &&
          !encode_only->decode_available && encode_only->encode_available &&
          encode_only->encode_output_size_kind == pae::EncodeOutputSizeKind::EXACT &&
          encode_only->encode_output_size == 6U,
      "ascii_one_way_actions");
}

}  // namespace

void* operator new(std::size_t size) {
  g_allocation_count.fetch_add(1U, std::memory_order_relaxed);
  if (void* storage = std::malloc(size)) return storage;
  throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
  g_allocation_count.fetch_add(1U, std::memory_order_relaxed);
  if (void* storage = std::malloc(size)) return storage;
  throw std::bad_alloc{};
}

void operator delete(void* storage) noexcept { std::free(storage); }
void operator delete[](void* storage) noexcept { std::free(storage); }
void operator delete(void* storage, std::size_t) noexcept { std::free(storage); }
void operator delete[](void* storage, std::size_t) noexcept { std::free(storage); }

int main(int argc, char** argv) {
  Runner runner;
  runner.Check(argc == 7, "arguments");
  if (argc != 7) return runner.Finish();

  CheckBinary(runner, argv[1]);
  CheckDecimal(runner, argv[2]);
  CheckComputedAndBounded(runner, argv[3], argv[4]);
  CheckAscii(runner, argv[5], argv[6]);

  auto no_allocation = Compile(argv[1], runner, "no_allocation_compile");
  const std::size_t before = g_allocation_count.load(std::memory_order_relaxed);
  const auto field = no_allocation.Field(0U);
  const auto action = no_allocation.PipelineMessageExecution(0U, 0U);
  const std::size_t after = g_allocation_count.load(std::memory_order_relaxed);
  runner.Check(field.has_value() && action.has_value() && before == after,
               "metadata_queries_do_not_allocate");

  pae::CompiledProtocol empty;
  runner.Check(!empty.PipelineMessageExecution(0U, 0U).has_value(), "empty_owner_execution_query");
  auto moved_result = pae::CompileProtocolJson(ReadFile(argv[1]));
  if (moved_result.Succeeded()) {
    pae::CompiledProtocol source = std::move(moved_result).TakeCompiled();
    pae::CompiledProtocol target = std::move(source);
    runner.Check(!source.PipelineMessageExecution(0U, 0U).has_value() &&
                     !source.Field(0U).has_value() && target.Field(0U).has_value() &&
                     target.PipelineMessageExecution(0U, 0U).has_value() &&
                     !target.PipelineMessageExecution(target.PipelineCount(), 0U).has_value() &&
                     !target.PipelineMessageExecution(0U, target.MessageCount()).has_value(),
                 "move_and_out_of_range_queries");
  } else {
    runner.Check(false, "move_and_out_of_range_queries");
  }
  return runner.Finish();
}
