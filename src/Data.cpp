#include "Data.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "Csv.hpp"

namespace fs = std::filesystem;

bool UnitDef::has(const std::string& attr) const {
    return std::find(attributes.begin(), attributes.end(), attr) != attributes.end();
}

int UnitDef::attrValue(const std::string& attr, int def) const {
    auto it = attrValues.find(attr);
    return it == attrValues.end() ? def : it->second;
}

std::string UnitDef::abbrev() const {
    std::string s;
    bool start = true;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            if (start) s += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            start = false;
        } else start = true;
    }
    if (s.size() > 2) s.resize(2);
    return s;
}

const Terrain* GameData::terrainBySymbol(char c) const {
    for (auto& t : terrains_) if (t.symbol == c) return &t;
    return nullptr;
}
const Terrain* GameData::terrainById(const std::string& id) const {
    for (auto& t : terrains_) if (t.id == id) return &t;
    return nullptr;
}
const UnitDef* GameData::unit(const std::string& id) const {
    for (auto& u : units_) if (u.id == id) return &u;
    return nullptr;
}
const Mission* GameData::mission(const std::string& id) const {
    for (auto& m : missions_) if (m.id == id) return &m;
    return nullptr;
}
std::vector<const Mission*> GameData::missionsOf(const std::string& campaignId) const {
    std::vector<const Mission*> out;
    for (auto& m : missions_) if (m.campaign == campaignId) out.push_back(&m);
    return out;
}

namespace {

const std::vector<std::string> kKnownAttributes = {"radio", "double_attack", "camouflage", "heilung"};

template <class T>
void requireUnique(const std::vector<T>& v, const std::string& id, const CsvRow& row, const char* what) {
    for (auto& e : v)
        if (e.id == id) row.fail(std::string("duplicate ") + what + " id '" + id + "'");
}

} // namespace

void GameData::load(const std::string& rootDir) {
    root_ = rootDir;
    const std::string d = rootDir + "/data/";

    // ---- terrain.csv --------------------------------------------------------
    terrains_.clear();
    for (auto& row : readCsv(d + "terrain.csv")) {
        Terrain t;
        t.id = toLower(row.requireStr("id"));
        t.name = row.requireStr("name");
        std::string sym = row.requireStr("symbol");
        if (sym.size() != 1) row.fail("symbol must be exactly one character");
        t.symbol = sym[0];
        auto clampc = [&](const char* col) {
            int v = row.requireInt(col);
            if (v < 0 || v > 255) row.fail(std::string(col) + " must be 0-255");
            return static_cast<std::uint8_t>(v);
        };
        t.r = clampc("r"); t.g = clampc("g"); t.b = clampc("b");
        t.moveCostInfantry = row.requireInt("move_cost_infantry");
        t.moveCostVehicle = row.requireInt("move_cost_vehicle");
        t.damageMult = row.requireFloat("damage_multiplier");
        if (t.damageMult <= 0) row.fail("damage_multiplier must be > 0");
        requireUnique(terrains_, t.id, row, "terrain");
        if (terrainBySymbol(t.symbol)) row.fail(std::string("duplicate terrain symbol '") + t.symbol + "'");
        terrains_.push_back(t);
    }
    if (terrains_.empty()) throw std::runtime_error("terrain.csv contains no terrain");

    // ---- weapons.csv (kept local, units copy what they use) ----------------------
    std::map<std::string, WeaponDef> weapons;
    for (auto& row : readCsv(d + "weapons.csv")) {
        WeaponDef w;
        w.id = toLower(row.requireStr("id"));
        w.name = row.requireStr("name");
        w.range = row.requireInt("range");
        w.damage = row.requireFloat("damage");
        w.ap = row.requireFloat("ap");
        w.willDamage = row.intOr("will_damage", 0);
        w.cooldown = row.intOr("cooldown", 0);
        w.needsRadio = row.intOr("needs_radio", 0) != 0;
        std::string type = toLower(row.str("type", "damage"));
        if (type == "damage") w.type = WeaponType::Damage;
        else if (type == "heal") w.type = WeaponType::Heal;
        else if (type == "repair") w.type = WeaponType::Repair;
        else if (type == "refuel") w.type = WeaponType::Refuel;
        else if (type == "supply") w.type = WeaponType::Supply;
        else row.fail("type must be damage, heal, repair, refuel or supply");
        if (w.range < 1) row.fail("range must be >= 1");
        if (w.cooldown < 0) row.fail("cooldown must be >= 0");
        if (weapons.count(w.id)) row.fail("duplicate weapon id '" + w.id + "'");
        weapons[w.id] = w;
    }

    // ---- units.csv -------------------------------------------------------------
    units_.clear();
    for (auto& row : readCsv(d + "units.csv")) {
        UnitDef u;
        u.id = toLower(row.requireStr("id"));
        u.name = row.requireStr("name");
        u.sprite = row.str("sprite", u.id + ".png");
        std::string cls = toLower(row.requireStr("class"));
        if (cls == "infantry") u.cls = UnitClass::Infantry;
        else if (cls == "vehicle") u.cls = UnitClass::Vehicle;
        else row.fail("class must be 'infantry' or 'vehicle'");
        u.hp = row.requireInt("hp");
        u.will = row.requireInt("will");
        u.armour = row.requireInt("armour");
        u.move = row.requireInt("move");
        u.cost = row.requireInt("cost");
        u.fuel = row.intOr("fuel", 0);
        if (u.fuel < 0) row.fail("fuel must be >= 0");
        u.vision = row.intOr("vision", u.cls == UnitClass::Infantry ? 5 : 4);
        if (u.vision < 1) row.fail("vision must be >= 1");
        if (u.hp < 1) row.fail("hp must be >= 1");
        if (u.armour < 0) row.fail("armour must be >= 0");
        if (u.armour > 0 && u.cls == UnitClass::Infantry)
            row.fail("infantry cannot have armour (armour must be 0)");
        for (auto& a : split(row.str("attributes"), '|')) {
            std::string attr = toLower(trim(a));
            if (attr.empty()) continue;
            int value = 0;                       // "heilung(10)" -> name heilung, value 10
            size_t lp = attr.find('(');
            if (lp != std::string::npos) {
                size_t rp = attr.find(')', lp);
                if (rp == std::string::npos) row.fail("attribute '" + attr + "': missing ')'");
                value = std::atoi(attr.substr(lp + 1, rp - lp - 1).c_str());
                attr = trim(attr.substr(0, lp));
            }
            if (attr == "heal") attr = "heilung";   // English alias
            if (std::find(kKnownAttributes.begin(), kKnownAttributes.end(), attr) == kKnownAttributes.end())
                std::cerr << "warning: " << row.file << ":" << row.line
                          << ": unknown attribute '" << attr << "' (no effect)\n";
            u.attributes.push_back(attr);
            u.attrValues[attr] = value;
        }
        for (int i = 1; i <= cfg::MaxWeapons; ++i) {
            std::string col = "weapon" + std::to_string(i);
            if (!row.has(col)) continue;
            std::string wid = toLower(row.str(col));
            auto it = weapons.find(wid);
            if (it == weapons.end()) row.fail(col + ": unknown weapon '" + wid + "'");
            u.weapons.push_back(it->second);
        }
        requireUnique(units_, u.id, row, "unit");
        units_.push_back(u);
    }
    if (units_.empty()) throw std::runtime_error("units.csv contains no units");

    // ---- campaigns.csv / missions.csv -------------------------------------------
    campaigns_.clear();
    for (auto& row : readCsv(d + "campaigns.csv")) {
        Campaign c;
        c.id = toLower(row.requireStr("id"));
        c.name = row.requireStr("name");
        c.description = row.str("description");
        requireUnique(campaigns_, c.id, row, "campaign");
        campaigns_.push_back(c);
    }
    missions_.clear();
    for (auto& row : readCsv(d + "missions.csv")) {
        Mission m;
        m.id = toLower(row.requireStr("id"));
        m.campaign = toLower(row.requireStr("campaign"));
        m.name = row.requireStr("name");
        m.description = row.str("description");
        std::string obj = toLower(row.requireStr("objective"));
        if (obj == "capture") m.objective = Objective::Capture;
        else if (obj == "eliminate") m.objective = Objective::Eliminate;
        else row.fail("objective must be 'capture' or 'eliminate'");
        m.mapFile = row.requireStr("map");
        m.budget = row.requireInt("budget");
        m.maxTurns = row.intOr("max_turns", 0);
        bool known = std::any_of(campaigns_.begin(), campaigns_.end(),
                                 [&](const Campaign& c) { return c.id == m.campaign; });
        if (!known) row.fail("unknown campaign '" + m.campaign + "'");
        requireUnique(missions_, m.id, row, "mission");
        missions_.push_back(m);
    }
}

std::string findGameRoot(const std::string& argv0) {
    std::vector<fs::path> candidates;
    std::error_code ec;
    candidates.push_back(fs::current_path(ec));
    candidates.push_back(fs::current_path(ec) / "..");
    if (!argv0.empty()) {
        fs::path exe = fs::absolute(argv0, ec).parent_path();
        candidates.push_back(exe);
        candidates.push_back(exe / "..");
        candidates.push_back(exe / "../..");
    }
#ifdef STRATEGIZER_SOURCE_DIR
    candidates.push_back(STRATEGIZER_SOURCE_DIR);
#endif
    for (auto& c : candidates)
        if (fs::exists(c / "data" / "units.csv", ec)) return fs::weakly_canonical(c, ec).string();
    return "";
}
