#include "util/config/config.h"
#include "util/util_env.h"
#include "util/log/log.h"
#include "dxvk/dxvk_options.h"
#include <cassert>
#include <cstdlib>

namespace dxvk {
class DxvkDevice {
public:
    DxvkDevice(const DxvkOptions& options) : m_options(options) { }
    const DxvkOptions& config() const { return m_options; }
private:
    const DxvkOptions& m_options;
};
#include "compile-method.inc"
void Logger::info(const std::string&) { }
namespace env {
std::string getEnvVar(const char* name) {
    const char* value = std::getenv(name);
    return value ? value : "";
}
std::string getExeName() { return "OpenBLOPS-NX.nro"; }
}
}

int main(int argc, char** argv) {
    assert(argc == 6);
    const auto config = dxvk::Config::getUserConfig();
    const dxvk::DxvkOptions options(config);
    assert(options.maxMemoryBudget == (std::strtoull(argv[1], nullptr, 10) << 20));
    assert(config.getOption<int32_t>("d3d9.maxAvailableMemory", 4096) == std::atoi(argv[2]));
    assert(options.shaderCompilationMethod == argv[3]);
    assert(options.numCompilerThreads == std::atoi(argv[4]));
    const dxvk::DxvkDevice device(options);
    assert(std::string(dxvk::method_name(dxvk::resolve_compile_method(&device))) == argv[5]);
}
