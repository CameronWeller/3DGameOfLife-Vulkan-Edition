#pragma once

#include <vector>
#include <memory>
#include "AppState.h"
#include "SaveManager.h"

namespace VulkanHIP {

class MenuSystem {
public:
    MenuSystem();
    ~MenuSystem();
    
    MenuSystem(const MenuSystem&) = delete;
    MenuSystem& operator=(const MenuSystem&) = delete;
    
    void initialize(SaveManager* saveManager);
    void drawMenu();
    void drawSavePicker();
    void drawSettings();
    void drawNewProjectDialog();
    
    // State management
    App::State getCurrentState() const { return currentState_; }
    void setCurrentState(App::State state) { currentState_ = state; }
    
    // Menu state getters
    bool shouldShowSavePicker() const { return showSavePicker_; }
    bool shouldShowNewProjectDialog() const { return showNewProjectDialog_; }
    bool shouldShowSettings() const { return showSettings_; }
    
private:
    App::State currentState_ = App::State::Menu;
    App::MenuState menuState_;
    SaveManager* saveManager_ = nullptr;
    
    bool showSavePicker_ = false;
    bool showNewProjectDialog_ = false;
    bool showSettings_ = false;
    
    std::vector<App::SaveInfo> saveFiles_;
    int selectedSaveIndex_ = -1;
    
    void refreshSaveFiles();
};

} // namespace VulkanHIP