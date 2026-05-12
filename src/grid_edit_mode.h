#pragma once

#include "mode.h"

// Calibration mode: arrow keys nudge the grid origin, mouse wheel resizes the
// tile. Shift switches to fine steps. Values are applied live.
class GridEditMode : public IMode {
public:
    void Enter()     override;
    void Update()    override;
    void DrawPanel() override;

private:
    bool prevArrow_[4] = { false, false, false, false };
    bool firstFrame_   = true;
};
