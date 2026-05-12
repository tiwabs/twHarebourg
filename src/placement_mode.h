#pragma once

#include "mode.h"

// Default mode: left-click places the player, right-click places the target.
// Renders a hover highlight on walkable cells.
class PlacementMode : public IMode {
public:
    void Update()    override;
    void DrawWorld() override;
    void DrawPanel() override;
};
