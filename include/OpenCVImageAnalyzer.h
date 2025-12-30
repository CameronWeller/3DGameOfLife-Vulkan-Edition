#pragma once

#include "UXTestingFramework.h"
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/features2d.hpp>
#include <string>
#include <vector>
#include <memory>

namespace UXTesting {

class OpenCVImageAnalyzer : public ImageAnalyzer {
public:
    OpenCVImageAnalyzer();
    ~OpenCVImageAnalyzer() override;

    double compareImages(const std::string& image1, const std::string& image2) override;
    bool findImageInScreen(const std::string& templateImage, int& x, int& y) override;
    bool waitForImage(const std::string& templateImage, int timeoutMs, int& x, int& y) override;
    std::vector<std::pair<int, int>> findAllMatches(const std::string& templateImage) override;
    
    // Additional OpenCV-specific methods
    bool findImageInScreen(const cv::Mat& screenImage, const std::string& templateImage, int& x, int& y);
    double compareImages(const cv::Mat& image1, const cv::Mat& image2);
    cv::Mat loadImage(const std::string& filename);
    cv::Mat screenToMat(const std::vector<uint8_t>& screenData, int width, int height);
    std::vector<uint8_t> matToBytes(const cv::Mat& image);

private:
    bool isOpenCVAvailable_;
    double defaultThreshold_;
    
    // Helper methods
    cv::Mat preprocessImage(const cv::Mat& image);
    std::vector<cv::Point> findTemplateMatches(const cv::Mat& screen, const cv::Mat& templ, double threshold);
    double calculateSimilarity(const cv::Mat& img1, const cv::Mat& img2);
    cv::Mat resizeImage(const cv::Mat& image, double scale);
    cv::Mat convertToGrayscale(const cv::Mat& image);
};

} // namespace UXTesting 