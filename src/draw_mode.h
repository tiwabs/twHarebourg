#pragma once

#include "grid.h"
#include "mode.h"

// Map-editing mode: left-click cycles a cell type (empty -> walkable -> LoS),
// right-click erases. Drags paint continuously.
//
// The grid state is snapshotted on entry so the Cancel button can revert.
// When entering via "+ Nouvelle", the caller pre-sets the snapshot (taken
// before adding the new map) via SetPreSnapshot().
class DrawMode : public IMode {
public:
    void Enter()     override;
    void Update()    override;
    void DrawWorld() override;
    void DrawPanel() override;

    void SetPreSnapshot(Grid::GridSnapshot snap);

private:
    Grid::GridSnapshot snapshot_;
    bool               hasPreSnapshot_ = false;

    // Drag state for left-click painting.
    int  dragType_    = -1;
    Cell lastPainted_ = { -1, -1 };

    // Panel state (persist across frames).
    char nameBuf_[128]   = {};
    int  lastMapIdx_     = -1;
    int  rowsBuf_        = 0;
    int  colsBuf_        = 0;
    int  lastSizeIdx_    = -1;

    void SaveAndExit();
    void CancelAndExit();
};
