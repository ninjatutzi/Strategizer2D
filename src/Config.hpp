#pragma once

// Global game constants. Everything that is meant to be tweaked by hand
// (units, weapons, terrain, campaigns, maps) lives in the CSV files in data/.
namespace cfg {

constexpr int MinMapSize = 4;        // maps may have any rectangular size between these limits
constexpr int MaxMapSize = 32;       // (size = number/length of the [tiles] rows in the .map file)
constexpr int MaxWeapons = 5;        // weapon slots per unit
constexpr int CamoRevealRange = 2;   // camouflaged units are only seen within this distance

} // namespace cfg
