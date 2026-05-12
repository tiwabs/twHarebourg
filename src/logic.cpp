#include "logic.h"

namespace {

// Determines the base (un-stacked) Confusion rotation angle from the target's
// current HP percentage. These thresholds mirror the in-game Harebourg mechanic:
//   90-100 % HP  ->  +90° clockwise
//   75-89  % HP  ->  -90° counter-clockwise
//   45-74  % HP  ->  180°
//   30-44  % HP  ->  -90° counter-clockwise
//    0-29  % HP  ->  +90° clockwise
int base_rotation_from_hp(int hp) {
    if (hp >= 90) return  90;
    if (hp >= 75) return -90;
    if (hp >= 45) return 180;
    if (hp >= 30) return -90;
    return                90; // 0..29
}

const char* label_from_base(int rot) {
    if (rot == 90)  return "90 Horaire";
    if (rot == -90) return "90 Contre Horaire";
    if (rot == 180) return "180";
    return "?";
}

// Rotate the isometric grid delta (dc, dr) by the given angle in degrees.
// Only multiples of 90° are supported; any other value returns the input unchanged.
// The isometric axes are treated as a regular 2D integer grid for rotation purposes.
std::pair<int, int> rotate_delta(int dc, int dr, int angleDeg) {
    // Normalise angle to [0, 360).
    int a = ((angleDeg % 360) + 360) % 360;
    if (a == 0)   return {dc, dr};
    if (a == 90)  return {dr, -dc};
    if (a == 270) return {-dr, dc};
    if (a == 180) return {-dc, -dr};
    return {dc, dr};
}

}
int Logic::BaseRotation() const {
    return base_rotation_from_hp(hpPercent);
}

// Total Confusion rotation = base rotation (from HP) + 90° * melee stacks.
// Each time the player is hit in melee range, the rotation shifts by 90°,
// so stacking is additive. The result is normalised to [0, 360).
int Logic::ConfusionRotation() const {
    return ((BaseRotation() + meleeStacks * 90) % 360 + 360) % 360;
}

std::string Logic::ConfusionLabel() const {
    return label_from_base(BaseRotation());
}

// Compute the optimal cell for the player to cast the Confusion spell from so
// that the resulting teleportation lands the target on a walkable cell that is
// also in the player's line of sight.
//
// Algorithm:
//   1. Require both player and target to be set.
//   2. Verify there is line-of-sight from player to target (spell must be castable).
//   3. Compute the delta vector from player to target in grid coordinates.
//   4. Rotate that delta by the current ConfusionRotation to obtain the expected
//      displacement the spell will apply to the target.
//   5. The suggested cast cell is player + rotated delta (i.e. where the player
//      should stand so the spell lands the target one step away in that direction).
//   6. Validate: the cell must be walkable, not equal to the player position,
//      and have line-of-sight from the player (it must be reachable/castable).
std::optional<Cell> Logic::SuggestedCastCell(const Grid& grid) const {
    if (!player || !target) return std::nullopt;

    if (!grid.HasLineOfSight(*player, *target)) return std::nullopt;

    const int dc = target->c - player->c;
    const int dr = target->r - player->r;
    const auto [cdc, cdr] = rotate_delta(dc, dr, ConfusionRotation());

    const Cell cell{ player->c + cdc, player->r + cdr };
    if (cell == *player) return std::nullopt;
    if (!grid.IsWalkable(cell.c, cell.r)) return std::nullopt;
    if (!grid.HasLineOfSight(*player, cell)) return std::nullopt;
    return cell;
}
