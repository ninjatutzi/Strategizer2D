#pragma once
#include <array>
#include <deque>
#include <string>
#include <vector>

#include "Data.hpp"

enum class Side { Player, Enemy, Neutral };
inline Side opposite(Side s) { return s == Side::Player ? Side::Enemy : Side::Player; }

struct Unit {
    int id = 0;
    const UnitDef* def = nullptr;
    Side side = Side::Player;
    int x = 0, y = 0;
    int hp = 0;
    int will = 0;
    int fuel = 0;                // vehicles only (0 max = no fuel system)
    int attacksLeft = 0;
    std::array<int, cfg::MaxWeapons> cooldown{};
    std::array<int, cfg::MaxWeapons> ammo{};   // shots left per weapon (-1 = unlimited)
    bool holdPosition = false;   // AI: never moves, only shoots
    bool alive = true;

    bool moved = false;          // has used its move this turn (a unit may move once AND attack)
    bool active = true;          // false = dormant enemy that spawns when its group is triggered
    std::string group;           // trigger group (enemies only)

    bool present() const { return alive && active; }
};

// Buildings block their tile (nobody can stand on them). Infantry standing next to one can
// capture it; any weapon can destroy it. Owned buildings give income and may offer actions.
struct Building {
    const BuildingDef* def = nullptr;
    std::string name;
    int x = 0, y = 0;
    Side owner = Side::Neutral;
    bool objective = true;       // capture / destroy target
    bool protect = false;        // the mission is lost if this building falls
    int hp = 100, maxHp = 100;
    int armour = 0;
    bool destroyed = false;
    int attacksLeft = 0;         // uses of its weapons this turn (owner only)
    int recruitsLeft = 0;        // units that can still be bought here this turn
    std::array<int, cfg::MaxWeapons> ammo{};
    std::array<int, cfg::MaxWeapons> cooldown{};
};

struct BattleEvent {
    enum class Kind { Message, Move, Attack, Capture, Death, Spawn, Heal, Destroyed, Recruit };
    Kind kind = Kind::Message;
    std::string text;
    int ax = 0, ay = 0, bx = 0, by = 0;
    int damage = 0;
    Side side = Side::Neutral;
};

enum class Phase { Deploy, PlayerTurn, EnemyTurn, Won, Lost };

struct ObjectiveLine {
    std::string text;
    bool done = false;   // already fulfilled (shown with a check mark)
};

// Damage rule (per the design document):
//   armour == 0 : damage * terrainMultiplier
//   armour  > 0 : (damage * AP / armour) * terrainMultiplier
// Result is rounded, but never below 1 when the attack does any damage.
int computeDamage(const WeaponDef& w, int defenderArmour, float terrainMult);

class Battle {
public:
    // Loads the map of the mission. Throws std::runtime_error on bad data.
    Battle(const GameData& data, const Mission& mission);

    // ---- map ---------------------------------------------------------------------
    int width() const { return cfg::MapSize; }
    int height() const { return cfg::MapSize; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < width() && y < height(); }
    const Terrain& terrainAt(int x, int y) const { return *tiles_[y * width() + x]; }
    const std::vector<Building>& buildings() const { return buildings_; }
    const Building* buildingAt(int x, int y) const;
    Building* buildingAt(int x, int y);
    Building* buildingByIndex(int i) { return i >= 0 && i < static_cast<int>(buildings_.size()) ? &buildings_[i] : nullptr; }

    // ---- units -------------------------------------------------------------------
    const std::vector<Unit>& units() const { return units_; }
    Unit* unitAt(int x, int y);
    const Unit* unitAt(int x, int y) const;
    Unit* unitById(int id);
    const Unit* unitById(int id) const;
    int aliveCount(Side s) const;
    // Fog of war: can `viewer` see this unit? (vision range of the viewer's units, camouflage)
    bool visibleTo(const Unit& u, Side viewer) const;
    bool tileVisible(int x, int y, Side viewer) const;
    bool hasRadio(Side s) const;

    // ---- state -------------------------------------------------------------------
    const Mission& mission() const { return mission_; }
    Phase phase() const { return phase_; }
    int round() const { return round_; }
    int money() const { return money_; }                 // spendable in shops (unspent deployment budget carries over)
    int incomeNow() const;                               // money per round from owned buildings
    int kills() const { return kills_; }
    std::vector<ObjectiveLine> objectiveLines() const;
    const std::string& endReason() const { return endReason_; }

    // ---- deployment --------------------------------------------------------------
    bool inDeployZone(int x, int y) const;
    int budgetTotal() const { return mission_.budget; }
    int budgetLeft() const;
    bool canDeploy(const UnitDef& def, int x, int y) const;
    bool deploy(const UnitDef& def, int x, int y);
    bool undeploy(int x, int y);   // refunds the cost
    bool startBattle();            // needs >= 1 deployed unit

    // ---- player actions ----------------------------------------------------------
    bool canMove(const Unit& u) const;
    // cost[y*w+x] >= 0 for tiles the unit can end its move on, otherwise -1
    std::vector<int> reachable(const Unit& u) const;
    bool moveUnit(Unit& u, int x, int y);

    bool weaponReady(const Unit& u, int weaponIdx) const;
    bool canAttack(const Unit& a, int weaponIdx, const Unit& target) const;
    // damage of an attack, or the HP restored by a support weapon (heal/repair/supply)
    int previewDamage(const Unit& a, int weaponIdx, const Unit& target) const;
    bool canAttackBuilding(const Unit& a, int weaponIdx, const Building& b) const;
    int previewDamageBuilding(const Unit& a, int weaponIdx, const Building& b) const;
    bool attackBuilding(Unit& a, int weaponIdx, Building& b);
    bool attack(Unit& a, int weaponIdx, Unit& target);

    // ---- capturing: an infantry unit next to a building that can still move may take it ----
    bool canCapture(const Unit& u, const Building& b) const;
    bool captureBuilding(Unit& u, Building& b);

    // ---- building actions (e.g. a supply post heals/repairs/supplies adjacent friendly units) ----
    bool buildingWeaponReady(const Building& b, int weaponIdx) const;
    bool canBuildingSupport(const Building& b, int weaponIdx, const Unit& target) const;
    int previewSupport(const Building& b, int weaponIdx, const Unit& target) const;
    bool buildingSupport(Building& b, int weaponIdx, Unit& target);

    // ---- shop: buy units at an owned building with recruits > 0 ----
    bool canRecruit(const Building& b, const UnitDef& def) const;
    bool recruit(Building& b, const UnitDef& def);
    bool findSpawnTile(const Building& b, UnitClass cls, int& x, int& y) const;
    bool blockedByBuilding(int x, int y) const;   // intact building on that tile

    void endPlayerTurn();

    // ---- enemy turn: call repeatedly (e.g. every 0.4s) while phase()==EnemyTurn ------
    void stepAI();

    // UI pulls the events produced since the last call
    std::vector<BattleEvent> takeEvents();

private:
    void loadMap(const std::string& path);
    void beginPhase(Side s);
    void endPhase(Side s);
    void checkEnd();
    void finish(Phase p, const std::string& reason);
    void emit(BattleEvent e) { events_.push_back(std::move(e)); }
    void message(const std::string& text, Side s = Side::Neutral);
    void initUnit(Unit& u, const UnitDef& def) const;
    struct SupportResult { int healed = 0; bool refuelled = false, rearmed = false; int transferred = 0; };
    SupportResult applySupport(const WeaponDef& w, int& srcAmmo, Unit& t);
    std::string supportText(const std::string& who, const std::string& weapon, const Unit& t, const SupportResult& r) const;
    bool aiWantsBuilding(const Building& b) const;
    void checkTriggers(const Unit& mover);
    void spawnGroup(const std::string& group, int tx, int ty);
    int manhattan(int ax, int ay, int bx, int by) const;

    // pathfinding
    std::vector<int> dijkstra(const Unit& u) const;                 // cost for passing, -1 unreachable
    std::vector<int> distanceField(int gx, int gy, UnitClass c) const;

    // AI
    bool aiAttack(Unit& u);
    bool aiMove(Unit& u);

    const GameData& data_;
    Mission mission_;
    std::vector<const Terrain*> tiles_;
    std::vector<Building> buildings_;
    std::vector<Unit> units_;
    std::vector<std::pair<int, int>> deployZone_;
    struct Trigger { std::string group; int x0, y0, x1, y1; };
    std::vector<Trigger> triggers_;
    std::vector<std::string> firedGroups_;
    std::vector<BattleEvent> events_;
    std::deque<int> aiQueue_;
    Phase phase_ = Phase::Deploy;
    int round_ = 1;
    int nextId_ = 1;
    int money_ = 0;
    int kills_ = 0;
    std::string endReason_;
};
