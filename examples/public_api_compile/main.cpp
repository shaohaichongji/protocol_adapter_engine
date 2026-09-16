#include <pae/compiler.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: pae_public_compile_example <config.pae.json>\n";
    return 2;
  }
  std::ifstream input(argv[1], std::ios::binary);
  std::ostringstream bytes;
  bytes << input.rdbuf();
  if (!input || (!input.good() && !input.eof())) {
    std::cerr << "could not read configuration\n";
    return 2;
  }

  auto result = pae::CompileProtocolJson(bytes.str());
  if (!result.Succeeded()) {
    const auto* diagnostic = result.Diagnostic();
    std::cerr << "compile failed";
    if (diagnostic != nullptr) {
      std::cerr << " pointer=" << diagnostic->json_pointer << " detail=" << diagnostic->detail;
    }
    std::cerr << '\n';
    return 1;
  }

  const auto protocol = result.Compiled()->Protocol();
  if (!protocol.has_value()) {
    std::cerr << "compiled protocol has no metadata\n";
    return 1;
  }
  std::cout << "schema=" << protocol->schema_version << " protocol=" << protocol->id
            << " pipelines=" << result.Compiled()->PipelineCount()
            << " messages=" << result.Compiled()->MessageCount() << '\n';
  return 0;
}
