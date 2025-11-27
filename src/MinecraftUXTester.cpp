#include "MinecraftUXTesting.h"
#include <iostream>
#include <memory>

using namespace UXTesting;

int main(int argc, char* argv[]) {
    std::cout << "Minecraft UX Testing Framework" << std::endl;
    std::cout << "==============================" << std::endl;
    
    // Check command line arguments
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <test_type> [options]" << std::endl;
        std::cout << "Test types:" << std::endl;
        std::cout << "  basic     - Basic functionality tests" << std::endl;
        std::cout << "  performance - Performance tests" << std::endl;
        std::cout << "  compatibility - Compatibility tests" << std::endl;
        std::cout << "  accessibility - Accessibility tests" << std::endl;
        std::cout << "  all       - Run all tests" << std::endl;
        return 1;
    }
    
    // Create Minecraft configuration
    MinecraftConfig config;
    
    // Try to detect Minecraft installation
    // Note: In a real implementation, this would search common installation paths
    config.launcherPath = "C:\\Program Files (x86)\\Minecraft Launcher\\MinecraftLauncher.exe";
    config.username = "testuser";
    config.version = "latest";
    config.worldName = "UXTestWorld";
    config.isBedrock = false;
    config.windowWidth = 1920;
    config.windowHeight = 1080;
    config.fullscreen = false;
    
    // Create testing manager
    MinecraftUXTestingManager manager;
    manager.setMinecraftConfig(config);
    manager.setTestWorldPath("./test_worlds");
    manager.setBackupWorldPath("./backup_worlds");
    
    std::string testType = argv[1];
    std::vector<TestResult> results;
    
    try {
        if (testType == "basic") {
            std::cout << "Running basic functionality tests..." << std::endl;
            manager.createBasicTestSuite();
            results = manager.runBasicTests();
        }
        else if (testType == "performance") {
            std::cout << "Running performance tests..." << std::endl;
            manager.createPerformanceTestSuite();
            results = manager.runPerformanceTests();
        }
        else if (testType == "compatibility") {
            std::cout << "Running compatibility tests..." << std::endl;
            manager.createCompatibilityTestSuite();
            results = manager.runCompatibilityTests();
        }
        else if (testType == "accessibility") {
            std::cout << "Running accessibility tests..." << std::endl;
            manager.createAccessibilityTestSuite();
            results = manager.runAccessibilityTests();
        }
        else if (testType == "all") {
            std::cout << "Running all tests..." << std::endl;
            manager.createBasicTestSuite();
            manager.createPerformanceTestSuite();
            manager.createCompatibilityTestSuite();
            manager.createAccessibilityTestSuite();
            results = manager.runAllTests();
        }
        else {
            std::cerr << "Unknown test type: " << testType << std::endl;
            return 1;
        }
        
        // Display results
        std::cout << "\nTest Results Summary:" << std::endl;
        std::cout << "=====================" << std::endl;
        
        int passed = 0;
        int failed = 0;
        
        for (const auto& result : results) {
            if (result.success) {
                passed++;
                std::cout << "✓ PASS: " << result.message << " (" << result.duration.count() << "ms)" << std::endl;
            } else {
                failed++;
                std::cout << "✗ FAIL: " << result.message << " (" << result.duration.count() << "ms)" << std::endl;
            }
        }
        
        std::cout << "\nSummary: " << passed << " passed, " << failed << " failed" << std::endl;
        
        // Generate reports
        if (!results.empty()) {
            manager.generateTestReport("./test_reports");
            if (testType == "performance" || testType == "all") {
                manager.generatePerformanceReport("./test_reports");
            }
        }
        
        return failed > 0 ? 1 : 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error during testing: " << e.what() << std::endl;
        return 1;
    }
} 