#pragma once

#include <memory>
#include <vector>
#include <atomic>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <queue>

namespace UXMirror {

    /**
     * @brief Fundamental Optimization Loop System
     * Implements continuous feedback between UX-Mirror and Game of Life
     * Core prototype functionality for real-time adaptation
     */

    // Optimization targets and their current values
    struct OptimizationTarget {
        std::string name;
        double currentValue;
        double targetValue;
        double tolerance;
        double improvementRate;
        std::chrono::steady_clock::time_point lastUpdate;
        bool isImproving;
        
        OptimizationTarget(const std::string& n, double target, double tol = 0.05) 
            : name(n), currentValue(0.0), targetValue(target), tolerance(tol), 
              improvementRate(0.0), lastUpdate(std::chrono::steady_clock::now()), isImproving(false) {}
    };

    // Optimization strategy types
    enum class OptimizationStrategy {
        PERFORMANCE_FIRST,      // Prioritize frame rate and compute performance
        UX_FIRST,              // Prioritize user experience metrics
        BALANCED,              // Balance performance and UX
        ADAPTIVE              // Dynamically switch based on conditions
    };

    // Optimization action types
    enum class OptimizationAction {
        // Performance optimizations
        REDUCE_GRID_SIZE,
        INCREASE_GRID_SIZE,
        ADJUST_WORKGROUP_SIZE,
        MODIFY_MEMORY_LAYOUT,
        CHANGE_UPDATE_FREQUENCY,
        
        // UX optimizations  
        ADJUST_CONTROL_SENSITIVITY,
        MODIFY_UI_LAYOUT,
        CHANGE_VISUAL_FEEDBACK,
        UPDATE_INTERACTION_ZONES,
        ADJUST_CAMERA_SMOOTHING,
        
        // Hybrid optimizations
        REDISTRIBUTE_COMPUTE_LOAD,
        REBALANCE_PRIORITY_QUEUES,
        OPTIMIZE_DATA_FLOW
    };

    // Optimization command with parameters
    struct OptimizationCommand {
        OptimizationAction action;
        std::unordered_map<std::string, double> parameters;
        double expectedImprovement;
        uint32_t priority;
        std::chrono::steady_clock::time_point timestamp;
        
        OptimizationCommand(OptimizationAction act, double improvement = 0.0, uint32_t pri = 1)
            : action(act), expectedImprovement(improvement), priority(pri), 
              timestamp(std::chrono::steady_clock::now()) {}
    };

    /**
     * @brief Performance Optimization Loop
     * Continuously monitors and optimizes GPU/compute performance
     */
    class PerformanceOptimizationLoop {
    public:
        PerformanceOptimizationLoop();
        ~PerformanceOptimizationLoop();
        
        // Core loop management
        bool initialize();
        void start();
        void stop();
        bool isRunning() const { return running; }
        
        // Target management
        void addTarget(const std::string& name, double targetValue, double tolerance = 0.05);
        void updateTarget(const std::string& name, double currentValue);
        void removeTarget(const std::string& name);
        
        // Optimization control
        void setStrategy(OptimizationStrategy strategy) { currentStrategy = strategy; }
        OptimizationStrategy getStrategy() const { return currentStrategy; }
        
        // Metrics and feedback
        struct LoopMetrics {
            std::atomic<uint64_t> optimizationsConducted{0};
            std::atomic<uint64_t> successfulOptimizations{0};
            std::atomic<double> averageImprovement{0.0};
            std::atomic<uint64_t> totalOptimizationTime{0};
            std::atomic<double> currentPerformanceScore{0.0};
        };
        
        const LoopMetrics& getMetrics() const { return metrics; }
        void resetMetrics();
        
        // Command injection for external control
        void injectOptimizationCommand(const OptimizationCommand& command);
        std::vector<OptimizationCommand> getPendingCommands();
        
        // Callback for optimization results
        using OptimizationCallback = std::function<void(OptimizationAction, bool, double)>;
        void setOptimizationCallback(OptimizationCallback callback) { optimizationCallback = callback; }

    private:
        std::atomic<bool> running{false};
        std::thread optimizationThread;
        OptimizationStrategy currentStrategy{OptimizationStrategy::BALANCED};
        
        // Targets and tracking
        std::mutex targetMutex;
        std::unordered_map<std::string, OptimizationTarget> targets;
        
        // Command queue
        std::mutex commandMutex;
        std::vector<OptimizationCommand> pendingCommands;
        
        // Metrics
        LoopMetrics metrics;
        OptimizationCallback optimizationCallback;
        
        // Internal methods
        void optimizationLoopFunction();
        void analyzePerformanceTargets();
        OptimizationCommand generateOptimizationCommand();
        bool executeOptimizationCommand(const OptimizationCommand& command);
        void updateMetrics(OptimizationAction action, bool success, double improvement);
        
        // Strategy-specific optimization logic
        OptimizationCommand generatePerformanceFirstCommand();
        OptimizationCommand generateBalancedCommand();
        OptimizationCommand generateAdaptiveCommand();
    };

    /**
     * @brief UX Optimization Loop  
     * Continuously monitors and optimizes user experience
     */
    class UXOptimizationLoop {
    public:
        UXOptimizationLoop();
        ~UXOptimizationLoop();
        
        // Core loop management
        bool initialize();
        void start();
        void stop();
        bool isRunning() const { return running; }
        
        // UX metrics input
        void updateEngagementLevel(double level);
        void updateFrustrationIndex(double index);
        void updateUsabilityScore(double score);
        void updateInteractionLatency(double latencyMs);
        void updateVisualConsistency(double consistency);
        
        // Optimization results
        struct UXMetrics {
            std::atomic<double> engagementLevel{0.0};
            std::atomic<double> frustrationIndex{0.0};
            std::atomic<double> usabilityScore{0.0};
            std::atomic<double> interactionLatency{0.0};
            std::atomic<double> visualConsistency{0.0};
            std::atomic<double> overallUXScore{0.0};
        };
        
        const UXMetrics& getUXMetrics() const { return uxMetrics; }
        
        // Command generation for UX improvements
        std::vector<OptimizationCommand> generateUXOptimizations();
        
        // Callback for UX changes
        using UXOptimizationCallback = std::function<void(OptimizationAction, const std::unordered_map<std::string, double>&)>;
        void setUXOptimizationCallback(UXOptimizationCallback callback) { uxCallback = callback; }

    private:
        std::atomic<bool> running{false};
        std::thread uxOptimizationThread;
        
        // UX tracking
        UXMetrics uxMetrics;
        
        // Historical data for trend analysis
        struct UXHistory {
            std::vector<double> engagementHistory;
            std::vector<double> frustrationHistory;
            std::vector<double> usabilityHistory;
            std::chrono::steady_clock::time_point lastUpdate;
        } history;
        
        std::mutex historyMutex;
        UXOptimizationCallback uxCallback;
        
        // Internal methods
        void uxOptimizationLoopFunction();
        void updateUXHistory();
        double calculateUXTrend(const std::vector<double>& data);
        OptimizationCommand generateUXCommand();
        void analyzeInteractionPatterns();
    };

    /**
     * @brief Master Optimization Coordinator
     * Coordinates between performance and UX optimization loops
     * Resolves conflicts and prioritizes optimizations
     */
    class OptimizationCoordinator {
    public:
        OptimizationCoordinator();
        ~OptimizationCoordinator();
        
        // Initialization and lifecycle
        bool initialize();
        void start();
        void stop();
        bool isRunning() const { return running; }
        
        // Loop management
        void setPerformanceLoop(std::shared_ptr<PerformanceOptimizationLoop> perfLoop);
        void setUXLoop(std::shared_ptr<UXOptimizationLoop> uxLoop);
        
        // Coordination strategy
        void setCoordinationStrategy(OptimizationStrategy strategy);
        
        // Real-time metrics input (from agents)
        void reportPerformanceMetrics(float frameTime, float computeTime, float gpuUtil, uint64_t memUsage);
        void reportUXMetrics(float engagement, float frustration, float usability, float latency);
        void reportSimulationMetrics(uint64_t cellsProcessed, float memoryEfficiency);
        
        // Optimization execution
        bool executeOptimization(const OptimizationCommand& command);
        std::vector<OptimizationCommand> getOptimizationQueue();
        
        // Results and metrics
        struct CoordinatorMetrics {
            std::atomic<uint64_t> totalOptimizations{0};
            std::atomic<uint64_t> conflictResolutions{0};
            std::atomic<double> systemEfficiencyScore{0.0};
            std::atomic<double> optimizationRate{0.0}; // optimizations per second
        };
        
        const CoordinatorMetrics& getCoordinatorMetrics() const { return coordinatorMetrics; }
        
        // Integration callbacks (for agents to implement optimizations)
        using PerformanceOptimizationExecutor = std::function<bool(OptimizationAction, const std::unordered_map<std::string, double>&)>;
        using UXOptimizationExecutor = std::function<bool(OptimizationAction, const std::unordered_map<std::string, double>&)>;
        
        void setPerformanceExecutor(PerformanceOptimizationExecutor executor) { perfExecutor = executor; }
        void setUXExecutor(UXOptimizationExecutor executor) { uxExecutor = executor; }

    private:
        std::atomic<bool> running{false};
        std::thread coordinationThread;
        OptimizationStrategy coordinationStrategy{OptimizationStrategy::BALANCED};
        
        // Connected loops
        std::shared_ptr<PerformanceOptimizationLoop> performanceLoop;
        std::shared_ptr<UXOptimizationLoop> uxLoop;
        
        // Coordination queue
        std::mutex queueMutex;
        std::vector<OptimizationCommand> optimizationQueue;
        
        // Metrics and tracking
        CoordinatorMetrics coordinatorMetrics;
        
        // Executors (implemented by agents)
        PerformanceOptimizationExecutor perfExecutor;
        UXOptimizationExecutor uxExecutor;
        
        // Internal coordination logic
        void coordinationLoopFunction();
        void processOptimizationQueue();
        bool resolveOptimizationConflicts(std::vector<OptimizationCommand>& commands);
        void prioritizeOptimizations(std::vector<OptimizationCommand>& commands);
        double calculateSystemEfficiency();
        
        // Conflict resolution strategies
        bool areConflicting(const OptimizationCommand& cmd1, const OptimizationCommand& cmd2);
        OptimizationCommand resolveConflict(const OptimizationCommand& cmd1, const OptimizationCommand& cmd2);
    };

    /**
     * @brief Utility functions for optimization loop management
     */
    namespace OptimizationUtils {
        // Performance calculation helpers
        double calculatePerformanceScore(float frameTime, float computeTime, float gpuUtil);
        double calculateUXScore(float engagement, float frustration, float usability);
        double calculateSystemEfficiency(double perfScore, double uxScore);
        
        // Optimization parameter helpers
        std::unordered_map<std::string, double> generateGridSizeParams(int currentSize, bool increase);
        std::unordered_map<std::string, double> generateWorkgroupParams(int currentX, int currentY, int currentZ);
        std::unordered_map<std::string, double> generateSensitivityParams(float currentSensitivity, double adjustment);
        
        // Validation helpers
        bool validateOptimizationCommand(const OptimizationCommand& command);
        bool isPerformanceAction(OptimizationAction action);
        bool isUXAction(OptimizationAction action);
        bool isHybridAction(OptimizationAction action);
    }

} // namespace UXMirror 