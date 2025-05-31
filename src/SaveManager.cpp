#include "SaveManager.h"
#include "VulkanEngine.h"
#include "VulkanContext.h"
#include "VulkanMemoryManager.h"
#include "Logger.h"
#include <vulkan/vulkan.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace VulkanHIP {

SaveManager::SaveManager(VulkanEngine* engine) : engine_(engine) {
    try {
        // Get the user's home directory
        const char* homeDir = getenv("USERPROFILE");
        if (!homeDir) {
            homeDir = getenv("HOME");
        }
        if (!homeDir) {
            throw SaveError(SaveError::Type::Unknown, "Could not determine home directory");
        }

        // Create the patterns directory
        patternDirectory_ = fs::path(homeDir) / "VulkanEngine" / "patterns";
        previewDirectory_ = patternDirectory_ / "previews";
        if (!createPatternDirectory()) {
            throw SaveError(SaveError::Type::FileAccessDenied, 
                          "Failed to create pattern directory: " + patternDirectory_.string());
        }
        fs::create_directories(previewDirectory_);
    } catch (const SaveError& e) {
        handleSaveError(e);
        throw;
    } catch (const std::exception& e) {
        throw SaveError(SaveError::Type::Unknown, std::string("Unexpected error: ") + e.what());
    }
}

SaveManager::~SaveManager() {
    try {
        // Cleanup old autosaves on exit
        cleanupOldAutoSaves(5); // Keep last 5 autosaves
    } catch (...) {
        // Log error but don't throw from destructor
    }
}

void SaveManager::handleSaveError(const SaveError& error) {
    setLastError(error.details);
    switch (error.type) {
        case SaveError::Type::FileNotFound:
            std::cerr << "File not found: " << error.details << std::endl;
            break;
        case SaveError::Type::FileAccessDenied:
            std::cerr << "Access denied: " << error.details << std::endl;
            break;
        case SaveError::Type::FileCorrupted:
            std::cerr << "File corrupted: " << error.details << std::endl;
            break;
        case SaveError::Type::InvalidData:
            std::cerr << "Invalid data: " << error.details << std::endl;
            break;
        case SaveError::Type::OutOfMemory:
            std::cerr << "Out of memory: " << error.details << std::endl;
            break;
        default:
            std::cerr << "Unknown error: " << error.details << std::endl;
    }
}

bool SaveManager::savePattern(const std::string& filename, const VoxelData& voxelData, const PatternMetadata& metadata) {
    if (filename.empty()) {
        setLastError("Invalid filename");
        return false;
    }

    try {
        std::string path = (patternDirectory_ / filename).string();
        std::ofstream file(path);
        if (!file) {
            throw SaveError(SaveError::Type::FileAccessDenied, 
                          "Failed to open file for writing: " + path);
        }

        // Create JSON object
        json patternJson;
        patternJson["metadata"] = serializeMetadata(metadata);
        patternJson["voxelData"] = serializeVoxelData(voxelData);

        // Write JSON to file
        file << std::setw(4) << patternJson;
        if (file.fail()) {
            file.close();
            fs::remove(path); // Clean up failed save
            throw SaveError(SaveError::Type::FileCorrupted, 
                          "Failed to write pattern data to file: " + path);
        }

        lastPatternFile_ = filename;
        clearLastError();
        return true;
    } catch (const SaveError& e) {
        handleSaveError(e);
        return false;
    } catch (const std::exception& e) {
        handleSaveError(SaveError(SaveError::Type::Unknown, 
                                std::string("Unexpected error during save: ") + e.what()));
        return false;
    }
}

bool SaveManager::loadPattern(const std::string& filename, VoxelData& voxelData, PatternMetadata& metadata) {
    if (filename.empty()) {
        setLastError("Invalid filename");
        return false;
    }

    try {
        std::string path = (patternDirectory_ / filename).string();
        std::ifstream file(path);
        if (!file) {
            throw SaveError(SaveError::Type::FileNotFound, 
                          "Failed to open pattern file: " + path);
        }

        // Read JSON from file
        json patternJson;
        file >> patternJson;

        // Parse metadata and voxel data
        if (!deserializeMetadata(patternJson["metadata"], metadata)) {
            throw SaveError(SaveError::Type::InvalidData, 
                          "Failed to parse pattern metadata: " + path);
        }
        if (!deserializeVoxelData(patternJson["voxelData"], voxelData)) {
            throw SaveError(SaveError::Type::InvalidData, 
                          "Failed to parse voxel data: " + path);
        }

        lastPatternFile_ = filename;
        clearLastError();
        return true;
    } catch (const SaveError& e) {
        handleSaveError(e);
        return false;
    } catch (const std::exception& e) {
        handleSaveError(SaveError(SaveError::Type::Unknown, 
                                std::string("Unexpected error during load: ") + e.what()));
        return false;
    }
}

bool SaveManager::deletePattern(const std::string& filename) {
    try {
        std::string path = (patternDirectory_ / filename).string();
        if (!fs::exists(path)) {
            throw SaveError(SaveError::Type::FileNotFound, 
                          "Pattern file does not exist: " + path);
        }

        if (!fs::remove(path)) {
            throw SaveError(SaveError::Type::FileAccessDenied, 
                          "Failed to delete pattern file: " + path);
        }

        // Also delete preview if it exists
        std::string previewPath = getPreviewPath(filename);
        if (fs::exists(previewPath)) {
            fs::remove(previewPath);
        }

        if (lastPatternFile_ == filename) {
            lastPatternFile_.clear();
        }

        clearLastError();
        return true;
    } catch (const SaveError& e) {
        handleSaveError(e);
        return false;
    } catch (const std::exception& e) {
        handleSaveError(SaveError(SaveError::Type::Unknown, 
                                std::string("Unexpected error during delete: ") + e.what()));
        return false;
    }
}

std::vector<App::SaveInfo> SaveManager::getPatternFiles() {
    std::vector<App::SaveInfo> patterns;
    try {
        for (const auto& entry : fs::directory_iterator(patternDirectory_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".pattern") {
                if (validatePatternFile(entry.path())) {
                    patterns.push_back(createSaveInfo(entry.path()));
                }
            }
        }
        std::sort(patterns.begin(), patterns.end(),
            [](const App::SaveInfo& a, const App::SaveInfo& b) {
                return a.timestamp > b.timestamp;
            });
    } catch (const std::exception&) {
        // Return empty vector on error
    }
    return patterns;
}

std::string SaveManager::getLastPatternFile() const {
    return lastPatternFile_;
}

bool SaveManager::hasPatternFiles() const {
    try {
        for (const auto& entry : fs::directory_iterator(patternDirectory_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".pattern") {
                return true;
            }
        }
    } catch (const std::exception&) {
        // Return false on error
    }
    return false;
}

void SaveManager::setPatternDirectory(const std::filesystem::path& path) {
    patternDirectory_ = path;
    previewDirectory_ = path / "previews";
    createPatternDirectory();
}

std::filesystem::path SaveManager::getPatternDirectory() const {
    return patternDirectory_;
}

bool SaveManager::createPatternDirectory() {
    try {
        fs::create_directories(patternDirectory_);
        fs::create_directories(previewDirectory_);
        return true;
    } catch (...) {
        return false;
    }
}

bool SaveManager::validatePatternFile(const std::filesystem::path& path) const {
    try {
        if (!fs::exists(path) || !fs::is_regular_file(path)) {
            return false;
        }

        std::ifstream file(path);
        if (!file) {
            return false;
        }

        // Try to parse JSON
        json patternJson;
        try {
            file >> patternJson;
        } catch (...) {
            return false;
        }

        // Check required fields
        if (!patternJson.contains("metadata") || !patternJson.contains("voxelData")) {
            return false;
        }

        // Check file permissions
        auto perms = fs::status(path).permissions();
        if ((perms & fs::perms::owner_read) == fs::perms::none) {
            return false;
        }

        return true;
    } catch (...) {
        return false;
    }
}

App::SaveInfo SaveManager::createSaveInfo(const std::filesystem::path& path) const {
    App::SaveInfo info;
    info.filename = path.filename().string();
    info.timestamp = fs::last_write_time(path).time_since_epoch().count();
    
    // Try to read metadata for additional info
    try {
        std::ifstream file(path);
        json patternJson;
        file >> patternJson;
        
        if (patternJson.contains("metadata")) {
            const auto& metadata = patternJson["metadata"];
            info.name = metadata.value("name", info.filename);
            info.description = metadata.value("description", "");
            info.author = metadata.value("author", "");
            info.version = metadata.value("version", "1.0");
        }
    } catch (...) {
        // Use default values if metadata can't be read
    }
    
    return info;
}

std::string SaveManager::generatePatternFileName() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "pattern_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".pattern";
    return ss.str();
}

void SaveManager::generatePreview(const std::string& filename, const VoxelData& voxelData) {
    try {
        // Create a higher resolution preview image
        const int PREVIEW_WIDTH = 512;
        const int PREVIEW_HEIGHT = 512;
        
        // Create image with better format
        VkImage image;
        VkDeviceMemory imageMemory;
        VkImageView imageView;
        VkSampler sampler;
        
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = PREVIEW_WIDTH;
        imageInfo.extent.height = PREVIEW_HEIGHT;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        
        if (vkCreateImage(engine_->getDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create preview image!");
        }
        
        // Allocate memory for the image
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(engine_->getDevice(), image, &memRequirements);
        
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = engine_->findMemoryType(memRequirements.memoryTypeBits, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        if (vkAllocateMemory(engine_->getDevice(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            vkDestroyImage(engine_->getDevice(), image, nullptr);
            throw std::runtime_error("Failed to allocate preview image memory!");
        }
        
        vkBindImageMemory(engine_->getDevice(), image, imageMemory, 0);
        
        // Create image view
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(engine_->getDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            vkDestroyImage(engine_->getDevice(), image, nullptr);
            vkFreeMemory(engine_->getDevice(), imageMemory, nullptr);
            throw std::runtime_error("Failed to create preview image view!");
        }
        
        // Create sampler
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        
        if (vkCreateSampler(engine_->getDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            vkDestroyImageView(engine_->getDevice(), imageView, nullptr);
            vkDestroyImage(engine_->getDevice(), image, nullptr);
            vkFreeMemory(engine_->getDevice(), imageMemory, nullptr);
            throw std::runtime_error("Failed to create preview image sampler!");
        }
        
        // Create command buffer for rendering
        VkCommandBuffer cmdBuffer = engine_->beginSingleTimeCommands();
        
        // Set up camera for preview
        glm::vec3 center = voxelData.getCenter();
        float radius = voxelData.getBoundingRadius();
        glm::vec3 cameraPos = center + glm::vec3(radius * 2.0f, radius * 1.5f, radius * 2.0f);
        
        glm::mat4 view = glm::lookAt(cameraPos, center, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), 
            static_cast<float>(PREVIEW_WIDTH) / PREVIEW_HEIGHT, 0.1f, radius * 10.0f);
        
        // Render pattern preview
        engine_->renderPatternPreview(cmdBuffer, voxelData, view, proj);
        
        // Save the rendered image
        std::string previewPath = getPreviewPath(filename);
        if (!engine_->saveImageToFile(image, previewPath)) {
            throw std::runtime_error("Failed to save preview image!");
        }
        
        // Cleanup
        engine_->endSingleTimeCommands(cmdBuffer);
        vkDestroySampler(engine_->getDevice(), sampler, nullptr);
        vkDestroyImageView(engine_->getDevice(), imageView, nullptr);
        vkDestroyImage(engine_->getDevice(), image, nullptr);
        vkFreeMemory(engine_->getDevice(), imageMemory, nullptr);
        
    } catch (const std::exception& e) {
        std::cerr << "Error generating preview: " << e.what() << std::endl;
    }
}

bool SaveManager::hasPreview(const std::string& filename) const {
    std::string previewPath = getPreviewPath(filename);
    return fs::exists(previewPath);
}

std::string SaveManager::getPreviewPath(const std::string& filename) const {
    std::string baseName = fs::path(filename).stem().string();
    return (previewDirectory_ / (baseName + ".png")).string();
}

nlohmann::json SaveManager::serializeVoxelData(const VoxelData& voxelData) const {
    json j;
    
    // Serialize grid dimensions
    j["dimensions"] = {
        voxelData.dimensions.x,
        voxelData.dimensions.y,
        voxelData.dimensions.z
    };
    
    // Serialize voxel data as a flat array of booleans
    std::vector<bool> flatData;
    flatData.reserve(voxelData.dimensions.x * voxelData.dimensions.y * voxelData.dimensions.z);
    
    for (int z = 0; z < voxelData.dimensions.z; z++) {
        for (int y = 0; y < voxelData.dimensions.y; y++) {
            for (int x = 0; x < voxelData.dimensions.x; x++) {
                flatData.push_back(voxelData.getVoxel(x, y, z));
            }
        }
    }
    
    // Convert bool vector to uint8_t vector for better JSON compatibility
    std::vector<uint8_t> packedData;
    packedData.reserve((flatData.size() + 7) / 8);
    
    for (size_t i = 0; i < flatData.size(); i += 8) {
        uint8_t byte = 0;
        for (size_t bit = 0; bit < 8 && (i + bit) < flatData.size(); bit++) {
            if (flatData[i + bit]) {
                byte |= (1 << bit);
            }
        }
        packedData.push_back(byte);
    }
    
    j["data"] = packedData;
    return j;
}

bool SaveManager::deserializeVoxelData(const nlohmann::json& json, VoxelData& voxelData) const {
    try {
        // Parse dimensions
        auto dims = json["dimensions"].get<std::vector<int>>();
        if (dims.size() != 3) {
            return false;
        }
        
        voxelData.dimensions = glm::ivec3(dims[0], dims[1], dims[2]);
        
        // Parse packed data
        auto packedData = json["data"].get<std::vector<uint8_t>>();
        
        // Calculate expected size
        size_t totalVoxels = dims[0] * dims[1] * dims[2];
        size_t expectedPackedSize = (totalVoxels + 7) / 8;
        
        if (packedData.size() != expectedPackedSize) {
            return false;
        }
        
        // Unpack data into voxel grid
        for (int z = 0; z < dims[2]; z++) {
            for (int y = 0; y < dims[1]; y++) {
                for (int x = 0; x < dims[0]; x++) {
                    size_t index = z * dims[1] * dims[0] + y * dims[0] + x;
                    size_t byteIndex = index / 8;
                    size_t bitIndex = index % 8;
                    
                    bool value = (packedData[byteIndex] & (1 << bitIndex)) != 0;
                    voxelData.setVoxel(x, y, z, value);
                }
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

nlohmann::json SaveManager::serializeMetadata(const PatternMetadata& metadata) const {
    json j;
    j["name"] = metadata.name;
    j["description"] = metadata.description;
    j["author"] = metadata.author;
    j["version"] = metadata.version;
    j["ruleSet"] = metadata.ruleSet;
    j["gridSize"] = {metadata.gridSize.x, metadata.gridSize.y, metadata.gridSize.z};
    j["voxelSize"] = metadata.voxelSize;
    j["creationTime"] = metadata.creationTime;
    j["modificationTime"] = metadata.modificationTime;
    j["population"] = metadata.population;
    j["generation"] = metadata.generation;
    j["tags"] = metadata.tags;
    return j;
}

bool SaveManager::deserializeMetadata(const nlohmann::json& json, PatternMetadata& metadata) const {
    try {
        metadata.name = json["name"].get<std::string>();
        metadata.description = json["description"].get<std::string>();
        metadata.author = json["author"].get<std::string>();
        metadata.version = json["version"].get<std::string>();
        metadata.ruleSet = json["ruleSet"].get<std::string>();
        
        auto gridSize = json["gridSize"].get<std::vector<float>>();
        metadata.gridSize = glm::vec3(gridSize[0], gridSize[1], gridSize[2]);
        
        metadata.voxelSize = json["voxelSize"].get<float>();
        metadata.creationTime = json["creationTime"].get<std::time_t>();
        metadata.modificationTime = json["modificationTime"].get<std::time_t>();
        metadata.population = json["population"].get<uint32_t>();
        metadata.generation = json["generation"].get<uint32_t>();
        metadata.tags = json["tags"].get<std::vector<std::string>>();
        
        return true;
    } catch (...) {
        return false;
    }
}

void SaveManager::cleanupOldAutoSaves(int maxAutoSaves) {
    try {
        std::vector<fs::directory_entry> autoSaves;
        for (const auto& entry : fs::directory_iterator(patternDirectory_)) {
            if (entry.is_regular_file() && 
                entry.path().extension() == ".pattern" && 
                entry.path().filename().string().find("autosave_") == 0) {
                autoSaves.push_back(entry);
            }
        }

        // Sort by last write time, newest first
        std::sort(autoSaves.begin(), autoSaves.end(),
            [](const auto& a, const auto& b) {
                return fs::last_write_time(a) > fs::last_write_time(b);
            });

        // Remove old autosaves
        for (size_t i = maxAutoSaves; i < autoSaves.size(); ++i) {
            fs::remove(autoSaves[i]);
        }
    } catch (const std::exception&) {
        // Log error but don't throw
    }
}

} // namespace VulkanHIP 