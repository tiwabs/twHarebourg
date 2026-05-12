#include "app.h"

#include "draw_mode.h"
#include "grid_edit_mode.h"
#include "placement_mode.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include <cmath>

#include <commctrl.h>
#include <windows.h>
#include <windowsx.h>

#include <memory>
#include <optional>
#include <utility>

namespace {

constexpr int kHotkeyMod = MOD_NOREPEAT;

// Pixel sizes of the ImGui title bar elements. Slightly oversized to make
// the drag area forgiving.
constexpr int kTitleBarHeight   = 28;
constexpr int kCloseButtonWidth = 28;

void InitImGuiForWindow(GLFWwindow* window) {
    glfwMakeContextCurrent(window);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void ShutdownImGuiContext(ImGuiContext* ctx) {
    if (!ctx) return;
    ImGui::SetCurrentContext(ctx);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(ctx);
}

// Subclass procedure for the borderless control window: makes the ImGui
// title bar draggable natively. WM_NCHITTEST returns HTCAPTION over the
// drag area, so Windows handles the move loop. The close button on the
// right is left as HTCLIENT so ImGui can intercept the click.
LRESULT CALLBACK ControlSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                     UINT_PTR, DWORD_PTR) {
    switch (msg) {
        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hWnd, &pt);
            RECT rc;
            GetClientRect(hWnd, &rc);
            if (pt.y >= 0 && pt.y < kTitleBarHeight &&
                pt.x >= 0 && pt.x < rc.right - kCloseButtonWidth) {
                return HTCAPTION;
            }
            break;
        }
        case WM_NCLBUTTONDBLCLK:
            // Suppress the default "maximize on title-bar double-click" behavior.
            return 0;
        default:
            break;
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

}

App::App()
    : overlay_(),
      control_(),
      hotkeys_(overlay_.HWnd()) {
    if (!overlay_.Ok() || !control_.Ok()) return;

    if (!glfwGetWindowAttrib(overlay_.Glfw(), GLFW_TRANSPARENT_FRAMEBUFFER)) {
        MessageBoxW(nullptr,
                    L"GLFW_TRANSPARENT_FRAMEBUFFER is not supported on this system.",
                    L"twHarebourg",
                    MB_ICONWARNING);
    }

    IMGUI_CHECKVERSION();

    overlayCtx_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(overlayCtx_);
    InitImGuiForWindow(overlay_.Glfw());

    controlCtx_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(controlCtx_);
    InitImGuiForWindow(control_.Glfw());
    // Disable safe-area padding so our auto-fit window can match the OS window
    // size exactly (otherwise ImGui clamps auto-fit to viewport - 6px which
    // leaves a black strip at the bottom and triggers a phantom scrollbar).
    ImGui::GetStyle().DisplaySafeAreaPadding = ImVec2(0.0f, 0.0f);

    SetWindowSubclass(overlay_.HWnd(), SubclassProc, 0, reinterpret_cast<DWORD_PTR>(this));
    SetWindowSubclass(control_.HWnd(), ControlSubclassProc, 0, 0);
    RegisterHotkeys();

    if (!grid_.LoadFromFile("config.json")) {
        MessageBoxW(nullptr,
                    L"Failed to load config.json.",
                    L"twHarebourg",
                    MB_ICONWARNING);
    }
    grid_.SwitchToMap(0);

    EnterPlacement();
}

App::~App() {
    if (mode_) mode_->Exit();
    mode_.reset();

    ShutdownImGuiContext(overlayCtx_);
    ShutdownImGuiContext(controlCtx_);
    overlayCtx_ = controlCtx_ = nullptr;

    hotkeys_.UnregisterAll();
    if (overlay_.HWnd())
        RemoveWindowSubclass(overlay_.HWnd(), SubclassProc, 0);
    if (control_.HWnd())
        RemoveWindowSubclass(control_.HWnd(), ControlSubclassProc, 0);
}

void App::RegisterHotkeys() {
    if (!hotkeys_.Register(HK_PLACEMENT_TOGGLE, kHotkeyMod, VK_F2)    ||
        !hotkeys_.Register(HK_QUIT,             kHotkeyMod, VK_F10)   ||
        !hotkeys_.Register(HK_INC_MELEE,        kHotkeyMod, VK_PRIOR) ||
        !hotkeys_.Register(HK_DEC_MELEE,        kHotkeyMod, VK_NEXT)  ||
        !hotkeys_.Register(HK_RESET_MELEE,      kHotkeyMod, VK_DELETE)||
        !hotkeys_.Register(HK_GRID_TOGGLE,      kHotkeyMod, VK_F8)    ||
        !hotkeys_.Register(HK_SAVE,             kHotkeyMod, VK_F9)) {
        MessageBoxW(nullptr,
                    L"Failed to register one or more hotkeys. F2/F8/F9/F10/PgUp/PgDwn/Suppr may already be used.",
                    L"twHarebourg",
                    MB_ICONWARNING);
    }
}

void App::SetMode(std::unique_ptr<IMode> next, ModeKind kind) {
    if (mode_) mode_->Exit();
    mode_     = std::move(next);
    modeKind_ = kind;
    if (mode_) {
        mode_->SetApp(this);
        mode_->Enter();
    }
}

void App::EnterPlacement() {
    SetMode(std::make_unique<PlacementMode>(), ModeKind::Placement);
}

void App::EnterDraw() {
    overlay_.SetClickThrough(false);
    SetMode(std::make_unique<DrawMode>(), ModeKind::Draw);
}

void App::EnterDrawAfterNewMap() {
    auto snap = grid_.GetGridSnapshot();

    const int rows = grid_.Rows() > 0 ? grid_.Rows() : 20;
    const int cols = grid_.Cols() > 0 ? grid_.Cols() : 20;
    grid_.AddMap("Nouvelle map", rows, cols);
    grid_.SwitchToMap(grid_.MapCount() - 1);

    logic_.player = std::nullopt;
    logic_.target = std::nullopt;

    overlay_.SetClickThrough(false);

    auto dm = std::make_unique<DrawMode>();
    dm->SetPreSnapshot(std::move(snap));
    SetMode(std::move(dm), ModeKind::Draw);
}

void App::EnterGridEdit() {
    overlay_.SetClickThrough(false);
    SetMode(std::make_unique<GridEditMode>(), ModeKind::GridEdit);
}

void App::OnHotkey(int id) {
    switch (id) {
        case HK_PLACEMENT_TOGGLE:
            overlay_.SetClickThrough(!overlay_.ClickThrough());
            break;
        case HK_QUIT:
            overlay_.RequestClose();
            break;
        case HK_INC_MELEE:
            logic_.IncMelee();
            break;
        case HK_DEC_MELEE:
            logic_.DecMelee();
            break;
        case HK_RESET_MELEE:
            logic_.ResetMelee();
            break;
        case HK_GRID_TOGGLE:
            if (modeKind_ == ModeKind::GridEdit) EnterPlacement();
            else                                  EnterGridEdit();
            break;
        case HK_SAVE:
            grid_.SaveToFile("config.json");
            break;
        default:
            break;
    }
}

LRESULT CALLBACK App::SubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                   UINT_PTR, DWORD_PTR refData) {
    App* app = reinterpret_cast<App*>(refData);
    if (app) {
        switch (msg) {
            case WM_MOUSEACTIVATE:
                return MA_NOACTIVATE;
            case WM_HOTKEY:
                app->OnHotkey(static_cast<int>(wParam));
                return 0;
            default:
                break;
        }
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

void App::DrawPlayerTargetOverlays() {
    if (logic_.player) {
        grid_.DrawNoLoSOverlay(*logic_.player);
        grid_.FillCell(logic_.player->c, logic_.player->r,
                       IM_COL32(0, 120, 255, 90),
                       IM_COL32(255, 255, 255, 120));
    }
    if (logic_.target) {
        grid_.FillCell(logic_.target->c, logic_.target->r,
                       IM_COL32(255, 0, 0, 90),
                       IM_COL32(255, 255, 255, 120));
    }
    if (auto suggested = logic_.SuggestedCastCell(grid_)) {
        grid_.FillCell(suggested->c, suggested->r,
                       IM_COL32(0, 220, 255, 120),
                       IM_COL32(255, 255, 255, 120));
        if (logic_.player) {
            grid_.DrawCellLine(logic_.player->c, logic_.player->r,
                               suggested->c, suggested->r,
                               IM_COL32(0, 220, 255, 190), 3.0f);
        }
        grid_.DrawCellMarker(suggested->c, suggested->r,
                             IM_COL32(0, 220, 255, 210), nullptr);
    }
}

void App::RenderOverlayFrame() {
    glfwMakeContextCurrent(overlay_.Glfw());
    ImGui::SetCurrentContext(overlayCtx_);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (mode_) mode_->Update();

    grid_.Draw();
    if (mode_) mode_->DrawWorld();
    DrawPlayerTargetOverlays();

    ImGui::Render();

    int fbW = 0, fbH = 0;
    overlay_.GetFramebufferSize(fbW, fbH);
    glViewport(0, 0, fbW, fbH);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    overlay_.SwapBuffers();
}

void App::RenderControlFrame() {
    glfwMakeContextCurrent(control_.Glfw());
    ImGui::SetCurrentContext(controlCtx_);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Fixed width. Height is driven by the unclamped ideal content size below
    // — ImGui::GetWindowSize() would already be clamped to the OS window's
    // current height (the viewport), causing a feedback loop that collapses
    // the window down to just the title bar.
    constexpr float kPanelWidth = 330.0f;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(kPanelWidth, 0.0f), ImGuiCond_Always);

    bool open = true;
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize   |
        ImGuiWindowFlags_NoMove     |
        ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("twHarebourg", &open, flags)) {
        if (mode_) mode_->DrawPanel();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));   // breathing room below the last item
    }
    ImGui::End();

    if (!open) control_.RequestClose();

    if (ImGuiWindow* win = ImGui::FindWindowByName("twHarebourg")) {
        const float padding  = ImGui::GetStyle().WindowPadding.y;
        const float desiredH = win->ContentSizeIdeal.y + win->TitleBarHeight + padding * 2.0f;
        const int newW = static_cast<int>(kPanelWidth);
        const int newH = static_cast<int>(std::ceil(desiredH));
        int curW = 0, curH = 0;
        control_.GetSize(curW, curH);
        if (newW > 0 && newH > 0 && (curW != newW || curH != newH))
            control_.SetSize(newW, newH);
    }

    ImGui::Render();

    int fbW = 0, fbH = 0;
    control_.GetFramebufferSize(fbW, fbH);
    glViewport(0, 0, fbW, fbH);
    glClearColor(0.10f, 0.10f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    control_.SwapBuffers();
}

int App::Run() {
    while (!overlay_.ShouldClose() && !control_.ShouldClose()) {
        glfwPollEvents();
        RenderOverlayFrame();
        RenderControlFrame();
    }
    return 0;
}
