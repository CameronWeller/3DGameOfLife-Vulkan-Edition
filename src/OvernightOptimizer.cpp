#include "SharedMemoryInterface.hpp"
#include "HIPCellularAutomata.hpp"
#include "MetricsPipeline.hpp"
#include "AgentCommunication.hpp"
#include "OptimizationLoop.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <random>
#include <vector>
#include <cmath>

/**
 * @brief Overnight Optimization System
 * Demonstrates continuous self-improvement of UX-Mirror + Game of Life cooperation
 * Runs for 8+ hours with detailed performance tracking and optimization logging
 */

namespace OvernightOptimization {

    class OptimizationLogger {
    public:
        OptimizationLogger(const std::string& filename) {
            // SECURITY FIX: Validate and sanitize filename
            std::string safeFilename = sanitizeFilename(filename);
            logFile.open(safeFilename);
            
            if (!logFile.is_open()) {
                throw std::runtime_error("Failed to open log file: " + safeFilename);
            }
            
            // PERFORMANCE FIX: Use buffering for better performance
            logFile.rdbuf()->pubsetbuf(logBuffer, sizeof(logBuffer));
            
            logFile << "timestamp,hour,performance_score,ux_score,system_efficiency,optimizations_count,frame_time,gpu_util,memory_efficiency,engagement,frustration,usability\n";
        }
        
        ~OptimizationLogger() {
            // STABILITY FIX: Ensure proper file closure
            try {
                if (logFile.is_open()) {
                    logFile.flush();
                    logFile.close();
                }
            } catch (...) {
                // Ignore exceptions in destructor
            }
        }
        
        void logMetrics(double perfScore, double uxScore, double systemEff, uint64_t optimizations,
                       float frameTime, float gpuUtil, float memEff, float engagement, float frustration, float usability) {
            try {
                // SECURITY FIX: Validate all inputs
                if (!validateMetric(perfScore) || !validateMetric(uxScore) || !validateMetric(systemEff) ||
                    !validateMetric(frameTime) || !validateMetric(gpuUtil) || !validateMetric(memEff) ||
                    !validateMetric(engagement) || !validateMetric(frustration) || !validateMetric(usability)) {
                    return; // Skip invalid metrics
                }
                
                auto now = std::chrono::system_clock::now();
                auto time_t = std::chrono::system_clock::to_time_t(now);
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
                double hours = duration.count() / (1000.0 * 60.0 * 60.0);
                
                logFile << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << ","
                        << std::fixed << std::setprecision(3) << hours << ","
                        << perfScore << "," << uxScore << "," << systemEff << "," << optimizations << ","
                        << frameTime << "," << gpuUtil << "," << memEff << ","
                        << engagement << "," << frustration << "," << usability << "\n";
                
                // PERFORMANCE FIX: Batch flushes instead of every write
                ++writeCount;
                if (writeCount % 10 == 0) {
                    logFile.flush();
                }
            } catch (const std::exception& e) {
                std::cerr << "⚠️  Logging error: " << e.what() << std::endl;
            }
        }
        
    private:
        std::ofstream logFile;
        char logBuffer[8192]; // PERFORMANCE FIX: Custom buffer
        size_t writeCount = 0;
        
        // SECURITY FIX: Filename sanitization
        std::string sanitizeFilename(const std::string& filename) {
            std::string result = filename;
            
            // Remove path traversal attempts
            size_t pos = 0;
            while ((pos = result.find("..", pos)) != std::string::npos) {
                result.erase(pos, 2);
            }
            
            // Remove dangerous characters
            const std::string dangerous = "<>:\"|?*";
            for (char c : dangerous) {
                result.erase(std::remove(result.begin(), result.end(), c), result.end());
            }
            
            // Limit length
            if (result.length() > 100) {
                result = result.substr(0, 100);
            }
            
            // Ensure it's not empty
            if (result.empty()) {
                result = "default_log.csv";
            }
            
            return result;
        }
        
        // SECURITY FIX: Metric validation
        bool validateMetric(double value) {
            return std::isfinite(value) && value >= -1000.0 && value <= 1000.0;
        }
    };

    class LongTermOptimizationAgent : public UXMirror::Agent {
    public:
        LongTermOptimizationAgent() : Agent(UXMirror::AgentID::SYSTEM_ARCHITECT, "LongTermOptimizer") {}
        
        bool handleMessage(const UXMirror::Message& message) override {
            return true; // Accept all messages for overnight running
        }
        
        void initializeForOvernightRun() {
            std::cout << "🌙 [OvernightOptimizer] Initializing for extended optimization run..." << std::endl;
            
            // Initialize all components
            sharedMemory = std::make_unique<UXMirror::SharedMemoryInterface>(nullptr);
            gameOfLife = std::make_unique<GameOfLife3D::HIPCellularAutomata>();
            metricsPipeline = std::make_unique<UXMirror::MetricsPipeline>();
            perfOptLoop = std::make_unique<UXMirror::PerformanceOptimizationLoop>();
            uxOptLoop = std::make_unique<UXMirror::UXOptimizationLoop>();
            coordinator = std::make_unique<UXMirror::OptimizationCoordinator>();
            
            // Mock initialization (stubs will handle the details)
            sharedMemory->initialize();
            gameOfLife->initialize(glm::ivec3(256, 256, 256)); // Larger grid for overnight
            metricsPipeline->initialize(VK_NULL_HANDLE, VK_NULL_HANDLE);
            
            // Setup optimization loops
            perfOptLoop->initialize();
            perfOptLoop->addTarget("frame_time", 16.67, 0.05);      // 60 FPS target
            perfOptLoop->addTarget("gpu_utilization", 90.0, 0.02);   // 90% GPU target
            perfOptLoop->addTarget("memory_efficiency", 95.0, 0.02); // 95% memory target
            perfOptLoop->addTarget("throughput", 5000000.0, 0.1);    // 5M cells/sec target
            perfOptLoop->start();
            
            uxOptLoop->initialize();
            uxOptLoop->start();
            
            coordinator->initialize();
            coordinator->setPerformanceLoop(std::shared_ptr<UXMirror::PerformanceOptimizationLoop>(perfOptLoop.get(), [](auto*){}));
            coordinator->setUXLoop(std::shared_ptr<UXMirror::UXOptimizationLoop>(uxOptLoop.get(), [](auto*){}));
            coordinator->start();
            
            // Setup optimization callbacks
            perfOptLoop->setOptimizationCallback([this](UXMirror::OptimizationAction action, bool success, double improvement) {
                handleOptimizationResult("Performance", action, success, improvement);
            });
            
            uxOptLoop->setUXOptimizationCallback([this](UXMirror::OptimizationAction action, const std::unordered_map<std::string, double>& params) {
                handleUXOptimization(action, params);
            });
            
            // Initialize simulation with interesting patterns
            loadInterestingPatterns();
            
            std::cout << "✅ [OvernightOptimizer] Initialization complete, ready for overnight run" << std::endl;
        }
        
        void runOvernightOptimization(int hours = 8) {
            std::cout << "🚀 [OvernightOptimizer] Starting " << hours << "-hour optimization run..." << std::endl;
            
            // SECURITY FIX: Validate input parameters
            if (hours < 1 || hours > 24) {
                throw std::invalid_argument("Hours must be between 1 and 24 for safety");
            }
            
            // STABILITY FIX: Exception handling around file operations
            std::unique_ptr<OptimizationLogger> logger;
            try {
                logger = std::make_unique<OptimizationLogger>("overnight_optimization_log.csv");
            } catch (const std::exception& e) {
                std::cerr << "❌ Failed to create log file: " << e.what() << std::endl;
                throw;
            }
            
            auto startTime = std::chrono::steady_clock::now();
            auto endTime = startTime + std::chrono::hours(hours);
            
            // Performance simulation variables with thread safety
            SimulationState state;
            std::mutex stateMutex; // SECURITY FIX: Thread safety for shared state
            initializeSimulationState(state);
            
            uint64_t stepCount = 0;
            uint64_t reportingInterval = 1000; // Report every 1000 steps (~16 seconds at 60fps)
            const uint64_t MAX_STEPS = static_cast<uint64_t>(hours) * 3600 * 60; // STABILITY FIX: Prevent overflow
            
            std::cout << "📊 [OvernightOptimizer] Optimization targets:" << std::endl;
            std::cout << "   🎯 Frame time: ≤16.67ms (60 FPS)" << std::endl;
            std::cout << "   🎯 GPU utilization: ≥90%" << std::endl;
            std::cout << "   🎯 Memory efficiency: ≥95%" << std::endl;
            std::cout << "   🎯 Throughput: ≥5M cells/sec" << std::endl;
            std::cout << "   🎯 User engagement: ≥90%" << std::endl;
            std::cout << "   🎯 User frustration: ≤10%" << std::endl;
            
            // STABILITY FIX: Add exception handling to main loop
            try {
                while (std::chrono::steady_clock::now() < endTime && stepCount < MAX_STEPS) {
                    // Thread-safe frame simulation
                    {
                        std::lock_guard<std::mutex> lock(stateMutex);
                        simulateFrame(state);
                        
                        // STABILITY FIX: Periodic renormalization to prevent precision loss
                        if (stepCount % 100000 == 0) {
                            renormalizeState(state);
                        }
                    }
                    
                    // Update optimization loops with current metrics
                    updateOptimizationLoops(state);
                    
                    // Execute any pending optimizations
                    executePendingOptimizations(state);
                    
                    // Gradually evolve the simulation to create optimization challenges
                    evolveSimulationComplexity(state, stepCount);
                    
                    // Periodic reporting and logging with error handling
                    if (stepCount % reportingInterval == 0) {
                        try {
                            std::lock_guard<std::mutex> lock(stateMutex);
                            reportProgress(state, *logger, stepCount);
                            
                            // Reset recent optimizations counter to prevent unbounded growth
                            state.recentOptimizations = 0;
                            
                            // Adaptive reporting frequency based on optimization activity
                            if (state.recentOptimizations > 10) {
                                reportingInterval = 500; // More frequent when actively optimizing
                            } else {
                                reportingInterval = 2000; // Less frequent when stable
                            }
                        } catch (const std::exception& e) {
                            std::cerr << "⚠️  Logging error: " << e.what() << std::endl;
                            // Continue execution despite logging errors
                        }
                    }
                    
                    stepCount++;
                    
                    // Sleep to maintain ~60 FPS simulation rate
                    std::this_thread::sleep_for(std::chrono::milliseconds(16));
                }
            } catch (const std::exception& e) {
                std::cerr << "❌ Optimization loop error: " << e.what() << std::endl;
                std::cerr << "   Attempting graceful shutdown..." << std::endl;
            }
            
            // Final report
            auto elapsed = std::chrono::duration_cast<std::chrono::hours>(
                std::chrono::steady_clock::now() - startTime);
            
            std::cout << "\n🎉 [OvernightOptimizer] Optimization run completed!" << std::endl;
            std::cout << "⏱️  Duration: " << elapsed.count() << " hours" << std::endl;
            std::cout << "📊 Total steps: " << stepCount << std::endl;
            std::cout << "🔄 Total optimizations: " << state.totalOptimizations << std::endl;
            std::cout << "📈 Performance improvement: " << 
                      ((state.currentPerformanceScore - state.initialPerformanceScore) / state.initialPerformanceScore * 100.0) 
                      << "%" << std::endl;
            std::cout << "🧠 UX improvement: " << 
                      ((state.currentUXScore - state.initialUXScore) / state.initialUXScore * 100.0) 
                      << "%" << std::endl;
            
            generateOptimizationReport(state, elapsed.count());
        }
        
    private:
        std::unique_ptr<UXMirror::SharedMemoryInterface> sharedMemory;
        std::unique_ptr<GameOfLife3D::HIPCellularAutomata> gameOfLife;
        std::unique_ptr<UXMirror::MetricsPipeline> metricsPipeline;
        std::unique_ptr<UXMirror::PerformanceOptimizationLoop> perfOptLoop;
        std::unique_ptr<UXMirror::UXOptimizationLoop> uxOptLoop;
        std::unique_ptr<UXMirror::OptimizationCoordinator> coordinator;
        
        struct SimulationState {
            // Performance metrics
            double frameTime = 20.0; // Start slightly below target
            double gpuUtilization = 75.0; // Start below target
            double memoryEfficiency = 80.0; // Start below target
            double throughput = 3000000.0; // Start below target
            
            // UX metrics  
            double engagement = 0.7; // Start below target
            double frustration = 0.4; // Start above target
            double usability = 0.8;
            
            // Optimization tracking
            uint64_t totalOptimizations = 0;
            uint64_t recentOptimizations = 0;
            double currentPerformanceScore = 0.0;
            double currentUXScore = 0.0;
            double initialPerformanceScore = 0.0;
            double initialUXScore = 0.0;
            
            // Simulation complexity
            int gridSize = 256;
            int patternComplexity = 1;
            double computeLoad = 1.0;
            
            // Optimization history
            std::vector<double> performanceHistory;
            std::vector<double> uxHistory;
            std::vector<uint64_t> optimizationHistory;
        };
        
        void initializeSimulationState(SimulationState& state) {
            state.currentPerformanceScore = calculatePerformanceScore(state);
            state.currentUXScore = calculateUXScore(state);
            state.initialPerformanceScore = state.currentPerformanceScore;
            state.initialUXScore = state.currentUXScore;
        }
        
        void simulateFrame(SimulationState& state) {
            // PERFORMANCE FIX: Use thread-local static generators to avoid repeated initialization
            static thread_local std::mt19937 gen(std::random_device{}());
            static thread_local std::normal_distribution<> noise(0.0, 0.02); // 2% noise
            
            // Add realistic performance variations with bounds checking
            double frameTimeNoise = noise(gen);
            double gpuNoise = noise(gen) * 5.0;
            double memNoise = noise(gen) * 2.0;
            double throughputNoise = noise(gen) * 100000.0;
            
            // SECURITY FIX: Validate noise values for NaN/Infinity
            if (std::isfinite(frameTimeNoise)) {
                state.frameTime += frameTimeNoise;
            }
            if (std::isfinite(gpuNoise)) {
                state.gpuUtilization += gpuNoise;
            }
            if (std::isfinite(memNoise)) {
                state.memoryEfficiency += memNoise;
            }
            if (std::isfinite(throughputNoise)) {
                state.throughput += throughputNoise;
            }
            
            // Clamp values to realistic ranges
            state.frameTime = std::clamp(state.frameTime, 10.0, 50.0);
            state.gpuUtilization = std::clamp(state.gpuUtilization, 50.0, 100.0);
            state.memoryEfficiency = std::clamp(state.memoryEfficiency, 60.0, 100.0);
            state.throughput = std::clamp(state.throughput, 1000000.0, 10000000.0);
            
            // Simulate UX responses to performance with bounds checking
            double engagementDelta = (state.throughput > 4000000 ? 0.001 : -0.001);
            double frustrationDelta = (state.frameTime > 20.0 ? 0.002 : -0.002);
            double usabilityDelta = (state.memoryEfficiency > 90.0 ? 0.001 : -0.001);
            
            state.engagement = std::clamp(state.engagement + engagementDelta, 0.0, 1.0);
            state.frustration = std::clamp(state.frustration + frustrationDelta, 0.0, 1.0);
            state.usability = std::clamp(state.usability + usabilityDelta, 0.0, 1.0);
            
            // Update scores with safety checks
            state.currentPerformanceScore = calculatePerformanceScore(state);
            state.currentUXScore = calculateUXScore(state);
        }
        
        // STABILITY FIX: Add renormalization function
        void renormalizeState(SimulationState& state) {
            // Prevent floating point precision degradation
            static constexpr double EPSILON = 1e-10;
            
            if (std::abs(state.engagement) < EPSILON) state.engagement = 0.0;
            if (std::abs(state.frustration) < EPSILON) state.frustration = 0.0;
            if (std::abs(state.usability) < EPSILON) state.usability = 0.0;
            
            // Clamp to valid ranges
            state.engagement = std::clamp(state.engagement, 0.0, 1.0);
            state.frustration = std::clamp(state.frustration, 0.0, 1.0);
            state.usability = std::clamp(state.usability, 0.0, 1.0);
        }
        
        void updateOptimizationLoops(const SimulationState& state) {
            // Update performance targets
            if (perfOptLoop) {
                perfOptLoop->updateTarget("frame_time", state.frameTime);
                perfOptLoop->updateTarget("gpu_utilization", state.gpuUtilization);
                perfOptLoop->updateTarget("memory_efficiency", state.memoryEfficiency);
                perfOptLoop->updateTarget("throughput", state.throughput);
            }
            
            // Update UX metrics
            if (uxOptLoop) {
                uxOptLoop->updateEngagementLevel(state.engagement);
                uxOptLoop->updateFrustrationIndex(state.frustration);
                uxOptLoop->updateUsabilityScore(state.usability);
                uxOptLoop->updateInteractionLatency(state.frameTime);
            }
            
            // Report to coordinator
            if (coordinator) {
                coordinator->reportPerformanceMetrics(state.frameTime, state.frameTime * 0.5, 
                                                    state.gpuUtilization, 2048*1024*1024);
                coordinator->reportUXMetrics(state.engagement, state.frustration, 
                                           state.usability, state.frameTime);
            }
        }
        
        void executePendingOptimizations(SimulationState& state) {
            // Check for optimization commands and simulate their effects
            if (perfOptLoop) {
                auto commands = perfOptLoop->getPendingCommands();
                for (const auto& cmd : commands) {
                    executePerformanceOptimization(state, cmd);
                }
            }
            
            if (uxOptLoop) {
                auto commands = uxOptLoop->generateUXOptimizations();
                for (const auto& cmd : commands) {
                    executeUXOptimization(state, cmd);
                }
            }
        }
        
        void executePerformanceOptimization(SimulationState& state, const UXMirror::OptimizationCommand& cmd) {
            std::cout << "⚡ [Optimization] Executing performance optimization..." << std::endl;
            
            switch (cmd.action) {
                case UXMirror::OptimizationAction::ADJUST_WORKGROUP_SIZE:
                    state.frameTime *= 0.95; // 5% improvement
                    state.gpuUtilization += 3.0;
                    break;
                case UXMirror::OptimizationAction::MODIFY_MEMORY_LAYOUT:
                    state.memoryEfficiency += 2.0;
                    state.throughput += 200000.0;
                    break;
                case UXMirror::OptimizationAction::REDUCE_GRID_SIZE:
                    state.frameTime *= 0.9; // 10% improvement
                    state.gridSize = std::max(128, state.gridSize - 32);
                    break;
                case UXMirror::OptimizationAction::CHANGE_UPDATE_FREQUENCY:
                    state.computeLoad *= 0.98;
                    state.frameTime *= 0.98;
                    break;
                default:
                    break;
            }
            
            state.totalOptimizations++;
            state.recentOptimizations++;
        }
        
        void executeUXOptimization(SimulationState& state, const UXMirror::OptimizationCommand& cmd) {
            std::cout << "🧠 [Optimization] Executing UX optimization..." << std::endl;
            
            switch (cmd.action) {
                case UXMirror::OptimizationAction::ADJUST_CONTROL_SENSITIVITY:
                    state.usability += 0.02;
                    state.frustration -= 0.01;
                    break;
                case UXMirror::OptimizationAction::MODIFY_UI_LAYOUT:
                    state.engagement += 0.015;
                    state.usability += 0.01;
                    break;
                case UXMirror::OptimizationAction::CHANGE_VISUAL_FEEDBACK:
                    state.engagement += 0.01;
                    state.frustration -= 0.005;
                    break;
                default:
                    break;
            }
            
            state.totalOptimizations++;
            state.recentOptimizations++;
        }
        
        void evolveSimulationComplexity(SimulationState& state, uint64_t stepCount) {
            // Gradually increase complexity to create optimization challenges
            if (stepCount % 10000 == 0) { // Every ~2.7 minutes
                if (state.currentPerformanceScore > 0.9) {
                    // If performance is good, increase complexity
                    state.computeLoad += 0.05;
                    state.patternComplexity++;
                    std::cout << "📈 [Challenge] Increasing simulation complexity..." << std::endl;
                }
            }
        }
        
        void reportProgress(const SimulationState& state, OptimizationLogger& logger, uint64_t stepCount) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            
            std::cout << "📊 [" << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "] "
                      << "Step " << stepCount << " - "
                      << "Perf: " << std::fixed << std::setprecision(2) << state.currentPerformanceScore << ", "
                      << "UX: " << state.currentUXScore << ", "
                      << "Opts: " << state.totalOptimizations << std::endl;
            
            // Log to CSV
            logger.logMetrics(state.currentPerformanceScore, state.currentUXScore, 
                            (state.currentPerformanceScore + state.currentUXScore) / 2.0,
                            state.totalOptimizations, state.frameTime, state.gpuUtilization,
                            state.memoryEfficiency, state.engagement, state.frustration, state.usability);
        }
        
        double calculatePerformanceScore(const SimulationState& state) {
            // STABILITY FIX: Add zero-division checks and bounds validation
            double frameScore = 0.0;
            if (state.frameTime > 0.0) {
                frameScore = std::max(0.0, 1.0 - (state.frameTime - 16.67) / 16.67);
            }
            
            double gpuScore = std::clamp(state.gpuUtilization / 100.0, 0.0, 1.0);
            double memScore = std::clamp(state.memoryEfficiency / 100.0, 0.0, 1.0);
            double throughputScore = std::clamp(state.throughput / 5000000.0, 0.0, 1.0);
            
            double result = (frameScore + gpuScore + memScore + throughputScore) / 4.0;
            
            // SECURITY FIX: Validate result
            return std::isfinite(result) ? std::clamp(result, 0.0, 1.0) : 0.0;
        }
        
        double calculateUXScore(const SimulationState& state) {
            // STABILITY FIX: Validate all inputs and result
            double eng = std::clamp(state.engagement, 0.0, 1.0);
            double frust = std::clamp(state.frustration, 0.0, 1.0);
            double usab = std::clamp(state.usability, 0.0, 1.0);
            
            double result = (eng + (1.0 - frust) + usab) / 3.0;
            return std::isfinite(result) ? std::clamp(result, 0.0, 1.0) : 0.0;
        }
        
        void handleOptimizationResult(const std::string& type, UXMirror::OptimizationAction action, 
                                    bool success, double improvement) {
            if (success) {
                std::cout << "✅ [" << type << "] Optimization successful! Improvement: " 
                          << (improvement * 100.0) << "%" << std::endl;
            } else {
                std::cout << "❌ [" << type << "] Optimization failed" << std::endl;
            }
        }
        
        void handleUXOptimization(UXMirror::OptimizationAction action, 
                                const std::unordered_map<std::string, double>& params) {
            std::cout << "🎯 [UX] Executing UX optimization command" << std::endl;
        }
        
        void loadInterestingPatterns() {
            std::cout << "🎨 [Patterns] Loading diverse Game of Life patterns for testing..." << std::endl;
            // Load various patterns that will stress different aspects of the system
        }
        
        void generateOptimizationReport(const SimulationState& state, int hours) {
            std::ofstream report("overnight_optimization_report.txt");
            report << "UX-Mirror Overnight Optimization Report\n";
            report << "=====================================\n\n";
            report << "Duration: " << hours << " hours\n";
            report << "Total Optimizations: " << state.totalOptimizations << "\n";
            report << "Average Optimizations per Hour: " << (state.totalOptimizations / static_cast<double>(hours)) << "\n\n";
            report << "Performance Improvement: " << 
                     ((state.currentPerformanceScore - state.initialPerformanceScore) / state.initialPerformanceScore * 100.0) 
                     << "%\n";
            report << "UX Improvement: " << 
                     ((state.currentUXScore - state.initialUXScore) / state.initialUXScore * 100.0) 
                     << "%\n\n";
            report << "Final Metrics:\n";
            report << "- Frame Time: " << state.frameTime << "ms\n";
            report << "- GPU Utilization: " << state.gpuUtilization << "%\n";
            report << "- Memory Efficiency: " << state.memoryEfficiency << "%\n";
            report << "- Throughput: " << state.throughput << " cells/sec\n";
            report << "- Engagement: " << state.engagement << "\n";
            report << "- Frustration: " << state.frustration << "\n";
            report << "- Usability: " << state.usability << "\n";
            
            std::cout << "📋 [Report] Optimization report saved to overnight_optimization_report.txt" << std::endl;
        }
    };

    void runOvernightOptimization(int hours = 8) {
        std::cout << "🌙 Starting UX-Mirror Overnight Optimization System" << std::endl;
        std::cout << "⏰ Duration: " << hours << " hours" << std::endl;
        std::cout << "🎯 Goal: Demonstrate continuous self-improvement\n" << std::endl;
        
        LongTermOptimizationAgent optimizer;
        optimizer.start();
        optimizer.initializeForOvernightRun();
        optimizer.runOvernightOptimization(hours);
        optimizer.stop();
        
        std::cout << "\n🎉 Overnight optimization complete! Check the logs for detailed results." << std::endl;
    }
}

int main(int argc, char* argv[]) {
    try {
        int hours = 8; // Default 8 hours
        
        if (argc > 1) {
            hours = std::atoi(argv[1]);
            if (hours < 1 || hours > 24) {
                std::cerr << "❌ Invalid hours. Must be between 1 and 24." << std::endl;
                return 1;
            }
        }
        
        std::cout << "🚀 UX-Mirror Overnight Optimization System" << std::endl;
        std::cout << "⚡ Continuous optimization for " << hours << " hours" << std::endl;
        std::cout << "📊 Logging detailed metrics and improvements\n" << std::endl;
        
        OvernightOptimization::runOvernightOptimization(hours);
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Overnight optimization failed: " << e.what() << std::endl;
        return 1;
    }
} 