#pragma once

class App;

// Common interface for the three user-facing modes (placement, draw, gridEdit).
// The active mode owns the per-frame input handling, mode-specific grid
// overlays, and the side-panel content.
class IMode {
public:
    virtual ~IMode() = default;

    virtual void Enter() {}
    virtual void Exit()  {}

    // Per-frame input + state update.
    virtual void Update() {}

    // Mode-specific overlays drawn on top of the base grid.
    virtual void DrawWorld() {}

    // ImGui side-panel content (the surrounding window is created by App).
    virtual void DrawPanel() {}

    void SetApp(App* a) { app_ = a; }

protected:
    App* app_ = nullptr;
};
