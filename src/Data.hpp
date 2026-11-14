#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "Config.hpp"

enum class UnitClass { Infantry, Vehicle };

struct Terrain {
    std::string id, name;
    char symbol = 'P';
    std::uint8_t r = 0, g = 0, b = 0;
    int moveCostInfantry = 1;   // 0 = impassable
    int moveCostVehicle = 1;    // 0 = impassable
    float damageMult = 1.f;     // < 1 defense buff, > 1 defense debuff
    int moveCost(UnitClass c) const { return c == UnitClass::Infantry ? moveCostInfantry : moveCostVehicle; }
};

struct WeaponDef {
    std::string id, name;
    int range = 1;
    float damage = 0;
    float ap = 1;          // armour piercing
    int willDamage = 0;    // damage to the target's willpower
    int cooldown = 0;      // turns the weapon is unusable after firing
    bool needsRadio = false;
};

struct UnitDef {
    std::string id, name, sprite;
    UnitClass cls = UnitClass::Infantry;
    int hp = 1, will = 5, armour = 0, move = 1, cost = 0;
    std::vector<std::string> attributes;
    std::vector<WeaponDef> weapons;   // up to cfg::MaxWeapons

    bool has(const std::string& attr) const;
    bool isInfantry() const { return cls == UnitClass::Infantry; }
    int maxActions() const { return has("double_attack") ? 2 : 1; }
    std::string abbrev() const;       // short label for units without a sprite
};

struct Campaign {
    std::string id, name, description;
};

enum class Objective { Capture, Eliminate };

struct Mission {
    std::string id, campaign, name, description;
    Objective objective = Objective::Eliminate;
    std::string mapFile;
    int budget = 0;
    int maxTurns = 0;   // 0 = unlimited
};

class GameData {
public:
    // Loads data/*.csv from the given root directory. Throws std::runtime_error.
    void load(const std::string& rootDir);

    const std::string& root() const { return root_; }
    const std::vector<Terrain>& terrains() const { return terrains_; }
    const std::vector<UnitDef>& units() const { return units_; }
    const std::vector<Campaign>& campaigns() const { return campaigns_; }
    const std::vector<Mission>& missions() const { return missions_; }

    const Terrain* terrainBySymbol(char c) const;
    const Terrain* terrainById(const std::string& id) const;
    const UnitDef* unit(const std::string& id) const;
    const Mission* mission(const std::string& id) const;
    std::vector<const Mission*> missionsOf(const std::string& campaignId) const;

private:
    std::string root_;
    std::vector<Terrain> terrains_;
    std::vector<UnitDef> units_;
    std::vector<Campaign> campaigns_;
    std::vector<Mission> missions_;
};

// Looks for a directory containing data/units.csv (cwd, parent dirs, next to the
// executable, compile-time source dir). Returns "" if none is found.
std::string findGameRoot(const std::string& argv0);
