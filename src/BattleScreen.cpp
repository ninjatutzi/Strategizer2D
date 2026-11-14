#include "BattleScreen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Screens.hpp"

namespace {

constexpr float kTile = 60.f;
constexpr float kBoardX = 20.f, kBoardY = 70.f;
constexpr float kPanelX = 770.f, kPanelY = 70.f, kPanelW = 410.f, kPanelH = 720.f;
constexpr float kInnerX = kPanelX + 20.f, kInnerW = kPanelW - 40.f;
constexpr int kDeployRows = 6;
constexpr float kDeployRowH = 62.f;

sf::Color shade(sf::Color c, float f) {
    auto ch = [&](sf::Uint8 v) { return static_cast<sf::Uint8>(std::clamp(v * f, 0.f, 255.f)); };
    return {ch(c.r), ch(c.g), ch(c.b), c.a};
}
sf::Color withAlpha(sf::Color c, int a) { c.a = static_cast<sf::Uint8>(std::clamp(a, 0, 255)); return c; }
sf::Color sideColor(Side s) { return s == Side::Player ? col::Player : s == Side::Enemy ? col::Enemy : sf::Color(190, 190, 190); }
sf::Color hpColor(float f) { return f > 0.5f ? sf::Color(90, 190, 90) : f > 0.25f ? sf::Color(230, 190, 60) : sf::Color(220, 70, 60); }

std::string attrLabel(const UnitDef& d, const std::string& a) {
    if (a == "radio") return "Funkgerät";
    if (a == "double_attack") return "Doppelangriff";
    if (a == "camouflage") return "Tarnung";
    if (a == "heilung") return "Heilung " + std::to_string(d.attrValue("heilung")) + "/Runde";
    return a;
}

std::string attrList(const UnitDef& d) {
    std::string s;
    for (auto& a : d.attributes) s += (s.empty() ? "" : ", ") + attrLabel(d, a);
    return s;
}

const sf::Color kSupportGreen(70, 210, 110);

std::string num(float v) {
    char buf[32];
    if (std::fabs(v - std::round(v)) < 0.05f) std::snprintf(buf, sizeof buf, "%d", static_cast<int>(std::lround(v)));
    else std::snprintf(buf, sizeof buf, "%.1f", v);
    return buf;
}

std::string weaponLine(const WeaponDef& w) {
    std::string r = "Reichw. " + std::to_string(w.range);
    switch (w.type) {
    case WeaponType::Damage:
        return r + "  Schaden " + num(w.damage) + "  AP " + num(w.ap) + "  Moral " + std::to_string(w.willDamage);
    case WeaponType::Heal: return r + "  Heilt Infanterie +" + num(w.damage) + " HP";
    case WeaponType::Repair: return r + "  Repariert Fahrzeuge +" + num(w.damage) + " HP";
    case WeaponType::Refuel: return r + "  Füllt Treibstoff auf";
    case WeaponType::Supply: return r + "  Repariert +" + num(w.damage) + " HP und tankt auf";
    }
    return r;
}

std::string weaponShort(const WeaponDef& w) {
    switch (w.type) {
    case WeaponType::Damage:
        return "R" + std::to_string(w.range) + "  Dmg " + num(w.damage) + "  AP " + num(w.ap) + "  Moral " + std::to_string(w.willDamage) +
               (w.cooldown ? "  CD " + std::to_string(w.cooldown) : "");
    case WeaponType::Heal: return "R" + std::to_string(w.range) + "  Heilt Inf. +" + num(w.damage);
    case WeaponType::Repair: return "R" + std::to_string(w.range) + "  Repariert +" + num(w.damage);
    case WeaponType::Refuel: return "R" + std::to_string(w.range) + "  Tankt auf";
    case WeaponType::Supply: return "R" + std::to_string(w.range) + "  Repariert +" + num(w.damage) + " & tankt";
    }
    return "";
}

} // namespace

BattleScreen::BattleScreen(App& a, const Mission& m) : Screen(a), mission_(m), battle_(a.data, m) {
    startBtn_ = {{kInnerX, 700, kInnerW, 44}, "Schlacht beginnen"};
    endBtn_ = {{kInnerX, 700, kInnerW, 44}, "Zug beenden (Leertaste)"};
    menuBtn_ = {{kInnerX, 750, kInnerW, 34}, "Mission verlassen"};
    resultMenu_ = {{kScreenW / 2 - 230, 470, 210, 46}, "Zu den Missionen"};
    resultRetry_ = {{kScreenW / 2 + 20, 470, 210, 46}, "Neu starten"};
    log("Stelle deine Truppen in der blauen Zone auf.", col::Text);
}

// ---------------------------------------------------------------------------------
// geometry
// ---------------------------------------------------------------------------------
sf::Vector2f BattleScreen::tileOrigin(int tx, int ty) const { return {kBoardX + tx * kTile, kBoardY + ty * kTile}; }
sf::Vector2f BattleScreen::tileCenter(int tx, int ty) const {
    auto o = tileOrigin(tx, ty);
    return {o.x + kTile / 2, o.y + kTile / 2};
}
bool BattleScreen::tileAt(sf::Vector2f p, int& tx, int& ty) const {
    float fx = (p.x - kBoardX) / kTile, fy = (p.y - kBoardY) / kTile;
    if (fx < 0 || fy < 0) return false;
    tx = static_cast<int>(fx);
    ty = static_cast<int>(fy);
    return battle_.inBounds(tx, ty);
}

// ---------------------------------------------------------------------------------
// state helpers
// ---------------------------------------------------------------------------------
void BattleScreen::log(const std::string& s, sf::Color c) {
    log_.emplace_back(s, c);
    while (log_.size() > 40) log_.pop_front();
}

void BattleScreen::consumeEvents() {
    for (auto& e : battle_.takeEvents()) {
        // fog of war: events that happen out of sight are not shown
        bool seen = battle_.tileVisible(e.ax, e.ay, Side::Player) || battle_.tileVisible(e.bx, e.by, Side::Player);
        switch (e.kind) {
        case BattleEvent::Kind::Message:
            log(e.text, e.side == Side::Enemy ? col::Enemy : col::Text);
            break;
        case BattleEvent::Kind::Attack:
            if (!seen) break;
            shots_.push_back({tileCenter(e.ax, e.ay), tileCenter(e.bx, e.by), sideColor(e.side), 0});
            popups_.push_back({"-" + std::to_string(e.damage), tileCenter(e.bx, e.by), e.side == Side::Player ? col::Accent : sf::Color(255, 120, 100), 0});
            log(e.text, sideColor(e.side));
            break;
        case BattleEvent::Kind::Move:
            if (seen) shots_.push_back({tileCenter(e.ax, e.ay), tileCenter(e.bx, e.by), withAlpha(sideColor(e.side), 150), 0});
            break;
        case BattleEvent::Kind::Capture:
            if (!seen && e.side == Side::Enemy) break;
            popups_.push_back({"Eingenommen", tileCenter(e.ax, e.ay), sideColor(e.side), 0});
            log(e.text, sideColor(e.side));
            break;
        case BattleEvent::Kind::Death:
            if (!seen) break;
            popups_.push_back({"Zerstört", tileCenter(e.ax, e.ay), sf::Color(255, 255, 255), 0});
            log(e.text, sideColor(e.side));
            break;
        case BattleEvent::Kind::Heal:
            if (!seen) break;
            if (e.ax != e.bx || e.ay != e.by) shots_.push_back({tileCenter(e.ax, e.ay), tileCenter(e.bx, e.by), kSupportGreen, 0});
            popups_.push_back({e.damage > 0 ? "+" + std::to_string(e.damage) : "Tank voll", tileCenter(e.bx, e.by), kSupportGreen, 0});
            log(e.text, kSupportGreen);
            break;
        case BattleEvent::Kind::Destroyed:
            if (!seen) break;
            popups_.push_back({"Zerstört", tileCenter(e.ax, e.ay), sf::Color(255, 255, 255), 0});
            log(e.text, sideColor(e.side));
            break;
        case BattleEvent::Kind::Spawn:
            popups_.push_back({"Hinterhalt!", tileCenter(e.ax, e.ay), sf::Color(255, 120, 100), 0});
            log(e.text, col::Enemy);
            break;
        }
    }
}

void BattleScreen::select(int id) {
    selectedId_ = id;
    selectedWeapon_ = -1;   // no weapon selected by default: range is only shown on demand
    refreshSelection();
}

void BattleScreen::refreshSelection() {
    reach_.clear();
    const Unit* u = selected();
    if (!u || !u->alive) { selectedId_ = -1; return; }
    bool ownMovable = u->side == Side::Player && battle_.canMove(*u);
    if (ownMovable || (u->side == Side::Enemy && battle_.visibleTo(*u, Side::Player))) reach_ = battle_.reachable(*u);
}

void BattleScreen::afterAction() {
    const Unit* u = selected();
    if (!u || !battle_.weaponReady(*u, selectedWeapon_) || u->attacksLeft <= 0) selectedWeapon_ = -1;
    refreshSelection();
}

void BattleScreen::toggleWeapon(int i) {
    const Unit* u = selected();
    if (!u || u->side != Side::Player || i < 0 || i >= static_cast<int>(u->def->weapons.size())) return;
    selectedWeapon_ = selectedWeapon_ == i ? -1 : i;
}

void BattleScreen::selectNextReady() {
    const auto& us = battle_.units();
    if (us.empty()) return;
    int start = 0;
    for (size_t i = 0; i < us.size(); ++i) if (us[i].id == selectedId_) start = static_cast<int>(i) + 1;
    for (size_t k = 0; k < us.size(); ++k) {
        const Unit& u = us[(start + k) % us.size()];
        if (u.alive && u.side == Side::Player && (u.attacksLeft > 0 || battle_.canMove(u))) { select(u.id); return; }
    }
}

void BattleScreen::startBattle() {
    if (battle_.startBattle()) {
        deployDef_ = nullptr;
        selectedId_ = -1;
        consumeEvents();
    }
}

void BattleScreen::endTurn() {
    if (battle_.phase() != Phase::PlayerTurn) return;
    battle_.endPlayerTurn();
    selectedId_ = -1;
    reach_.clear();
    aiTimer_ = 0.5f;
    consumeEvents();
}

// ---------------------------------------------------------------------------------
// input
// ---------------------------------------------------------------------------------
void BattleScreen::event(const sf::Event& e, sf::Vector2f mouse) {
    if (e.type == sf::Event::MouseButtonPressed) {
        if (e.mouseButton.button == sf::Mouse::Left) onLeftClick(mouse);
        else if (e.mouseButton.button == sf::Mouse::Right) onRightClick(mouse);
    } else if (e.type == sf::Event::MouseWheelScrolled) {
        if (battle_.phase() == Phase::Deploy) {
            int maxScroll = std::max(0, static_cast<int>(app.data.units().size()) - kDeployRows);
            deployScroll_ = std::clamp(deployScroll_ - e.mouseWheelScroll.delta, 0.f, static_cast<float>(maxScroll));
        }
    } else if (e.type == sf::Event::KeyPressed) {
        onKey(e.key.code);
    }
}

void BattleScreen::onKey(sf::Keyboard::Key k) {
    Phase ph = battle_.phase();
    if (ph == Phase::Won || ph == Phase::Lost) {
        if (k == sf::Keyboard::Enter || k == sf::Keyboard::Space)
            app.goTo(std::make_unique<MissionScreen>(app, *std::find_if(app.data.campaigns().begin(), app.data.campaigns().end(),
                       [&](const Campaign& c) { return c.id == mission_.campaign; })));
        return;
    }
    if (k == sf::Keyboard::Q) {
        selectedWeapon_ = -1;
    } else if (k == sf::Keyboard::Escape) {
        if (selectedWeapon_ >= 0) selectedWeapon_ = -1;
        else {
            selectedId_ = -1;
            deployDef_ = nullptr;
            reach_.clear();
        }
    } else if (k == sf::Keyboard::Space || k == sf::Keyboard::Enter) {
        if (ph == Phase::Deploy) startBattle();
        else if (ph == Phase::PlayerTurn) endTurn();
    } else if (k == sf::Keyboard::Tab && ph == Phase::PlayerTurn) {
        selectNextReady();
    } else if (k >= sf::Keyboard::Num1 && k <= sf::Keyboard::Num5) {
        toggleWeapon(k - sf::Keyboard::Num1);
    }
}

void BattleScreen::onRightClick(sf::Vector2f m) {
    int tx, ty;
    if (battle_.phase() == Phase::Deploy) {
        if (tileAt(m, tx, ty)) battle_.undeploy(tx, ty);
        else deployDef_ = nullptr;
    } else {
        selectedId_ = -1;
        reach_.clear();
    }
}

void BattleScreen::onLeftClick(sf::Vector2f m) {
    Phase ph = battle_.phase();
    auto backToMissions = [&] {
        for (auto& c : app.data.campaigns())
            if (c.id == mission_.campaign) { app.goTo(std::make_unique<MissionScreen>(app, c)); return; }
        app.goTo(std::make_unique<CampaignScreen>(app));
    };

    if (ph == Phase::Won || ph == Phase::Lost) {
        if (resultMenu_.contains(m)) backToMissions();
        else if (resultRetry_.contains(m)) app.goTo(std::make_unique<BattleScreen>(app, mission_));
        return;
    }
    if (menuBtn_.contains(m)) { backToMissions(); return; }

    int tx, ty;
    bool onBoard = tileAt(m, tx, ty);

    if (ph == Phase::Deploy) {
        if (startBtn_.contains(m) && battle_.aliveCount(Side::Player) > 0) { startBattle(); return; }
        for (size_t i = 0; i < deployRects_.size(); ++i)
            if (deployRects_[i].contains(m)) {
                const UnitDef* d = &app.data.units()[static_cast<size_t>(deployScroll_) + i];
                deployDef_ = deployDef_ == d ? nullptr : d;
                return;
            }
        if (onBoard && deployDef_) battle_.deploy(*deployDef_, tx, ty);
        return;
    }
    if (ph != Phase::PlayerTurn) return;
    if (endBtn_.contains(m)) { endTurn(); return; }
    for (size_t i = 0; i < weaponRects_.size(); ++i)
        if (weaponRects_[i].contains(m)) {
            toggleWeapon(static_cast<int>(i));
            return;
        }
    if (onBoard) clickBoard(tx, ty);
}

void BattleScreen::clickBoard(int tx, int ty) {
    Unit* sel = battle_.unitById(selectedId_);
    bool own = sel && sel->alive && sel->side == Side::Player;
    Unit* t = battle_.unitAt(tx, ty);
    if (t && !battle_.visibleTo(*t, Side::Player)) t = nullptr;

    if (own && selectedWeapon_ >= 0) {
        // weapon mode: clicking a target in range fires, clicking empty ground leaves weapon mode
        if (t && battle_.canAttack(*sel, selectedWeapon_, *t)) {   // enemy in range, or friendly unit for support weapons
            battle_.attack(*sel, selectedWeapon_, *t);
            consumeEvents();
            afterAction();
            return;
        }
        if (!t) {
            Building* b = battle_.buildingAt(tx, ty);
            if (b && battle_.canAttackBuilding(*sel, selectedWeapon_, *b)) {
                battle_.attackBuilding(*sel, selectedWeapon_, *b);
                consumeEvents();
                afterAction();
                return;
            }
            selectedWeapon_ = -1;
            return;
        }
    } else if (own && !t && battle_.canMove(*sel) && !reach_.empty() && reach_[ty * battle_.width() + tx] >= 0) {
        battle_.moveUnit(*sel, tx, ty);
        consumeEvents();
        afterAction();
        return;
    }
    if (t) { select(t->id); return; }
    selectedId_ = -1;
    selectedWeapon_ = -1;
    reach_.clear();
}

// ---------------------------------------------------------------------------------
// update
// ---------------------------------------------------------------------------------
void BattleScreen::update(float dt) {
    for (auto& p : popups_) p.age += dt;
    for (auto& s : shots_) s.age += dt;
    popups_.erase(std::remove_if(popups_.begin(), popups_.end(), [](const Popup& p) { return p.age > 1.2f; }), popups_.end());
    shots_.erase(std::remove_if(shots_.begin(), shots_.end(), [](const Shot& s) { return s.age > 0.4f; }), shots_.end());

    if (battle_.phase() == Phase::EnemyTurn) {
        aiTimer_ -= dt;
        if (aiTimer_ <= 0) {
            battle_.stepAI();
            consumeEvents();
            aiTimer_ = 0.55f;
            if (battle_.phase() == Phase::PlayerTurn) { selectedId_ = -1; reach_.clear(); }
        }
    }
    if ((battle_.phase() == Phase::Won || battle_.phase() == Phase::Lost) && !finishedHandled_) {
        finishedHandled_ = true;
        selectedId_ = -1;
        reach_.clear();
        if (battle_.phase() == Phase::Won) app.markCompleted(mission_.id);
    }
}

// ---------------------------------------------------------------------------------
// drawing: board
// ---------------------------------------------------------------------------------
void BattleScreen::drawTile(int tx, int ty) {
    const Terrain& t = battle_.terrainAt(tx, ty);
    sf::Color base(t.r, t.g, t.b);
    sf::Vector2f o = tileOrigin(tx, ty);
    auto& w = app.window;

    sf::RectangleShape bg({kTile, kTile});
    bg.setPosition(o);
    bg.setFillColor(base);
    bg.setOutlineColor(sf::Color(0, 0, 0, 55));
    bg.setOutlineThickness(-1.f);
    w.draw(bg);

    auto circle = [&](float fx, float fy, float r, sf::Color c, float sy = 1.f) {
        sf::CircleShape s(r * kTile);
        s.setOrigin(r * kTile, r * kTile);
        s.setScale(1.f, sy);
        s.setPosition(o.x + fx * kTile, o.y + fy * kTile);
        s.setFillColor(c);
        w.draw(s);
    };
    auto tri = [&](sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Color fill) {
        sf::ConvexShape s(3);
        s.setPoint(0, {o.x + a.x * kTile, o.y + a.y * kTile});
        s.setPoint(1, {o.x + b.x * kTile, o.y + b.y * kTile});
        s.setPoint(2, {o.x + c.x * kTile, o.y + c.y * kTile});
        s.setFillColor(fill);
        w.draw(s);
    };

    if (t.id == "forest") {
        circle(0.30f, 0.34f, 0.17f, shade(base, 0.72f));
        circle(0.68f, 0.30f, 0.15f, shade(base, 0.78f));
        circle(0.48f, 0.68f, 0.19f, shade(base, 0.66f));
        circle(0.82f, 0.74f, 0.12f, shade(base, 0.74f));
    } else if (t.id == "hills") {
        circle(0.34f, 0.62f, 0.28f, shade(base, 0.88f), 0.6f);
        circle(0.68f, 0.45f, 0.24f, shade(base, 0.8f), 0.6f);
    } else if (t.id == "mountain") {
        tri({0.08f, 0.88f}, {0.38f, 0.18f}, {0.68f, 0.88f}, shade(base, 0.72f));
        tri({0.45f, 0.88f}, {0.72f, 0.34f}, {0.94f, 0.88f}, shade(base, 0.85f));
        tri({0.38f, 0.18f}, {0.30f, 0.38f}, {0.46f, 0.38f}, sf::Color(240, 242, 245));
    } else if (t.id == "river") {
        for (int i = 0; i < 3; ++i) {
            float fy = 0.25f + i * 0.25f;
            float off = (i % 2) * 0.15f;
            sf::RectangleShape l({kTile * 0.5f, 3.f});
            l.setPosition(o.x + (0.1f + off) * kTile, o.y + fy * kTile);
            l.setFillColor(shade(base, 1.35f));
            w.draw(l);
        }
    }
}

void BattleScreen::drawBuilding(const Building& b) {
    auto& w = app.window;
    sf::Vector2f o = tileOrigin(b.x, b.y);
    if (b.destroyed) {   // rubble
        const float rub[5][3] = {{0.28f, 0.62f, 0.13f}, {0.48f, 0.52f, 0.16f}, {0.68f, 0.64f, 0.12f}, {0.40f, 0.74f, 0.10f}, {0.58f, 0.76f, 0.09f}};
        for (auto& r : rub) {
            sf::CircleShape c(r[2] * kTile, 6);
            c.setOrigin(r[2] * kTile, r[2] * kTile);
            c.setPosition(o.x + r[0] * kTile, o.y + r[1] * kTile);
            c.setFillColor(sf::Color(70, 64, 60));
            c.setOutlineColor(sf::Color(30, 28, 26));
            c.setOutlineThickness(1.f);
            w.draw(c);
        }
        sf::RectangleShape beam({kTile * 0.4f, 4.f});
        beam.setOrigin(kTile * 0.2f, 2.f);
        beam.setPosition(o.x + kTile * 0.5f, o.y + kTile * 0.4f);
        beam.setRotation(-25.f);
        beam.setFillColor(sf::Color(110, 84, 56));
        w.draw(beam);
        return;
    }
    sf::RectangleShape body({kTile * 0.44f, kTile * 0.34f});
    body.setPosition(o.x + kTile * 0.34f, o.y + kTile * 0.5f);
    body.setFillColor(sf::Color(120, 100, 80));
    body.setOutlineColor(sf::Color(40, 30, 20));
    body.setOutlineThickness(1.f);
    w.draw(body);
    sf::ConvexShape roof(3);
    roof.setPoint(0, {o.x + kTile * 0.28f, o.y + kTile * 0.5f});
    roof.setPoint(1, {o.x + kTile * 0.56f, o.y + kTile * 0.26f});
    roof.setPoint(2, {o.x + kTile * 0.84f, o.y + kTile * 0.5f});
    roof.setFillColor(sf::Color(160, 66, 52));
    w.draw(roof);
    // flag at the top-left corner so a unit standing on the building does not hide it
    sf::RectangleShape pole({2.f, kTile * 0.4f});
    pole.setPosition(o.x + kTile * 0.08f, o.y + kTile * 0.06f);
    pole.setFillColor(sf::Color(40, 40, 40));
    w.draw(pole);
    sf::RectangleShape flag({kTile * 0.2f, kTile * 0.13f});
    flag.setPosition(o.x + kTile * 0.08f + 2.f, o.y + kTile * 0.06f);
    flag.setFillColor(sideColor(b.owner));
    flag.setOutlineColor(sf::Color(20, 20, 20));
    flag.setOutlineThickness(1.f);
    w.draw(flag);
    if (b.objective) app.text("\xE2\x98\x85", o.x + kTile * 0.4f, o.y + 1, 13, col::Accent);   // star = objective
    if (b.hp < b.maxHp) {   // damaged: show a health bar
        float f = static_cast<float>(b.hp) / b.maxHp;
        drawBar(o.x + 5, o.y + kTile - 8, kTile - 10, 5, f, hpColor(f));
    }
}

void BattleScreen::drawBadge(float x, float y, char letter, sf::Color fill, bool plus) {
    sf::CircleShape c(6.5f);
    c.setOrigin(6.5f, 6.5f);
    c.setPosition(x, y);
    c.setFillColor(fill);
    c.setOutlineColor(sf::Color(10, 10, 14, 220));
    c.setOutlineThickness(1.f);
    app.window.draw(c);
    if (plus) {
        sf::RectangleShape h({8.f, 2.f}), v({2.f, 8.f});
        h.setOrigin(4.f, 1.f); v.setOrigin(1.f, 4.f);
        h.setPosition(x, y); v.setPosition(x, y);
        app.window.draw(h); app.window.draw(v);
    } else {
        std::string t(1, letter);
        float w = app.textWidth(t, 10, true);
        app.text(t, x - w / 2, y - 8, 10, sf::Color::White, true);
    }
}

void BattleScreen::drawUnits() {
    Phase ph = battle_.phase();
    for (auto& u : battle_.units()) {
        if (!u.alive || !battle_.visibleTo(u, Side::Player)) continue;
        sf::Vector2f c = tileCenter(u.x, u.y);
        sf::Vector2f o = tileOrigin(u.x, u.y);
        bool exhausted = ph == Phase::PlayerTurn && u.side == Side::Player && u.attacksLeft == 0 && !battle_.canMove(u);
        app.drawUnitIcon(*u.def, {c.x, c.y - 2}, kTile * 0.74f, sideColor(u.side), exhausted ? 0.5f : 1.f);
        drawBar(o.x + 5, o.y + kTile - 8, kTile - 10, 5, static_cast<float>(u.hp) / u.def->hp, hpColor(static_cast<float>(u.hp) / u.def->hp));
        // small status icons in the top-right corner of the tile (right to left)
        float bx = o.x + kTile - 8, by = o.y + 8;
        auto badge = [&](char l, sf::Color c, bool plus = false) { drawBadge(bx, by, l, c, plus); bx -= 14; };
        if (u.will <= 0 && ph != Phase::Deploy) badge('!', sf::Color(210, 50, 40));                 // morale broken
        if (u.def->fuel > 0 && u.fuel <= 2) badge('F', u.fuel == 0 ? sf::Color(210, 50, 40) : sf::Color(215, 140, 30));   // low fuel
        bool support = u.def->has("heilung");                                                       // self-healing or can heal/repair others
        for (auto& wp : u.def->weapons) if (wp.isSupport()) support = true;
        if (support) badge('+', sf::Color(40, 160, 80), true);
        if (u.def->has("camouflage")) badge('T', sf::Color(90, 110, 80));
        if (u.def->has("radio")) badge('R', sf::Color(60, 110, 200));
        if (u.def->has("double_attack")) badge('2', sf::Color(190, 110, 30));
        if (u.id == selectedId_) {
            app.rect({o.x, o.y, kTile, kTile}, sf::Color::Transparent, col::Accent, 3.f);
        }
    }
}

void BattleScreen::drawBar(float x, float y, float w, float h, float frac, sf::Color c) {
    app.rect({x, y, w, h}, sf::Color(20, 20, 24, 220));
    app.rect({x, y, std::max(0.f, w * std::clamp(frac, 0.f, 1.f)), h}, c);
}

void BattleScreen::drawEffects() {
    for (auto& s : shots_) {
        float len = std::hypot(s.to.x - s.from.x, s.to.y - s.from.y);
        float ang = std::atan2(s.to.y - s.from.y, s.to.x - s.from.x) * 180.f / 3.14159265f;
        sf::RectangleShape line({len, 3.f});
        line.setOrigin(0, 1.5f);
        line.setPosition(s.from);
        line.setRotation(ang);
        sf::Color c = s.color;
        c.a = static_cast<sf::Uint8>(c.a * std::max(0.f, 1.f - s.age / 0.4f));
        line.setFillColor(c);
        app.window.draw(line);
    }
    for (auto& p : popups_) {
        float k = p.age / 1.2f;
        sf::Color c = p.color;
        c.a = static_cast<sf::Uint8>(255 * (1.f - k * k));
        float w = app.textWidth(p.text, 20, true);
        app.text(p.text, p.pos.x - w / 2 + 1, p.pos.y - 14 - 36 * k + 1, 20, sf::Color(0, 0, 0, c.a), true);
        app.text(p.text, p.pos.x - w / 2, p.pos.y - 14 - 36 * k, 20, c, true);
    }
}

void BattleScreen::drawBoard(sf::Vector2f mouse) {
    int tx, ty;
    if (tileAt(mouse, tx, ty)) { hx_ = tx; hy_ = ty; }
    else if (mouse.x >= 0) { hx_ = hy_ = -1; }

    const int W = battle_.width(), H = battle_.height();
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) drawTile(x, y);

    Phase ph = battle_.phase();
    // deploy zone
    if (ph == Phase::Deploy) {
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (battle_.inDeployZone(x, y)) {
                    auto o = tileOrigin(x, y);
                    bool ok = deployDef_ && battle_.canDeploy(*deployDef_, x, y);
                    app.rect({o.x, o.y, kTile, kTile}, withAlpha(col::Player, ok ? 95 : 55), withAlpha(col::Player, 160), 1.f);
                }
    }
    for (auto& b : battle_.buildings()) drawBuilding(b);

    // fog of war (not during deployment so the whole map can be studied)
    if (ph != Phase::Deploy) {
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (!battle_.tileVisible(x, y, Side::Player)) {
                    auto o = tileOrigin(x, y);
                    app.rect({o.x, o.y, kTile, kTile}, sf::Color(135, 135, 140, 120));
                }
    }

    // selection overlays: movement range OR (when a weapon is selected) weapon range - never both
    const Unit* sel = selected();
    if (sel && sel->alive) {
        bool own = sel->side == Side::Player;
        bool weaponMode = own && selectedWeapon_ >= 0;
        if (!weaponMode && !reach_.empty())
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    if (reach_[y * W + x] >= 0) {
                        auto o = tileOrigin(x, y);
                        app.rect({o.x, o.y, kTile, kTile}, withAlpha(own ? col::Player : col::Enemy, own ? 100 : 60),
                                 withAlpha(own ? col::Player : col::Enemy, 200), 1.f);
                    }
        if (weaponMode && ph == Phase::PlayerTurn) {
            const WeaponDef& wp = sel->def->weapons[selectedWeapon_];
            const bool support = wp.isSupport();   // support weapons are shown in green, attacks in orange/red
            const sf::Color rangeC = support ? kSupportGreen : sf::Color(255, 150, 40);
            const sf::Color targetC = support ? kSupportGreen : sf::Color(255, 60, 60);
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x) {
                    int d = std::abs(x - sel->x) + std::abs(y - sel->y);
                    if (d < 1 || d > wp.range) continue;
                    auto o = tileOrigin(x, y);
                    app.rect({o.x, o.y, kTile, kTile}, withAlpha(rangeC, 55), withAlpha(rangeC, 170), 1.f);
                }
            for (auto& u : battle_.units())
                if (u.present() && battle_.canAttack(*sel, selectedWeapon_, u)) {
                    auto o = tileOrigin(u.x, u.y);
                    app.rect({o.x, o.y, kTile, kTile}, withAlpha(targetC, 90), targetC, 3.f);
                }
            for (auto& b : battle_.buildings())
                if (battle_.canAttackBuilding(*sel, selectedWeapon_, b)) {
                    auto o = tileOrigin(b.x, b.y);
                    app.rect({o.x, o.y, kTile, kTile}, withAlpha(targetC, 60), targetC, 3.f);
                }
        }
    }

    drawUnits();

    if (hx_ >= 0) {
        auto o = tileOrigin(hx_, hy_);
        app.rect({o.x, o.y, kTile, kTile}, sf::Color::Transparent, sf::Color(255, 255, 255, 200), 2.f);
    }
    drawEffects();

    // preview tooltip for the selected weapon (damage / healing)
    if (sel && sel->side == Side::Player && selectedWeapon_ >= 0 && hx_ >= 0 && ph == Phase::PlayerTurn) {
        std::string s;
        const Unit* t = battle_.unitAt(hx_, hy_);
        if (t && battle_.canAttack(*sel, selectedWeapon_, *t)) {
            int v = battle_.previewDamage(*sel, selectedWeapon_, *t);
            if (sel->def->weapons[selectedWeapon_].isSupport()) {
                s = "Heilung +" + std::to_string(v) + "  (HP " + std::to_string(t->hp) + " > " + std::to_string(t->hp + v) + ")";
                if (v == 0) s = "Tankt auf";
            } else {
                s = "Schaden " + std::to_string(v) + "  (HP " + std::to_string(t->hp) + " > " + std::to_string(std::max(0, t->hp - v)) + ")";
            }
        } else if (!t) {
            const Building* b = battle_.buildingAt(hx_, hy_);
            if (b && battle_.canAttackBuilding(*sel, selectedWeapon_, *b)) {
                int v = battle_.previewDamageBuilding(*sel, selectedWeapon_, *b);
                s = "Schaden " + std::to_string(v) + "  (HP " + std::to_string(b->hp) + " > " + std::to_string(std::max(0, b->hp - v)) + ")";
            }
        }
        if (!s.empty()) {
            float w = app.textWidth(s, 15) + 16;
            auto c = tileCenter(hx_, hy_);
            float x = std::clamp(c.x - w / 2, kBoardX, kBoardX + W * kTile - w);
            float y = std::max(kBoardY + 2.f, c.y - kTile / 2 - 30);
            app.rect({x, y, w, 24}, sf::Color(15, 15, 20, 235), col::Accent, 1.f);
            app.text(s, x + 8, y + 2, 15, col::Text);
        }
    }
}

void BattleScreen::drawTopBar() {
    app.text(mission_.name, 20, 8, 24, col::Text, true);
    std::string obj = mission_.objective == Objective::Capture ? "Ziel: Alle \xE2\x98\x85-Gebäude einnehmen (nur Infanterie) oder zerstören - oder alle Gegner vernichten"
                                                              : "Ziel: Alle gegnerischen Truppen vernichten";
    app.text(obj, 20, 40, 14, col::TextDim);

    std::string phase;
    sf::Color pc = col::Text;
    switch (battle_.phase()) {
    case Phase::Deploy: phase = "Aufstellung"; pc = col::Accent; break;
    case Phase::PlayerTurn: phase = "Dein Zug"; pc = col::Player; break;
    case Phase::EnemyTurn: phase = "Gegnerzug ..."; pc = col::Enemy; break;
    case Phase::Won: phase = "Sieg!"; pc = col::Good; break;
    case Phase::Lost: phase = "Niederlage"; pc = col::Enemy; break;
    }
    app.text(phase, kPanelX + 20, 8, 24, pc, true);
    if (battle_.phase() != Phase::Deploy) {
        std::string r = "Runde " + std::to_string(battle_.round()) + (mission_.maxTurns > 0 ? " / " + std::to_string(mission_.maxTurns) : "");
        float w = app.textWidth(r, 18);
        app.text(r, kPanelX + kPanelW - 20 - w, 12, 18, (mission_.maxTurns > 0 && battle_.round() >= mission_.maxTurns) ? col::Enemy : col::Text);
    }
}

// ---------------------------------------------------------------------------------
// drawing: panel
// ---------------------------------------------------------------------------------
void BattleScreen::drawUnitCard(const UnitDef& d, const Unit* u, float x, float y, float w, bool weaponRows, sf::Vector2f mouse) {
    Side side = u ? u->side : Side::Player;
    app.drawUnitIcon(d, {x + 30, y + 30}, 52, sideColor(side));
    app.text(d.name, x + 70, y + 2, 20, col::Text, true);
    std::string sub = std::string(d.isInfantry() ? "Infanterie" : "Fahrzeug") +
                      (u ? (side == Side::Player ? "  -  Verbündet" : "  -  Feind") : "  -  Kosten " + std::to_string(d.cost));
    app.text(sub, x + 70, y + 29, 13, col::TextDim);

    float cy = y + 66;
    int hp = u ? u->hp : d.hp, will = u ? u->will : d.will;
    app.text("HP", x, cy - 2, 14, col::TextDim);
    drawBar(x + 56, cy + 2, w - 56 - 70, 10, static_cast<float>(hp) / d.hp, hpColor(static_cast<float>(hp) / d.hp));
    app.text(std::to_string(hp) + "/" + std::to_string(d.hp), x + w - 62, cy - 2, 14, col::Text);
    cy += 22;
    app.text("Moral", x, cy - 2, 14, col::TextDim);
    for (int i = 0; i < d.will; ++i)
        app.rect({x + 56 + i * 16.f, cy + 1, 12, 12}, i < will ? sf::Color(120, 170, 255) : sf::Color(55, 60, 75));
    if (u && will <= 0) app.text("gebrochen: kann sich nicht bewegen", x + 62 + d.will * 16.f, cy - 2, 12, sf::Color(255, 120, 100));
    cy += 24;
    if (d.fuel > 0) {
        int fuel = u ? u->fuel : d.fuel;
        app.text("Treibstoff", x, cy - 2, 14, col::TextDim);
        for (int i = 0; i < d.fuel; ++i)
            app.rect({x + 76 + i * 14.f, cy + 1, 10, 12}, i < fuel ? (fuel <= 2 ? sf::Color(230, 150, 40) : sf::Color(200, 190, 90)) : sf::Color(55, 60, 75));
        if (u && fuel == 0) app.text("leer: kann sich nicht bewegen", x + 82 + d.fuel * 14.f, cy - 2, 12, sf::Color(255, 120, 100));
        cy += 24;
    }
    app.text("Rüstung " + std::to_string(d.armour) + "   Bewegung " + std::to_string(d.move) + "   Sicht " + std::to_string(d.vision) + "   Kosten " + std::to_string(d.cost),
             x, cy, 14, col::Text);
    cy += 22;
    if (!d.attributes.empty()) {
        app.text("Besonders: " + attrList(d), x, cy, 14, col::Accent);
        cy += 22;
    }
    if (u && u->holdPosition && side == Side::Enemy) { app.text("Hält seine Stellung", x, cy, 13, col::TextDim); cy += 20; }

    cy += 4;
    app.text("Waffen", x, cy, 14, col::TextDim);
    cy += 22;
    if (weaponRows) weaponRects_.clear();
    for (int i = 0; i < static_cast<int>(d.weapons.size()); ++i) {
        const WeaponDef& wp = d.weapons[i];
        sf::FloatRect r{x - 6, cy, w + 12, 36};
        bool ready = u ? battle_.weaponReady(*u, i) : true;
        bool isSel = weaponRows && u && u->side == Side::Player && i == selectedWeapon_;
        if (weaponRows) {
            weaponRects_.push_back(r);
            if (isSel) app.rect(r, sf::Color(70, 80, 105), wp.isSupport() ? kSupportGreen : col::Accent, 1.f);
            else if (u && u->side == Side::Player && r.contains(mouse)) app.rect(r, sf::Color(54, 61, 78));
        }
        sf::Color tc = ready ? (wp.isSupport() ? kSupportGreen : col::Text) : col::TextDim;
        app.text((weaponRows && u && u->side == Side::Player ? std::to_string(i + 1) + "  " : "") + wp.name, x, cy + 1, 15, tc, true);
        app.text(weaponLine(wp), x, cy + 18, 12, col::TextDim);
        std::string extra;
        if (wp.cooldown > 0) extra += "Abklingzeit " + std::to_string(wp.cooldown) + " ";
        if (wp.needsRadio) extra += "Funk nötig ";
        if (u && u->cooldown[i] > 0) extra = "bereit in " + std::to_string(u->cooldown[i]) + " Zug";
        else if (u && wp.needsRadio && !battle_.hasRadio(u->side)) extra = "kein Funkgerät!";
        if (!extra.empty()) {
            float ew = app.textWidth(extra, 12);
            app.text(extra, x + w - ew, cy + 1, 12, ready ? col::TextDim : sf::Color(255, 150, 120));
        }
        cy += 38;
    }
}

void BattleScreen::drawDeployPanel(sf::Vector2f mouse) {
    float y = kPanelY + 10;
    app.text("Truppen aufstellen", kInnerX, y, 20, col::Text, true);
    app.text("Budget: " + std::to_string(battle_.budgetLeft()) + " / " + std::to_string(battle_.budgetTotal()),
             kInnerX, y + 28, 17, col::Accent, true);
    app.paragraph("Truppe wählen, dann ein Feld in der blauen Zone anklicken. Rechtsklick entfernt eine Truppe.", kInnerX, y + 54, kInnerW, 13);

    deployRects_.clear();
    const auto& units = app.data.units();
    float listY = y + 100;
    int first = static_cast<int>(deployScroll_);
    for (int i = 0; i < kDeployRows && first + i < static_cast<int>(units.size()); ++i) {
        const UnitDef& d = units[first + i];
        sf::FloatRect r{kInnerX, listY + i * kDeployRowH, kInnerW, kDeployRowH - 4};
        deployRects_.push_back(r);
        bool affordable = d.cost <= battle_.budgetLeft();
        bool isSel = deployDef_ == &d;
        sf::Color bg = isSel ? sf::Color(70, 82, 112) : r.contains(mouse) && affordable ? sf::Color(60, 68, 88) : col::PanelLight;
        app.rect(r, bg, isSel ? col::Accent : sf::Color(80, 88, 105), isSel ? 2.f : 1.f);
        app.drawUnitIcon(d, {r.left + 30, r.top + r.height / 2}, 46, col::Player, affordable ? 1.f : 0.4f);
        sf::Color tc = affordable ? col::Text : col::TextDim;
        app.text(d.name, r.left + 62, r.top + 4, 17, tc, true);
        std::string cost = std::to_string(d.cost);
        app.text(cost, r.left + r.width - 12 - app.textWidth(cost, 18, true), r.top + 4, 18, affordable ? col::Accent : col::Enemy, true);
        app.text("HP " + std::to_string(d.hp) + "  Moral " + std::to_string(d.will) + "  Rüst. " + std::to_string(d.armour) +
                     "  Bew. " + std::to_string(d.move) + "  Sicht " + std::to_string(d.vision), r.left + 62, r.top + 26, 12, col::TextDim);
        std::string extra = attrList(d);
        if (d.fuel > 0) extra += std::string(extra.empty() ? "" : ", ") + "Treibstoff " + std::to_string(d.fuel);
        if (!extra.empty()) app.text(extra, r.left + 62, r.top + 40, 12, col::Accent);
    }
    if (units.size() > kDeployRows) {
        std::string hint = "Mausrad: Liste scrollen (" + std::to_string(first + 1) + "-" +
                           std::to_string(std::min<int>(units.size(), first + kDeployRows)) + " von " + std::to_string(units.size()) + ")";
        app.text(hint, kInnerX, listY + kDeployRows * kDeployRowH - 2, 11, col::TextDim);
    }

    float dy = listY + kDeployRows * kDeployRowH + 18;
    if (deployDef_) {
        app.text("Waffen: " + deployDef_->name, kInnerX, dy, 14, col::TextDim);
        dy += 20;
        for (auto& w : deployDef_->weapons) {
            app.text(w.name, kInnerX, dy, 13, w.isSupport() ? kSupportGreen : col::Text, true);
            std::string info = weaponShort(w);
            app.text(info, kInnerX + 140, dy, 12, col::TextDim);
            dy += 17;
        }
    }

    startBtn_.enabled = battle_.aliveCount(Side::Player) > 0;
    startBtn_.label = startBtn_.enabled ? "Schlacht beginnen (Leertaste)" : "Stelle mindestens eine Truppe auf";
    app.button(startBtn_, mouse, startBtn_.enabled);
    app.button(menuBtn_, mouse);
}

void BattleScreen::drawBattlePanel(sf::Vector2f mouse) {
    const Unit* sel = selected();
    float y = kPanelY + 10;
    weaponRects_.clear();
    if (sel) {
        drawUnitCard(*sel->def, sel, kInnerX, y, kInnerW, true, mouse);
    } else {
        app.text("Keine Truppe ausgewählt", kInnerX, y, 18, col::TextDim);
        app.paragraph("Klicke auf eine deiner Truppen: blaue Felder = Bewegung. Wähle eine Waffe (Klick oder 1-5), um ihre "
                      "Reichweite zu sehen (orange), rot = angreifbar, grün = Heilung/Versorgung eigener Truppen. Q / Esc wählt die Waffe ab. Jede Truppe darf pro Runde "
                      "bewegen UND angreifen. Gegner außerhalb deiner Sicht sind im Nebel verborgen. Tab = nächste Truppe.",
                      kInnerX, y + 30, kInnerW, 13);
    }

    // hovered tile info
    float hy = kPanelY + 335;
    app.rect({kInnerX - 10, hy - 6, kInnerW + 20, 1}, sf::Color(80, 88, 105));
    if (hx_ >= 0) {
        const Terrain& t = battle_.terrainAt(hx_, hy_);
        char buf[160];
        std::snprintf(buf, sizeof buf, "%s   Bewegung Inf. %d / Fzg. %s   Schaden x%.2f", t.name.c_str(), t.moveCostInfantry,
                      t.moveCostVehicle ? std::to_string(t.moveCostVehicle).c_str() : "-", t.damageMult);
        app.text(buf, kInnerX, hy, 13, col::Text);
        const Unit* hu = battle_.unitAt(hx_, hy_);
        if (hu && battle_.visibleTo(*hu, Side::Player)) {
            app.text(hu->def->name + "  HP " + std::to_string(hu->hp) + "/" + std::to_string(hu->def->hp) + "  Moral " +
                         std::to_string(hu->will) + "/" + std::to_string(hu->def->will) + "  Rüstung " + std::to_string(hu->def->armour),
                     kInnerX, hy + 20, 13, sideColor(hu->side));
        } else if (const Building* b = battle_.buildingAt(hx_, hy_)) {
            std::string o = b->owner == Side::Player ? "gehört dir" : b->owner == Side::Enemy ? "gehört dem Gegner" : "neutral";
            std::string txt = b->name + " (" + o + ")" + (b->objective ? " - Ziel" : "") + "  HP " + std::to_string(b->hp) + "/" + std::to_string(b->maxHp) +
                              (b->armour ? "  Rüst. " + std::to_string(b->armour) : "");
            if (b->destroyed) txt = b->name + " (zerstört)";
            app.text(txt, kInnerX, hy + 20, 13, sideColor(b->owner));
        }
    } else {
        app.text("Fahre über das Spielfeld für Details", kInnerX, hy, 13, col::TextDim);
    }

    // log (newest at the bottom, wrapped)
    float ly = kPanelY + 380;
    app.text("Ereignisse", kInnerX, ly, 14, col::TextDim);
    std::vector<std::pair<std::string, sf::Color>> lines;
    for (auto& [s, c] : log_)
        for (auto& l : app.wrap(s, 13, kInnerW)) lines.emplace_back(l, c);
    int maxLines = 14;
    int from = std::max(0, static_cast<int>(lines.size()) - maxLines);
    float yy = ly + 22;
    for (int i = from; i < static_cast<int>(lines.size()); ++i) {
        app.text(lines[i].first, kInnerX, yy, 13, lines[i].second);
        yy += 17;
    }

    endBtn_.enabled = battle_.phase() == Phase::PlayerTurn;
    endBtn_.label = battle_.phase() == Phase::EnemyTurn ? "Gegner ist am Zug ..." : "Zug beenden (Leertaste)";
    app.button(endBtn_, mouse, endBtn_.enabled);
    app.button(menuBtn_, mouse);
}

void BattleScreen::drawPanel(sf::Vector2f mouse) {
    app.rect({kPanelX, kPanelY, kPanelW, kPanelH}, col::Panel, sf::Color(80, 88, 105), 1.f);
    if (battle_.phase() == Phase::Deploy) drawDeployPanel(mouse);
    else drawBattlePanel(mouse);
}

void BattleScreen::drawEndOverlay(sf::Vector2f mouse) {
    bool won = battle_.phase() == Phase::Won;
    app.rect({0, 0, kScreenW, kScreenH}, sf::Color(0, 0, 0, 150));
    sf::FloatRect box{kScreenW / 2 - 280, 280, 560, 260};
    app.rect(box, col::Panel, won ? col::Good : col::Enemy, 3.f);
    std::string title = won ? "Sieg!" : "Niederlage";
    app.text(title, box.left + (box.width - app.textWidth(title, 48, true)) / 2, box.top + 24, 48, won ? col::Good : col::Enemy, true);
    float ty = box.top + 100;
    for (auto& l : app.wrap(battle_.endReason(), 18, box.width - 60)) {
        app.text(l, box.left + (box.width - app.textWidth(l, 18)) / 2, ty, 18, col::Text);
        ty += 26;
    }
    app.text("Runden: " + std::to_string(battle_.round()), box.left + box.width / 2 - 40, ty + 6, 15, col::TextDim);
    app.button(resultMenu_, mouse, won);
    app.button(resultRetry_, mouse);
}

void BattleScreen::draw(sf::Vector2f mouse) {
    drawTopBar();
    drawBoard(mouse);
    drawPanel(mouse);
    if (battle_.phase() == Phase::Won || battle_.phase() == Phase::Lost) drawEndOverlay(mouse);
}
