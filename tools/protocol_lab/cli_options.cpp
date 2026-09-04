#include "cli_options.h"

#include <iostream>
#include <set>
#include <string_view>

namespace pae::protocol_lab {
namespace {

bool SetPath(std::filesystem::path& target, const char* value) {
  if (!target.empty()) {
    return false;
  }
  target = value;
  return !target.empty();
}

bool SetString(std::string& target, const char* value, std::string_view default_value = {}) {
  if (target != default_value) {
    return false;
  }
  target = value;
  return !target.empty();
}

}  // namespace

std::string CommandName(Command command) {
  switch (command) {
    case Command::INSPECT:
      return "inspect";
    case Command::ENCODE:
      return "encode";
    case Command::REPLAY:
      return "replay";
    case Command::COMPARE:
      return "compare";
  }
  return "unknown";
}

void PrintUsage() {
  std::cerr
      << "usage:\n"
      << "  pae_protocol_lab inspect --config <file> (--frame-bin <file> | --frame-hex <file>)\n"
      << "  pae_protocol_lab encode --config <file> --values <file>\n"
      << "  pae_protocol_lab replay --bundle <run-directory> [--config <file>]\n"
      << "  pae_protocol_lab compare (--left-run <run> --right-run <run> | frame pair)\n"
      << "common: [--output text|json] [--record-root <dir>] [--expect-status <status>]\n";
}

bool ParseArguments(int argc, char** argv, Arguments& output, bool& early_success) {
  early_success = false;
  if (argc == 2 && std::string_view{argv[1]} == "--help") {
    PrintUsage();
    early_success = true;
    return true;
  }
  if (argc == 2 && std::string_view{argv[1]} == "--version") {
    std::cout << "pae_protocol_lab " << kToolVersion << '\n';
    early_success = true;
    return true;
  }
  if (argc < 2) {
    return false;
  }
  const std::string_view command{argv[1]};
  if (command == "inspect") {
    output.command = Command::INSPECT;
  } else if (command == "encode") {
    output.command = Command::ENCODE;
  } else if (command == "replay") {
    output.command = Command::REPLAY;
  } else if (command == "compare") {
    output.command = Command::COMPARE;
  } else {
    return false;
  }

  std::set<std::string_view> seen_options;
  for (int index = 2; index < argc; ++index) {
    const std::string_view name{argv[index]};
    if (name == "--help") {
      PrintUsage();
      early_success = true;
      return index + 1 == argc;
    }
    if (name == "--version") {
      if (index + 1 != argc) {
        return false;
      }
      std::cout << "pae_protocol_lab " << kToolVersion << '\n';
      early_success = true;
      return true;
    }
    if (!seen_options.insert(name).second || index + 1 >= argc) {
      return false;
    }
    const char* value = argv[++index];
    bool accepted = false;
    if (name == "--config") {
      accepted = SetPath(output.config, value);
    } else if (name == "--output") {
      accepted = SetString(output.output, value, "text");
    } else if (name == "--record-root") {
      accepted = SetPath(output.record_root, value);
    } else if (name == "--expect-status") {
      accepted = SetString(output.expect_status, value);
    } else if (name == "--frame-bin") {
      accepted = SetPath(output.frame_binary, value);
    } else if (name == "--frame-hex") {
      accepted = SetPath(output.frame_hex, value);
    } else if (name == "--values") {
      accepted = SetPath(output.values, value);
    } else if (name == "--bundle") {
      accepted = SetPath(output.bundle, value);
    } else if (name == "--left-run") {
      accepted = SetPath(output.left_run, value);
    } else if (name == "--right-run") {
      accepted = SetPath(output.right_run, value);
    } else if (name == "--left-frame-bin") {
      accepted = SetPath(output.left_frame_binary, value);
    } else if (name == "--right-frame-bin") {
      accepted = SetPath(output.right_frame_binary, value);
    } else if (name == "--left-frame-hex") {
      accepted = SetPath(output.left_frame_hex, value);
    } else if (name == "--right-frame-hex") {
      accepted = SetPath(output.right_frame_hex, value);
    }
    if (!accepted) {
      return false;
    }
  }
  if (output.output != "text" && output.output != "json") {
    return false;
  }
  if (output.command == Command::INSPECT) {
    return !output.config.empty() && (output.frame_binary.empty() != output.frame_hex.empty()) &&
           output.values.empty() && output.bundle.empty() && output.left_run.empty() &&
           output.right_run.empty() && output.left_frame_binary.empty() &&
           output.right_frame_binary.empty() && output.left_frame_hex.empty() &&
           output.right_frame_hex.empty();
  }
  if (output.command == Command::ENCODE) {
    return !output.config.empty() && !output.values.empty() && output.frame_binary.empty() &&
           output.frame_hex.empty() && output.bundle.empty() && output.left_run.empty() &&
           output.right_run.empty() && output.left_frame_binary.empty() &&
           output.right_frame_binary.empty() && output.left_frame_hex.empty() &&
           output.right_frame_hex.empty();
  }
  if (output.command == Command::REPLAY) {
    return !output.bundle.empty() && output.values.empty() && output.frame_binary.empty() &&
           output.frame_hex.empty() && output.left_run.empty() && output.right_run.empty() &&
           output.left_frame_binary.empty() && output.right_frame_binary.empty() &&
           output.left_frame_hex.empty() && output.right_frame_hex.empty();
  }
  if (!output.config.empty() || !output.values.empty() || !output.bundle.empty() ||
      !output.record_root.empty() || !output.expect_status.empty()) {
    return false;
  }
  const bool run_pair = !output.left_run.empty() && !output.right_run.empty() &&
                        output.left_frame_binary.empty() && output.right_frame_binary.empty() &&
                        output.left_frame_hex.empty() && output.right_frame_hex.empty();
  const bool frame_pair = output.left_run.empty() && output.right_run.empty() &&
                          (output.left_frame_binary.empty() != output.left_frame_hex.empty()) &&
                          (output.right_frame_binary.empty() != output.right_frame_hex.empty());
  return run_pair || frame_pair;
}

}  // namespace pae::protocol_lab
