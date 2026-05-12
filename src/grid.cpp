#include "grid.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>

#include "json.hpp"

using nlohmann::json;

// Load the grid layout from a JSON config file.
// The cell data is stored as a run-length encoded list of rows under
// map.details: each entry is [row, col_start, col_end, type].
// Cell types: 0 = non-existing, 1 = walkable, 2 = blocks line-of-sight.
bool Grid::LoadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Grid: cannot open " << path << std::endl;
        return false;
    }

    json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        std::cerr << "Grid: invalid JSON in " << path << " : " << e.what() << std::endl;
        return false;
    }

    baseW   = j.value("base_width", 1920);
    baseH   = j.value("base_height", 1080);
    originX = j.value("grid_origin_x", 0);
    originY = j.value("grid_origin_y", 0);
    tileW   = j.value("tile_w", 0);
    tileH   = j.value("tile_h", 0);

    const auto& size = j["map"]["size"];
    rows = size[0].get<int>();
    cols = size[1].get<int>();

    cells.assign(rows, std::vector<int>(cols, 0));

    for (const auto& d : j["map"]["details"]) {
        const int r  = d[0].get<int>();
        const int c0 = d[1].get<int>();
        const int c1 = d[2].get<int>();
        const int t  = d[3].get<int>();

        if (r < 0 || r >= rows) continue;
        for (int c = std::max(0, c0); c <= c1 && c < cols; ++c) {
            cells[r][c] = t;
        }
    }

    return true;
}

// Save the current grid calibration values (origin, tile size, base resolution)
// back to the JSON config file, preserving all other existing fields (map data, etc.).
// The file is read first so that keys not managed here are not lost.
bool Grid::SaveToFile(const std::string& path) const {
    json j;
    {
        std::ifstream in(path);
        if (in) {
            try {
                in >> j;
            } catch (...) {
                j = json::object();
            }
        }
    }

    j["base_width"]    = baseW;
    j["base_height"]   = baseH;
    j["grid_origin_x"] = originX;
    j["grid_origin_y"] = originY;
    j["tile_w"]        = tileW;
    j["tile_h"]        = tileH;

    std::ofstream out(path);
    if (!out) {
        std::cerr << "Grid: cannot write " << path << std::endl;
        return false;
    }
    out << j.dump(2);
    return true;
}

bool Grid::IsExisting(int c, int r) const {
    if (r < 0 || r >= rows || c < 0 || c >= cols) return false;
    return cells[r][c] != 0;
}

bool Grid::IsWalkable(int c, int r) const {
    if (r < 0 || r >= rows || c < 0 || c >= cols) return false;
    return cells[r][c] == 1;
}

bool Grid::BlocksLoS(int c, int r) const {
    if (r < 0 || r >= rows || c < 0 || c >= cols) return false;
    return cells[r][c] == 2;
}

// DDA (Digital Differential Analyzer) raycast from cell center a to cell center b.
// The ray steps through the isometric grid one cell boundary at a time and checks
// whether any intermediate cell has type == 2 (blocks LoS).
// A special case handles exact diagonal crossings where the ray passes through the
// shared corner of four cells: both adjacent cells are checked to avoid false positives.
bool Grid::HasLineOfSight(Cell a, Cell b) const {
    if (a == b) return true;

    // Use cell centres (+0.5 offset) so the ray starts and ends in the middle
    // of each cell rather than at the corner, giving symmetric behaviour.
    const double x0 = a.c + 0.5;
    const double y0 = a.r + 0.5;
    const double x1 = b.c + 0.5;
    const double y1 = b.r + 0.5;

    const double dx = x1 - x0;
    const double dy = y1 - y0;

    const int stepX = (dx > 0.0) ? 1 : (dx < 0.0 ? -1 : 0);
    const int stepY = (dy > 0.0) ? 1 : (dy < 0.0 ? -1 : 0);

    // tDeltaX / tDeltaY: parametric distance the ray travels to cross one cell
    // boundary in X or Y respectively.
    constexpr double INF = std::numeric_limits<double>::infinity();
    const double tDeltaX = (stepX == 0) ? INF : 1.0 / std::fabs(dx);
    const double tDeltaY = (stepY == 0) ? INF : 1.0 / std::fabs(dy);

    // tMaxX / tMaxY: parametric t value at which the ray first crosses a cell
    // boundary in X or Y. Initialised to the first half-cell crossing.
    double tMaxX = (stepX == 0) ? INF : tDeltaX * 0.5;
    double tMaxY = (stepY == 0) ? INF : tDeltaY * 0.5;

    int cx = a.c;
    int cy = a.r;

    while (true) {
        if (tMaxX < tMaxY) {
            cx += stepX;
            tMaxX += tDeltaX;
        } else if (tMaxY < tMaxX) {
            cy += stepY;
            tMaxY += tDeltaY;
        } else {
            // tMaxX == tMaxY: the ray hits an exact corner shared by four cells.
            // Check both neighbours to ensure neither blocks the path.
            if (BlocksLoS(cx + stepX, cy)) return false;
            if (BlocksLoS(cx, cy + stepY)) return false;
            cx += stepX;
            cy += stepY;
            tMaxX += tDeltaX;
            tMaxY += tDeltaY;
        }
        if (cx == b.c && cy == b.r) return true;
        if (BlocksLoS(cx, cy)) return false;
    }
}

void Grid::DrawNoLoSOverlay(Cell from) const {
    if (tileW <= 0 || tileH <= 0) return;

    const ImU32 fill = IM_COL32(0, 0, 0, 110);
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 q[4];

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (!IsWalkable(c, r)) continue;
            const Cell here{ c, r };
            if (here == from) continue;
            if (HasLineOfSight(from, here)) continue;

            Diamond(c, r, q);
            dl->AddQuadFilled(q[0], q[1], q[2], q[3], fill);
        }
    }
}

// Compute the screen-space transform for the current display resolution.
// All grid coordinates are authored for a base resolution (baseW x baseH);
// this function scales them to the actual display so the overlay fits any screen.
Grid::Transform Grid::CurrentTransform() const {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float scaleX = display.x / static_cast<float>(baseW);
    const float scaleY = display.y / static_cast<float>(baseH);
    return Transform{
        originX * scaleX,
        originY * scaleY,
        tileW * 0.5f * scaleX,
        tileH * 0.5f * scaleY,
    };
}

// Convert isometric grid coordinates (c, r) to the four screen-space corners
// of the diamond-shaped cell. The isometric layout places cell (c, r) at:
//   screen_x = originX + (c - r) * halfW
//   screen_y = originY + (c + r) * halfH
void Grid::Diamond(int c, int r, ImVec2 out[4]) const {
    const Transform t = CurrentTransform();
    const float cx = t.ox + (c - r) * t.halfW;
    const float cy = t.oy + (c + r) * t.halfH;
    out[0] = ImVec2(cx,           cy - t.halfH);
    out[1] = ImVec2(cx + t.halfW, cy);
    out[2] = ImVec2(cx,           cy + t.halfH);
    out[3] = ImVec2(cx - t.halfW, cy);
}

ImVec2 Grid::CellCenter(int c, int r) const {
    const Transform t = CurrentTransform();
    return ImVec2(t.ox + (c - r) * t.halfW, t.oy + (c + r) * t.halfH);
}

// Pick the walkable cell at the given screen position by testing each cell with
// the diamond containment formula: |dx/halfW| + |dy/halfH| <= 1.
// Iterates over all cells; this is acceptable for the small maps used here.
std::optional<Cell> Grid::PickCell(ImVec2 screenPos) const {
    const Transform t = CurrentTransform();
    if (t.halfW <= 0.0f || t.halfH <= 0.0f) return std::nullopt;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (!IsWalkable(c, r)) continue;
            const float cx = t.ox + (c - r) * t.halfW;
            const float cy = t.oy + (c + r) * t.halfH;
            const float dx = std::fabs(screenPos.x - cx);
            const float dy = std::fabs(screenPos.y - cy);
            if (dx / t.halfW + dy / t.halfH <= 1.0f) {
                return Cell{c, r};
            }
        }
    }
    return std::nullopt;
}

void Grid::Draw() const {
    if (tileW <= 0 || tileH <= 0) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    const ImU32 walkableLine = IM_COL32(255, 255, 255, 45);
    const ImU32 blockedLine  = IM_COL32(200, 200, 200, 150);
    const ImU32 blockedFill  = IM_COL32(40, 40, 40, 180);

    ImVec2 q[4];
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const int t = cells[r][c];
            if (t == 0) continue;

            Diamond(c, r, q);
            if (t == 2) {
                dl->AddQuadFilled(q[0], q[1], q[2], q[3], blockedFill);
                dl->AddQuad(q[0], q[1], q[2], q[3], blockedLine, 1.0f);
            } else {
                dl->AddQuad(q[0], q[1], q[2], q[3], walkableLine, 1.0f);
            }
        }
    }
}

void Grid::FillCell(int c, int r, ImU32 fill, ImU32 outline) const {
    ImVec2 q[4];
    Diamond(c, r, q);
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddQuadFilled(q[0], q[1], q[2], q[3], fill);
    if (outline) {
        dl->AddQuad(q[0], q[1], q[2], q[3], outline, 1.5f);
    }
}

void Grid::DrawCellMarker(int c, int r, ImU32 color, const char* label) const {
    const ImVec2 center = CellCenter(c, r);
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddCircleFilled(center, 10.0f, color);
    if (label && *label) {
        dl->AddText(
            ImVec2(center.x + 14.0f, center.y - 8.0f),
            IM_COL32(255, 255, 255, 220),
            label
        );
    }
}

void Grid::DrawCellLine(int c0, int r0, int c1, int r1, ImU32 color, float thickness) const {
    const ImVec2 a = CellCenter(c0, r0);
    const ImVec2 b = CellCenter(c1, r1);
    ImGui::GetBackgroundDrawList()->AddLine(a, b, color, thickness);
}

// Resize the tile dimensions while keeping the isometric 2:1 aspect ratio
// (tileW = 2 * tileH). A minimum size is enforced to keep cells visible.
void Grid::ResizeTile(int step) {
    int newW = tileW + step * 2;
    int newH = tileH + step;
    if (newW < 12) newW = 12;
    if (newH < 6)  newH = 6;
    tileW = newW;
    tileH = newH;
}
