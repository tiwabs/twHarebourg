#pragma once

#include <optional>
#include <string>

#include "grid.h"

// Encapsulates the game-logic state needed to compute the suggested Confusion
// cast cell for the Harebourg boss fight.
class Logic {
public:
    int                 hpPercent   = 100; // Target HP as a percentage (0-100)
    int                 meleeStacks = 0;   // Number of melee hits accumulated this phase
    std::optional<Cell> player;            // Player position on the grid (if set)
    std::optional<Cell> target;            // Target (boss) position on the grid (if set)

    void IncMelee()   { ++meleeStacks; }
    void DecMelee()   { if (meleeStacks > 0) --meleeStacks; }
    void ResetMelee() { meleeStacks = 0; }

    int BaseRotation() const;

    int ConfusionRotation() const;

    std::string ConfusionLabel() const;

    std::optional<Cell> SuggestedCastCell(const Grid& grid) const;
};
