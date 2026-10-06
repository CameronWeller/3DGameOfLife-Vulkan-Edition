// The menus: pause, settings, stamps & rules (the "inventory"), and new world.
// They are Dear ImGui windows, rebuilt every frame by buildUi(); see
// ui/Widgets.h for the building blocks and ui/Theme.h for the look.

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <stdexcept>

#include <GLFW/glfw3.h>

#include "game/Game.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "platform/Paths.h"
#include "ui/MenuFont.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace gol3d {
namespace {

using ui::px;

constexpr double MENU_FADE_SECONDS = 0.14;

// Vertical breathing room inside a card.
void gap(float height) {
    ImGui::Dummy(ImVec2(0, px(height)));
}

// A separator with a little room on both sides.
void divider() {
    gap(2);
    ImGui::Separator();
    gap(2);
}

// Settings rows: a label column and a control column that fills the rest.
void optionSection(const char* name, bool first = false) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    if (!first) gap(4);
    ui::sectionLabel(name);
}

void optionRow(const char* label) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

// A wide tooltip for the widget just drawn, shown at once.
void tooltip(const char* text, float width) {
    ImGui::SetNextWindowSize(ImVec2(px(width), 0));
    ImGui::BeginTooltip();
    ImGui::TextWrapped("%s", text);
    ImGui::EndTooltip();
}

const ImU32 WHITE = IM_COL32(255, 255, 255, 255);

} // namespace

// ------------------------------------------------------------------- setup

void Game::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // no imgui.ini next to the game
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ui::applyMenuStyle();
    baseStyle_ = ImGui::GetStyle();
    // Installed after the game's own GLFW callbacks, which ImGui then chains to.
    ImGui_ImplGlfw_InitForVulkan(window_, true);

    VkInstance instance = gpu_.instance;
    auto loadFunction = [](const char* name, void* user) {
        return vkGetInstanceProcAddr(*static_cast<VkInstance*>(user), name);
    };
    ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, loadFunction, static_cast<void*>(&instance));
    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = VK_API_VERSION_1_3;
    info.Instance = gpu_.instance;
    info.PhysicalDevice = gpu_.physicalDevice;
    info.Device = gpu_.device;
    info.QueueFamily = gpu_.queueFamily;
    info.Queue = gpu_.queue;
    info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE +
                              IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE + 4;
    info.MinImageCount = 2;
    info.ImageCount =
        std::max<uint32_t>(2, static_cast<uint32_t>(renderer_.swapchain().imageCount()));
    info.UseDynamicRendering = true;
    info.PipelineInfoMain.PipelineRenderingCreateInfo = renderer_.renderingInfo();
    if (!ImGui_ImplVulkan_Init(&info)) {
        throw std::runtime_error("Could not initialize the menu renderer.");
    }
    imguiReady_ = true;
}

void Game::shutdownImGui() {
    if (!imguiReady_) return;
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    imguiReady_ = false;
}

// Applies the GUI scale. Automatic scale follows the window height (1x at 720
// lines) in quarter steps. Dear ImGui rasterizes glyphs at the size they are
// drawn, so text stays sharp at every scale without rebuilding the font atlas.
void Game::updateUiScale() {
    float scale = static_cast<float>(settings_.guiScale);
    if (settings_.guiScale <= 0) {
        float height = static_cast<float>(renderer_.swapchain().extent().height);
        scale = std::clamp(std::round(4.0f * height / 720.0f) / 4.0f, 1.0f, 4.0f);
    }
    if (scale == uiScale_) return;
    if (uiScale_ == 0.0f) {
        ImFontConfig font;
        font.FontDataOwnedByAtlas = false; // the TTF lives in the executable (MenuFont.h)
        void* ttf = const_cast<unsigned char*>(MENU_FONT_TTF);
        titleFont_ = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(ttf, MENU_FONT_TTF_SIZE,
                                                                ui::BODY_FONT_SIZE, &font);
    }
    ImGuiStyle& style = ImGui::GetStyle();
    style = baseStyle_;
    style.ScaleAllSizes(scale);
    style.FontSizeBase = ui::BODY_FONT_SIZE;
    style.FontScaleMain = scale;
    uiScale_ = scale;
}

void Game::saveSettingsIfPersistent() {
    if (persistSettings_) saveSettings(settings_, settingsFilePath());
}

// --------------------------------------------------------------- per frame

void Game::buildUi() {
    if (!imguiReady_) return;
    updateUiScale();
    ui::setLayout(uiScale_, titleFont_);
    // While playing, the mouse belongs to the game unless the tutorial panel is
    // open and the cursor is free to click it.
    ImGuiIO& io = ImGui::GetIO();
    bool gameOwnsMouse = screen_ == Screen::Playing && (cursorCaptured_ || !tutorial_.active());
    if (gameOwnsMouse) {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    } else {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Menus fade in over a short moment instead of popping up.
    if (screen_ != shownScreen_) {
        shownScreen_ = screen_;
        menuOpenedAt_ = glfwGetTime();
    }
    float fade = 1.0f;
    if (!capturing_) {
        fade = static_cast<float>(
            std::clamp((glfwGetTime() - menuOpenedAt_) / MENU_FADE_SECONDS, 0.0, 1.0));
    }
    float alpha = screen_ == Screen::Playing ? 1.0f : fade * (2.0f - fade); // ease out
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    switch (screen_) {
        case Screen::Playing:
            drawHud();
            if (hudVisible_ && tutorial_.active()) {
                tutorial::Tutorial::Status status{describeRule(rule()), generation_, population_,
                                                  running_};
                handleTutorialRequest(tutorial_.draw(uiScale_, status));
            }
            break;
        case Screen::Paused:
            drawPauseMenu();
            break;
        case Screen::Settings:
            drawSettingsMenu();
            break;
        case Screen::Inventory:
            drawInventory();
            break;
        case Screen::NewWorld:
            drawNewWorldMenu();
            break;
    }
    ImGui::PopStyleVar();

    if (updater_ && !updateAnnounced_ && updater_->state() == Updater::State::Available) {
        updateAnnounced_ = true;
        notify("Update available: 3D Life " + updater_->release().version +
               ". Press Esc for details.");
    }
    drawToast();
    ImGui::Render();
}

// ------------------------------------------------------------------- pause

void Game::drawPauseMenu() {
    if (ui::beginCard("##pause", 340)) {
        ui::cardHeader("3D Life", "Paused");
        ui::statChips({{"RULE", rule().name},
                       {"GENERATION", ui::withCommas(generation_)},
                       {"ALIVE", ui::withCommas(population_)}});
        gap(2);
        if (ui::menuItem("Resume", "Esc", ui::ButtonKind::Primary)) resumeGame();
        gap(2);
        ui::sectionLabel("Simulation");
        drawSimulationControls(runningBeforePause_); // takes effect on resume
        divider();
        if (ui::menuItem("Stamps & Rules", "Tab")) screen_ = Screen::Inventory;
        if (ui::menuItem("Tutorial")) openTutorial(tutorial_.lessonIndex());
        if (ui::menuItem("New World...")) openNewWorldScreen();
        if (ui::menuItem("Save World", "Ctrl+S")) saveWorldWithMessage();
        if (ui::menuItem("Load World", "Ctrl+O")) loadWorldWithMessage();
        if (ui::menuItem("Settings")) screen_ = Screen::Settings;
        divider();
        if (ui::menuItem("Quit", "Ctrl+Q", ui::ButtonKind::Danger)) {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
        drawUpdatePanel();
    }
    ImGui::End();
}

// Run/pause, slower/faster and fast-forward, for players who don't know the keys.
void Game::drawSimulationControls(bool& running) {
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float fullWidth = ImGui::GetContentRegionAvail().x;
    const float height = ui::buttonHeight();
    const float smallWidth = height * 1.15f;
    const float toggleWidth = fullWidth * 0.34f;

    // [Running|Paused] [-] speed [+]
    if (running) ui::pushAccentButton();
    bool toggled = ImGui::Button(running ? "Running" : "Paused", ImVec2(toggleWidth, height));
    if (running) ui::popAccentButton();
    if (toggled) {
        running = !running;
        stepDebt_ = 0.0; // the speed starts fresh, as with G
    }
    ui::hoverHint("G in game");
    ImGui::SameLine();
    if (ImGui::Button("-##slower", ImVec2(smallWidth, height))) changeSpeed(-1);
    ui::hoverHint("Half as fast ([)");
    ImGui::SameLine();
    const std::string label = speed_.unlimited() ? std::string("max speed") : speed_.label();
    const float labelWidth = fullWidth - toggleWidth - 2.0f * smallWidth - 3.0f * spacing;
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(labelWidth, height));
    const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(at.x + (labelWidth - textSize.x) * 0.5f, at.y + (height - textSize.y) * 0.5f), WHITE,
        label.c_str());
    ImGui::SameLine();
    if (ImGui::Button("+##faster", ImVec2(smallWidth, height))) changeSpeed(1);
    ui::hoverHint("Twice as fast (])");

    // [Skip 100 generations] [Skip 1,000], or [Cancel fast-forward] while one runs.
    const float halfWidth = (fullWidth - spacing) * 0.5f;
    const bool forwarding = fastForward_ > 0;
    const char* skipLabel = forwarding ? "Cancel fast-forward" : "Skip 100 generations";
    if (ImGui::Button(skipLabel, ImVec2(forwarding ? fullWidth : halfWidth, height))) {
        queueFastForward(100);
    }
    if (!forwarding) {
        ui::hoverHint("J in game. Runs as fast as the GPU allows, then returns to the set speed.");
        ImGui::SameLine();
        if (ImGui::Button("Skip 1,000", ImVec2(halfWidth, height))) queueFastForward(1000);
        ui::hoverHint("Shift+J in game");
    }
}

// Update status and actions at the bottom of the pause menu.
void Game::drawUpdatePanel() {
    if (!updater_) return;
    using State = Updater::State;
    const State state = updater_->state();
    if (state == State::Checking || state == State::Idle) return;
    const ReleaseInfo release = updater_->release();
    const ImU32 noticeColor = ui::color(ui::palette::NOTICE);
    divider();
    switch (state) {
        case State::Available: {
            ui::note("Update available: 3D Life " + release.version + " (you have " +
                         updater_->currentVersion() + ")",
                     noticeColor);
            bool installs = updater_->method() != InstallMethod::OpenPage;
            if (installs &&
                ui::menuItem("Download and Install", nullptr, ui::ButtonKind::Primary)) {
                updater_->installAsync();
            }
            if (ui::menuItem(installs ? "Release Notes" : "Open Download Page")) {
                openInBrowser(release.pageUrl);
            }
            break;
        }
        case State::Downloading:
            ui::note("Downloading and verifying 3D Life " + release.version + "...", noticeColor);
            break;
        case State::Ready:
            if (updater_->method() == InstallMethod::AppImage) {
                ui::note("Updated to " + release.version + ". Restart to use it.",
                         ui::accentColor());
                if (ui::menuItem("Restart Now", nullptr, ui::ButtonKind::Primary)) {
                    restartPath_ = updater_->appImagePath();
                    glfwSetWindowShouldClose(window_, GLFW_TRUE);
                }
            } else {
                ui::note("3D Life " + release.version + " is downloaded and verified.",
                         ui::accentColor());
                bool install =
                    ui::menuItem("Install and Restart", nullptr, ui::ButtonKind::Primary);
                if (install && updater_->launchInstaller()) {
                    glfwSetWindowShouldClose(window_, GLFW_TRUE);
                }
            }
            break;
        case State::Failed:
            ui::note(updater_->error(), ui::color(ui::palette::PROBLEM));
            if (ui::menuItem("Open Download Page")) openInBrowser(RELEASES_PAGE);
            break;
        case State::UpToDate:
            ui::mutedText("3D Life " + updater_->currentVersion() + " is up to date.");
            break;
        case State::Checking:
        case State::Idle:
            break;
    }
}

// ---------------------------------------------------------------- settings

void Game::drawSettingsMenu() {
    if (ui::beginCard("##settings", 440)) {
        ui::cardHeader("Settings");
        if (ImGui::BeginTable("##options", 2)) {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, px(150));
            ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);

            optionSection("View", true);
            optionRow("Field of view");
            ImGui::SliderFloat("##fov", &settings_.fov, Settings::MIN_FOV, Settings::MAX_FOV,
                               "%.0f");
            optionRow("Render distance");
            if (ImGui::SliderInt("##render", &settings_.renderDistance,
                                 Settings::MIN_RENDER_DISTANCE, Settings::MAX_RENDER_DISTANCE,
                                 "%d blocks")) {
                refreshPending_ = true; // the block list is culled to the render distance
            }
            optionRow("Fullscreen");
            bool wantFullscreen = fullscreen_;
            if (ImGui::Checkbox("##fullscreen", &wantFullscreen)) toggleFullscreen();
            optionRow("Smooth lighting");
            ImGui::Checkbox("##ao", &settings_.smoothLighting);
            ui::hoverHint("Shades block corners by the blocks around them.");
            optionRow("Animate changes");
            ImGui::Checkbox("##animate", &settings_.animate);
            ui::hoverHint("At slow speeds, newborn cells grow in and dying ones shrink away.");

            optionSection("Mouse");
            optionRow("Sensitivity");
            ImGui::SliderInt("##sens", &settings_.sensitivity, Settings::MIN_SENSITIVITY,
                             Settings::MAX_SENSITIVITY, "%d%%");
            optionRow("Invert vertical");
            ImGui::Checkbox("##invert", &settings_.invertY);

            optionSection("Interface");
            optionRow("GUI scale");
            ImGui::SliderInt("##gui", &settings_.guiScale, 0, Settings::MAX_GUI_SCALE,
                             settings_.guiScale == 0 ? "Auto" : "%dx");
            optionRow("Show HUD");
            ImGui::Checkbox("##hud", &hudVisible_);
            optionRow("Chunk borders");
            ImGui::Checkbox("##borders", &showChunkBorders_);

            optionSection("Simulation");
            optionRow("Speed");
            int exponent = speed_.exponent();
            std::string speedText =
                speed_.unlimited() ? std::string("As fast as possible") : speed_.label();
            if (ImGui::SliderInt("##speed", &exponent, SimulationSpeed::MIN_EXPONENT,
                                 SimulationSpeed::UNLIMITED_EXPONENT, speedText.c_str())) {
                speed_.setExponent(exponent);
            }
            ui::hoverHint("[ and ] halve or double it in game; Shift jumps 8x.");
            optionRow("Time per frame");
            ImGui::SliderInt("##budget", &settings_.simBudget, Settings::MIN_SIM_BUDGET,
                             Settings::MAX_SIM_BUDGET, "%d ms");
            const std::string budgetHint =
                "Most time each frame may spend simulating. When a world needs more, the "
                "simulation slows down instead of the frame rate. Fast-forward (J) and max "
                "speed use at least " +
                std::to_string(FLAT_OUT_MIN_BUDGET_MS) + " ms.";
            ui::hoverHint(budgetHint.c_str());

            optionSection("Updates");
            optionRow("Check at startup");
            ImGui::Checkbox("##updates", &settings_.checkUpdates);
            ImGui::SameLine();
            if (ImGui::Button("Check now")) {
                if (!updater_) updater_ = std::make_unique<Updater>(GOL3D_VERSION_STRING, exeDir_);
                updateAnnounced_ = false;
                updater_->checkAsync();
            }
            ImGui::EndTable();
        }
        gap(6);
        switch (ui::buttonPair("Reset to Defaults", "Done", ui::ButtonKind::Primary)) {
            case ui::PairChoice::Left:
                settings_ = Settings{};
                break;
            case ui::PairChoice::Right:
                saveSettingsIfPersistent();
                screen_ = Screen::Paused;
                break;
            case ui::PairChoice::None:
                break;
        }
        if (persistSettings_) ui::mutedText("Saved to " + settingsFilePath().string());
    }
    ImGui::End();
}

// ------------------------------------------------------- stamps and rules

void Game::drawInventory() {
    if (ui::beginCard("##inventory", 760)) {
        ui::cardHeader("Stamps & Rules", "Tab to close");
        // Two columns: what to build on the left, the rule on the right.
        ImGui::BeginTable("##inventory-columns", 2, ImGuiTableFlags_BordersInnerV);
        ImGui::TableSetupColumn("build", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("rule", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        drawStampPicker();
        gap(6);
        drawMaterialPicker();
        ImGui::TableSetColumnIndex(1);
        drawRulePicker();
        ImGui::EndTable();

        gap(6);
        switch (ui::buttonPair("New World with This Rule", "Done", ui::ButtonKind::Primary)) {
            case ui::PairChoice::Left:
                newWorld(true);
                notify(std::string("New world: ") + rule().name);
                closeInventory();
                break;
            case ui::PairChoice::Right:
                closeInventory();
                break;
            case ui::PairChoice::None:
                break;
        }
    }
    ImGui::End();
}

// The hotbar as clickable tiles, with the empty hand first.
void Game::drawStampPicker() {
    ui::sectionLabel("Stamps");
    ui::mutedText("Left click places, right click removes. Q/E rotate, Z/C tilt around x.");
    ImDrawList* draw = ImGui::GetWindowDrawList();
    constexpr int TILES_PER_ROW = 5;
    const float spacing = px(6);
    const float tile = std::floor(
        (ImGui::GetContentRegionAvail().x - spacing * (TILES_PER_ROW - 1)) / TILES_PER_ROW);
    for (int slot = -1; slot < STAMP_COUNT; ++slot) {
        const int tileIndex = slot + 1; // the empty hand is tile 0
        if (tileIndex % TILES_PER_ROW != 0) ImGui::SameLine(0, spacing);
        ImGui::PushID(slot);
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max(min.x + tile, min.y + tile);
        const bool clicked = ImGui::InvisibleButton("tile", ImVec2(tile, tile));
        const bool hovered = ImGui::IsItemHovered();
        draw->AddRectFilled(
            min, max, ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg),
            px(6));
        if (slot == brush_.hotbarSlot) {
            draw->AddRectFilled(min, max, ui::accentColor(40), px(6));
            draw->AddRect(min, max, ui::accentColor(), px(6), px(2));
        }
        if (slot >= 0) {
            ui::drawIcon5x5(draw, min, tile, stampInfos()[slot].icon,
                            ui::materialColor(brush_.material));
        } else {
            // The empty hand: a slashed circle.
            const ImVec2 center(min.x + tile * 0.5f, min.y + tile * 0.5f);
            const float radius = tile * 0.22f;
            const float slash = radius * 0.7f;
            const ImU32 muted = ImGui::GetColorU32(ImGuiCol_TextDisabled);
            draw->AddCircle(center, radius, muted, 0, px(1.5f));
            draw->AddLine(ImVec2(center.x - slash, center.y + slash),
                          ImVec2(center.x + slash, center.y - slash), muted, px(1.5f));
        }
        if (hovered) {
            if (slot >= 0) {
                const StampInfo& info = stampInfos()[slot];
                ImGui::SetTooltip("%d. %s\n%s", slot + 1, info.name, info.description);
            } else {
                ImGui::SetTooltip("Empty hand\nNothing to place and no placement outline.");
            }
        }
        if (clicked) selectSlot(slot);
        ImGui::PopID();
    }
    if (!brush_.emptyHand()) {
        const StampInfo& info = stampInfos()[brush_.hotbarSlot];
        ui::note(std::string(info.name) + ": " + info.description, WHITE);
    } else {
        ui::note("Empty hand: nothing to place.", WHITE);
    }
}

// What stamps are made of: Life follows the rule, the others are static blocks.
void Game::drawMaterialPicker() {
    ui::sectionLabel("Material");
    ui::mutedText("What stamps are made of. M cycles through them in game.");
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float spacing = px(6);
    const float count = static_cast<float>(cellTypes().size());
    const float width =
        std::floor((ImGui::GetContentRegionAvail().x - spacing * (count - 1.0f)) / count);
    const float height = ImGui::GetFrameHeight() * 1.9f;
    for (const CellType& type : cellTypes()) {
        if (type.kind != CellKind::Life) ImGui::SameLine(0, spacing);
        ImGui::PushID(static_cast<int>(type.kind));
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max(min.x + width, min.y + height);
        const bool clicked = ImGui::InvisibleButton("material", ImVec2(width, height));
        const bool hovered = ImGui::IsItemHovered();
        draw->AddRectFilled(
            min, max, ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg),
            px(6));
        if (type.kind == brush_.material) {
            draw->AddRectFilled(min, max, ui::materialColor(type.kind, 36), px(6));
            draw->AddRect(min, max, ui::materialColor(type.kind), px(6), px(2));
        }
        const float icon = height * 0.5f;
        ui::drawCubeIcon(draw, ImVec2(min.x + px(10) + icon * 0.5f, min.y + height * 0.5f), icon,
                         type.rgb);
        const ImVec2 textSize = ImGui::CalcTextSize(type.name);
        draw->AddText(ImVec2(min.x + px(18) + icon, min.y + (height - textSize.y) * 0.5f), WHITE,
                      type.name);
        if (hovered) tooltip(type.description, 300);
        if (clicked) brush_.material = type.kind;
        ImGui::PopID();
    }
    const CellType& selected = cellType(brush_.material);
    ui::note(std::string(selected.name) + ": " + selected.description, WHITE);
}

void Game::drawRulePicker() {
    ui::sectionLabel("Rule");
    ui::mutedText("Switching rules keeps the current cells.");
    const float listHeight =
        ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(lifeRules().size()) +
        ImGui::GetStyle().WindowPadding.y * 2.0f;
    ImGui::BeginChild("##rules", ImVec2(0, listHeight), ImGuiChildFlags_Borders);
    for (size_t index = 0; index < lifeRules().size(); ++index) {
        const LifeRule& candidate = lifeRules()[index];
        const std::string notation = describeRule(candidate);
        ImGui::PushID(static_cast<int>(index));
        if (ImGui::Selectable(candidate.name, index == ruleIndex_)) {
            ruleIndex_ = index;
            announceRule();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetNextWindowSize(ImVec2(px(320), 0));
            ImGui::BeginTooltip();
            ImGui::TextWrapped("%s", explainRule(candidate).c_str());
            ImGui::Spacing();
            ImGui::TextWrapped("%s", candidate.description);
            ImGui::EndTooltip();
        }
        ui::rightAlignNext(ImGui::CalcTextSize(notation.c_str()).x);
        ImGui::TextDisabled("%s", notation.c_str());
        ImGui::PopID();
    }
    ImGui::EndChild();
    ui::note(std::string(rule().name) + ": " + explainRule(rule()), WHITE);
    ui::mutedText(rule().description);
}

// --------------------------------------------------------------- new world

void Game::drawNewWorldMenu() {
    if (ui::beginCard("##newworld", 440)) {
        ui::cardHeader("New World");
        const LifeRule& chosen = lifeRules()[newWorldForm_.rule];
        if (ImGui::BeginTable("##newworld-options", 2)) {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, px(100));
            ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);
            optionRow("Rule");
            if (ImGui::BeginCombo("##rule", chosen.name)) {
                for (int index = 0; index < static_cast<int>(lifeRules().size()); ++index) {
                    if (ImGui::Selectable(lifeRules()[index].name, index == newWorldForm_.rule)) {
                        newWorldForm_.rule = index;
                    }
                    if (ImGui::IsItemHovered()) {
                        tooltip(explainRule(lifeRules()[index]).c_str(), 300);
                    }
                }
                ImGui::EndCombo();
            }
            optionRow("Seed");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - px(80));
            ImGui::InputInt("##seed", &newWorldForm_.seed, 0);
            ImGui::SameLine();
            if (ImGui::Button("Random", ImVec2(-FLT_MIN, 0))) {
                newWorldForm_.seed = newRandomSeed();
            }
            optionRow("Start with");
            if (ImGui::RadioButton("Rule's seed soup", !newWorldForm_.empty)) {
                newWorldForm_.empty = false;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Empty world", newWorldForm_.empty)) newWorldForm_.empty = true;
            ImGui::EndTable();
        }
        gap(4);
        // Re-read the rule: the combo may have changed it.
        const LifeRule& described = lifeRules()[newWorldForm_.rule];
        ui::note(explainRule(described), WHITE);
        ui::mutedText(described.description);
        gap(6);
        switch (ui::buttonPair("Cancel", "Create World", ui::ButtonKind::Primary)) {
            case ui::PairChoice::Left:
                screen_ = Screen::Paused;
                break;
            case ui::PairChoice::Right:
                ruleIndex_ = static_cast<size_t>(newWorldForm_.rule);
                rng_.seed(static_cast<uint32_t>(newWorldForm_.seed));
                newWorld(!newWorldForm_.empty);
                notify(std::string("New world: ") + rule().name +
                       (newWorldForm_.empty ? " (empty)" : ""));
                resumeGame();
                break;
            case ui::PairChoice::None:
                break;
        }
    }
    ImGui::End();
}

} // namespace gol3d
