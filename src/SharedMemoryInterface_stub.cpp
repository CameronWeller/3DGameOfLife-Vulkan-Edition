#include "SharedMemoryInterface.hpp"
#include <iostream>

namespace UXMirror {

    SharedMemoryInterface::SharedMemoryInterface(VulkanContext* context) 
        : vulkanContext(context), device(VK_NULL_HANDLE), physicalDevice(VK_NULL_HANDLE) {}

    SharedMemoryInterface::~SharedMemoryInterface() {
        cleanup();
    }

    bool SharedMemoryInterface::initialize() {
        std::cout << "🔧 [SharedMemory] Mock initialization complete" << std::endl;
        return true;
    }

    void SharedMemoryInterface::cleanup() {
        buffers.clear();
        semaphores.clear();
    }

    bool SharedMemoryInterface::createSharedBuffer(const std::string& name, size_t size, VkBufferUsageFlags usage) {
        auto buffer = std::make_unique<SharedBuffer>();
        buffer->size = size;
        buffer->usage = usage;
        
        std::lock_guard<std::mutex> lock(bufferMutex);
        buffers[name] = std::move(buffer);
        
        std::cout << "📦 [SharedMemory] Created buffer '" << name << "' (" << (size / 1024 / 1024) << "MB)" << std::endl;
        return true;
    }

    SharedBuffer* SharedMemoryInterface::getBuffer(const std::string& name) {
        std::lock_guard<std::mutex> lock(bufferMutex);
        auto it = buffers.find(name);
        return (it != buffers.end()) ? it->second.get() : nullptr;
    }

    bool SharedMemoryInterface::destroyBuffer(const std::string& name) {
        std::lock_guard<std::mutex> lock(bufferMutex);
        return buffers.erase(name) > 0;
    }

    bool SharedMemoryInterface::createTimelineSemaphore(const std::string& name) {
        std::lock_guard<std::mutex> lock(semaphoreMutex);
        semaphores[name] = VK_NULL_HANDLE;
        return true;
    }

    bool SharedMemoryInterface::signalSemaphore(const std::string& name, uint64_t value) {
        return true;
    }

    bool SharedMemoryInterface::waitSemaphore(const std::string& name, uint64_t value, uint64_t timeout) {
        return true;
    }

    bool SharedMemoryInterface::writeSimulationState(const std::vector<CellStateData>& cellData) {
        metrics.transferCount++;
        metrics.totalBytesTransferred += cellData.size() * sizeof(CellStateData);
        return true;
    }

    bool SharedMemoryInterface::readSimulationState(std::vector<CellStateData>& cellData) {
        metrics.transferCount++;
        return true;
    }

    bool SharedMemoryInterface::writeUXFeedback(const UXFeedbackData& feedback) {
        metrics.transferCount++;
        return true;
    }

    bool SharedMemoryInterface::readUXFeedback(UXFeedbackData& feedback) {
        metrics.transferCount++;
        return true;
    }

    bool SharedMemoryInterface::mapBufferToHIP(const std::string& bufferName) {
        return true;
    }

    bool SharedMemoryInterface::unmapBufferFromHIP(const std::string& bufferName) {
        return true;
    }

    hipDeviceptr_t SharedMemoryInterface::getHIPPointer(const std::string& bufferName) {
        return nullptr;
    }

    void SharedMemoryInterface::resetMetrics() {
        metrics.transferCount = 0;
        metrics.totalTransferTime = 0;
        metrics.lastTransferTime = 0;
        metrics.totalBytesTransferred = 0;
    }

    void SharedMemoryInterface::logError(const std::string& message) {
        std::cerr << "❌ [SharedMemory] " << message << std::endl;
    }

    bool SharedMemoryInterface::validateBuffer(const SharedBuffer* buffer) {
        return buffer != nullptr;
    }

    SharedBufferGuard::SharedBufferGuard(SharedMemoryInterface* interface, const std::string& bufferName)
        : interface(interface), buffer(interface->getBuffer(bufferName)), name(bufferName), ownsSync(false) {}

    SharedBufferGuard::~SharedBufferGuard() {}

    bool SharedBufferGuard::waitForVulkan(uint64_t timeout) {
        return true;
    }

    bool SharedBufferGuard::signalHIP() {
        return true;
    }

} // namespace UXMirror 