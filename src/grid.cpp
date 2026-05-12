#include "grid.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>

#include "json.hpp"

using nlohmann::json;

// Helper: decode the run-length "details" array into a MapData cell grid.
static MapData LoadMapData(const json& jm) {
    MapData m;
    m.name = jm.value("name", "Map 1");
    const auto& size = jm["size"];
    m.rows = size[0].get<int>();
    m.cols = size[1].get<int>();
    m.cells.assign(m.rows, std::vector<int>(m.cols, 0));

    for (const auto& d : jm["details"]) {
        const int r  = d[0].get<int>();
        const int c0 = d[1].get<int>();
        const int c1 = d[2].get<int>();
        const int t  = d[3].get<int>();
        if (r < 0 || r >= m.rows) continue;
        for (int c = std::max(0, c0); c <= c1 && c < m.cols; ++c)
            m.cells[r][c] = t;
    }
    return m;
}

// Helper: encode a MapData cell grid back into the run-length "details" format.
static json MakeDetails(const MapData& m) {
    json details = json::array();
    for (int r = 0; r < m.rows; ++r) {
        int c = 0;
        while (c < m.cols) {
            if (m.cells[r][c] == 0) { ++c; continue; }
            const int type = m.cells[r][c];
            const int c0   = c;
            while (c < m.cols && m.cells[r][c] == type) ++c;
            details.push_back(json::array({r, c0, c - 1, type}));
        }
    }
    return details;
}

// Load the grid from a JSON config file.
// Supports both the legacy single-map format ("map" key) and the new
// multi-map format ("maps" array). The calibration values (origin, tile
// size, base resolution) are shared across all maps.
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

    baseW   = j.value("base_width",    1920);
    baseH   = j.value("base_height",   1080);
    originX = j.value("grid_origin_x", 0);
    originY = j.value("grid_origin_y", 0);
    tileW   = j.value("tile_w",        0);
    tileH   = j.value("tile_h",        0);

    maps.clear();

    if (j.contains("maps") && j["maps"].is_array()) {
        // New multi-map format
        for (const auto& jm : j["maps"])
            maps.push_back(LoadMapData(jm));
        activeMapIdx = j.value("active_map", 0);
    } else if (j.contains("map")) {
        // Legacy single-map format — wrap it transparently
        json jm = j["map"];
        if (!jm.contains("name")) jm["name"] = "Map 1";
        maps.push_back(LoadMapData(jm));
        activeMapIdx = 0;
    }

    if (maps.empty()) {
        // Fallback: create a blank 20x20 map so the app is always usable
        AddMap("Map 1", 20, 20);
        activeMapIdx = 0;
    }

    if (activeMapIdx < 0 || activeMapIdx >= static_cast<int>(maps.size()))
        activeMapIdx = 0;

    return true;
}

// Save calibration values AND all map data back to the JSON config file.
// Any unrelated keys already in the file are preserved.
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
    j["active_map"]    = activeMapIdx;

    // Remove the legacy single-map key if still present
    j.erase("map");

    json jmaps = json::array();
    for (const auto& m : maps) {
        json jm;
        jm["name"]    = m.name;
        jm["size"]    = json::array({m.rows, m.cols});
        jm["details"] = MakeDetails(m);
        jmaps.push_back(std::move(jm));
    }
    j["maps"] = std::move(jmaps);

    std::ofstream out(path);
    if (!out) {
        std::cerr << "Grid: cannot write " << path << std::endl;
        return false;
    }
    out << j.dump(2);
    return true;
}

bool Grid::IsExisting(int c, int r) const {
    if (maps.empty()) return false;
    const MapData& m = maps[activeMapIdx];
    if (r < 0 || r >= m.rows || c < 0 || c >= m.cols) return false;
    return m.cells[r][c] != 0;
}

bool Grid::IsWalkable(int c, int r) const {
    if (maps.empty()) return false;
    const MapData& m = maps[activeMapIdx];
    if (r < 0 || r >= m.rows || c < 0 || c >= m.cols) return false;
    return m.cells[r][c] == 1;
}

bool Grid::BlocksLoS(int c, int r) const {
    if (maps.empty()) return false;
    const MapData& m = maps[activeMapIdx];
    if (r < 0 || r >= m.rows || c < 0 || c >= m.cols) return false;
    return m.cells[r][c] == 2;
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
    if (tileW <= 0 || tileH <= 0 || maps.empty()) return;

    const MapData& m   = maps[activeMapIdx];
    const ImU32    fill = IM_COL32(0, 0, 0, 110);
    ImDrawList*    dl   = ImGui::GetBackgroundDrawList();
    ImVec2 q[4];

    for (int r = 0; r < m.rows; ++r) {
        for (int c = 0; c < m.cols; ++c) {
            if (m.cells[r][c] != 1) continue;
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
std::optional<Cell> Grid::PickCell(ImVec2 screenPos) const {
    const Transform t = CurrentTransform();
    if (t.halfW <= 0.0f || t.halfH <= 0.0f || maps.empty()) return std::nullopt;

    const MapData& m = maps[activeMapIdx];
    for (int r = 0; r < m.rows; ++r) {
        for (int c = 0; c < m.cols; ++c) {
            if (m.cells[r][c] != 1) continue;
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

// Pick any in-bounds cell using the analytic inverse of the isometric transform.
// Works for non-existing cells too (needed for draw mode painting).
std::optional<Cell> Grid::PickAnyCell(ImVec2 screenPos) const {
    const Transform t = CurrentTransform();
    if (t.halfW <= 0.0f || t.halfH <= 0.0f || maps.empty()) return std::nullopt;

    const MapData& m = maps[activeMapIdx];
    const float ndx = (screenPos.x - t.ox) / t.halfW;
    const float ndy = (screenPos.y - t.oy) / t.halfH;

    const int ic = static_cast<int>(std::round((ndx + ndy) * 0.5f));
    const int ir = static_cast<int>(std::round((ndy - ndx) * 0.5f));

    if (ic < 0 || ic >= m.cols || ir < 0 || ir >= m.rows) return std::nullopt;
    return Cell{ic, ir};
}

void Grid::Draw() const {
    if (tileW <= 0 || tileH <= 0 || maps.empty()) return;

    const MapData& m = maps[activeMapIdx];
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    const ImU32 walkableLine = IM_COL32(255, 255, 255, 45);

    // Pass 1: walkable floor cells
    ImVec2 q[4];
    for (int r = 0; r < m.rows; ++r) {
        for (int c = 0; c < m.cols; ++c) {
            if (m.cells[r][c] != 1) continue;
            Diamond(c, r, q);
            dl->AddQuad(q[0], q[1], q[2], q[3], walkableLine, 1.0f);
        }
    }

    // Pass 2: LoS-blocker cubes — drawn in painter's order (increasing r+c)
    for (int depth = 0; depth < m.rows + m.cols - 1; ++depth) {
        for (int r = std::max(0, depth - m.cols + 1); r <= depth && r < m.rows; ++r) {
            const int c = depth - r;
            if (c < 0 || c >= m.cols) continue;
            if (m.cells[r][c] != 2) continue;
            DrawCube(c, r,
                IM_COL32( 70,  70,  90, 230),   // top   — grey-blue lit
                IM_COL32( 25,  25,  40, 235),   // left  — darkest (shadow)
                IM_COL32( 50,  50,  70, 230),   // right — medium shadow
                IM_COL32(190, 190, 210, 150));  // outline
        }
    }
}

// Draw-mode overlay: renders every cell in the bounding box with a color
// that indicates its current type, so the user can see what they're painting.
void Grid::DrawEditOverlay() const {
    if (tileW <= 0 || tileH <= 0 || maps.empty()) return;

    const MapData& m = maps[activeMapIdx];
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    const ImU32 emptyOutline    = IM_COL32(100, 100, 100,  60);
    const ImU32 walkableFill    = IM_COL32(  0, 180,   0,  50);
    const ImU32 walkableOutline = IM_COL32(  0, 255,   0, 160);

    // Pass 1: empty grid cells + walkable cells
    ImVec2 q[4];
    for (int r = 0; r < m.rows; ++r) {
        for (int c = 0; c < m.cols; ++c) {
            Diamond(c, r, q);
            const int type = m.cells[r][c];
            if (type == 1) {
                dl->AddQuadFilled(q[0], q[1], q[2], q[3], walkableFill);
                dl->AddQuad(q[0], q[1], q[2], q[3], walkableOutline, 1.5f);
            } else if (type != 2) {
                dl->AddQuad(q[0], q[1], q[2], q[3], emptyOutline, 1.0f);
            }
        }
    }

    // Pass 2: LoS-blocker cubes in painter's order
    for (int depth = 0; depth < m.rows + m.cols - 1; ++depth) {
        for (int r = std::max(0, depth - m.cols + 1); r <= depth && r < m.rows; ++r) {
            const int c = depth - r;
            if (c < 0 || c >= m.cols) continue;
            if (m.cells[r][c] != 2) continue;
            DrawCube(c, r,
                IM_COL32(200, 100,   0, 190),   // top   — amber lit
                IM_COL32(120,  50,   0, 210),   // left  — dark amber shadow
                IM_COL32(160,  75,   0, 200),   // right — medium amber
                IM_COL32(255, 160,   0, 200));  // outline
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

// Draw an isometric cube at cell (c, r).
// The cube top face is the normal diamond shifted UP by cubeH pixels.
// Two side faces (left-bottom and right-bottom) complete the 3D illusion.
//   top[0..3] = diamond corners shifted up by cubeH
//   Left face  = floor[left], floor[bottom], top[bottom], top[left]
//   Right face = floor[right], floor[bottom], top[bottom], top[right]
void Grid::DrawCube(int c, int r,
                    ImU32 topFill, ImU32 leftFill, ImU32 rightFill,
                    ImU32 outline) const {
    const Transform t = CurrentTransform();
    const float cx = t.ox + (c - r) * t.halfW;
    const float cy = t.oy + (c + r) * t.halfH;

    // Cube height in screen pixels (half tile height)
    const float cubeH = t.halfH;

    // Floor diamond corners
    const ImVec2 fTop    = {cx,             cy - t.halfH};
    const ImVec2 fRight  = {cx + t.halfW,   cy          };
    const ImVec2 fBottom = {cx,             cy + t.halfH};
    const ImVec2 fLeft   = {cx - t.halfW,   cy          };
    (void)fTop; // top floor corner not needed for sides

    // Top face = floor diamond shifted up
    const ImVec2 tTop    = {cx,             cy - t.halfH - cubeH};
    const ImVec2 tRight  = {cx + t.halfW,   cy           - cubeH};
    const ImVec2 tBottom = {cx,             cy + t.halfH - cubeH};
    const ImVec2 tLeft   = {cx - t.halfW,   cy           - cubeH};

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Left face (shadow side)
    dl->AddQuadFilled(fLeft, fBottom, tBottom, tLeft, leftFill);
    if (outline) dl->AddQuad(fLeft, fBottom, tBottom, tLeft, outline, 1.0f);

    // Right face (mid-shadow side)
    dl->AddQuadFilled(fRight, fBottom, tBottom, tRight, rightFill);
    if (outline) dl->AddQuad(fRight, fBottom, tBottom, tRight, outline, 1.0f);

    // Top face (lit)
    dl->AddQuadFilled(tTop, tRight, tBottom, tLeft, topFill);
    if (outline) dl->AddQuad(tTop, tRight, tBottom, tLeft, outline, 1.5f);
}

// Show a semi-transparent preview of what the cell under the cursor would
// become after a left-click (cycle: 0 → 1 → 2 → 0).
void Grid::DrawHoverPreview(Cell hovered) const {
    if (tileW <= 0 || tileH <= 0 || maps.empty()) return;

    const int current = GetCellType(hovered.c, hovered.r);
    const int next    = (current + 1) % 3;

    ImVec2 q[4];
    Diamond(hovered.c, hovered.r, q);
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    if (next == 0) {
        // Would become empty — dim red to signal deletion
        dl->AddQuadFilled(q[0], q[1], q[2], q[3], IM_COL32(220,  40,  40,  70));
        dl->AddQuad      (q[0], q[1], q[2], q[3], IM_COL32(255,  80,  80, 220), 2.0f);
        // Draw a small X at the center
        const ImVec2 ctr = CellCenter(hovered.c, hovered.r);
        const float  s   = 8.0f;
        dl->AddLine({ctr.x - s, ctr.y - s}, {ctr.x + s, ctr.y + s}, IM_COL32(255, 80, 80, 230), 2.0f);
        dl->AddLine({ctr.x + s, ctr.y - s}, {ctr.x - s, ctr.y + s}, IM_COL32(255, 80, 80, 230), 2.0f);
    } else if (next == 1) {
        // Would become walkable — green tint
        dl->AddQuadFilled(q[0], q[1], q[2], q[3], IM_COL32(  0, 220,   0,  70));
        dl->AddQuad      (q[0], q[1], q[2], q[3], IM_COL32(  0, 255,   0, 220), 2.0f);
    } else {
        // Would become LoS blocker — preview cube in semi-transparent amber
        DrawCube(hovered.c, hovered.r,
            IM_COL32(220, 120,   0, 120),
            IM_COL32(140,  60,   0, 130),
            IM_COL32(180,  90,   0, 125),
            IM_COL32(255, 180,   0, 180));
    }
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

// ---- Active-map dimension accessors ----

int Grid::Rows() const {
    if (maps.empty()) return 0;
    return maps[activeMapIdx].rows;
}

int Grid::Cols() const {
    if (maps.empty()) return 0;
    return maps[activeMapIdx].cols;
}

// ---- Multi-map management ----

const std::string& Grid::MapName(int idx) const {
    static const std::string empty;
    if (idx < 0 || idx >= static_cast<int>(maps.size())) return empty;
    return maps[idx].name;
}

bool Grid::SwitchToMap(int idx) {
    if (idx < 0 || idx >= static_cast<int>(maps.size())) return false;
    activeMapIdx = idx;
    return true;
}

void Grid::AddMap(const std::string& name, int rows, int cols) {
    MapData m;
    m.name = name;
    m.rows = rows;
    m.cols = cols;
    m.cells.assign(rows, std::vector<int>(cols, 0));
    maps.push_back(std::move(m));
}

void Grid::DeleteMap(int idx) {
    if (idx < 0 || idx >= static_cast<int>(maps.size())) return;
    if (static_cast<int>(maps.size()) <= 1) return; // always keep at least one map
    maps.erase(maps.begin() + idx);
    if (activeMapIdx >= static_cast<int>(maps.size()))
        activeMapIdx = static_cast<int>(maps.size()) - 1;
}

void Grid::RenameMap(int idx, const std::string& name) {
    if (idx < 0 || idx >= static_cast<int>(maps.size())) return;
    maps[idx].name = name;
}

// ---- Draw-mode cell editing ----

void Grid::SetCellType(int c, int r, int type) {
    if (maps.empty()) return;
    MapData& m = maps[activeMapIdx];
    if (r < 0 || r >= m.rows || c < 0 || c >= m.cols) return;
    m.cells[r][c] = type;
}

int Grid::GetCellType(int c, int r) const {
    if (maps.empty()) return 0;
    const MapData& m = maps[activeMapIdx];
    if (r < 0 || r >= m.rows || c < 0 || c >= m.cols) return 0;
    return m.cells[r][c];
}

// Resize the active map's bounding box.
// New rows/cols are filled with type 0 (non-existing); existing cells are kept.
Grid::GridSnapshot Grid::GetGridSnapshot() const {
    return { maps, activeMapIdx };
}

void Grid::RestoreGridSnapshot(const GridSnapshot& snapshot) {
    maps         = snapshot.maps;
    activeMapIdx = snapshot.activeIdx;
}

void Grid::ResizeGrid(int newRows, int newCols) {
    if (maps.empty()) return;
    if (newRows < 1) newRows = 1;
    if (newCols < 1) newCols = 1;
    MapData& m = maps[activeMapIdx];
    m.cells.resize(newRows, std::vector<int>(newCols, 0));
    for (auto& row : m.cells) row.resize(newCols, 0);
    m.rows = newRows;
    m.cols = newCols;
}
