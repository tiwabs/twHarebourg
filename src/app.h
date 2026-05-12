#pragma once

#include <memory>

#include "control_window.h"
#include "grid.h"
#include "hotkey_manager.h"
#include "logic.h"
#include "mode.h"
#include "overlay_window.h"

struct ImGuiContext;

class App {
public:
    enum class ModeKind { Placement, Draw, GridEdit };

    enum HotkeyId : int {
        HK_PLACEMENT_TOGGLE = 1,
        HK_QUIT,
        HK_INC_MELEE,
        HK_DEC_MELEE,
        HK_RESET_MELEE,
        HK_GRID_TOGGLE,
        HK_SAVE,
    };

    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool Ok() const { return overlay_.Ok() && control_.Ok(); }

    int Run();

    OverlayWindow& Overlay() { return overlay_; }
    ControlWindow& Control() { return control_; }
    Grid&          GetGrid() { return grid_;    }
    Logic&         GetLogic(){ return logic_;   }
    ModeKind       CurrentMode() const { return modeKind_; }

    void EnterPlacement();
    void EnterDraw();
    void EnterDrawAfterNewMap();
    void EnterGridEdit();

    // Subclass-callable handlers.
    void OnHotkey(int id);

private:
    OverlayWindow  overlay_;
    ControlWindow  control_;
    HotkeyManager  hotkeys_;
    Grid           grid_;
    Logic          logic_;

    std::unique_ptr<IMode> mode_;
    ModeKind               modeKind_ = ModeKind::Placement;

    ImGuiContext* overlayCtx_ = nullptr;
    ImGuiContext* controlCtx_ = nullptr;

    void SetMode(std::unique_ptr<IMode> next, ModeKind kind);
    void RegisterHotkeys();
    void DrawPlayerTargetOverlays();
    void RenderOverlayFrame();
    void RenderControlFrame();

    static LRESULT CALLBACK SubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
};
