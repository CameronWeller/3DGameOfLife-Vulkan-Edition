#pragma once

#include "UXTestingFramework.h"
#include <windows.h>
#include <string>
#include <vector>

namespace UXTesting {

// Windows-specific application launcher
class WindowsApplicationLauncher : public ApplicationLauncher {
public:
    WindowsApplicationLauncher();
    ~WindowsApplicationLauncher() override;

    bool launch(const AppConfig& config) override;
    bool terminate(const std::string& windowTitle) override;
    bool isRunning(const std::string& windowTitle) override;
    bool waitForWindow(const std::string& windowTitle, int timeoutMs) override;

private:
    HWND findWindowByTitle(const std::string& title);
    static BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam);
    static std::vector<HWND> foundWindows_;
    static std::string searchTitle_;
};

// Windows-specific input simulator
class WindowsInputSimulator : public InputSimulator {
public:
    WindowsInputSimulator();
    ~WindowsInputSimulator() override;

    bool mouseClick(int x, int y, bool rightClick = false) override;
    bool mouseDoubleClick(int x, int y) override;
    bool mouseDrag(int startX, int startY, int endX, int endY) override;
    bool keyPress(const std::string& key) override;
    bool keyCombination(const std::vector<std::string>& keys) override;
    bool textInput(const std::string& text) override;
    bool wait(int milliseconds) override;

private:
    bool sendMouseInput(int x, int y, DWORD flags);
    bool sendKeyInput(WORD vkCode, bool keyDown);
    WORD getVirtualKeyCode(const std::string& key);
    void setCursorPosition(int x, int y);
};

// Windows-specific screen capture
class WindowsScreenCapture : public ScreenCapture {
public:
    WindowsScreenCapture();
    ~WindowsScreenCapture() override;

    std::string captureScreenshot(const std::string& filename) override;
    bool captureRegion(int x, int y, int width, int height, const std::string& filename) override;
    std::vector<uint8_t> getScreenshotData() override;

private:
    bool saveBitmapToFile(HBITMAP hBitmap, const std::string& filename);
    std::vector<uint8_t> bitmapToBytes(HBITMAP hBitmap);
    int screenWidth_;
    int screenHeight_;
};

// OpenCV-based image analyzer
class OpenCVImageAnalyzer : public ImageAnalyzer {
public:
    OpenCVImageAnalyzer();
    ~OpenCVImageAnalyzer() override;

    double compareImages(const std::string& image1, const std::string& image2) override;
    bool findImageInScreen(const std::string& templateImage, int& x, int& y) override;
    bool waitForImage(const std::string& templateImage, int timeoutMs, int& x, int& y) override;
    std::vector<std::pair<int, int>> findAllMatches(const std::string& templateImage) override;

private:
    bool isOpenCVAvailable_;
    // TODO: Add OpenCV includes and member variables
};

} // namespace UXTesting 