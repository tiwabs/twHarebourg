#include "draw_mode.h"

#include "app.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <utility>

void DrawMode::SetPreSnapshot(Grid::GridSnapshot snap) {
    snapshot_       = std::move(snap);
    hasPreSnapshot_ = true;
}

void DrawMode::Enter() {
    if (!hasPreSnapshot_)
        snapshot_ = app_->GetGrid().GetGridSnapshot();

    app_->Overlay().SetClickThrough(false);

    dragType_     = -1;
    lastPainted_  = { -1, -1 };
    lastMapIdx_   = -1;
    lastSizeIdx_  = -1;
}

void DrawMode::Update() {
    OverlayWindow& overlay = app_->Overlay();
    Grid&          grid    = app_->GetGrid();
    const ImGuiIO& io      = ImGui::GetIO();

    if (overlay.ClickThrough() || io.WantCaptureMouse) return;

    // Left button: cycle the first cell, then lock that target type for the
    // whole drag so all painted cells get the same value.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (auto picked = grid.PickAnyCell(io.MousePos)) {
            const int current = grid.GetCellType(picked->c, picked->r);
            dragType_    = (current + 1) % 3;
            lastPainted_ = *picked;
            grid.SetCellType(picked->c, picked->r, dragType_);
        }
    } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && dragType_ >= 0) {
        if (auto picked = grid.PickAnyCell(io.MousePos)) {
            if (*picked != lastPainted_) {
                grid.SetCellType(picked->c, picked->r, dragType_);
                lastPainted_ = *picked;
            }
        }
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        dragType_    = -1;
        lastPainted_ = { -1, -1 };
    }

    // Right button: erase on click or hold.
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        if (auto picked = grid.PickAnyCell(io.MousePos))
            grid.SetCellType(picked->c, picked->r, 0);
    }
}

void DrawMode::DrawWorld() {
    OverlayWindow& overlay = app_->Overlay();
    Grid&          grid    = app_->GetGrid();
    const ImGuiIO& io      = ImGui::GetIO();

    grid.DrawEditOverlay();

    if (overlay.ClickThrough() || io.WantCaptureMouse) return;

    if (auto hovered = grid.PickAnyCell(io.MousePos))
        grid.DrawHoverPreview(*hovered);
}

void DrawMode::DrawPanel() {
    Grid& grid = app_->GetGrid();

    // Map name (sync the local buffer with the grid when the active map changes).
    {
        const int curIdx = grid.ActiveMapIndex();
        if (lastMapIdx_ != curIdx) {
            std::snprintf(nameBuf_, sizeof(nameBuf_), "%s", grid.MapName(curIdx).c_str());
            lastMapIdx_ = curIdx;
        }
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::InputTextWithHint("##mapname", "Nom de la map", nameBuf_, sizeof(nameBuf_)))
            grid.RenameMap(curIdx, nameBuf_);
    }

    // Grid size.
    {
        const int curIdx = grid.ActiveMapIndex();
        if (lastSizeIdx_ != curIdx) {
            rowsBuf_     = grid.Rows();
            colsBuf_     = grid.Cols();
            lastSizeIdx_ = curIdx;
        }

        const float inputW = 50.0f;

        ImGui::Text("Lignes");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(inputW);
        if (ImGui::InputInt("##rows", &rowsBuf_, 0, 0)) {
            if (rowsBuf_ < 1)  rowsBuf_ = 1;
            if (rowsBuf_ > 60) rowsBuf_ = 60;
            grid.ResizeGrid(rowsBuf_, colsBuf_);
        }
        ImGui::SameLine();
        ImGui::Text("Cols");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(inputW);
        if (ImGui::InputInt("##cols", &colsBuf_, 0, 0)) {
            if (colsBuf_ < 1)  colsBuf_ = 1;
            if (colsBuf_ > 60) colsBuf_ = 60;
            grid.ResizeGrid(rowsBuf_, colsBuf_);
        }
    }

    ImGui::Separator();

    // Save / Cancel.
    {
        const float halfW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(40, 130, 60, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(60, 170, 80, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(30, 110, 50, 255));
        if (ImGui::Button("(F9) Sauvegarder", ImVec2(halfW, 0.0f)))
            SaveAndExit();
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        if (ImGui::Button("Annuler", ImVec2(halfW, 0.0f)))
            CancelAndExit();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("G: vide > walkable > LoS    D: effacer");

    ImGui::Separator();
    if (ImGui::Button("(F10) Quitter", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
        app_->Overlay().RequestClose();
}

void DrawMode::SaveAndExit() {
    app_->GetGrid().SaveToFile("config.json");
    app_->EnterPlacement();
}

void DrawMode::CancelAndExit() {
    app_->GetGrid().RestoreGridSnapshot(snapshot_);
    app_->EnterPlacement();
}
