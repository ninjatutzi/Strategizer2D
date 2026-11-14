#pragma once

// Global game constants. Everything that is meant to be tweaked by hand
// (units, weapons, terrain, campaigns, maps) lives in the CSV files in data/.
namespace cfg {

constexpr int MapSize = 12;          // every map is MapSize x MapSize tiles
constexpr int MaxWeapons = 5;        // weapon slots per unit
constexpr int CamoRevealRange = 2;   // camouflaged units are only seen within this distance

} // namespace cfg
