#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include <atomic>
#include <mutex>
#include <iomanip>
#include <cmath>
#include <random>

// Minimal simulation state for overnight optimization
struct SimulationState {
    double frameTime{16.67};
    double gpuUtilization{75.0};
    double memoryEfficiency{80.0};
    double cellsThroughput{3500000.0};
    double engagement{85.0};
    double frustration{15.0};
    uint64_t step{0};
    std::atomic<bool> isOptimizing{false};
};

class MinimalOvernightOptimizer {
private:
    SimulationState state;
    std::mutex stateMutex;
    std::ofstream logFile;
    std::atomic<bool> running{false};
    
    // Optimization targets
    const double TARGET_FRAME_TIME = 16.67;
    const double TARGET_GPU_UTIL = 90.0;
    const double TARGET_MEMORY_EFF = 95.0;
    const double TARGET_THROUGHPUT = 5000000.0;
    const double TARGET_ENGAGEMENT = 90.0;
    const double TARGET_FRUSTRATION = 10.0;

public:
    MinimalOvernightOptimizer(const std::string& logFileName) {
        logFile.open(logFileName, std::ios::out | std::ios::app);
        if (!logFile.is_open()) {
            throw std::runtime_error("Failed to open log file: " + logFileName);
        }
        
        logFile << "\n=== UX-Mirror Overnight Optimization Started ===\n";
        logFile << "Timestamp: " << getCurrentTimeString() << "\n";
        logFile.flush();
    }
    
    ~MinimalOvernightOptimizer() {
        if (logFile.is_open()) {
            logFile << "\n=== Optimization Session Ended ===\n";
            logFile << "Timestamp: " << getCurrentTimeString() << "\n";
            logFile.close();
        }
    }
    
    void runOptimization(int hours) {
        if (hours < 1 || hours > 24) {
            throw std::invalid_argument("Hours must be between 1 and 24");
        }
        
        running = true;
        const auto endTime = std::chrono::steady_clock::now() + std::chrono::hours(hours);
        const auto stepInterval = std::chrono::milliseconds(16); // 60 FPS
        
        logFile << "Starting " << hours << " hour optimization run\n";
        logFile << "Target Performance:\n";
        logFile << "  Frame Time: <= " << TARGET_FRAME_TIME << "ms\n";
        logFile << "  GPU Utilization: >= " << TARGET_GPU_UTIL << "%\n";
        logFile << "  Memory Efficiency: >= " << TARGET_MEMORY_EFF << "%\n";
        logFile << "  Throughput: >= " << TARGET_THROUGHPUT << " cells/sec\n";
        logFile << "  Engagement: >= " << TARGET_ENGAGEMENT << "%\n";
        logFile << "  Frustration: <= " << TARGET_FRUSTRATION << "%\n\n";
        logFile.flush();
        
        auto lastLogTime = std::chrono::steady_clock::now();
        const auto logInterval = std::chrono::minutes(5);
        
        while (running && std::chrono::steady_clock::now() < endTime) {
            auto stepStart = std::chrono::high_resolution_clock::now();
            
            // Simulate one optimization step
            performOptimizationStep();
            
            // Calculate step timing
            auto stepEnd = std::chrono::high_resolution_clock::now();
            auto stepDuration = std::chrono::duration_cast<std::chrono::microseconds>(stepEnd - stepStart);
            
            // Update frame time
            {
                std::lock_guard<std::mutex> lock(stateMutex);
                state.frameTime = stepDuration.count() / 1000.0; // Convert to milliseconds
                state.step++;
            }
            
            // Log progress every 5 minutes
            auto now = std::chrono::steady_clock::now();
            if (now - lastLogTime >= logInterval) {
                logProgress();
                lastLogTime = now;
            }
            
            // Sleep to maintain target framerate
            std::this_thread::sleep_until(stepStart + stepInterval);
        }
        
        running = false;
        logFinalResults();
    }
    
    void stop() {
        running = false;
    }

private:
    void performOptimizationStep() {
        std::lock_guard<std::mutex> lock(stateMutex);
        
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        static thread_local std::uniform_real_distribution<> variation(-0.1, 0.1);
        
        state.isOptimizing = true;
        
        // Simulate GPU optimization
        if (state.gpuUtilization < TARGET_GPU_UTIL) {
            state.gpuUtilization += std::abs(variation(gen)) * 2.0;
            state.gpuUtilization = std::min(state.gpuUtilization, 100.0);
        }
        
        // Simulate memory optimization
        if (state.memoryEfficiency < TARGET_MEMORY_EFF) {
            state.memoryEfficiency += std::abs(variation(gen)) * 1.5;
            state.memoryEfficiency = std::min(state.memoryEfficiency, 100.0);
        }
        
        // Simulate throughput optimization
        if (state.cellsThroughput < TARGET_THROUGHPUT) {
            state.cellsThroughput += std::abs(variation(gen)) * 50000.0;
        }
        
        // Simulate UX optimization
        if (state.engagement < TARGET_ENGAGEMENT) {
            state.engagement += std::abs(variation(gen)) * 0.5;
            state.engagement = std::min(state.engagement, 100.0);
        }
        
        if (state.frustration > TARGET_FRUSTRATION) {
            state.frustration -= std::abs(variation(gen)) * 0.3;
            state.frustration = std::max(state.frustration, 0.0);
        }
        
        // Add some realistic drift
        state.gpuUtilization += variation(gen) * 0.5;
        state.memoryEfficiency += variation(gen) * 0.3;
        state.engagement += variation(gen) * 0.2;
        state.frustration += variation(gen) * 0.1;
        
        // Clamp values to realistic ranges
        state.gpuUtilization = std::clamp(state.gpuUtilization, 0.0, 100.0);
        state.memoryEfficiency = std::clamp(state.memoryEfficiency, 0.0, 100.0);
        state.engagement = std::clamp(state.engagement, 0.0, 100.0);
        state.frustration = std::clamp(state.frustration, 0.0, 100.0);
        
        state.isOptimizing = false;
    }
    
    void logProgress() {
        std::lock_guard<std::mutex> lock(stateMutex);
        
        logFile << "[" << getCurrentTimeString() << "] Step " << state.step << ":\n";
        logFile << "  Frame Time: " << std::fixed << std::setprecision(2) << state.frameTime << "ms";
        if (state.frameTime <= TARGET_FRAME_TIME) logFile << " ✓";
        logFile << "\n";
        
        logFile << "  GPU Utilization: " << std::fixed << std::setprecision(1) << state.gpuUtilization << "%";
        if (state.gpuUtilization >= TARGET_GPU_UTIL) logFile << " ✓";
        logFile << "\n";
        
        logFile << "  Memory Efficiency: " << std::fixed << std::setprecision(1) << state.memoryEfficiency << "%";
        if (state.memoryEfficiency >= TARGET_MEMORY_EFF) logFile << " ✓";
        logFile << "\n";
        
        logFile << "  Throughput: " << std::fixed << std::setprecision(0) << state.cellsThroughput << " cells/sec";
        if (state.cellsThroughput >= TARGET_THROUGHPUT) logFile << " ✓";
        logFile << "\n";
        
        logFile << "  Engagement: " << std::fixed << std::setprecision(1) << state.engagement << "%";
        if (state.engagement >= TARGET_ENGAGEMENT) logFile << " ✓";
        logFile << "\n";
        
        logFile << "  Frustration: " << std::fixed << std::setprecision(1) << state.frustration << "%";
        if (state.frustration <= TARGET_FRUSTRATION) logFile << " ✓";
        logFile << "\n\n";
        
        logFile.flush();
    }
    
    void logFinalResults() {
        std::lock_guard<std::mutex> lock(stateMutex);
        
        logFile << "\n=== FINAL OPTIMIZATION RESULTS ===\n";
        logFile << "Total Steps: " << state.step << "\n";
        logFile << "Average FPS: " << std::fixed << std::setprecision(1) << (1000.0 / state.frameTime) << "\n\n";
        
        int targetsAchieved = 0;
        logFile << "Performance Targets:\n";
        
        if (state.frameTime <= TARGET_FRAME_TIME) {
            logFile << "✓ Frame Time: " << state.frameTime << "ms <= " << TARGET_FRAME_TIME << "ms\n";
            targetsAchieved++;
        } else {
            logFile << "✗ Frame Time: " << state.frameTime << "ms > " << TARGET_FRAME_TIME << "ms\n";
        }
        
        if (state.gpuUtilization >= TARGET_GPU_UTIL) {
            logFile << "✓ GPU Utilization: " << state.gpuUtilization << "% >= " << TARGET_GPU_UTIL << "%\n";
            targetsAchieved++;
        } else {
            logFile << "✗ GPU Utilization: " << state.gpuUtilization << "% < " << TARGET_GPU_UTIL << "%\n";
        }
        
        if (state.memoryEfficiency >= TARGET_MEMORY_EFF) {
            logFile << "✓ Memory Efficiency: " << state.memoryEfficiency << "% >= " << TARGET_MEMORY_EFF << "%\n";
            targetsAchieved++;
        } else {
            logFile << "✗ Memory Efficiency: " << state.memoryEfficiency << "% < " << TARGET_MEMORY_EFF << "%\n";
        }
        
        if (state.cellsThroughput >= TARGET_THROUGHPUT) {
            logFile << "✓ Throughput: " << state.cellsThroughput << " >= " << TARGET_THROUGHPUT << " cells/sec\n";
            targetsAchieved++;
        } else {
            logFile << "✗ Throughput: " << state.cellsThroughput << " < " << TARGET_THROUGHPUT << " cells/sec\n";
        }
        
        if (state.engagement >= TARGET_ENGAGEMENT) {
            logFile << "✓ Engagement: " << state.engagement << "% >= " << TARGET_ENGAGEMENT << "%\n";
            targetsAchieved++;
        } else {
            logFile << "✗ Engagement: " << state.engagement << "% < " << TARGET_ENGAGEMENT << "%\n";
        }
        
        if (state.frustration <= TARGET_FRUSTRATION) {
            logFile << "✓ Frustration: " << state.frustration << "% <= " << TARGET_FRUSTRATION << "%\n";
            targetsAchieved++;
        } else {
            logFile << "✗ Frustration: " << state.frustration << "% > " << TARGET_FRUSTRATION << "%\n";
        }
        
        double successRate = (double)targetsAchieved / 6.0 * 100.0;
        logFile << "\nOptimization Success Rate: " << std::fixed << std::setprecision(1) << successRate << "% ";
        logFile << "(" << targetsAchieved << "/6 targets achieved)\n";
        
        if (successRate >= 80.0) {
            logFile << "🎉 EXCELLENT optimization results!\n";
        } else if (successRate >= 60.0) {
            logFile << "👍 GOOD optimization results!\n";
        } else {
            logFile << "⚠️  Optimization needs improvement.\n";
        }
        
        logFile.flush();
    }
    
    std::string getCurrentTimeString() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

int main(int argc, char* argv[]) {
    try {
        int hours = 8; // Default to 8 hours
        
        if (argc > 1) {
            hours = std::stoi(argv[1]);
        }
        
        std::cout << "UX-Mirror Overnight Optimization System\n";
        std::cout << "======================================\n";
        std::cout << "Starting " << hours << " hour optimization run...\n";
        std::cout << "Press Ctrl+C to stop early\n\n";
        
        // Create optimizer with timestamped log file
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "optimization_log_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".txt";
        
        MinimalOvernightOptimizer optimizer(ss.str());
        
        std::cout << "Log file: " << ss.str() << "\n";
        std::cout << "Optimization running...\n";
        
        // Run optimization
        optimizer.runOptimization(hours);
        
        std::cout << "\nOptimization completed successfully!\n";
        std::cout << "Check the log file for detailed results.\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 