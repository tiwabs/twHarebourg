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

// Isometric grid that handles loading, drawing, and querying the battle map.
// Cell values stored internally:
//   0 = non-existing (not part of the map)
//   1 = walkable
//   2 = walkable but blocks line-of-sight
class Grid {
public:
    bool LoadFromFile(const std::string& path);
    bool SaveToFile(const std::string& path) const;
    void Draw() const;

    int Rows() const { return rows; }
    int Cols() const { return cols; }

    bool IsExisting(int c, int r) const;
    bool IsWalkable(int c, int r) const;
    bool BlocksLoS(int c, int r) const;

    bool HasLineOfSight(Cell a, Cell b) const;

    void DrawNoLoSOverlay(Cell from) const;

    ImVec2 CellCenter(int c, int r) const;

    std::optional<Cell> PickCell(ImVec2 screenPos) const;

    void FillCell(int c, int r, ImU32 fill, ImU32 outline) const;
    void DrawCellMarker(int c, int r, ImU32 color, const char* label) const;
    void DrawCellLine(int c0, int r0, int c1, int r1, ImU32 color, float thickness) const;

    // Grid edit operations
    void NudgeOrigin(int dx, int dy) { originX += dx; originY += dy; }
    void ResizeTile(int step);

    int OriginX() const { return originX; }
    int OriginY() const { return originY; }
    int TileW()   const { return tileW; }
    int TileH()   const { return tileH; }

private:
    // Cached scaling data computed from the current display size and the
    // reference resolution (baseW x baseH). Recomputed every frame.
    struct Transform {
        float ox;
        float oy;
        float halfW;
        float halfH;
    };

    Transform CurrentTransform() const;
    void Diamond(int c, int r, ImVec2 out[4]) const;

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
    int rows    = 0;
    int cols    = 0;

    std::vector<std::vector<int>> cells;
};
