#include "grid_edit_mode.h"

#include "app.h"
#include "ui_panel.h"

#include "imgui/imgui.h"

#include <windows.h>

void GridEditMode::Enter() {
    firstFrame_ = true;
    for (bool& b : prevArrow_) b = false;
}

void GridEditMode::Update() {
    Grid& grid = app_->GetGrid();

    const bool now[4] = {
        (GetAsyncKeyState(VK_LEFT)  & 0x8000) != 0,
        (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0,
        (GetAsyncKeyState(VK_UP)    & 0x8000) != 0,
        (GetAsyncKeyState(VK_DOWN)  & 0x8000) != 0,
    };
    const int dx[4] = { -1,  1,  0, 0 };
    const int dy[4] = {  0,  0, -1, 1 };

    const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const int  step  = shift ? 1 : 4;

    // Rising-edge: nudge once per press, not at the system key-repeat rate.
    if (!firstFrame_) {
        for (int i = 0; i < 4; ++i) {
            if (now[i] && !prevArrow_[i])
                grid.NudgeOrigin(dx[i] * step, dy[i] * step);
        }
    }
    for (int i = 0; i < 4; ++i) prevArrow_[i] = now[i];
    firstFrame_ = false;

    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse && io.MouseWheel != 0.0f) {
        const int wheelStep = shift ? 1 : 2;
        grid.ResizeTile(io.MouseWheel > 0.0f ? wheelStep : -wheelStep);
    }
}

void GridEditMode::DrawPanel() {
    DrawNormalModePanel(*app_);
}
