#include "WindowsUXTesting.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <thread>
#include <chrono>

namespace UXTesting {

// Static member initialization
std::vector<HWND> WindowsApplicationLauncher::foundWindows_;
std::string WindowsApplicationLauncher::searchTitle_;

// WindowsApplicationLauncher implementation
WindowsApplicationLauncher::WindowsApplicationLauncher() {
}

WindowsApplicationLauncher::~WindowsApplicationLauncher() {
}

bool WindowsApplicationLauncher::launch(const AppConfig& config) {
    try {
        // Build command line
        std::string commandLine = "\"" + config.executablePath + "\"";
        for (const auto& arg : config.launchArguments) {
            commandLine += " " + arg;
        }

        // Create process
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_SHOW;

        BOOL success = CreateProcessA(
            nullptr,
            const_cast<char*>(commandLine.c_str()),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            config.workingDirectory.empty() ? nullptr : config.workingDirectory.c_str(),
            &si,
            &pi
        );

        if (!success) {
            std::cerr << "Failed to launch application: " << config.executablePath << std::endl;
            return false;
        }

        // Close process and thread handles
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        // Wait for window to appear
        if (!config.windowTitle.empty()) {
            return waitForWindow(config.windowTitle, config.launchTimeoutMs);
        }

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception launching application: " << e.what() << std::endl;
        return false;
    }
}

bool WindowsApplicationLauncher::terminate(const std::string& windowTitle) {
    HWND hwnd = findWindowByTitle(windowTitle);
    if (hwnd == nullptr) {
        return false;
    }

    // Send close message
    PostMessage(hwnd, WM_CLOSE, 0, 0);
    
    // Wait for window to close
    for (int i = 0; i < 50; i++) { // 5 seconds timeout
        if (!IsWindow(hwnd)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Force terminate if window still exists
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (hProcess != nullptr) {
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
        return true;
    }

    return false;
}

bool WindowsApplicationLauncher::isRunning(const std::string& windowTitle) {
    return findWindowByTitle(windowTitle) != nullptr;
}

bool WindowsApplicationLauncher::waitForWindow(const std::string& windowTitle, int timeoutMs) {
    auto startTime = std::chrono::steady_clock::now();
    
    while (std::chrono::steady_clock::now() - startTime < std::chrono::milliseconds(timeoutMs)) {
        if (findWindowByTitle(windowTitle) != nullptr) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return false;
}

HWND WindowsApplicationLauncher::findWindowByTitle(const std::string& title) {
    foundWindows_.clear();
    searchTitle_ = title;
    
    EnumWindows(enumWindowsProc, 0);
    
    if (!foundWindows_.empty()) {
        return foundWindows_[0];
    }
    
    return nullptr;
}

BOOL CALLBACK WindowsApplicationLauncher::enumWindowsProc(HWND hwnd, LPARAM lParam) {
    char windowTitle[256];
    GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));
    
    if (strstr(windowTitle, searchTitle_.c_str()) != nullptr) {
        foundWindows_.push_back(hwnd);
    }
    
    return TRUE;
}

// WindowsInputSimulator implementation
WindowsInputSimulator::WindowsInputSimulator() {
}

WindowsInputSimulator::~WindowsInputSimulator() {
}

bool WindowsInputSimulator::mouseClick(int x, int y, bool rightClick) {
    setCursorPosition(x, y);
    
    DWORD flags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP;
    if (rightClick) {
        flags = MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP;
    }
    
    return sendMouseInput(x, y, flags);
}

bool WindowsInputSimulator::mouseDoubleClick(int x, int y) {
    setCursorPosition(x, y);
    
    // First click
    if (!sendMouseInput(x, y, MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP)) {
        return false;
    }
    
    // Small delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Second click
    return sendMouseInput(x, y, MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP);
}

bool WindowsInputSimulator::mouseDrag(int startX, int startY, int endX, int endY) {
    setCursorPosition(startX, startY);
    
    // Mouse down
    if (!sendMouseInput(startX, startY, MOUSEEVENTF_LEFTDOWN)) {
        return false;
    }
    
    // Move to end position
    setCursorPosition(endX, endY);
    
    // Mouse up
    return sendMouseInput(endX, endY, MOUSEEVENTF_LEFTUP);
}

bool WindowsInputSimulator::keyPress(const std::string& key) {
    WORD vkCode = getVirtualKeyCode(key);
    if (vkCode == 0) {
        return false;
    }
    
    return sendKeyInput(vkCode, true) && sendKeyInput(vkCode, false);
}

bool WindowsInputSimulator::keyCombination(const std::vector<std::string>& keys) {
    // Press all keys
    for (const auto& key : keys) {
        WORD vkCode = getVirtualKeyCode(key);
        if (vkCode == 0) {
            return false;
        }
        if (!sendKeyInput(vkCode, true)) {
            return false;
        }
    }
    
    // Release all keys in reverse order
    for (auto it = keys.rbegin(); it != keys.rend(); ++it) {
        WORD vkCode = getVirtualKeyCode(*it);
        if (!sendKeyInput(vkCode, false)) {
            return false;
        }
    }
    
    return true;
}

bool WindowsInputSimulator::textInput(const std::string& text) {
    for (char c : text) {
        // Convert character to virtual key code
        WORD vkCode = VkKeyScanA(c);
        if (vkCode == -1) {
            continue;
        }
        
        // Handle shift key if needed
        bool shiftNeeded = (vkCode >> 8) & 1;
        if (shiftNeeded) {
            sendKeyInput(VK_SHIFT, true);
        }
        
        // Send key
        sendKeyInput(vkCode & 0xFF, true);
        sendKeyInput(vkCode & 0xFF, false);
        
        if (shiftNeeded) {
            sendKeyInput(VK_SHIFT, false);
        }
        
        // Small delay between characters
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return true;
}

bool WindowsInputSimulator::wait(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    return true;
}

bool WindowsInputSimulator::sendMouseInput(int x, int y, DWORD flags) {
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dx = x * (65535 / GetSystemMetrics(SM_CXSCREEN));
    input.mi.dy = y * (65535 / GetSystemMetrics(SM_CYSCREEN));
    input.mi.dwFlags = flags | MOUSEEVENTF_ABSOLUTE;
    
    return SendInput(1, &input, sizeof(INPUT)) > 0;
}

bool WindowsInputSimulator::sendKeyInput(WORD vkCode, bool keyDown) {
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vkCode;
    input.ki.dwFlags = keyDown ? 0 : KEYEVENTF_KEYUP;
    
    return SendInput(1, &input, sizeof(INPUT)) > 0;
}

WORD WindowsInputSimulator::getVirtualKeyCode(const std::string& key) {
    static const std::map<std::string, WORD> keyMap = {
        {"ENTER", VK_RETURN},
        {"TAB", VK_TAB},
        {"SHIFT", VK_SHIFT},
        {"CTRL", VK_CONTROL},
        {"ALT", VK_MENU},
        {"ESC", VK_ESCAPE},
        {"SPACE", VK_SPACE},
        {"BACKSPACE", VK_BACK},
        {"DELETE", VK_DELETE},
        {"HOME", VK_HOME},
        {"END", VK_END},
        {"PAGEUP", VK_PRIOR},
        {"PAGEDOWN", VK_NEXT},
        {"F1", VK_F1},
        {"F2", VK_F2},
        {"F3", VK_F3},
        {"F4", VK_F4},
        {"F5", VK_F5},
        {"F6", VK_F6},
        {"F7", VK_F7},
        {"F8", VK_F8},
        {"F9", VK_F9},
        {"F10", VK_F10},
        {"F11", VK_F11},
        {"F12", VK_F12}
    };
    
    auto it = keyMap.find(key);
    if (it != keyMap.end()) {
        return it->second;
    }
    
    // Try to convert single character
    if (key.length() == 1) {
        return VkKeyScanA(key[0]) & 0xFF;
    }
    
    return 0;
}

void WindowsInputSimulator::setCursorPosition(int x, int y) {
    SetCursorPos(x, y);
}

// WindowsScreenCapture implementation
WindowsScreenCapture::WindowsScreenCapture() {
    screenWidth_ = GetSystemMetrics(SM_CXSCREEN);
    screenHeight_ = GetSystemMetrics(SM_CYSCREEN);
}

WindowsScreenCapture::~WindowsScreenCapture() {
}

std::string WindowsScreenCapture::captureScreenshot(const std::string& filename) {
    // Get DC for entire screen
    HDC hScreenDC = GetDC(nullptr);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    
    // Create bitmap
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, screenWidth_, screenHeight_);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
    
    // Copy screen to bitmap
    BitBlt(hMemoryDC, 0, 0, screenWidth_, screenHeight_, hScreenDC, 0, 0, SRCCOPY);
    
    // Save to file
    std::string result = filename;
    if (!saveBitmapToFile(hBitmap, filename)) {
        result = "";
    }
    
    // Cleanup
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(nullptr, hScreenDC);
    
    return result;
}

bool WindowsScreenCapture::captureRegion(int x, int y, int width, int height, const std::string& filename) {
    // Get DC for entire screen
    HDC hScreenDC = GetDC(nullptr);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    
    // Create bitmap for region
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
    
    // Copy region to bitmap
    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, x, y, SRCCOPY);
    
    // Save to file
    bool result = saveBitmapToFile(hBitmap, filename);
    
    // Cleanup
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(nullptr, hScreenDC);
    
    return result;
}

std::vector<uint8_t> WindowsScreenCapture::getScreenshotData() {
    // Get DC for entire screen
    HDC hScreenDC = GetDC(nullptr);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    
    // Create bitmap
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, screenWidth_, screenHeight_);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
    
    // Copy screen to bitmap
    BitBlt(hMemoryDC, 0, 0, screenWidth_, screenHeight_, hScreenDC, 0, 0, SRCCOPY);
    
    // Convert to bytes
    std::vector<uint8_t> data = bitmapToBytes(hBitmap);
    
    // Cleanup
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(nullptr, hScreenDC);
    
    return data;
}

bool WindowsScreenCapture::saveBitmapToFile(HBITMAP hBitmap, const std::string& filename) {
    BITMAP bm;
    GetObject(hBitmap, sizeof(bm), &bm);
    
    BITMAPINFOHEADER bi = { 0 };
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bm.bmWidth;
    bi.biHeight = bm.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    
    // Calculate row size (must be aligned to 4 bytes)
    int rowSize = ((bm.bmWidth * 3 + 3) / 4) * 4;
    int imageSize = rowSize * bm.bmHeight;
    
    // Allocate memory for bitmap bits
    std::vector<uint8_t> bits(imageSize);
    
    // Get bitmap bits
    HDC hDC = CreateCompatibleDC(nullptr);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hDC, hBitmap);
    GetDIBits(hDC, hBitmap, 0, bm.bmHeight, bits.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    SelectObject(hDC, hOldBitmap);
    DeleteDC(hDC);
    
    // Write BMP file
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // BMP file header
    uint16_t signature = 0x4D42; // "BM"
    uint32_t fileSize = 54 + imageSize;
    uint16_t reserved1 = 0;
    uint16_t reserved2 = 0;
    uint32_t dataOffset = 54;
    
    file.write(reinterpret_cast<const char*>(&signature), sizeof(signature));
    file.write(reinterpret_cast<const char*>(&fileSize), sizeof(fileSize));
    file.write(reinterpret_cast<const char*>(&reserved1), sizeof(reserved1));
    file.write(reinterpret_cast<const char*>(&reserved2), sizeof(reserved2));
    file.write(reinterpret_cast<const char*>(&dataOffset), sizeof(dataOffset));
    
    // BMP info header
    uint32_t infoSize = 40;
    int32_t width = bm.bmWidth;
    int32_t height = bm.bmHeight;
    uint16_t planes = 1;
    uint16_t bitCount = 24;
    uint32_t compression = 0;
    uint32_t imageSizeHeader = imageSize;
    int32_t xPixelsPerMeter = 0;
    int32_t yPixelsPerMeter = 0;
    uint32_t colorsUsed = 0;
    uint32_t importantColors = 0;
    
    file.write(reinterpret_cast<const char*>(&infoSize), sizeof(infoSize));
    file.write(reinterpret_cast<const char*>(&width), sizeof(width));
    file.write(reinterpret_cast<const char*>(&height), sizeof(height));
    file.write(reinterpret_cast<const char*>(&planes), sizeof(planes));
    file.write(reinterpret_cast<const char*>(&bitCount), sizeof(bitCount));
    file.write(reinterpret_cast<const char*>(&compression), sizeof(compression));
    file.write(reinterpret_cast<const char*>(&imageSizeHeader), sizeof(imageSizeHeader));
    file.write(reinterpret_cast<const char*>(&xPixelsPerMeter), sizeof(xPixelsPerMeter));
    file.write(reinterpret_cast<const char*>(&yPixelsPerMeter), sizeof(yPixelsPerMeter));
    file.write(reinterpret_cast<const char*>(&colorsUsed), sizeof(colorsUsed));
    file.write(reinterpret_cast<const char*>(&importantColors), sizeof(importantColors));
    
    // Write bitmap data (BMP is stored bottom-up)
    for (int y = bm.bmHeight - 1; y >= 0; y--) {
        file.write(reinterpret_cast<const char*>(&bits[y * rowSize]), rowSize);
    }
    
    file.close();
    return true;
}

std::vector<uint8_t> WindowsScreenCapture::bitmapToBytes(HBITMAP hBitmap) {
    BITMAP bm;
    GetObject(hBitmap, sizeof(bm), &bm);
    
    BITMAPINFOHEADER bi = { 0 };
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bm.bmWidth;
    bi.biHeight = bm.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    
    // Calculate row size (must be aligned to 4 bytes)
    int rowSize = ((bm.bmWidth * 3 + 3) / 4) * 4;
    int imageSize = rowSize * bm.bmHeight;
    
    // Allocate memory for bitmap bits
    std::vector<uint8_t> bits(imageSize);
    
    // Get bitmap bits
    HDC hDC = CreateCompatibleDC(nullptr);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hDC, hBitmap);
    GetDIBits(hDC, hBitmap, 0, bm.bmHeight, bits.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    SelectObject(hDC, hOldBitmap);
    DeleteDC(hDC);
    
    // Convert to RGB format (remove padding)
    std::vector<uint8_t> rgbData;
    rgbData.reserve(bm.bmWidth * bm.bmHeight * 3);
    
    for (int y = 0; y < bm.bmHeight; y++) {
        for (int x = 0; x < bm.bmWidth; x++) {
            int srcIndex = y * rowSize + x * 3;
            // BMP stores BGR, convert to RGB
            rgbData.push_back(bits[srcIndex + 2]); // R
            rgbData.push_back(bits[srcIndex + 1]); // G
            rgbData.push_back(bits[srcIndex + 0]); // B
        }
    }
    
    return rgbData;
}

// OpenCVImageAnalyzer implementation
OpenCVImageAnalyzer::OpenCVImageAnalyzer() : isOpenCVAvailable_(false) {
    // TODO: Check if OpenCV is available and initialize
}

OpenCVImageAnalyzer::~OpenCVImageAnalyzer() {
}

double OpenCVImageAnalyzer::compareImages(const std::string& image1, const std::string& image2) {
    // TODO: Implement OpenCV-based image comparison
    return 0.0;
}

bool OpenCVImageAnalyzer::findImageInScreen(const std::string& templateImage, int& x, int& y) {
    // TODO: Implement template matching
    return false;
}

bool OpenCVImageAnalyzer::waitForImage(const std::string& templateImage, int timeoutMs, int& x, int& y) {
    // TODO: Implement waiting for image to appear
    return false;
}

std::vector<std::pair<int, int>> OpenCVImageAnalyzer::findAllMatches(const std::string& templateImage) {
    // TODO: Implement finding all matches
    return std::vector<std::pair<int, int>>();
}

} // namespace UXTesting 