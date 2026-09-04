#pragma once

namespace pae::protocol_lab {

class RecordFileSystem;

int RunApplication(int argc, char** argv);
int RunApplicationWithFileSystem(int argc, char** argv, RecordFileSystem& file_system);

}  // namespace pae::protocol_lab
