#include "Battle.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <fstream>
#include <queue>
#include <stdexcept>

#include "Csv.hpp"

int computeDamage(const WeaponDef& w, int defenderArmour, float terrainMult) {
    float dmg;
    if (defenderArmour <= 0) dmg = w.damage * terrainMult;
    else dmg = (w.damage * w.ap / static_cast<float>(defenderArmour)) * terrainMult;
    int result = static_cast<int>(std::lround(dmg));
    if (result < 1 && dmg > 0.f) result = 1;
    return result;
}

// ---------------------------------------------------------------------------------
// Map loading
// ---------------------------------------------------------------------------------
Battle::Battle(const GameData& data, const Mission& mission) : data_(data), mission_(mission) {
    loadMap(data.root() + "/data/maps/" + mission.mapFile);
}

void Battle::loadMap(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open map " + path);

    auto fail = [&](int line, const std::string& msg) {
        throw std::runtime_error(path + ":" + std::to_string(line) + ": " + msg);
    };

    std::string section, raw;
    int lineNo = 0;
    std::vector<std::string> rows;
    int nextUnitId = 1;
    while (std::getline(in, raw)) {
        ++lineNo;
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') {
            section = toLower(line);
            continue;
        }
        if (section == "[tiles]") {
            if (static_cast<int>(line.size()) != cfg::MapSize)
                fail(lineNo, "tile row must have exactly " + std::to_string(cfg::MapSize) + " characters");
            rows.push_back(line);
        } else if (section == "[deploy]") {
            auto p = split(line, ',');
            if (p.size() != 4) fail(lineNo, "deploy line must be x0,y0,x1,y1");
            int v[4];
            for (int i = 0; i < 4; ++i) v[i] = std::atoi(trim(p[i]).c_str());
            for (int y = v[1]; y <= v[3]; ++y)
                for (int x = v[0]; x <= v[2]; ++x) {
                    if (!inBounds(x, y)) fail(lineNo, "deploy zone outside the map");
                    deployZone_.emplace_back(x, y);
                }
        } else if (section == "[buildings]") {
            auto p = split(line, ',');
            if (p.size() < 4) fail(lineNo, "building line must be name,x,y,owner[,objective[,hp[,armour]]]");
            Building b;
            b.name = trim(p[0]);
            b.x = std::atoi(trim(p[1]).c_str());
            b.y = std::atoi(trim(p[2]).c_str());
            std::string o = toLower(trim(p[3]));
            b.owner = o == "player" ? Side::Player : o == "enemy" ? Side::Enemy : Side::Neutral;
            b.objective = p.size() < 5 || trim(p[4]) != "0";
            if (p.size() > 5 && !trim(p[5]).empty()) b.hp = b.maxHp = std::max(1, std::atoi(trim(p[5]).c_str()));
            if (p.size() > 6 && !trim(p[6]).empty()) b.armour = std::max(0, std::atoi(trim(p[6]).c_str()));
            if (!inBounds(b.x, b.y)) fail(lineNo, "building outside the map");
            buildings_.push_back(b);
        } else if (section == "[triggers]") {
            auto p = split(line, ',');
            if (p.size() != 5) fail(lineNo, "trigger line must be group,x0,y0,x1,y1");
            Trigger t;
            t.group = toLower(trim(p[0]));
            t.x0 = std::atoi(trim(p[1]).c_str()); t.y0 = std::atoi(trim(p[2]).c_str());
            t.x1 = std::atoi(trim(p[3]).c_str()); t.y1 = std::atoi(trim(p[4]).c_str());
            if (!inBounds(t.x0, t.y0) || !inBounds(t.x1, t.y1) || t.x0 > t.x1 || t.y0 > t.y1)
                fail(lineNo, "trigger rectangle invalid or outside the map");
            triggers_.push_back(t);
        } else if (section == "[enemies]") {
            auto p = split(line, ',');
            if (p.size() < 3) fail(lineNo, "enemy line must be unit_id,x,y[,aggressive|hold]");
            const UnitDef* def = data_.unit(toLower(trim(p[0])));
            if (!def) fail(lineNo, "unknown unit '" + trim(p[0]) + "'");
            Unit u;
            u.id = nextUnitId++;
            u.def = def;
            u.side = Side::Enemy;
            u.x = std::atoi(trim(p[1]).c_str());
            u.y = std::atoi(trim(p[2]).c_str());
            u.hp = def->hp;
            u.will = def->will;
            u.fuel = def->fuel;
            u.holdPosition = p.size() > 3 && toLower(trim(p[3])) == "hold";
            if (p.size() > 4 && !trim(p[4]).empty()) {
                u.group = toLower(trim(p[4]));
                u.active = false;   // dormant until the group is triggered
            }
            if (!inBounds(u.x, u.y)) fail(lineNo, "enemy outside the map");
            units_.push_back(u);
        } else {
            fail(lineNo, "line outside of a known section ([tiles] [deploy] [buildings] [triggers] [enemies])");
        }
    }
    nextId_ = nextUnitId;

    if (static_cast<int>(rows.size()) != cfg::MapSize)
        throw std::runtime_error(path + ": map needs exactly " + std::to_string(cfg::MapSize) + " tile rows, found " +
                                 std::to_string(rows.size()));
    tiles_.resize(cfg::MapSize * cfg::MapSize);
    for (int y = 0; y < cfg::MapSize; ++y)
        for (int x = 0; x < cfg::MapSize; ++x) {
            const Terrain* t = data_.terrainBySymbol(rows[y][x]);
            if (!t) throw std::runtime_error(path + ": unknown terrain symbol '" + std::string(1, rows[y][x]) +
                                             "' at x=" + std::to_string(x) + " y=" + std::to_string(y));
            tiles_[y * width() + x] = t;
        }

    // validation
    if (deployZone_.empty()) throw std::runtime_error(path + ": [deploy] zone is empty");
    for (auto& u : units_) {
        if (!u.group.empty() &&
            std::none_of(triggers_.begin(), triggers_.end(), [&](const Trigger& t) { return t.group == u.group; }))
            throw std::runtime_error(path + ": enemy group '" + u.group + "' has no [triggers] entry");
        if (terrainAt(u.x, u.y).moveCost(u.def->cls) == 0)
            throw std::runtime_error(path + ": enemy '" + u.def->id + "' placed on impassable terrain");
        for (auto& o : units_)
            if (&o != &u && o.x == u.x && o.y == u.y)
                throw std::runtime_error(path + ": two enemies on tile " + std::to_string(u.x) + "," + std::to_string(u.y));
    }
    if (mission_.objective == Objective::Capture &&
        std::none_of(buildings_.begin(), buildings_.end(), [](const Building& b) { return b.objective; }))
        throw std::runtime_error(path + ": capture mission needs at least one objective building");
}

// ---------------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------------
const Building* Battle::buildingAt(int x, int y) const {
    for (auto& b : buildings_) if (b.x == x && b.y == y) return &b;
    return nullptr;
}
Building* Battle::buildingAt(int x, int y) {
    for (auto& b : buildings_) if (b.x == x && b.y == y) return &b;
    return nullptr;
}

Unit* Battle::unitAt(int x, int y) {
    for (auto& u : units_) if (u.present() && u.x == x && u.y == y) return &u;
    return nullptr;
}
const Unit* Battle::unitAt(int x, int y) const {
    for (auto& u : units_) if (u.present() && u.x == x && u.y == y) return &u;
    return nullptr;
}
Unit* Battle::unitById(int id) {
    for (auto& u : units_) if (u.id == id) return &u;
    return nullptr;
}
const Unit* Battle::unitById(int id) const {
    for (auto& u : units_) if (u.id == id) return &u;
    return nullptr;
}

int Battle::aliveCount(Side s) const {
    return static_cast<int>(std::count_if(units_.begin(), units_.end(),
                                          [&](const Unit& u) { return u.alive && u.side == s; }));
}

int Battle::manhattan(int ax, int ay, int bx, int by) const { return std::abs(ax - bx) + std::abs(ay - by); }

bool Battle::visibleTo(const Unit& target, Side viewer) const {
    if (!target.present()) return false;
    if (target.side == viewer) return true;
    const bool camo = target.def->has("camouflage");
    for (auto& u : units_) {
        if (!u.present() || u.side != viewer) continue;
        int limit = u.def->vision;
        if (camo) limit = std::min(limit, cfg::CamoRevealRange);
        if (manhattan(u.x, u.y, target.x, target.y) <= limit) return true;
    }
    return false;
}

bool Battle::tileVisible(int x, int y, Side viewer) const {
    for (auto& u : units_)
        if (u.present() && u.side == viewer && manhattan(u.x, u.y, x, y) <= u.def->vision) return true;
    return false;
}

bool Battle::hasRadio(Side s) const {
    return std::any_of(units_.begin(), units_.end(),
                       [&](const Unit& u) { return u.present() && u.side == s && u.def->has("radio"); });
}

std::vector<BattleEvent> Battle::takeEvents() {
    std::vector<BattleEvent> out;
    out.swap(events_);
    return out;
}

void Battle::message(const std::string& text, Side s) {
    BattleEvent e;
    e.kind = BattleEvent::Kind::Message;
    e.text = text;
    e.side = s;
    emit(e);
}

// ---------------------------------------------------------------------------------
// Deployment
// ---------------------------------------------------------------------------------
bool Battle::inDeployZone(int x, int y) const {
    return std::find(deployZone_.begin(), deployZone_.end(), std::make_pair(x, y)) != deployZone_.end();
}

int Battle::budgetLeft() const {
    int spent = 0;
    for (auto& u : units_) if (u.alive && u.side == Side::Player) spent += u.def->cost;
    return mission_.budget - spent;
}

bool Battle::canDeploy(const UnitDef& def, int x, int y) const {
    return phase_ == Phase::Deploy && inBounds(x, y) && inDeployZone(x, y) && !unitAt(x, y) &&
           terrainAt(x, y).moveCost(def.cls) > 0 && def.cost <= budgetLeft();
}

bool Battle::deploy(const UnitDef& def, int x, int y) {
    if (!canDeploy(def, x, y)) return false;
    Unit u;
    u.id = nextId_++;
    u.def = &def;
    u.side = Side::Player;
    u.x = x;
    u.y = y;
    u.hp = def.hp;
    u.will = def.will;
    u.fuel = def.fuel;
    units_.push_back(u);
    return true;
}

bool Battle::undeploy(int x, int y) {
    if (phase_ != Phase::Deploy) return false;
    for (auto it = units_.begin(); it != units_.end(); ++it)
        if (it->alive && it->side == Side::Player && it->x == x && it->y == y) {
            units_.erase(it);
            return true;
        }
    return false;
}

bool Battle::startBattle() {
    if (phase_ != Phase::Deploy || aliveCount(Side::Player) == 0) return false;
    phase_ = Phase::PlayerTurn;
    round_ = 1;
    beginPhase(Side::Player);
    message("Runde 1 - dein Zug");
    return true;
}

// ---------------------------------------------------------------------------------
// Turn flow
// ---------------------------------------------------------------------------------
void Battle::beginPhase(Side s) {
    for (auto& u : units_) {
        if (!u.alive || u.side != s) continue;
        u.attacksLeft = u.def->maxAttacks();
        u.moved = false;
        for (auto& c : u.cooldown) if (c > 0) --c;
        // passive self-healing: attribute heilung(N) restores N HP at the start of every own turn
        int regen = u.def->has("heilung") ? u.def->attrValue("heilung") : 0;
        if (regen > 0 && u.hp < u.def->hp) {
            int gained = std::min(regen, u.def->hp - u.hp);
            u.hp += gained;
            BattleEvent e;
            e.kind = BattleEvent::Kind::Heal;
            e.ax = e.bx = u.x;
            e.ay = e.by = u.y;
            e.damage = gained;
            e.side = s;
            e.text = u.def->name + " heilt sich: +" + std::to_string(gained) + " HP";
            emit(e);
        }
    }
}

// Willpower is restored at the end of the owning side's phase: a unit that was
// broken (will == 0) during the enemy's phase cannot move in its next phase and
// recovers right after it.
void Battle::endPhase(Side s) {
    for (auto& u : units_) {
        if (!u.alive || u.side != s) continue;
        u.will = u.def->will;
        u.attacksLeft = 0;
    }
}

void Battle::finish(Phase p, const std::string& reason) {
    phase_ = p;
    endReason_ = reason;
    message(reason);
}

void Battle::checkEnd() {
    if (phase_ != Phase::PlayerTurn && phase_ != Phase::EnemyTurn) return;
    if (aliveCount(Side::Enemy) == 0) return finish(Phase::Won, "Alle gegnerischen Truppen wurden vernichtet!");
    if (aliveCount(Side::Player) == 0) return finish(Phase::Lost, "Alle deine Truppen wurden vernichtet!");
    if (mission_.objective == Objective::Capture) {
        bool all = true;
        for (auto& b : buildings_) if (b.objective && b.owner != Side::Player && !b.destroyed) all = false;
        if (all) finish(Phase::Won, "Alle Ziele wurden eingenommen oder zerstört!");
    }
}

void Battle::endPlayerTurn() {
    if (phase_ != Phase::PlayerTurn) return;
    endPhase(Side::Player);
    phase_ = Phase::EnemyTurn;
    beginPhase(Side::Enemy);
    aiQueue_.clear();
    for (auto& u : units_) if (u.present() && u.side == Side::Enemy) aiQueue_.push_back(u.id);
    message("Gegnerzug", Side::Enemy);
}

// ---------------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------------
bool Battle::canMove(const Unit& u) const {
    return u.present() && !u.moved && u.will > 0 && (u.def->fuel == 0 || u.fuel > 0) &&
           ((u.side == Side::Player && phase_ == Phase::PlayerTurn) ||
            (u.side == Side::Enemy && phase_ == Phase::EnemyTurn));
}

std::vector<int> Battle::dijkstra(const Unit& u) const {
    const int w = width(), n = w * height();
    std::vector<int> dist(n, INT_MAX);
    using Node = std::pair<int, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;
    dist[u.y * w + u.x] = 0;
    pq.push({0, u.y * w + u.x});
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    while (!pq.empty()) {
        auto [d, idx] = pq.top();
        pq.pop();
        if (d > dist[idx]) continue;
        int cx = idx % w, cy = idx / w;
        for (int k = 0; k < 4; ++k) {
            int nx = cx + dx[k], ny = cy + dy[k];
            if (!inBounds(nx, ny)) continue;
            int cost = terrainAt(nx, ny).moveCost(u.def->cls);
            if (cost <= 0) continue;
            const Unit* o = unitAt(nx, ny);
            if (o && o->side != u.side) continue;   // enemies block, friends can be passed
            int nd = d + cost;
            if (nd > u.def->move) continue;
            if (nd < dist[ny * w + nx]) {
                dist[ny * w + nx] = nd;
                pq.push({nd, ny * w + nx});
            }
        }
    }
    for (auto& d : dist) if (d == INT_MAX) d = -1;
    return dist;
}

std::vector<int> Battle::reachable(const Unit& u) const {
    std::vector<int> dist = dijkstra(u);
    for (int y = 0; y < height(); ++y)
        for (int x = 0; x < width(); ++x) {
            int i = y * width() + x;
            if ((x == u.x && y == u.y) || unitAt(x, y)) dist[i] = -1;
        }
    return dist;
}

bool Battle::moveUnit(Unit& u, int x, int y) {
    if (!canMove(u) || !inBounds(x, y)) return false;
    auto r = reachable(u);
    if (r[y * width() + x] < 0) return false;
    BattleEvent e;
    e.kind = BattleEvent::Kind::Move;
    e.ax = u.x; e.ay = u.y; e.bx = x; e.by = y;
    e.side = u.side;
    u.x = x;
    u.y = y;
    u.moved = true;
    if (u.def->fuel > 0) --u.fuel;   // one fuel per turn in which a vehicle moves
    emit(e);
    tryCapture(u);
    if (u.side == Side::Player) checkTriggers(u);
    checkEnd();
    return true;
}

void Battle::tryCapture(Unit& u) {
    if (!u.def->isInfantry()) return;   // only infantry can take buildings
    for (auto& b : buildings_) {
        if (b.x == u.x && b.y == u.y && b.owner != u.side && !b.destroyed) {
            b.owner = u.side;
            BattleEvent e;
            e.kind = BattleEvent::Kind::Capture;
            e.ax = e.bx = b.x;
            e.ay = e.by = b.y;
            e.side = u.side;
            e.text = b.name + (u.side == Side::Player ? " eingenommen!" : " vom Gegner eingenommen!");
            emit(e);
        }
    }
}

// ---------------------------------------------------------------------------------
// Trigger groups: dormant enemies appear when a player unit stands on one of the
// group's trigger tiles (touching any tile of a group triggers the whole group)
// ---------------------------------------------------------------------------------
void Battle::checkTriggers(const Unit& mover) {
    for (auto& t : triggers_) {
        if (mover.x < t.x0 || mover.x > t.x1 || mover.y < t.y0 || mover.y > t.y1) continue;
        if (std::find(firedGroups_.begin(), firedGroups_.end(), t.group) != firedGroups_.end()) continue;
        firedGroups_.push_back(t.group);
        spawnGroup(t.group, mover.x, mover.y);
    }
}

void Battle::spawnGroup(const std::string& group, int tx, int ty) {
    int count = 0;
    for (auto& u : units_) {
        if (u.active || u.group != group || !u.alive) continue;
        if (unitAt(u.x, u.y)) {   // spawn tile taken: use the nearest free passable tile
            int best = INT_MAX, bx = -1, by = -1;
            for (int y = 0; y < height(); ++y)
                for (int x = 0; x < width(); ++x) {
                    if (unitAt(x, y) || terrainAt(x, y).moveCost(u.def->cls) == 0) continue;
                    int d = manhattan(x, y, u.x, u.y);
                    if (d < best) { best = d; bx = x; by = y; }
                }
            if (bx >= 0) { u.x = bx; u.y = by; }
        }
        u.active = true;
        ++count;
    }
    if (count == 0) return;
    BattleEvent e;
    e.kind = BattleEvent::Kind::Spawn;
    e.ax = e.bx = tx;
    e.ay = e.by = ty;
    e.side = Side::Enemy;
    e.text = "Hinterhalt! Feindliche Verstärkung ist aufgetaucht!";
    emit(e);
}

// ---------------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------------
bool Battle::weaponReady(const Unit& u, int wi) const {
    if (wi < 0 || wi >= static_cast<int>(u.def->weapons.size())) return false;
    if (u.cooldown[wi] > 0) return false;
    if (u.def->weapons[wi].needsRadio && !hasRadio(u.side)) return false;
    return true;
}

namespace {
bool supportUseful(const WeaponDef& w, const Unit& t) {
    const bool inf = t.def->isInfantry();
    const bool needsHp = t.hp < t.def->hp;
    const bool needsFuel = t.def->fuel > 0 && t.fuel < t.def->fuel;
    switch (w.type) {
    case WeaponType::Heal: return inf && needsHp;
    case WeaponType::Repair: return !inf && needsHp;
    case WeaponType::Refuel: return !inf && needsFuel;
    case WeaponType::Supply: return !inf && (needsHp || needsFuel);
    default: return false;
    }
}
} // namespace

bool Battle::canAttack(const Unit& a, int wi, const Unit& t) const {
    if (!a.present() || !t.present() || &a == &t || a.attacksLeft <= 0) return false;
    if (!((a.side == Side::Player && phase_ == Phase::PlayerTurn) ||
          (a.side == Side::Enemy && phase_ == Phase::EnemyTurn)))
        return false;
    if (!weaponReady(a, wi)) return false;
    const WeaponDef& w = a.def->weapons[wi];
    if (w.isSupport()) {
        if (t.side != a.side || !supportUseful(w, t)) return false;   // only friendly units that need it
    } else if (t.side == a.side || !visibleTo(t, a.side)) {
        return false;
    }
    int d = manhattan(a.x, a.y, t.x, t.y);
    return d >= 1 && d <= w.range;
}

int Battle::previewDamage(const Unit& a, int wi, const Unit& t) const {
    const WeaponDef& w = a.def->weapons[wi];
    if (w.isSupport()) {
        if (w.type == WeaponType::Refuel) return 0;
        return std::min(static_cast<int>(std::lround(w.damage)), t.def->hp - t.hp);
    }
    return computeDamage(w, t.def->armour, terrainAt(t.x, t.y).damageMult);
}

bool Battle::attack(Unit& a, int wi, Unit& t) {
    if (!canAttack(a, wi, t)) return false;
    const WeaponDef& w = a.def->weapons[wi];
    a.cooldown[wi] = w.cooldown > 0 ? w.cooldown + 1 : 0;
    --a.attacksLeft;

    if (w.isSupport()) {
        int healed = 0;
        if (w.type != WeaponType::Refuel) {
            healed = previewDamage(a, wi, t);
            t.hp += healed;
        }
        bool refuelled = false;
        if ((w.type == WeaponType::Refuel || w.type == WeaponType::Supply) && t.def->fuel > 0 && t.fuel < t.def->fuel) {
            t.fuel = t.def->fuel;
            refuelled = true;
        }
        BattleEvent e;
        e.kind = BattleEvent::Kind::Heal;
        e.ax = a.x; e.ay = a.y; e.bx = t.x; e.by = t.y;
        e.damage = healed;
        e.side = a.side;
        e.text = a.def->name + " versorgt " + t.def->name + " (" + w.name + "):";
        if (healed > 0) e.text += " +" + std::to_string(healed) + " HP";
        if (refuelled) e.text += " Treibstoff aufgefüllt";
        emit(e);
        return true;
    }

    int dmg = previewDamage(a, wi, t);
    t.hp -= dmg;
    int willBefore = t.will;
    t.will = std::max(0, t.will - w.willDamage);

    BattleEvent e;
    e.kind = BattleEvent::Kind::Attack;
    e.ax = a.x; e.ay = a.y; e.bx = t.x; e.by = t.y;
    e.damage = dmg;
    e.side = a.side;
    e.text = a.def->name + " greift " + t.def->name + " an (" + w.name + "): -" + std::to_string(dmg) + " HP";
    if (willBefore > 0 && t.will == 0 && t.hp > 0) e.text += " - Moral gebrochen!";
    emit(e);

    if (t.hp <= 0) {
        t.hp = 0;
        t.alive = false;
        BattleEvent d;
        d.kind = BattleEvent::Kind::Death;
        d.ax = d.bx = t.x;
        d.ay = d.by = t.y;
        d.side = t.side;
        d.text = t.def->name + " wurde vernichtet";
        emit(d);
    }
    checkEnd();
    return true;
}

// ---- buildings can be shot to pieces (so a mission can be won without infantry) ----------
bool Battle::canAttackBuilding(const Unit& a, int wi, const Building& b) const {
    if (!a.present() || a.attacksLeft <= 0 || b.destroyed || b.owner == a.side) return false;
    if (!((a.side == Side::Player && phase_ == Phase::PlayerTurn) ||
          (a.side == Side::Enemy && phase_ == Phase::EnemyTurn)))
        return false;
    if (!weaponReady(a, wi) || a.def->weapons[wi].isSupport()) return false;
    if (!tileVisible(b.x, b.y, a.side)) return false;
    int d = manhattan(a.x, a.y, b.x, b.y);
    return d >= 1 && d <= a.def->weapons[wi].range;
}

int Battle::previewDamageBuilding(const Unit& a, int wi, const Building& b) const {
    return computeDamage(a.def->weapons[wi], b.armour, terrainAt(b.x, b.y).damageMult);
}

bool Battle::attackBuilding(Unit& a, int wi, Building& b) {
    if (!canAttackBuilding(a, wi, b)) return false;
    const WeaponDef& w = a.def->weapons[wi];
    int dmg = previewDamageBuilding(a, wi, b);
    b.hp -= dmg;
    a.cooldown[wi] = w.cooldown > 0 ? w.cooldown + 1 : 0;
    --a.attacksLeft;

    BattleEvent e;
    e.kind = BattleEvent::Kind::Attack;
    e.ax = a.x; e.ay = a.y; e.bx = b.x; e.by = b.y;
    e.damage = dmg;
    e.side = a.side;
    e.text = a.def->name + " beschießt " + b.name + " (" + w.name + "): -" + std::to_string(dmg) + " HP";
    emit(e);
    if (b.hp <= 0) {
        b.hp = 0;
        b.destroyed = true;
        BattleEvent d;
        d.kind = BattleEvent::Kind::Destroyed;
        d.ax = d.bx = b.x;
        d.ay = d.by = b.y;
        d.side = a.side;
        d.text = b.name + " wurde zerstört!";
        emit(d);
    }
    checkEnd();
    return true;
}

// ---------------------------------------------------------------------------------
// AI
// ---------------------------------------------------------------------------------
std::vector<int> Battle::distanceField(int gx, int gy, UnitClass c) const {
    const int w = width(), n = w * height();
    std::vector<int> dist(n, INT_MAX);
    using Node = std::pair<int, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;
    dist[gy * w + gx] = 0;
    pq.push({0, gy * w + gx});
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    while (!pq.empty()) {
        auto [d, idx] = pq.top();
        pq.pop();
        if (d > dist[idx]) continue;
        int cx = idx % w, cy = idx / w;
        // cost of stepping from the neighbour onto (cx,cy) = entry cost of (cx,cy)
        int step = (idx == gy * w + gx) ? 1 : terrainAt(cx, cy).moveCost(c);
        if (step <= 0) continue;
        for (int k = 0; k < 4; ++k) {
            int nx = cx + dx[k], ny = cy + dy[k];
            if (!inBounds(nx, ny)) continue;
            int nd = d + step;
            if (nd < dist[ny * w + nx]) {
                dist[ny * w + nx] = nd;
                pq.push({nd, ny * w + nx});
            }
        }
    }
    return dist;
}

bool Battle::aiAttack(Unit& u) {
    bool acted = false;
    while (u.alive && u.attacksLeft > 0 && phase_ == Phase::EnemyTurn) {
        int bestW = -1;
        Unit* bestT = nullptr;
        float bestScore = -1.f;
        for (int wi = 0; wi < static_cast<int>(u.def->weapons.size()); ++wi) {
            if (u.def->weapons[wi].isSupport()) continue;   // the AI does not use support weapons
            for (auto& t : units_) {
                if (!canAttack(u, wi, t)) continue;
                int dmg = previewDamage(u, wi, t);
                float score = static_cast<float>(dmg) + (dmg >= t.hp ? 1000.f : 0.f) +
                              static_cast<float>(t.def->will > 0 && t.will > 0 ? u.def->weapons[wi].willDamage : 0) +
                              (200.f - static_cast<float>(t.hp)) * 0.05f;
                if (score > bestScore) {
                    bestScore = score;
                    bestW = wi;
                    bestT = &t;
                }
            }
        }
        if (!bestT) break;
        if (!attack(u, bestW, *bestT)) break;
        acted = true;
    }
    return acted;
}

bool Battle::aiMove(Unit& u) {
    if (u.holdPosition || !canMove(u)) return false;

    // pick a goal: nearest player unit, or (infantry in capture missions) the nearest
    // building not yet held by the enemy when no player unit is close
    const Unit* nearest = nullptr;
    int nearestD = INT_MAX;
    for (auto& p : units_) {
        if (!p.alive || p.side != Side::Player) continue;
        int d = manhattan(u.x, u.y, p.x, p.y);
        if (d < nearestD) { nearestD = d; nearest = &p; }
    }
    const Building* bld = nullptr;
    int bldD = INT_MAX;
    if (u.def->isInfantry() && mission_.objective == Objective::Capture) {
        for (auto& b : buildings_) {
            if (!b.objective || b.owner == Side::Enemy || b.destroyed) continue;
            int d = manhattan(u.x, u.y, b.x, b.y);
            if (d < bldD) { bldD = d; bld = &b; }
        }
    }
    int gx, gy;
    if (bld && (!nearest || nearestD > 4)) { gx = bld->x; gy = bld->y; }
    else if (nearest) { gx = nearest->x; gy = nearest->y; }
    else return false;

    auto field = distanceField(gx, gy, u.def->cls);
    auto reach = reachable(u);
    const int w = width();
    int cur = field[u.y * w + u.x];
    int bestIdx = -1, bestVal = cur;
    float bestMult = 10.f;
    for (int i = 0; i < w * height(); ++i) {
        if (reach[i] < 0 || field[i] == INT_MAX) continue;
        float mult = terrainAt(i % w, i / w).damageMult;
        if (field[i] < bestVal || (field[i] == bestVal && bestIdx >= 0 && mult < bestMult)) {
            bestVal = field[i];
            bestIdx = i;
            bestMult = mult;
        }
    }
    if (bestIdx < 0) return false;
    return moveUnit(u, bestIdx % w, bestIdx / w);
}

void Battle::stepAI() {
    if (phase_ != Phase::EnemyTurn) return;
    while (true) {
        if (aiQueue_.empty()) {
            endPhase(Side::Enemy);
            ++round_;
            if (mission_.maxTurns > 0 && round_ > mission_.maxTurns) {
                finish(Phase::Lost, "Die Zeit ist abgelaufen!");
                return;
            }
            phase_ = Phase::PlayerTurn;
            beginPhase(Side::Player);
            message("Runde " + std::to_string(round_) + " - dein Zug");
            return;
        }
        int id = aiQueue_.front();
        aiQueue_.pop_front();
        Unit* u = unitById(id);
        if (!u || !u->alive) continue;
        bool acted = aiAttack(*u);                        // shoot what is already in range
        if (phase_ == Phase::EnemyTurn && aiMove(*u)) acted = true;
        if (phase_ == Phase::EnemyTurn && aiAttack(*u)) acted = true;   // ...and again after moving
        if (phase_ != Phase::EnemyTurn) return;   // game ended during this action
        if (acted) return;
    }
}
