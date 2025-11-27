#pragma once

#include "UXTestingFramework.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace UXTesting {

// Minecraft-specific configuration
struct MinecraftConfig {
    std::string launcherPath;           // Path to Minecraft launcher
    std::string username;               // Minecraft username
    std::string password;               // Minecraft password (use secure storage in production)
    std::string version;                // Minecraft version to test
    std::string worldName;              // World name for testing
    bool isBedrock = false;             // Bedrock vs Java edition
    int windowWidth = 1920;
    int windowHeight = 1080;
    bool fullscreen = false;
};

// Minecraft UI element locations (relative coordinates)
struct MinecraftUIElements {
    // Main menu elements
    struct {
        int x = 960, y = 540;           // Center of screen
        int width = 200, height = 20;   // Play button
    } playButton;
    
    struct {
        int x = 960, y = 580;           // Options button
        int width = 200, height = 20;
    } optionsButton;
    
    struct {
        int x = 960, y = 620;           // Quit button
        int width = 200, height = 20;
    } quitButton;
    
    // Game UI elements
    struct {
        int x = 50, y = 50;             // Inventory button
        int width = 20, height = 20;
    } inventoryButton;
    
    struct {
        int x = 960, y = 1000;          // Hotbar center
        int width = 400, height = 20;
    } hotbar;
    
    struct {
        int x = 50, y = 1000;           // Health bar
        int width = 100, height = 10;
    } healthBar;
    
    struct {
        int x = 50, y = 1020;           // Hunger bar
        int width = 100, height = 10;
    } hungerBar;
    
    struct {
        int x = 1820, y = 50;           // Experience bar
        int width = 50, height = 200;
    } experienceBar;
};

// Minecraft test scenarios
class MinecraftTestScenario : public TestScenario {
public:
    MinecraftTestScenario(const std::string& name, const MinecraftConfig& config);
    ~MinecraftTestScenario() override;
    
    // Minecraft-specific scenario builders
    void addLoginSequence();
    void addWorldCreationSequence(const std::string& worldName, const std::string& worldType = "default");
    void addBasicMovementSequence();
    void addInventoryInteractionSequence();
    void addCraftingSequence();
    void addMiningSequence();
    void addBuildingSequence();
    void addCombatSequence();
    void addSaveAndQuitSequence();
    
    // Utility methods
    void addWaitForGameLoad();
    void addWaitForMenuLoad();
    void addWaitForWorldLoad();
    
private:
    MinecraftConfig minecraftConfig_;
    MinecraftUIElements uiElements_;
    
    // Helper methods
    void addMouseClickAtUI(const std::string& elementName, int offsetX = 0, int offsetY = 0);
    void addKeyPressForMovement(const std::string& direction);
    void addInventorySlotClick(int slotNumber);
};

// Minecraft UX Testing Manager
class MinecraftUXTestingManager {
public:
    MinecraftUXTestingManager();
    ~MinecraftUXTestingManager();
    
    // Configuration
    void setMinecraftConfig(const MinecraftConfig& config);
    void setTestWorldPath(const std::string& path);
    void setBackupWorldPath(const std::string& path);
    
    // Test scenario management
    void createBasicTestSuite();
    void createPerformanceTestSuite();
    void createCompatibilityTestSuite();
    void createAccessibilityTestSuite();
    
    // World management
    bool backupWorld();
    bool restoreWorld();
    bool createTestWorld();
    bool deleteTestWorld();
    
    // Test execution
    std::vector<TestResult> runBasicTests();
    std::vector<TestResult> runPerformanceTests();
    std::vector<TestResult> runCompatibilityTests();
    std::vector<TestResult> runAccessibilityTests();
    std::vector<TestResult> runAllTests();
    
    // Reporting
    void generateTestReport(const std::string& outputPath);
    void generatePerformanceReport(const std::string& outputPath);
    
private:
    UXTestingFramework framework_;
    MinecraftConfig minecraftConfig_;
    std::string testWorldPath_;
    std::string backupWorldPath_;
    std::vector<std::shared_ptr<MinecraftTestScenario>> scenarios_;
    
    // Helper methods
    void setupFramework();
    void createScenario(const std::string& name, std::function<void(MinecraftTestScenario&)> configurator);
    std::string getTimestampedWorldName();
};

// Predefined Minecraft test scenarios
namespace MinecraftScenarios {
    
    // Basic functionality tests
    std::shared_ptr<MinecraftTestScenario> createLoginTest(const MinecraftConfig& config);
    std::shared_ptr<MinecraftTestScenario> createWorldCreationTest(const MinecraftConfig& config);
    std::shared_ptr<MinecraftTestScenario> createBasicMovementTest(const MinecraftConfig& config);
    std::shared_ptr<MinecraftTestScenario> createInventoryTest(const MinecraftConfig& config);
    
    // Advanced functionality tests
    std::shared_ptr<MinecraftTestScenario> createCraftingTest(const MinecraftConfig& config);
    std::shared_ptr<MinecraftTestScenario> createMiningTest(const MinecraftConfig& config);
    std::shared_ptr<MinecraftTestScenario> createBuildingTest(const MinecraftConfig& config);
    std::shared_ptr<MinecraftTestScenario> createCombatTest(const MinecraftConfig& config);
    
    // Performance tests
    std::shared_ptr<MinecraftTestScenario> createFPSStressTest(const MinecraftConfig& config);
    std::shared_ptr<MinecraftTestScenario> createMemoryLeakTest(const MinecraftConfig& config);
    std::shared_ptr<MinecraftTestScenario> createLoadTimeTest(const MinecraftConfig& config);
    
    // Compatibility tests
    std::shared_ptr<MinecraftTestScenario> createResolutionTest(const MinecraftConfig& config);
    std::shared_ptr<MinecraftTestScenario> createFullscreenTest(const MinecraftConfig& config);
    std::shared_ptr<MinecraftTestScenario> createModCompatibilityTest(const MinecraftConfig& config);
    
    // Accessibility tests
    std::shared_ptr<MinecraftTestScenario> createKeyboardNavigationTest(const MinecraftConfig& config);
    std::shared_ptr<MinecraftTestScenario> createColorBlindnessTest(const MinecraftConfig& config);
    std::shared_ptr<MinecraftTestScenario> createHighContrastTest(const MinecraftConfig& config);
}

} // namespace UXTesting 