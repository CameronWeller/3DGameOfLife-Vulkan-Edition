#include "UXTestingFramework.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace UXTesting {

// TestScenario implementation
TestScenario::TestScenario(const std::string& name) : name_(name) {}

void TestScenario::setAppConfig(const AppConfig& config) {
    appConfig_ = config;
}

void TestScenario::addInputEvent(const InputEvent& event) {
    inputEvents_.push_back(event);
}

void TestScenario::addScreenRegion(const ScreenRegion& region) {
    screenRegions_.push_back(region);
}

void TestScenario::setPrecondition(std::function<bool()> condition) {
    precondition_ = condition;
}

void TestScenario::setPostcondition(std::function<bool()> condition) {
    postcondition_ = condition;
}

TestResult TestScenario::execute() {
    TestResult result;
    result.success = false;
    result.duration = std::chrono::milliseconds(0);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        // Check precondition
        if (precondition_ && !precondition_()) {
            result.message = "Precondition failed for scenario: " + name_;
            return result;
        }
        
        // Execute input events
        for (const auto& event : inputEvents_) {
            if (shouldStop_) {
                result.message = "Scenario stopped by user: " + name_;
                return result;
            }
            
            // TODO: Execute input event through framework
            std::cout << "Executing: " << event.description << std::endl;
            
            // Simulate execution time
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Check postcondition
        if (postcondition_ && !postcondition_()) {
            result.message = "Postcondition failed for scenario: " + name_;
            return result;
        }
        
        result.success = true;
        result.message = "Scenario completed successfully: " + name_;
        
    } catch (const std::exception& e) {
        result.message = "Exception in scenario " + name_ + ": " + e.what();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    return result;
}

void TestScenario::stop() {
    shouldStop_ = true;
}

// UXTestingFramework implementation
UXTestingFramework::UXTestingFramework() 
    : outputDirectory_("./test_output")
    , logLevel_(1)
    , enableScreenshots_(true)
    , initialized_(false) {
}

UXTestingFramework::~UXTestingFramework() {
    shutdown();
}

bool UXTestingFramework::initialize() {
    if (initialized_) return true;
    
    try {
        // Create output directory
        std::filesystem::create_directories(outputDirectory_);
        
        // TODO: Initialize platform-specific components
        // launcher_ = std::make_unique<WindowsApplicationLauncher>();
        // inputSimulator_ = std::make_unique<WindowsInputSimulator>();
        // screenCapture_ = std::make_unique<WindowsScreenCapture>();
        // imageAnalyzer_ = std::make_unique<OpenCVImageAnalyzer>();
        
        initialized_ = true;
        std::cout << "UX Testing Framework initialized successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize UX Testing Framework: " << e.what() << std::endl;
        return false;
    }
}

void UXTestingFramework::shutdown() {
    if (!initialized_) return;
    
    // Terminate any running applications
    for (const auto& scenario : scenarios_) {
        if (scenario->getAppConfig().windowTitle.empty()) continue;
        // TODO: launcher_->terminate(scenario->getAppConfig().windowTitle);
    }
    
    initialized_ = false;
    std::cout << "UX Testing Framework shutdown complete" << std::endl;
}

void UXTestingFramework::addScenario(std::shared_ptr<TestScenario> scenario) {
    std::lock_guard<std::mutex> lock(frameworkMutex_);
    scenarios_.push_back(scenario);
}

void UXTestingFramework::removeScenario(const std::string& name) {
    std::lock_guard<std::mutex> lock(frameworkMutex_);
    scenarios_.erase(
        std::remove_if(scenarios_.begin(), scenarios_.end(),
            [&name](const std::shared_ptr<TestScenario>& scenario) {
                return scenario->getName() == name;
            }),
        scenarios_.end()
    );
}

std::vector<TestResult> UXTestingFramework::runAllScenarios() {
    std::vector<TestResult> results;
    
    for (const auto& scenario : scenarios_) {
        results.push_back(runScenario(scenario->getName()));
    }
    
    return results;
}

TestResult UXTestingFramework::runScenario(const std::string& name) {
    std::shared_ptr<TestScenario> scenario = nullptr;
    
    {
        std::lock_guard<std::mutex> lock(frameworkMutex_);
        auto it = std::find_if(scenarios_.begin(), scenarios_.end(),
            [&name](const std::shared_ptr<TestScenario>& s) {
                return s->getName() == name;
            });
        
        if (it == scenarios_.end()) {
            TestResult result;
            result.success = false;
            result.message = "Scenario not found: " + name;
            return result;
        }
        
        scenario = *it;
    }
    
    // Launch application if needed
    if (!scenario->getAppConfig().executablePath.empty()) {
        // TODO: launcher_->launch(scenario->getAppConfig());
    }
    
    // Execute scenario
    TestResult result = scenario->execute();
    
    // Capture screenshot if enabled
    if (enableScreenshots_ && result.success) {
        result.screenshotPath = captureScreenshot("scenario_" + name + ".png");
    }
    
    // Log result
    std::string logFile = outputDirectory_ + "/" + name + "_result.log";
    std::ofstream log(logFile);
    if (log.is_open()) {
        log << "Scenario: " << name << std::endl;
        log << "Success: " << (result.success ? "true" : "false") << std::endl;
        log << "Message: " << result.message << std::endl;
        log << "Duration: " << result.duration.count() << "ms" << std::endl;
        log << "Screenshot: " << result.screenshotPath << std::endl;
        log.close();
    }
    
    return result;
}

void UXTestingFramework::setOutputDirectory(const std::string& path) {
    outputDirectory_ = path;
    if (initialized_) {
        std::filesystem::create_directories(outputDirectory_);
    }
}

void UXTestingFramework::setLogLevel(int level) {
    logLevel_ = level;
}

void UXTestingFramework::enableScreenshots(bool enable) {
    enableScreenshots_ = enable;
}

bool UXTestingFramework::isApplicationRunning(const std::string& windowTitle) {
    // TODO: Implement platform-specific window detection
    return false;
}

std::string UXTestingFramework::captureScreenshot(const std::string& filename) {
    if (!enableScreenshots_) return "";
    
    std::string finalFilename = filename;
    if (finalFilename.empty()) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "screenshot_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".png";
        finalFilename = ss.str();
    }
    
    std::string fullPath = outputDirectory_ + "/" + finalFilename;
    
    // TODO: Implement actual screenshot capture
    // return screenCapture_->captureScreenshot(fullPath);
    
    return fullPath;
}

bool UXTestingFramework::waitForWindow(const std::string& windowTitle, int timeoutMs) {
    // TODO: Implement platform-specific window waiting
    return false;
}

} // namespace UXTesting 