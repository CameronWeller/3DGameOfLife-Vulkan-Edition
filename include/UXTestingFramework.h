#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace UXTesting {

// Forward declarations
class ApplicationLauncher;
class InputSimulator;
class ScreenCapture;
class ImageAnalyzer;
class TestScenario;

// Application configuration
struct AppConfig {
    std::string executablePath;
    std::string workingDirectory;
    std::string windowTitle;
    std::vector<std::string> launchArguments;
    int expectedWidth = 1920;
    int expectedHeight = 1080;
    int launchTimeoutMs = 30000;
    int stabilizationDelayMs = 2000;
};

// Test result structure
struct TestResult {
    bool success;
    std::string message;
    std::chrono::milliseconds duration;
    std::string screenshotPath;
    std::vector<std::string> logs;
};

// Input event types
enum class InputType {
    MOUSE_CLICK,
    MOUSE_DOUBLE_CLICK,
    MOUSE_RIGHT_CLICK,
    MOUSE_DRAG,
    KEY_PRESS,
    KEY_COMBINATION,
    TEXT_INPUT,
    WAIT
};

// Input event structure
struct InputEvent {
    InputType type;
    int x = 0;
    int y = 0;
    std::string key;
    std::string text;
    int durationMs = 0;
    std::string description;
};

// Screen region for image analysis
struct ScreenRegion {
    int x, y, width, height;
    std::string name;
    std::string expectedImage;
    double similarityThreshold = 0.9;
};

// Test scenario definition
class TestScenario {
public:
    TestScenario(const std::string& name);
    ~TestScenario() = default;

    // Scenario configuration
    void setAppConfig(const AppConfig& config);
    void addInputEvent(const InputEvent& event);
    void addScreenRegion(const ScreenRegion& region);
    void setPrecondition(std::function<bool()> condition);
    void setPostcondition(std::function<bool()> condition);

    // Execution
    TestResult execute();
    void stop();

    // Getters
    const std::string& getName() const { return name_; }
    const AppConfig& getAppConfig() const { return appConfig_; }

private:
    std::string name_;
    AppConfig appConfig_;
    std::vector<InputEvent> inputEvents_;
    std::vector<ScreenRegion> screenRegions_;
    std::function<bool()> precondition_;
    std::function<bool()> postcondition_;
    std::atomic<bool> shouldStop_{false};
};

// Main testing framework
class UXTestingFramework {
public:
    UXTestingFramework();
    ~UXTestingFramework();

    // Framework initialization
    bool initialize();
    void shutdown();

    // Test management
    void addScenario(std::shared_ptr<TestScenario> scenario);
    void removeScenario(const std::string& name);
    std::vector<TestResult> runAllScenarios();
    TestResult runScenario(const std::string& name);

    // Configuration
    void setOutputDirectory(const std::string& path);
    void setLogLevel(int level);
    void enableScreenshots(bool enable);

    // Utility methods
    bool isApplicationRunning(const std::string& windowTitle);
    std::string captureScreenshot(const std::string& filename = "");
    bool waitForWindow(const std::string& windowTitle, int timeoutMs = 10000);

private:
    std::unique_ptr<ApplicationLauncher> launcher_;
    std::unique_ptr<InputSimulator> inputSimulator_;
    std::unique_ptr<ScreenCapture> screenCapture_;
    std::unique_ptr<ImageAnalyzer> imageAnalyzer_;
    
    std::vector<std::shared_ptr<TestScenario>> scenarios_;
    std::string outputDirectory_;
    int logLevel_;
    bool enableScreenshots_;
    bool initialized_;
    std::mutex frameworkMutex_;
};

// Application launcher interface
class ApplicationLauncher {
public:
    virtual ~ApplicationLauncher() = default;
    virtual bool launch(const AppConfig& config) = 0;
    virtual bool terminate(const std::string& windowTitle) = 0;
    virtual bool isRunning(const std::string& windowTitle) = 0;
    virtual bool waitForWindow(const std::string& windowTitle, int timeoutMs) = 0;
};

// Input simulator interface
class InputSimulator {
public:
    virtual ~InputSimulator() = default;
    virtual bool mouseClick(int x, int y, bool rightClick = false) = 0;
    virtual bool mouseDoubleClick(int x, int y) = 0;
    virtual bool mouseDrag(int startX, int startY, int endX, int endY) = 0;
    virtual bool keyPress(const std::string& key) = 0;
    virtual bool keyCombination(const std::vector<std::string>& keys) = 0;
    virtual bool textInput(const std::string& text) = 0;
    virtual bool wait(int milliseconds) = 0;
};

// Screen capture interface
class ScreenCapture {
public:
    virtual ~ScreenCapture() = default;
    virtual std::string captureScreenshot(const std::string& filename) = 0;
    virtual bool captureRegion(int x, int y, int width, int height, const std::string& filename) = 0;
    virtual std::vector<uint8_t> getScreenshotData() = 0;
};

// Image analyzer interface
class ImageAnalyzer {
public:
    virtual ~ImageAnalyzer() = default;
    virtual double compareImages(const std::string& image1, const std::string& image2) = 0;
    virtual bool findImageInScreen(const std::string& templateImage, int& x, int& y) = 0;
    virtual bool waitForImage(const std::string& templateImage, int timeoutMs, int& x, int& y) = 0;
    virtual std::vector<std::pair<int, int>> findAllMatches(const std::string& templateImage) = 0;
};

} // namespace UXTesting 