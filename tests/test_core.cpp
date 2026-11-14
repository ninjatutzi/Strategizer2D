// Headless tests for data loading, damage formulas and the battle rules.
#include <cstdlib>
#include <iostream>
#include <random>

#include "Battle.hpp"
#include "Csv.hpp"

static int failures = 0;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n";    \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

static void testDamageFormula() {
    WeaponDef w;
    w.damage = 20; w.ap = 3;
    // armour == 0 : damage * terrain
    CHECK(computeDamage(w, 0, 1.0f) == 20);
    CHECK(computeDamage(w, 0, 0.75f) == 15);
    // armour > 0 : (damage * AP / armour) * terrain
    CHECK(computeDamage(w, 6, 1.0f) == 10);   // 20*3/6
    CHECK(computeDamage(w, 6, 1.3f) == 13);   // 10*1.3
    CHECK(computeDamage(w, 10, 0.6f) == 4);   // 6*0.6 = 3.6 -> 4
    w.damage = 1; w.ap = 1;
    CHECK(computeDamage(w, 10, 0.6f) == 1);   // never 0 when the weapon does damage
}

static void testData(GameData& d) {
    CHECK(d.terrains().size() >= 5);
    CHECK(d.unit("rifleman") && d.unit("trooper") && d.unit("light_tank") &&
          d.unit("medium_tank") && d.unit("heavy_tank"));
    for (auto& u : d.units()) {
        CHECK(!u.weapons.empty());
        CHECK(u.weapons.size() <= cfg::MaxWeapons);
        if (u.isInfantry()) CHECK(u.armour == 0);
    }
    CHECK(d.unit("heavy_tank")->armour == 10);
    CHECK(d.unit("light_tank")->maxAttacks() == 2);
    CHECK(d.missions().size() >= 5);
    for (auto& m : d.missions()) Battle b(d, m);   // throws if a map is broken
}

static void testRules(GameData& d) {
    Battle b(d, *d.mission("dawn_1"));
    const UnitDef& rifle = *d.unit("rifleman");
    const UnitDef& heavy = *d.unit("heavy_tank");

    // deployment
    CHECK(b.phase() == Phase::Deploy);
    CHECK(!b.canDeploy(rifle, 0, 0));          // outside deploy zone
    CHECK(b.deploy(rifle, 4, 10));
    CHECK(!b.deploy(rifle, 4, 10));            // occupied
    CHECK(b.budgetLeft() == b.budgetTotal() - rifle.cost);
    CHECK(!b.deploy(heavy, 5, 10));            // too expensive
    CHECK(b.undeploy(4, 10));
    CHECK(b.budgetLeft() == b.budgetTotal());
    CHECK(!b.startBattle());                   // nothing deployed
    CHECK(b.deploy(rifle, 4, 10));
    CHECK(b.startBattle());
    CHECK(b.phase() == Phase::PlayerTurn);

    // movement: 3 points on plains
    Unit* u = b.unitAt(4, 10);
    CHECK(u && b.canMove(*u));
    auto r = b.reachable(*u);
    CHECK(r[8 * 12 + 4] == 2);                 // two plains
    CHECK(r[7 * 12 + 4] == -1);                // forest (cost 2) -> total 4 > 3 move points
    CHECK(r[7 * 12 + 3] == -1);                // 4 tiles away
    CHECK(b.moveUnit(*u, 4, 8));
    CHECK(!b.canMove(*u));                     // one move per turn
    CHECK(!b.moveUnit(*u, 4, 7));
    CHECK(u->attacksLeft == 1);                // ...but the unit may still shoot after moving
}

static void testWillAndCooldown(GameData& d) {
    Battle b(d, *d.mission("dawn_2"));
    b.deploy(*d.unit("trooper"), 4, 10);
    b.startBattle();
    // put an enemy rifleman next to the trooper by hand via friendly access
    Unit* tr = b.unitAt(4, 10);
    Unit* en = nullptr;
    for (auto& u : const_cast<std::vector<Unit>&>(b.units()))
        if (u.side == Side::Enemy) { en = &u; break; }
    CHECK(tr && en);
    en->x = 4; en->y = 9;
    int hp0 = en->hp;
    CHECK(b.canAttack(*tr, 1, *en));           // grenade, range 2
    CHECK(b.attack(*tr, 1, *en));
    CHECK(en->hp < hp0);
    CHECK(en->will == en->def->will - 2);
    CHECK(tr->cooldown[1] == 3);               // grenade cooldown 2 -> unusable for the next 2 turns
    CHECK(!b.canAttack(*tr, 0, *en));          // one attack per turn, already used
    CHECK(b.canMove(*tr));                     // moving is still possible after shooting
    // run one full round
    b.endPlayerTurn();
    while (b.phase() == Phase::EnemyTurn) b.stepAI();
    if (b.phase() == Phase::PlayerTurn) {
        CHECK(b.round() == 2);
        CHECK(!b.weaponReady(*tr, 1));         // still cooling down
    }
}

static void testCaptureCamoRadio(GameData& d) {
    // capture: nobody can stand on a building; infantry next to it (with its move left) takes it
    {
        Battle b(d, *d.mission("dawn_1"));
        b.deploy(*d.unit("light_tank"), 6, 9);
        b.deploy(*d.unit("rifleman"), 5, 9);
        b.startBattle();
        Unit* tank = b.unitAt(6, 9);
        Unit* inf = b.unitAt(5, 9);
        tank->x = 6; tank->y = 2;              // teleport next to the outpost (6,1)
        inf->x = 7; inf->y = 1;
        CHECK(b.blockedByBuilding(6, 1));
        CHECK(!b.moveUnit(*tank, 6, 1));       // cannot enter a building tile
        CHECK(!b.canCapture(*tank, *b.buildingAt(6, 1)));   // vehicles cannot capture
        CHECK(b.canCapture(*inf, *b.buildingAt(6, 1)));
        CHECK(b.moveUnit(*inf, 8, 1));         // moving away uses the move: no capture any more
        CHECK(!b.canCapture(*inf, *b.buildingAt(6, 1)));
        inf->x = 7; inf->y = 1; inf->moved = false;
        CHECK(b.captureBuilding(*inf, *b.buildingAt(6, 1)));
        CHECK(b.buildingAt(6, 1)->owner == Side::Player);
        CHECK(inf->moved);
        CHECK(b.phase() == Phase::Won);
    }
    // camouflage: hidden unless an enemy is within CamoRevealRange
    {
        Battle b(d, *d.mission("dawn_1"));
        b.deploy(*d.unit("rifleman"), 5, 9);
        b.startBattle();
        UnitDef camo = *d.unit("rifleman");
        camo.attributes = {"camouflage"};
        Unit hidden = *b.unitAt(5, 9);
        hidden.def = &camo; hidden.side = Side::Enemy; hidden.x = 5; hidden.y = 5;
        CHECK(!b.visibleTo(hidden, Side::Player));   // distance 4
        hidden.y = 7;
        CHECK(b.visibleTo(hidden, Side::Player));    // distance 2
    }
    // radio: weapons that need a radio only work while a friendly radio unit is alive
    {
        UnitDef art = *d.unit("rifleman");
        art.weapons[0].needsRadio = true;
        UnitDef radioman = *d.unit("rifleman");
        radioman.attributes = {"radio"};
        Battle b(d, *d.mission("dawn_1"));
        b.deploy(art, 4, 10);
        b.startBattle();
        Unit* a = b.unitAt(4, 10);
        CHECK(!b.weaponReady(*a, 0));
    }
    {
        UnitDef art = *d.unit("rifleman");
        art.weapons[0].needsRadio = true;
        UnitDef radioman = *d.unit("rifleman");
        radioman.attributes = {"radio"};
        Battle b(d, *d.mission("dawn_1"));
        b.deploy(art, 4, 10);
        b.deploy(radioman, 5, 10);
        b.startBattle();
        CHECK(b.weaponReady(*b.unitAt(4, 10), 0));
    }
}

static void testFogAndTriggers(GameData& d) {
    Battle b(d, *d.mission("dawn_2"));
    b.deploy(*d.unit("rifleman"), 5, 9);
    b.startBattle();
    Unit* me = b.unitAt(5, 9);
    // vision: rifleman sees 5 tiles
    CHECK(b.tileVisible(5, 4, Side::Player));
    CHECK(!b.tileVisible(5, 3, Side::Player));
    // dormant group is invisible, not blocking and not attackable
    int dormant = 0;
    for (auto& u : b.units()) if (!u.active) { ++dormant; CHECK(!b.visibleTo(u, Side::Player)); CHECK(b.unitAt(u.x, u.y) == nullptr); }
    CHECK(dormant == 2);
    CHECK(b.aliveCount(Side::Enemy) >= 7);     // dormant units still count as enemies
    // walking onto the trigger zone (8..10, 6..7) wakes the group
    me->x = 8; me->y = 8;
    CHECK(b.moveUnit(*me, 8, 7));
    int active = 0;
    for (auto& u : b.units()) if (u.group == "ambush" && u.active) ++active;
    CHECK(active == 2);
    bool sawSpawn = false;
    for (auto& e : b.takeEvents()) if (e.kind == BattleEvent::Kind::Spawn) sawSpawn = true;
    CHECK(sawSpawn);
}

static void testSupportAndBuildings(GameData& d) {
    // buildings can be destroyed -> capture mission won without infantry
    {
        Battle b(d, *d.mission("dawn_1"));
        b.deploy(*d.unit("light_tank"), 6, 9);
        b.startBattle();
        Unit* tank = b.unitAt(6, 9);
        tank->x = 6; tank->y = 2;
        Building* post = b.buildingAt(6, 1);
        CHECK(post && post->hp == 100 && !post->destroyed);
        CHECK(b.canAttackBuilding(*tank, 0, *post));
        CHECK(b.attackBuilding(*tank, 0, *post));
        CHECK(post->hp == 75);
        b.attackBuilding(*tank, 0, *post);
        CHECK(!b.attackBuilding(*tank, 0, *post));   // only 2 attacks per turn
        tank->attacksLeft = 2;
        b.attackBuilding(*tank, 0, *post);
        tank->attacksLeft = 2;
        CHECK(b.attackBuilding(*tank, 0, *post));
        CHECK(post->destroyed);
        CHECK(b.phase() == Phase::Won);
    }
    // healing, repairing, refuelling, fuel use, self healing
    {
        Battle b(d, *d.mission("iron_2"));
        b.deploy(*d.unit("medic"), 5, 10);
        b.deploy(*d.unit("rifleman"), 5, 9);
        b.deploy(*d.unit("light_tank"), 7, 10);
        b.deploy(*d.unit("truck"), 8, 10);
        b.deploy(*d.unit("ambulance"), 3, 10);
        b.startBattle();
        Unit* medic = b.unitAt(5, 10);
        Unit* rifle = b.unitAt(5, 9);
        Unit* tank = b.unitAt(7, 10);
        Unit* truck = b.unitAt(8, 10);
        Unit* amb = b.unitAt(3, 10);
        CHECK(medic->def->attrValue("heilung") == 10);
        CHECK(truck->def->fuel == 0 && tank->fuel == tank->def->fuel);
        CHECK(!b.canAttack(*medic, 0, *rifle));        // full HP: nothing to heal
        rifle->hp = 20;
        CHECK(b.canAttack(*medic, 0, *rifle));
        CHECK(!b.canAttack(*medic, 0, *tank));         // medics cannot heal vehicles
        CHECK(b.attack(*medic, 0, *rifle));
        CHECK(rifle->hp == 45);                        // +25
        CHECK(!b.canAttack(*amb, 0, *medic));          // out of range (2) and nothing to heal
        // fuel: a move costs 1; at 0 the vehicle stays put
        CHECK(b.moveUnit(*tank, 7, 8));
        CHECK(tank->fuel == tank->def->fuel - 1);
        tank->hp = 50;
        tank->fuel = 0;
        CHECK(!b.canMove(*tank));
        CHECK(!b.canAttack(*medic, 0, *tank));
        truck->x = 7; truck->y = 9;
        CHECK(truck->attacksLeft == 3 && truck->ammo[0] == 5);   // supply 5, repair 4, light heal 4
        CHECK(b.canAttack(*truck, 1, *tank));          // repair (vehicles only)
        CHECK(b.attack(*truck, 1, *tank));
        CHECK(tank->hp == 90);
        CHECK(truck->ammo[1] == 3);
        CHECK(!b.canAttack(*truck, 2, *tank));         // light heal is for infantry
        tank->ammo[0] = 1;                             // tank is low on ammo and fuel
        CHECK(b.canAttack(*truck, 0, *tank));          // supply: refills ALL ammo and fuel
        CHECK(b.attack(*truck, 0, *tank));
        CHECK(tank->ammo[0] == tank->def->weapons[0].ammo && tank->fuel == tank->def->fuel);
        CHECK(truck->ammo[0] == 4);
        CHECK(!b.canAttack(*truck, 0, *tank));         // nothing left to supply
        rifle->hp = 10;
        rifle->x = 7; rifle->y = 8;                    // next to the truck
        CHECK(b.canAttack(*truck, 2, *rifle));         // light heal
        CHECK(b.attack(*truck, 2, *rifle));
        CHECK(rifle->hp == 20);                        // +10 only
        CHECK(truck->attacksLeft == 0);
        CHECK(!b.canAttack(*truck, 0, *truck));        // cannot target itself
        CHECK(!b.canAttack(*amb, 0, *amb));
        // self heal: medic +10 per round, ambulance none
        medic->hp = 30;
        amb->hp = 30;
        b.endPlayerTurn();
        while (b.phase() == Phase::EnemyTurn) b.stepAI();
        CHECK(medic->hp == 40);
        CHECK(amb->hp == 30);
    }
}

static void testAmmoEconomyBuildings(GameData& d) {
    // ammo: Rifleman has 3 shots, Trooper's assault rifle 4 (weapons.csv), per-unit overrides are possible
    CHECK(d.unit("rifleman")->weapons[0].ammo == 3);
    CHECK(d.unit("trooper")->weapons[0].ammo == 4);
    {
        Battle b(d, *d.mission("dawn_2"));
        b.deploy(*d.unit("rifleman"), 4, 10);
        b.startBattle();
        Unit* me = b.unitAt(4, 10);
        Unit* en = nullptr;
        for (auto& u : const_cast<std::vector<Unit>&>(b.units())) if (u.side == Side::Enemy && u.active) { en = &u; break; }
        en->x = 4; en->y = 8; en->hp = 500;
        for (int i = 0; i < 3; ++i) {
            CHECK(b.canAttack(*me, 0, *en));
            CHECK(b.attack(*me, 0, *en));
            me->attacksLeft = 1;                   // pretend a new turn
        }
        CHECK(me->ammo[0] == 0);
        CHECK(!b.canAttack(*me, 0, *en));          // out of ammo
        CHECK(!b.weaponReady(*me, 0));
    }
    // supply truck -> supply truck: add / subtract instead of a full refill
    {
        Battle b(d, *d.mission("iron_2"));
        b.deploy(*d.unit("truck"), 5, 10);
        b.deploy(*d.unit("truck"), 6, 10);
        b.startBattle();
        Unit* giver = b.unitAt(5, 10);
        Unit* taker = b.unitAt(6, 10);
        taker->ammo[0] = 1;
        CHECK(b.attack(*giver, 0, *taker));
        CHECK(giver->ammo[0] == 5 - 1 - 2);        // pays 1 use + hands over 2
        CHECK(taker->ammo[0] == 3);
        giver->ammo[0] = 1;                        // only 1 left: cannot give any more
        giver->attacksLeft = 3;
        CHECK(!b.canAttack(*giver, 0, *taker));
    }
    // money: unspent budget carries over; kills pay; owned buildings pay income; shop
    {
        Battle b(d, *d.mission("iron_1"));         // has a player outpost (+10) and supply post (+5)
        b.deploy(*d.unit("light_tank"), 5, 10);
        const int left = b.budgetLeft();
        b.startBattle();
        CHECK(b.money() == left);
        CHECK(b.incomeNow() == 15);
        Building* post = b.buildingAt(11, 10);
        CHECK(post && post->owner == Side::Player && post->def->recruits == 1);
        const UnitDef& rifle = *d.unit("rifleman");
        CHECK(b.canRecruit(*post, rifle));
        int before = b.money();
        CHECK(b.recruit(*post, rifle));
        CHECK(b.money() == before - rifle.cost);
        CHECK(!b.canRecruit(*post, rifle));        // one purchase per building and turn
        Unit* fresh = nullptr;
        for (auto& u : const_cast<std::vector<Unit>&>(b.units())) if (u.def == &rifle && u.side == Side::Player) fresh = &u;
        CHECK(fresh && !b.blockedByBuilding(fresh->x, fresh->y) && !b.canMove(*fresh));
        before = b.money();
        b.endPlayerTurn();
        CHECK(b.money() == before + 15);
        while (b.phase() == Phase::EnemyTurn) b.stepAI();
        CHECK(b.canRecruit(*post, rifle) == (b.money() >= rifle.cost));   // available again next turn
        // supply post heals unlimited times
        Unit* t = b.unitAt(5, 10);
        Building* sp = b.buildingAt(0, 10);
        t->x = 1; t->y = 10;
        t->hp = 10;
        CHECK(sp->def->weapons.size() == 3 && sp->ammo[0] == -1);
        CHECK(!b.canBuildingSupport(*sp, 0, *t));  // field hospital heals infantry only
        CHECK(b.canBuildingSupport(*sp, 1, *t));   // repair bay
        CHECK(b.buildingSupport(*sp, 1, *t));
        CHECK(t->hp == 40);
    }
    // kill reward
    {
        Battle b(d, *d.mission("dawn_2"));
        b.deploy(*d.unit("rifleman"), 4, 10);
        b.startBattle();
        Unit* me = b.unitAt(4, 10);
        Unit* en = nullptr;
        for (auto& u : const_cast<std::vector<Unit>&>(b.units())) if (u.side == Side::Enemy && u.active) { en = &u; break; }
        en->x = 4; en->y = 9; en->hp = 1;
        int before = b.money();
        CHECK(b.attack(*me, 0, *en));
        CHECK(!en->alive);
        CHECK(b.money() == before + d.killReward() && b.kills() == 1);
    }
    // protect / survive
    {
        Battle b(d, *d.mission("dawn_4"));
        CHECK(b.mission().objective == Objective::Survive);
        b.deploy(*d.unit("rifleman"), 5, 10);
        b.startBattle();
        Building* home = b.buildingAt(6, 8);
        CHECK(home && home->protect && home->owner == Side::Player);
        auto lines = b.objectiveLines();
        CHECK(lines.size() == 2);
        // lose it -> mission lost
        home->hp = 1;
        // an enemy unit shoots the building
        Unit* en = nullptr;
        for (auto& u : const_cast<std::vector<Unit>&>(b.units())) if (u.side == Side::Enemy) { en = &u; break; }
        b.endPlayerTurn();
        en->x = 6; en->y = 7; en->attacksLeft = 1;
        b.attackBuilding(*en, 0, *home);
        CHECK(home->destroyed);
        CHECK(b.phase() == Phase::Lost);
    }
    {
        Battle b(d, *d.mission("dawn_4"));
        b.deploy(*d.unit("rifleman"), 5, 10);
        b.startBattle();
        // make every enemy harmless and wait the rounds out
        for (auto& u : const_cast<std::vector<Unit>&>(b.units())) if (u.side == Side::Enemy) u.holdPosition = true;
        for (int i = 0; i < 12 && (b.phase() == Phase::PlayerTurn); ++i) {
            b.endPlayerTurn();
            for (int g = 0; g < 100 && b.phase() == Phase::EnemyTurn; ++g) b.stepAI();
        }
        CHECK(b.phase() == Phase::Won);
    }
}

static void testMapSizes(GameData& d) {
    // maps may have any rectangular size; broken maps give a readable error
    const std::string path = d.root() + "/data/maps/_size_test.map";
    auto write = [&](const std::string& tiles) {
        FILE* f = fopen(path.c_str(), "w");
        fputs(tiles.c_str(), f);
        fclose(f);
    };
    auto row = [](int w) { return std::string(static_cast<size_t>(w), 'P') + "\n"; };
    Mission m = *d.mission("dawn_2");   // an 'eliminate' mission needs no buildings
    m.mapFile = "_size_test.map";

    std::string big = "[tiles]\n";
    for (int y = 0; y < 16; ++y) big += row(20);
    big += "[deploy]\n2,13,9,15\n[enemies]\nrifleman,15,2,aggressive\n";
    write(big);
    {
        Battle b(d, m);
        CHECK(b.width() == 20 && b.height() == 16);
        CHECK(b.inBounds(19, 15) && !b.inBounds(20, 15) && !b.inBounds(19, 16));
        CHECK(b.deploy(*d.unit("rifleman"), 5, 14));
        CHECK(b.startBattle());
        Unit* u = b.unitAt(5, 14);
        auto r = b.reachable(*u);
        CHECK(static_cast<int>(r.size()) == 20 * 16);
    }
    bool threw = false;
    write("[tiles]\n" + row(10) + row(10) + row(9) + row(10) + row(10) + "[deploy]\n0,0,1,1\n");   // ragged row
    try { Battle b(d, m); } catch (const std::exception& e) { threw = std::string(e.what()).find("same length") != std::string::npos; }
    CHECK(threw);
    threw = false;
    write("[tiles]\n" + row(2) + row(2) + "[deploy]\n0,0,1,1\n");   // too small
    try { Battle b(d, m); } catch (const std::exception& e) { threw = std::string(e.what()).find("not allowed") != std::string::npos; }
    CHECK(threw);
    std::remove(path.c_str());
}

// Random-ish games: random player, built-in AI. Must terminate without crashing.
static void testSimulation(GameData& d) {
    std::mt19937 rng(1234);
    for (auto& m : d.missions()) {
        for (int game = 0; game < 20; ++game) {
            Battle b(d, m);
            // deploy random units
            for (int tries = 0; tries < 60; ++tries) {
                const UnitDef& def = d.units()[rng() % d.units().size()];
                b.deploy(def, 1 + static_cast<int>(rng() % 10), 9 + static_cast<int>(rng() % 3));
            }
            if (!b.startBattle()) continue;
            for (int step = 0; step < 400 && (b.phase() == Phase::PlayerTurn || b.phase() == Phase::EnemyTurn); ++step) {
                if (b.phase() == Phase::PlayerTurn) {
                    for (auto& u : const_cast<std::vector<Unit>&>(b.units())) {
                        if (!u.alive || u.side != Side::Player) continue;
                        // try attacks first
                        for (auto& t : const_cast<std::vector<Unit>&>(b.units()))
                            for (int w = 0; w < (int)u.def->weapons.size(); ++w)
                                if (t.alive && b.canAttack(u, w, t)) b.attack(u, w, t);
                        if (b.canMove(u)) {
                            auto r = b.reachable(u);
                            std::vector<int> opts;
                            for (int i = 0; i < (int)r.size(); ++i) if (r[i] >= 0) opts.push_back(i);
                            if (!opts.empty()) { int i = opts[rng() % opts.size()]; b.moveUnit(u, i % 12, i / 12); }
                        }
                    }
                    if (b.phase() == Phase::PlayerTurn) b.endPlayerTurn();
                } else {
                    int guard = 0;
                    while (b.phase() == Phase::EnemyTurn && guard++ < 100) b.stepAI();
                }
                b.takeEvents();
            }
            // invariants
            for (auto& u : b.units()) {
                CHECK(u.hp >= 0 && u.hp <= u.def->hp);
                CHECK(u.will >= 0 && u.will <= u.def->will);
                if (u.alive) CHECK(b.terrainAt(u.x, u.y).moveCost(u.def->cls) > 0);
            }
            for (auto& a : b.units())
                for (auto& c : b.units())
                    if (&a != &c && a.alive && c.alive) CHECK(!(a.x == c.x && a.y == c.y));
        }
    }
}

static void testCsv() {
    // semicolon files with decimal commas (German Excel)
    std::string path = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/strat_test.csv";
    {
        FILE* f = fopen(path.c_str(), "w");
        fputs("id;value\r\n# comment\r\na;1,5\r\n;;\r\n", f);
        fclose(f);
    }
    auto rows = readCsv(path);
    CHECK(rows.size() == 1);
    CHECK(rows[0].requireFloat("value") == 1.5f);
    std::remove(path.c_str());
}

int main(int argc, char** argv) {
    std::string root = argc > 1 ? argv[1] : findGameRoot(argv[0]);
    GameData d;
    try {
        d.load(root);
        testDamageFormula();
        testCsv();
        testData(d);
        testRules(d);
        testWillAndCooldown(d);
        testCaptureCamoRadio(d);
        testFogAndTriggers(d);
        testSupportAndBuildings(d);
        testAmmoEconomyBuildings(d);
        testMapSizes(d);
        testSimulation(d);
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << "\n";
        return 1;
    }
    if (failures) { std::cerr << failures << " check(s) failed\n"; return 1; }
    std::cout << "all tests passed\n";
    return 0;
}
