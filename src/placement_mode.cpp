#include "placement_mode.h"

#include "app.h"
#include "ui_panel.h"

#include "imgui/imgui.h"

void PlacementMode::Update() {
    OverlayWindow& overlay = app_->Overlay();
    Grid&          grid    = app_->GetGrid();
    Logic&         logic   = app_->GetLogic();
    const ImGuiIO& io      = ImGui::GetIO();

    if (overlay.ClickThrough() || io.WantCaptureMouse) return;

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (auto picked = grid.PickCell(io.MousePos))
            logic.player = picked;
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        if (auto picked = grid.PickCell(io.MousePos))
            logic.target = picked;
    }
}

void PlacementMode::DrawWorld() {
    OverlayWindow& overlay = app_->Overlay();
    Grid&          grid    = app_->GetGrid();
    const ImGuiIO& io      = ImGui::GetIO();

    if (overlay.ClickThrough() || io.WantCaptureMouse) return;

    if (auto hovered = grid.PickCell(io.MousePos)) {
        grid.FillCell(hovered->c, hovered->r,
                      IM_COL32(255, 255, 255, 40),
                      IM_COL32(255, 255, 255, 140));
    }
}

void PlacementMode::DrawPanel() {
    DrawNormalModePanel(*app_);
}
