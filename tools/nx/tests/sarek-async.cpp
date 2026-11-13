#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace dxvk {
using thread = std::thread;
struct RcObject {};
namespace env {
std::string getEnvVar(const char *) { return {}; }
bool is32BitHostPlatform() { return false; }
void setThreadName(const char *) {}
}
namespace str {
template<class... T> std::string format(T&&... args) {
    std::ostringstream out;
    (out << ... << args);
    return out.str();
}
}
struct Logger { static void info(const std::string &) {} };
struct DxvkDevice {
    struct Config { int numShaderCompilerThreads = 2; } options;
    const Config &config() const { return options; }
};
struct DxvkGraphicsPipelineStateInfo {};
struct DxvkRenderPass { int format() const { return 0; } };
struct Control {
    std::mutex mutex;
    std::condition_variable condition;
    bool release = false, stopping = false;
    std::atomic<unsigned> started{0}, completed{0}, cached{0};
    std::atomic<bool> cacheStopped{false};
    unsigned expected = 0;
    void waitForStart() {
        std::unique_lock lock(mutex);
        assert(condition.wait_for(lock, std::chrono::seconds(5), [&] { return started.load() != 0; }));
    }
    void unblock() {
        { std::lock_guard lock(mutex); release = true; }
        condition.notify_all();
    }
    void waitForStop() {
        std::unique_lock lock(mutex);
        assert(condition.wait_for(lock, std::chrono::seconds(5), [&] { return stopping; }));
    }
};
struct DxvkGraphicsPipeline {
    Control &control;
    bool compilePipeline(const DxvkGraphicsPipelineStateInfo &, const DxvkRenderPass *) {
        std::unique_lock lock(control.mutex);
        ++control.started;
        control.condition.notify_all();
        control.condition.wait(lock, [&] { return control.release; });
        ++control.completed;
        return true;
    }
    void writePipelineStateToCache(const DxvkGraphicsPipelineStateInfo &, int) {
        assert(!control.cacheStopped.load());
        ++control.cached;
    }
};
}

#include "dxvk_asynccompiler.h"
#include "dxvk_asynccompiler.cpp"

namespace dxvk {
struct StateCache {
    Control &control;
    void stopWorkerThreads() {
        assert(control.completed.load() == control.expected);
        assert(control.cached.load() == control.expected);
        control.cacheStopped = true;
    }
};
struct PipelineLifetime {
    Control &control;
    ~PipelineLifetime() { assert(control.completed.load() == control.expected); }
};
struct CompilerDeleter {
    Control *control;
    void operator()(DxvkAsyncCompiler *compiler) const {
        { std::lock_guard lock(control->mutex); control->stopping = true; }
        control->condition.notify_all();
        delete compiler;
    }
};
class DxvkPipelineManager {
public:
    std::unique_ptr<StateCache> m_stateCache;
    std::unique_ptr<int> m_dyasync;
    std::unique_ptr<DxvkAsyncCompiler, CompilerDeleter> m_async;
    PipelineLifetime pipelines;
    DxvkPipelineManager(DxvkDevice &device, Control &control)
        : m_stateCache(new StateCache{control}), m_async(new DxvkAsyncCompiler(&device), CompilerDeleter{&control}), pipelines{control} {}
    ~DxvkPipelineManager();
    void stopWorkerThreads();
};
#include "manager-shutdown.inc"
}

int main() {
    using namespace dxvk;
    DxvkDevice device;
    DxvkRenderPass renderPass;
    {
        Control control;
        DxvkGraphicsPipeline pipeline{control};
        DxvkAsyncCompiler compiler(&device);
        assert(!compiler.isCompilingShaders());
        compiler.queueCompilation(&pipeline, {}, &renderPass);
        control.waitForStart();
        assert(compiler.isCompilingShaders());
        control.unblock();
        const auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (compiler.isCompilingShaders() && std::chrono::steady_clock::now() < timeout)
            std::this_thread::yield();
        assert(!compiler.isCompilingShaders());
        assert(control.completed == 1 && control.cached == 1);
    }
    for (bool explicitStop : {true, false}) {
        Control control;
        control.expected = 64;
        DxvkGraphicsPipeline pipeline{control};
        auto manager = std::make_unique<DxvkPipelineManager>(device, control);
        for (unsigned i = 0; i < control.expected; ++i)
            manager->m_async->queueCompilation(&pipeline, {}, &renderPass);
        control.waitForStart();
        assert(manager->m_async->isCompilingShaders());
        std::thread shutdown([&] {
            if (explicitStop) {
                manager->stopWorkerThreads();
                manager->stopWorkerThreads();
            }
            manager.reset();
        });
        control.waitForStop();
        control.unblock();
        shutdown.join();
        assert(control.cacheStopped);
        assert(control.completed == control.expected && control.cached == control.expected);
    }
    std::puts("PASS: async busy/idle, queued-work drain, cache lifetime, repeated stop and destructor shutdown");
}
