#pragma once

#include <optional>
#include <string>
#include <vector>

#include "imgui/imgui.h"

// A single isometric grid cell identified by column (c) and row (r).
struct Cell {
    int c = 0;
    int r = 0;

    bool operator==(const Cell& o) const { return c == o.c && r == o.r; }
    bool operator!=(const Cell& o) const { return !(*this == o); }
};

// Data for a single map (cell layout).
// Cell values:
//   0 = non-existing (not part of the map)
//   1 = walkable
//   2 = walkable but blocks line-of-sight
struct MapData {
    std::string name;
    int rows = 0;
    int cols = 0;
    std::vector<std::vector<int>> cells; // cells[r][c]
};

// Isometric grid that handles loading, drawing, and querying the battle map.
// Supports multiple named maps; the active map can be switched at runtime.
class Grid {
public:
    bool LoadFromFile(const std::string& path);
    bool SaveToFile(const std::string& path) const;

    // Normal rendering (walkable outline, LoS-blocker 3D cube)
    void Draw() const;
    // Draw mode overlay: shows all cells including non-existing ones
    void DrawEditOverlay() const;
    // Hover preview: shows what the cell under the cursor would become
    void DrawHoverPreview(Cell hovered) const;

    // ---- Multi-map management ----
    int                MapCount()       const { return static_cast<int>(maps.size()); }
    const std::string& MapName(int idx) const;
    int                ActiveMapIndex() const { return activeMapIdx; }
    bool               SwitchToMap(int idx);
    void               AddMap(const std::string& name, int rows, int cols);
    void               DeleteMap(int idx);
    void               RenameMap(int idx, const std::string& name);

    // ---- Active-map accessors ----
    int  Rows() const;
    int  Cols() const;

    bool IsExisting(int c, int r) const;
    bool IsWalkable(int c, int r) const;
    bool BlocksLoS(int c, int r) const;

    bool HasLineOfSight(Cell a, Cell b) const;

    void DrawNoLoSOverlay(Cell from) const;

    ImVec2 CellCenter(int c, int r) const;

    // Pick a walkable cell at screen position (used for player/target placement)
    std::optional<Cell> PickCell(ImVec2 screenPos) const;
    // Pick any in-bounds cell at screen position (used for draw mode)
    std::optional<Cell> PickAnyCell(ImVec2 screenPos) const;

    void FillCell(int c, int r, ImU32 fill, ImU32 outline) const;
    void DrawCellMarker(int c, int r, ImU32 color, const char* label) const;
    void DrawCellLine(int c0, int r0, int c1, int r1, ImU32 color, float thickness) const;

    // ---- Grid calibration edit (F8 mode) ----
    void NudgeOrigin(int dx, int dy) { originX += dx; originY += dy; }
    void ResizeTile(int step);

    int OriginX() const { return originX; }
    int OriginY() const { return originY; }
    int TileW()   const { return tileW; }
    int TileH()   const { return tileH; }

    // ---- Draw mode cell editing ----
    void SetCellType(int c, int r, int type);
    int  GetCellType(int c, int r) const;
    void ResizeGrid(int newRows, int newCols);

    // ---- Snapshot / restore for cancel in draw mode ----
    struct GridSnapshot {
        std::vector<MapData> maps;
        int activeIdx = 0;
    };
    GridSnapshot GetGridSnapshot() const;
    void         RestoreGridSnapshot(const GridSnapshot& snapshot);

private:
    struct Transform {
        float ox;
        float oy;
        float halfW;
        float halfH;
    };

    Transform CurrentTransform() const;
    void Diamond(int c, int r, ImVec2 out[4]) const;
    // Draw an isometric cube at cell (c, r) with per-face colours.
    void DrawCube(int c, int r,
                  ImU32 topFill, ImU32 leftFill, ImU32 rightFill,
                  ImU32 outline) const;

    // Reference resolution for which the grid coordinates were calibrated.
    // All pixel values in the config are relative to this resolution and are
    // scaled up/down at runtime to match the actual display.
    int baseW   = 1920;
    int baseH   = 1080;
    // Screen-space origin of cell (0, 0) in the reference resolution.
    int originX = 0;
    int originY = 0;
    // Tile dimensions in the reference resolution (width = 2 * height for 2:1 iso).
    int tileW   = 0;
    int tileH   = 0;

    std::vector<MapData> maps;
    int activeMapIdx = 0;
};
