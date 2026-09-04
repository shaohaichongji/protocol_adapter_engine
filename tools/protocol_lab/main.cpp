#include <exception>
#include <iostream>

#include "protocol_lab.h"

int main(int argc, char** argv) {
  try {
    return pae::protocol_lab::RunApplication(argc, argv);
  } catch (const std::exception& error) {
    std::cerr << "INTERNAL_ERROR detail=" << error.what() << '\n';
  } catch (...) {
    std::cerr << "INTERNAL_ERROR detail=unclassified exception\n";
  }
  return 10;
}
