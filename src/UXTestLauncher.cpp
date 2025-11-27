#include "UXTestingFramework.h"
#include "WindowsUXTesting.h"
#include <iostream>
#include <memory>

using namespace UXTesting;

int main(int argc, char* argv[]) {
    std::cout << "UX Testing Framework Launcher" << std::endl;
    std::cout << "=============================" << std::endl;
    
    // Create and initialize framework
    UXTestingFramework framework;
    
    if (!framework.initialize()) {
        std::cerr << "Failed to initialize UX Testing Framework" << std::endl;
        return 1;
    }
    
    // Configure framework
    framework.setOutputDirectory("./test_results");
    framework.setLogLevel(2);
    framework.enableScreenshots(true);
    
    // Create a simple test scenario
    auto scenario = std::make_shared<TestScenario>("Notepad Test");
    
    // Configure application
    AppConfig config;
    config.executablePath = "notepad.exe";
    config.windowTitle = "Untitled - Notepad";
    config.launchTimeoutMs = 10000;
    config.stabilizationDelayMs = 2000;
    scenario->setAppConfig(config);
    
    // Add input events
    InputEvent event1;
    event1.type = InputType::WAIT;
    event1.durationMs = 1000;
    event1.description = "Wait for Notepad to load";
    scenario->addInputEvent(event1);
    
    InputEvent event2;
    event2.type = InputType::TEXT_INPUT;
    event2.text = "Hello, UX Testing Framework!";
    event2.description = "Type test text";
    scenario->addInputEvent(event2);
    
    InputEvent event3;
    event3.type = InputType::WAIT;
    event3.durationMs = 500;
    event3.description = "Wait after typing";
    scenario->addInputEvent(event3);
    
    InputEvent event4;
    event4.type = InputType::KEY_COMBINATION;
    event4.key = "CTRL+S";
    event4.description = "Save file";
    scenario->addInputEvent(event4);
    
    // Add scenario to framework
    framework.addScenario(scenario);
    
    // Run the scenario
    std::cout << "Running scenario: " << scenario->getName() << std::endl;
    TestResult result = framework.runScenario(scenario->getName());
    
    // Display results
    std::cout << "\nTest Results:" << std::endl;
    std::cout << "Success: " << (result.success ? "PASS" : "FAIL") << std::endl;
    std::cout << "Message: " << result.message << std::endl;
    std::cout << "Duration: " << result.duration.count() << "ms" << std::endl;
    if (!result.screenshotPath.empty()) {
        std::cout << "Screenshot: " << result.screenshotPath << std::endl;
    }
    
    // Cleanup
    framework.shutdown();
    
    std::cout << "\nTest completed. Press Enter to exit..." << std::endl;
    std::cin.get();
    
    return result.success ? 0 : 1;
} 