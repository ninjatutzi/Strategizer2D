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
    bool holdPosition = false;   // AI: never moves, only shoots
    bool alive = true;

    bool moved = false;          // has used its move this turn (a unit may move once AND attack)
    bool active = true;          // false = dormant enemy that spawns when its group is triggered
    std::string group;           // trigger group (enemies only)

    bool present() const { return alive && active; }
};

struct Building {
    std::string name;
    int x = 0, y = 0;
    Side owner = Side::Neutral;
    bool objective = true;
    int hp = 100, maxHp = 100;   // buildings can be destroyed
    int armour = 0;
    bool destroyed = false;
};

struct BattleEvent {
    enum class Kind { Message, Move, Attack, Capture, Death, Spawn, Heal, Destroyed };
    Kind kind = Kind::Message;
    std::string text;
    int ax = 0, ay = 0, bx = 0, by = 0;
    int damage = 0;
    Side side = Side::Neutral;
};

enum class Phase { Deploy, PlayerTurn, EnemyTurn, Won, Lost };

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
    void tryCapture(Unit& u);
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
    std::string endReason_;
};
