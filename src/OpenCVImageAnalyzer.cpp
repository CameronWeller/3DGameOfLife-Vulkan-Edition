#include "OpenCVImageAnalyzer.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>

namespace UXTesting {

OpenCVImageAnalyzer::OpenCVImageAnalyzer() 
    : isOpenCVAvailable_(true)
    , defaultThreshold_(0.8) {
    
    // Verify OpenCV is working
    try {
        cv::Mat testMat(10, 10, CV_8UC3);
        isOpenCVAvailable_ = true;
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV not available: " << e.what() << std::endl;
        isOpenCVAvailable_ = false;
    }
}

OpenCVImageAnalyzer::~OpenCVImageAnalyzer() {
}

double OpenCVImageAnalyzer::compareImages(const std::string& image1, const std::string& image2) {
    if (!isOpenCVAvailable_) return 0.0;
    
    try {
        cv::Mat img1 = loadImage(image1);
        cv::Mat img2 = loadImage(image2);
        
        if (img1.empty() || img2.empty()) {
            return 0.0;
        }
        
        return compareImages(img1, img2);
    } catch (const cv::Exception& e) {
        std::cerr << "Error comparing images: " << e.what() << std::endl;
        return 0.0;
    }
}

bool OpenCVImageAnalyzer::findImageInScreen(const std::string& templateImage, int& x, int& y) {
    if (!isOpenCVAvailable_) return false;
    
    try {
        // Get current screen capture
        // Note: This would need to be integrated with ScreenCapture
        cv::Mat screenImage = cv::imread("current_screen.png");
        if (screenImage.empty()) {
            return false;
        }
        
        return findImageInScreen(screenImage, templateImage, x, y);
    } catch (const cv::Exception& e) {
        std::cerr << "Error finding image in screen: " << e.what() << std::endl;
        return false;
    }
}

bool OpenCVImageAnalyzer::waitForImage(const std::string& templateImage, int timeoutMs, int& x, int& y) {
    if (!isOpenCVAvailable_) return false;
    
    auto startTime = std::chrono::steady_clock::now();
    
    while (std::chrono::steady_clock::now() - startTime < std::chrono::milliseconds(timeoutMs)) {
        if (findImageInScreen(templateImage, x, y)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return false;
}

std::vector<std::pair<int, int>> OpenCVImageAnalyzer::findAllMatches(const std::string& templateImage) {
    std::vector<std::pair<int, int>> matches;
    if (!isOpenCVAvailable_) return matches;
    
    try {
        cv::Mat screenImage = cv::imread("current_screen.png");
        cv::Mat templ = loadImage(templateImage);
        
        if (screenImage.empty() || templ.empty()) {
            return matches;
        }
        
        std::vector<cv::Point> points = findTemplateMatches(screenImage, templ, defaultThreshold_);
        
        for (const auto& point : points) {
            matches.emplace_back(point.x, point.y);
        }
    } catch (const cv::Exception& e) {
        std::cerr << "Error finding all matches: " << e.what() << std::endl;
    }
    
    return matches;
}

bool OpenCVImageAnalyzer::findImageInScreen(const cv::Mat& screenImage, const std::string& templateImage, int& x, int& y) {
    try {
        cv::Mat templ = loadImage(templateImage);
        if (templ.empty()) {
            return false;
        }
        
        std::vector<cv::Point> matches = findTemplateMatches(screenImage, templ, defaultThreshold_);
        
        if (!matches.empty()) {
            x = matches[0].x;
            y = matches[0].y;
            return true;
        }
        
        return false;
    } catch (const cv::Exception& e) {
        std::cerr << "Error in findImageInScreen: " << e.what() << std::endl;
        return false;
    }
}

double OpenCVImageAnalyzer::compareImages(const cv::Mat& image1, const cv::Mat& image2) {
    try {
        // Resize images to same size for comparison
        cv::Mat img1 = image1.clone();
        cv::Mat img2 = image2.clone();
        
        if (img1.size() != img2.size()) {
            cv::resize(img2, img2, img1.size());
        }
        
        // Convert to grayscale for better comparison
        cv::Mat gray1, gray2;
        cv::cvtColor(img1, gray1, cv::COLOR_BGR2GRAY);
        cv::cvtColor(img2, gray2, cv::COLOR_BGR2GRAY);
        
        // Calculate structural similarity
        cv::Mat diff;
        cv::absdiff(gray1, gray2, diff);
        
        double similarity = 1.0 - (cv::sum(diff)[0] / (diff.rows * diff.cols * 255.0));
        
        return std::max(0.0, std::min(1.0, similarity));
    } catch (const cv::Exception& e) {
        std::cerr << "Error comparing images: " << e.what() << std::endl;
        return 0.0;
    }
}

cv::Mat OpenCVImageAnalyzer::loadImage(const std::string& filename) {
    try {
        cv::Mat image = cv::imread(filename, cv::IMREAD_COLOR);
        if (image.empty()) {
            std::cerr << "Failed to load image: " << filename << std::endl;
        }
        return image;
    } catch (const cv::Exception& e) {
        std::cerr << "Error loading image: " << e.what() << std::endl;
        return cv::Mat();
    }
}

cv::Mat OpenCVImageAnalyzer::screenToMat(const std::vector<uint8_t>& screenData, int width, int height) {
    if (screenData.size() != width * height * 3) {
        return cv::Mat();
    }
    
    try {
        cv::Mat image(height, width, CV_8UC3);
        std::memcpy(image.data, screenData.data(), screenData.size());
        
        // Convert from RGB to BGR (OpenCV format)
        cv::cvtColor(image, image, cv::COLOR_RGB2BGR);
        
        return image;
    } catch (const cv::Exception& e) {
        std::cerr << "Error converting screen data to Mat: " << e.what() << std::endl;
        return cv::Mat();
    }
}

std::vector<uint8_t> OpenCVImageAnalyzer::matToBytes(const cv::Mat& image) {
    try {
        cv::Mat bgrImage = image.clone();
        if (bgrImage.channels() == 1) {
            cv::cvtColor(bgrImage, bgrImage, cv::COLOR_GRAY2BGR);
        }
        
        // Convert from BGR to RGB
        cv::Mat rgbImage;
        cv::cvtColor(bgrImage, rgbImage, cv::COLOR_BGR2RGB);
        
        std::vector<uint8_t> bytes;
        bytes.assign(rgbImage.data, rgbImage.data + rgbImage.total() * rgbImage.channels());
        
        return bytes;
    } catch (const cv::Exception& e) {
        std::cerr << "Error converting Mat to bytes: " << e.what() << std::endl;
        return std::vector<uint8_t>();
    }
}

cv::Mat OpenCVImageAnalyzer::preprocessImage(const cv::Mat& image) {
    try {
        cv::Mat processed = image.clone();
        
        // Convert to grayscale
        if (processed.channels() == 3) {
            cv::cvtColor(processed, processed, cv::COLOR_BGR2GRAY);
        }
        
        // Apply Gaussian blur to reduce noise
        cv::GaussianBlur(processed, processed, cv::Size(3, 3), 0);
        
        return processed;
    } catch (const cv::Exception& e) {
        std::cerr << "Error preprocessing image: " << e.what() << std::endl;
        return image.clone();
    }
}

std::vector<cv::Point> OpenCVImageAnalyzer::findTemplateMatches(const cv::Mat& screen, const cv::Mat& templ, double threshold) {
    std::vector<cv::Point> matches;
    
    try {
        cv::Mat result;
        cv::matchTemplate(screen, templ, result, cv::TM_CCOEFF_NORMED);
        
        cv::Point minLoc, maxLoc;
        double minVal, maxVal;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
        
        // Find all locations above threshold
        for (int y = 0; y < result.rows; y++) {
            for (int x = 0; x < result.cols; x++) {
                float value = result.at<float>(y, x);
                if (value >= threshold) {
                    matches.emplace_back(x, y);
                }
            }
        }
        
        // Sort by confidence (descending)
        std::sort(matches.begin(), matches.end(), [&result](const cv::Point& a, const cv::Point& b) {
            return result.at<float>(a.y, a.x) > result.at<float>(b.y, b.x);
        });
        
    } catch (const cv::Exception& e) {
        std::cerr << "Error in template matching: " << e.what() << std::endl;
    }
    
    return matches;
}

double OpenCVImageAnalyzer::calculateSimilarity(const cv::Mat& img1, const cv::Mat& img2) {
    try {
        cv::Mat gray1, gray2;
        cv::cvtColor(img1, gray1, cv::COLOR_BGR2GRAY);
        cv::cvtColor(img2, gray2, cv::COLOR_BGR2GRAY);
        
        // Resize to same size
        if (gray1.size() != gray2.size()) {
            cv::resize(gray2, gray2, gray1.size());
        }
        
        // Calculate correlation coefficient
        cv::Mat result;
        cv::matchTemplate(gray1, gray2, result, cv::TM_CCOEFF_NORMED);
        
        return result.at<float>(0, 0);
    } catch (const cv::Exception& e) {
        std::cerr << "Error calculating similarity: " << e.what() << std::endl;
        return 0.0;
    }
}

cv::Mat OpenCVImageAnalyzer::resizeImage(const cv::Mat& image, double scale) {
    try {
        cv::Mat resized;
        cv::resize(image, resized, cv::Size(), scale, scale, cv::INTER_LINEAR);
        return resized;
    } catch (const cv::Exception& e) {
        std::cerr << "Error resizing image: " << e.what() << std::endl;
        return image.clone();
    }
}

cv::Mat OpenCVImageAnalyzer::convertToGrayscale(const cv::Mat& image) {
    try {
        cv::Mat gray;
        if (image.channels() == 3) {
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = image.clone();
        }
        return gray;
    } catch (const cv::Exception& e) {
        std::cerr << "Error converting to grayscale: " << e.what() << std::endl;
        return image.clone();
    }
}

} // namespace UXTesting 