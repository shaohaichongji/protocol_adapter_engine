#pragma once

namespace pae::protocol_lab {

class RecordFileSystem;
class IUdpExchangeAdapter;

class IProtocolLabExecutionObserver {
 public:
  virtual ~IProtocolLabExecutionObserver() = default;
  virtual void OnProtocolOperation(const char* operation) = 0;
};

int RunApplication(int argc, char** argv);
int RunApplicationWithFileSystem(int argc, char** argv, RecordFileSystem& file_system);
int RunApplicationWithDependencies(int argc, char** argv, RecordFileSystem& file_system,
                                   IUdpExchangeAdapter& udp_adapter,
                                   IProtocolLabExecutionObserver* execution_observer = nullptr);

}  // namespace pae::protocol_lab
