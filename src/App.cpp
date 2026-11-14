#include "App.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "BattleScreen.hpp"
#include "Csv.hpp"
#include "Screens.hpp"

namespace fs = std::filesystem;

App::App(GameData& d, const std::string& r) : data(d), root(r) {
    const char* fontCandidates[] = {
        "/assets/fonts/font.ttf",
    };
    bool ok = false;
    for (auto c : fontCandidates)
        if (fs::exists(root + c) && font.loadFromFile(root + c)) { ok = true; break; }
    const char* system[] = {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                            "C:/Windows/Fonts/segoeui.ttf", "/System/Library/Fonts/Supplemental/Arial.ttf"};
    for (auto c : system)
        if (!ok && font.loadFromFile(c)) ok = true;
    if (!ok) std::cerr << "warning: no font found (put a .ttf at assets/fonts/font.ttf)\n";
    if (!fs::exists(root + "/assets/fonts/font_bold.ttf") || !fontBold.loadFromFile(root + "/assets/fonts/font_bold.ttf"))
        fontBold = font;
    loadProgress();
}

void App::loadProgress() {
    std::ifstream in(root + "/progress.txt");
    std::string line;
    while (std::getline(in, line))
        if (!trim(line).empty()) done_.insert(trim(line));
}

void App::markCompleted(const std::string& id) {
    if (!done_.insert(id).second) return;
    std::ofstream out(root + "/progress.txt");
    for (auto& s : done_) out << s << "\n";
}

// ---------------------------------------------------------------------------------
// drawing helpers
// ---------------------------------------------------------------------------------
float App::text(const std::string& s, float x, float y, unsigned size, sf::Color c, bool bold) {
    sf::Text t;
    t.setFont(bold ? fontBold : font);
    t.setString(sf::String::fromUtf8(s.begin(), s.end()));
    t.setCharacterSize(size);
    t.setFillColor(c);
    if (bold && &fontBold == &font) t.setStyle(sf::Text::Bold);
    t.setPosition(std::round(x), std::round(y));
    window.draw(t);
    return t.getLocalBounds().width;
}

float App::textWidth(const std::string& s, unsigned size, bool bold) {
    sf::Text t;
    t.setFont(bold ? fontBold : font);
    t.setString(sf::String::fromUtf8(s.begin(), s.end()));
    t.setCharacterSize(size);
    return t.getLocalBounds().width;
}

std::vector<std::string> App::wrap(const std::string& s, unsigned size, float maxW) {
    std::vector<std::string> lines;
    std::string cur;
    for (auto& word : split(s, ' ')) {
        std::string trial = cur.empty() ? word : cur + " " + word;
        if (!cur.empty() && textWidth(trial, size) > maxW) {
            lines.push_back(cur);
            cur = word;
        } else cur = trial;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

float App::paragraph(const std::string& s, float x, float y, float maxW, unsigned size, sf::Color c,
                     float lineH) {
    if (lineH <= 0) lineH = size * 1.35f;
    for (auto& l : wrap(s, size, maxW)) {
        text(l, x, y, size, c);
        y += lineH;
    }
    return y;
}

void App::rect(sf::FloatRect r, sf::Color fill, sf::Color outline, float th) {
    sf::RectangleShape s({r.width, r.height});
    s.setPosition(r.left, r.top);
    s.setFillColor(fill);
    if (th > 0) {
        s.setOutlineColor(outline);
        s.setOutlineThickness(-th);
    }
    window.draw(s);
}

void App::button(const Button& b, sf::Vector2f mouse, bool highlighted) {
    bool hot = b.enabled && b.rect.contains(mouse);
    sf::Color fill = !b.enabled ? sf::Color(48, 52, 62) : hot ? sf::Color(84, 96, 125) : col::PanelLight;
    rect(b.rect, fill, highlighted ? col::Accent : sf::Color(90, 98, 115), highlighted ? 2.f : 1.f);
    unsigned size = 17;
    float w = textWidth(b.label, size);
    text(b.label, b.rect.left + (b.rect.width - w) / 2, b.rect.top + (b.rect.height - size) / 2 - 3, size,
         b.enabled ? col::Text : col::TextDim);
}

const sf::Texture* App::unitTexture(const UnitDef& def) {
    auto it = textures_.find(def.sprite);
    if (it == textures_.end()) {
        auto t = std::make_unique<sf::Texture>();
        std::string path = root + "/assets/units/" + def.sprite;
        if (!fs::exists(path) || !t->loadFromFile(path)) t.reset();
        else t->setSmooth(true);
        it = textures_.emplace(def.sprite, std::move(t)).first;
    }
    return it->second.get();
}

namespace {
bool hasWeaponType(const UnitDef& d, WeaponType t) {
    for (auto& w : d.weapons) if (w.type == t) return true;
    return false;
}
bool onlySupport(const UnitDef& d) {
    if (d.weapons.empty()) return false;
    for (auto& w : d.weapons) if (!w.isSupport()) return false;
    return true;
}
} // namespace

void App::drawUnitIcon(const UnitDef& def, sf::Vector2f c, float size, sf::Color team, float alpha) {
    auto a = [&](sf::Color col) { col.a = static_cast<sf::Uint8>(col.a * alpha); return col; };
    sf::CircleShape base(size * 0.5f);
    base.setOrigin(size * 0.5f, size * 0.5f);
    base.setPosition(c);
    base.setFillColor(a(sf::Color(team.r, team.g, team.b, 70)));
    base.setOutlineColor(a(team));
    base.setOutlineThickness(2.f);
    window.draw(base);

    if (const sf::Texture* tex = unitTexture(def)) {
        sf::Sprite sp(*tex);
        float s = (size * 0.9f) / std::max(tex->getSize().x, tex->getSize().y);
        sp.setScale(s, s);
        sp.setOrigin(tex->getSize().x / 2.f, tex->getSize().y / 2.f);
        sp.setPosition(c);
        sp.setColor(a(sf::Color::White));
        window.draw(sp);
        return;
    }
    // fallback: simple procedural shape so the game is playable without any art
    sf::Color body(static_cast<sf::Uint8>(team.r * 0.45f), static_cast<sf::Uint8>(team.g * 0.45f),
                   static_cast<sf::Uint8>(team.b * 0.45f));
    if (def.isInfantry()) {
        const bool medic = hasWeaponType(def, WeaponType::Heal);   // white helmet + red cross
        const bool helmet = def.id == "trooper" || medic;           // better infantry wears a helmet
        if (helmet) {
            sf::CircleShape dome(size * 0.27f);
            dome.setOrigin(size * 0.27f, size * 0.27f);
            dome.setPosition(c.x, c.y - size * 0.15f);
            dome.setFillColor(a(medic ? sf::Color(238, 238, 238) : sf::Color(96, 120, 70)));
            dome.setOutlineColor(a(sf::Color(30, 40, 25)));
            dome.setOutlineThickness(1.f);
            window.draw(dome);
        }
        sf::CircleShape head(size * (helmet ? 0.15f : 0.2f));
        head.setOrigin(head.getRadius(), head.getRadius());
        head.setPosition(c.x, c.y - size * (helmet ? 0.08f : 0.12f));
        head.setFillColor(a(sf::Color(225, 200, 170)));
        window.draw(head);
        if (helmet) {   // brim over the forehead
            sf::RectangleShape brim({size * 0.62f, size * 0.07f});
            brim.setOrigin(size * 0.31f, size * 0.035f);
            brim.setPosition(c.x, c.y - size * 0.13f);
            brim.setFillColor(a(medic ? sf::Color(190, 190, 190) : sf::Color(64, 82, 50)));
            window.draw(brim);
        }
        sf::RectangleShape torso({size * 0.42f, size * 0.26f});
        torso.setOrigin(size * 0.21f, 0);
        torso.setPosition(c.x, c.y + size * 0.06f);
        torso.setFillColor(a(medic ? sf::Color(235, 235, 235) : helmet ? sf::Color(78, 96, 62) : body));   // troopers wear green fatigues
        torso.setOutlineColor(a(sf::Color(230, 230, 230)));
        torso.setOutlineThickness(1.f);
        window.draw(torso);
        if (medic) {
            sf::RectangleShape h({size * 0.18f, size * 0.05f}), v({size * 0.05f, size * 0.18f});
            h.setOrigin(size * 0.09f, size * 0.025f); v.setOrigin(size * 0.025f, size * 0.09f);
            h.setPosition(c.x, c.y + size * 0.19f); v.setPosition(c.x, c.y + size * 0.19f);
            h.setFillColor(a(sf::Color(205, 40, 40))); v.setFillColor(a(sf::Color(205, 40, 40)));
            window.draw(h); window.draw(v);
        }
    } else {
        if (onlySupport(def)) {   // ambulance (white, red cross) / supply truck (cab + crate), no turret
            const bool amb = hasWeaponType(def, WeaponType::Heal) && !hasWeaponType(def, WeaponType::Supply);
            const float hw = size * 0.78f, hh = size * 0.40f, cy = c.y + size * 0.02f;
            for (int i = -1; i <= 1; ++i) {
                sf::CircleShape wheel(size * 0.075f);
                wheel.setOrigin(size * 0.075f, size * 0.075f);
                wheel.setPosition(c.x + i * size * 0.27f, cy + hh / 2);
                wheel.setFillColor(a(sf::Color(30, 30, 34)));
                window.draw(wheel);
            }
            sf::RectangleShape box({hw * 0.68f, hh});
            box.setOrigin(hw / 2, hh / 2);
            box.setPosition(c.x - hw * 0.0f, cy);
            box.setFillColor(a(amb ? sf::Color(238, 238, 238) : sf::Color(150, 132, 84)));
            box.setOutlineColor(a(sf::Color(230, 230, 230)));
            box.setOutlineThickness(1.f);
            window.draw(box);
            sf::RectangleShape cab({hw * 0.3f, hh * 0.72f});
            cab.setOrigin(0, hh * 0.36f);
            cab.setPosition(c.x - hw / 2 + hw * 0.68f, cy + hh * 0.14f);
            cab.setFillColor(a(body));
            cab.setOutlineColor(a(sf::Color(230, 230, 230)));
            cab.setOutlineThickness(1.f);
            window.draw(cab);
            if (amb) {
                sf::RectangleShape h({size * 0.22f, size * 0.06f}), v({size * 0.06f, size * 0.22f});
                h.setOrigin(size * 0.11f, size * 0.03f); v.setOrigin(size * 0.03f, size * 0.11f);
                h.setPosition(c.x - hw * 0.17f, cy); v.setPosition(c.x - hw * 0.17f, cy);
                h.setFillColor(a(sf::Color(205, 40, 40))); v.setFillColor(a(sf::Color(205, 40, 40)));
                window.draw(h); window.draw(v);
            } else {
                for (int i = 0; i < 2; ++i) {   // crate straps
                    sf::RectangleShape strap({size * 0.04f, hh});
                    strap.setOrigin(size * 0.02f, hh / 2);
                    strap.setPosition(c.x - hw * 0.3f + i * hw * 0.28f, cy);
                    strap.setFillColor(a(sf::Color(100, 86, 52)));
                    window.draw(strap);
                }
            }
            return;
        }
        // hull, tracks, turret and gun all grow with the armour class
        const float armour = static_cast<float>(def.armour);
        const float hw = size * (0.58f + 0.022f * armour), hh = size * (0.34f + 0.018f * armour);
        const float cy = c.y + size * 0.04f;
        sf::RectangleShape hull({hw, hh});
        hull.setOrigin(hw / 2, hh / 2);
        hull.setPosition(c.x, cy);
        hull.setFillColor(a(body));
        hull.setOutlineColor(a(sf::Color(230, 230, 230)));
        hull.setOutlineThickness(1.f);
        for (int side = -1; side <= 1; side += 2) {   // tracks
            sf::RectangleShape track({hw + size * 0.04f, size * 0.07f});
            track.setOrigin(track.getSize().x / 2, track.getSize().y / 2);
            track.setPosition(c.x, cy + side * (hh / 2 + size * 0.02f));
            track.setFillColor(a(sf::Color(35, 35, 40)));
            window.draw(track);
        }
        window.draw(hull);
        const float tr = size * (0.05f + 0.016f * armour);   // light 0.11, medium 0.16, heavy 0.21
        const float bl = size * (0.14f + 0.015f * armour) + tr;
        const float bt = size * (0.035f + 0.005f * armour);
        const int barrels = def.has("double_attack") ? 2 : 1;   // twin guns on the fast light tank
        for (int i = 0; i < barrels; ++i) {
            sf::RectangleShape barrel({bl, bt});
            float off = barrels == 2 ? (i == 0 ? -bt * 0.8f : bt * 0.8f) : 0.f;
            barrel.setOrigin(0, bt / 2);
            barrel.setPosition(c.x, cy + off);
            barrel.setFillColor(a(sf::Color(225, 225, 225)));
            window.draw(barrel);
        }
        sf::CircleShape turret(tr);
        turret.setOrigin(tr, tr);
        turret.setPosition(c.x, cy);
        turret.setFillColor(a(sf::Color(body.r + 45, body.g + 45, body.b + 45)));
        turret.setOutlineColor(a(sf::Color(235, 235, 235)));
        turret.setOutlineThickness(1.5f);
        window.draw(turret);
        if (def.armour >= 10) {   // heavy: extra front plate marking
            sf::RectangleShape plate({size * 0.06f, hh * 0.8f});
            plate.setOrigin(0, hh * 0.4f);
            plate.setPosition(c.x + hw / 2 - size * 0.08f, cy);
            plate.setFillColor(a(sf::Color(200, 200, 205)));
            window.draw(plate);
        }
    }
}

// ---------------------------------------------------------------------------------
// window / main loop
// ---------------------------------------------------------------------------------
void App::applyLetterbox() {
    sf::Vector2u ws = window.getSize();
    float wr = static_cast<float>(ws.x) / static_cast<float>(ws.y), vr = kScreenW / kScreenH;
    sf::FloatRect vp(0, 0, 1, 1);
    if (wr > vr) { vp.width = vr / wr; vp.left = (1 - vp.width) / 2; }
    else { vp.height = wr / vr; vp.top = (1 - vp.height) / 2; }
    sf::View v(sf::FloatRect(0, 0, kScreenW, kScreenH));
    v.setViewport(vp);
    window.setView(v);
}

#ifndef STRATEGIZER_VERSION
#define STRATEGIZER_VERSION "dev"
#endif

// Small semi-transparent version label, always visible in the top-left corner.
void App::drawVersion() {
    text(std::string("v") + STRATEGIZER_VERSION, 5, 1, 11, sf::Color(255, 255, 255, 110));
}

void App::screenshot(const std::string& path) {
    sf::Texture t;
    t.create(window.getSize().x, window.getSize().y);
    t.update(window);
    t.copyToImage().saveToFile(path);
}

void App::run(const std::string& shotDir) {
    window.create(sf::VideoMode(static_cast<unsigned>(kScreenW), static_cast<unsigned>(kScreenH)), "Strategizer2D",
                  sf::Style::Titlebar | sf::Style::Close | sf::Style::Resize);
    window.setFramerateLimit(60);
    applyLetterbox();
    screen_ = std::make_unique<MenuScreen>(*this);
    if (!shotDir.empty()) { runDemo(shotDir); return; }

    sf::Clock clock;
    while (window.isOpen()) {
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed) window.close();
            else if (e.type == sf::Event::Resized) applyLetterbox();
            else {
                sf::Vector2i px = sf::Mouse::getPosition(window);
                if (e.type == sf::Event::MouseButtonPressed || e.type == sf::Event::MouseButtonReleased)
                    px = {e.mouseButton.x, e.mouseButton.y};
                else if (e.type == sf::Event::MouseMoved) px = {e.mouseMove.x, e.mouseMove.y};
                else if (e.type == sf::Event::MouseWheelScrolled) px = {e.mouseWheelScroll.x, e.mouseWheelScroll.y};
                screen_->event(e, mapMouse(px.x, px.y));
            }
            if (hasPending_) { screen_ = std::move(pending_); hasPending_ = false; }
        }
        float dt = std::min(clock.restart().asSeconds(), 0.1f);
        screen_->update(dt);
        if (hasPending_) { screen_ = std::move(pending_); hasPending_ = false; }
        window.clear(sf::Color::Black);
        rect({0, 0, kScreenW, kScreenH}, col::Bg);
        sf::Vector2i px = sf::Mouse::getPosition(window);
        screen_->draw(mapMouse(px.x, px.y));
        drawVersion();
        window.display();
    }
}

// Scripted tour used to produce screenshots without a human at the keyboard.
void App::runDemo(const std::string& dir) {
    fs::create_directories(dir);
    auto frame = [&](const std::string& name, float simulate = 0.f) {
        for (int i = 0; i < 3; ++i) {
            sf::Event e;
            while (window.pollEvent(e)) {}
            if (simulate > 0) screen_->update(simulate);
            window.clear(sf::Color::Black);
            rect({0, 0, kScreenW, kScreenH}, col::Bg);
            screen_->draw({-1, -1});
            drawVersion();
            window.display();
        }
        screenshot(dir + "/" + name + ".png");
        std::cout << "saved " << name << ".png\n";
    };
    auto apply = [&] { if (hasPending_) { screen_ = std::move(pending_); hasPending_ = false; } };

    frame("1_menu");
    goTo(std::make_unique<CampaignScreen>(*this)); apply();
    frame("2_campaigns");
    goTo(std::make_unique<MissionScreen>(*this, data.campaigns()[0])); apply();
    frame("3_missions");

    // Mission 3: deploy a mixed force
    goTo(std::make_unique<BattleScreen>(*this, *data.mission("dawn_3"))); apply();
    auto* bs = dynamic_cast<BattleScreen*>(screen_.get());
    bs->battle().deploy(*data.unit("rifleman"), 3, 10);
    bs->battle().deploy(*data.unit("rifleman"), 4, 10);
    bs->battle().deploy(*data.unit("trooper"), 5, 10);
    bs->battle().deploy(*data.unit("trooper"), 6, 10);
    bs->battle().deploy(*data.unit("light_tank"), 5, 9);
    bs->setDeployDef(data.unit("medium_tank"));
    bs->setHover(7, 10);
    frame("4_deploy");

    bs->battle().startBattle();
    bs->setDeployDef(nullptr);
    Unit* tank = bs->battle().unitAt(5, 9);
    bs->select(tank->id);
    bs->setHover(5, 7);
    frame("5_battle_move");
    bs->toggleWeapon(0);
    frame("5b_battle_weapon_range");
    bs->toggleWeapon(0);

    // bring an enemy into range: move the tank next to an enemy for the attack preview
    bs->battle().moveUnit(*tank, 5, 5);
    bs->battle().endPlayerTurn();
    for (int i = 0; i < 40 && bs->battle().phase() == Phase::EnemyTurn; ++i) bs->battle().stepAI();
    for (auto& u : bs->battle().units())
        if (u.side == Side::Player && u.alive) { bs->select(u.id); break; }
    frame("6_after_enemy_turn", 0.1f);

    // ---- support units, fuel, destructible buildings (iron_2) ----------------------------
    goTo(std::make_unique<BattleScreen>(*this, *data.mission("iron_2"))); apply();
    bs = dynamic_cast<BattleScreen*>(screen_.get());
    {
        Battle& b = bs->battle();
        b.deploy(*data.unit("medic"), 4, 10);
        b.deploy(*data.unit("rifleman"), 5, 10);
        b.deploy(*data.unit("ambulance"), 6, 10);
        b.deploy(*data.unit("truck"), 7, 10);
        b.deploy(*data.unit("light_tank"), 8, 10);
        b.deploy(*data.unit("medium_tank"), 3, 10);
        b.startBattle();
        b.unitAt(5, 10)->hp = 20;
        b.unitAt(8, 10)->hp = 45;
        b.unitAt(8, 10)->fuel = 1;
        b.unitAt(3, 10)->hp = 100;
        b.unitAt(3, 10)->fuel = 0;
        bs->select(b.unitAt(6, 10)->id);
        b.unitAt(8, 10)->ammo[0] = 0; b.unitAt(8, 10)->ammo[1] = 0;   // light tank: no ammo, low fuel
        b.unitAt(4, 10)->moved = true;                                // medic already moved
        bs->toggleWeapon(0);
        bs->setHover(5, 10);
        frame("10_ambulance_heal");
        bs->select(b.unitAt(7, 10)->id);
        bs->toggleWeapon(0);
        bs->setHover(8, 10);
        frame("11_truck_supply");
        // tank next to a damaged building
        Unit* lt = b.unitAt(8, 10);
        lt->x = 9; lt->y = 6;
        b.buildingAt(9, 5)->hp = 40;
        bs->select(lt->id);
        bs->toggleWeapon(0);
        bs->setHover(9, 5);
        frame("12_attack_building");
    }

    // ---- shop, supply post, capture ---------------------------------------------------------
    goTo(std::make_unique<BattleScreen>(*this, *data.mission("iron_1"))); apply();
    bs = dynamic_cast<BattleScreen*>(screen_.get());
    {
        Battle& b = bs->battle();
        b.deploy(*data.unit("medium_tank"), 5, 10);
        b.deploy(*data.unit("rifleman"), 6, 10);
        b.startBattle();
        const auto& bl = b.buildings();
        for (int i = 0; i < static_cast<int>(bl.size()); ++i)
            if (bl[i].def->id == "outpost") bs->selectBuilding(i);
        bs->setHover(11, 10);
        frame("13_building_selected");
        bs->openShop(true);
        frame("14_shop");
        bs->openShop(false);
        for (int i = 0; i < static_cast<int>(bl.size()); ++i)
            if (bl[i].def->id == "supplypost") bs->selectBuilding(i);
        Unit* tk = b.unitAt(5, 10);
        Unit* rf = b.unitAt(6, 10);
        tk->hp = 60; rf->hp = 20;
        tk->x = 1; tk->y = 9;
        rf->x = 1; rf->y = 11;
        bs->toggleWeapon(1);
        bs->setHover(1, 9);
        frame("15_supplypost");
    }
    goTo(std::make_unique<BattleScreen>(*this, *data.mission("dawn_1"))); apply();
    bs = dynamic_cast<BattleScreen*>(screen_.get());
    {
        Battle& b = bs->battle();
        b.deploy(*data.unit("rifleman"), 5, 9);
        b.deploy(*data.unit("trooper"), 6, 9);
        b.startBattle();
        Unit* inf = b.unitAt(6, 9);
        inf->x = 6; inf->y = 2;
        Unit* other = b.unitAt(5, 9);
        other->x = 4; other->y = 2;
        bs->select(inf->id);
        bs->setHover(6, 1);
        frame("16_capture");
    }

    // ---- UI-level interaction check with synthetic mouse/keyboard events -----------------
    goTo(std::make_unique<BattleScreen>(*this, *data.mission("dawn_1"))); apply();
    bs = dynamic_cast<BattleScreen*>(screen_.get());
    auto click = [&](float x, float y, sf::Mouse::Button b = sf::Mouse::Left) {
        sf::Event e; e.type = sf::Event::MouseButtonPressed;
        e.mouseButton.button = b; e.mouseButton.x = (int)x; e.mouseButton.y = (int)y;
        screen_->event(e, {x, y});
    };
    auto key = [&](sf::Keyboard::Key k) {
        sf::Event e; e.type = sf::Event::KeyPressed; e.key.code = k;
        screen_->event(e, {0, 0});
    };
    auto tileXY = [](int tx, int ty) { return sf::Vector2f(190.f + tx * 60 + 30, 70.f + ty * 60 + 30); };
    auto check = [&](const char* what, bool ok) { std::cout << (ok ? "UI OK   " : "UI FAIL ") << what << "\n"; };
    frame("7_ui_start");                                    // lets the screen build its click rectangles
    click(1100, 200);                                       // first unit row (Rifleman)
    auto p = tileXY(3, 10); click(p.x, p.y);
    p = tileXY(4, 10); click(p.x, p.y);
    check("deploy by click (2 riflemen, budget 40)", bs->battle().aliveCount(Side::Player) == 2 && bs->battle().budgetLeft() == 40);
    click(p.x, p.y, sf::Mouse::Right);
    check("right click removes unit", bs->battle().aliveCount(Side::Player) == 1);
    key(sf::Keyboard::Space);
    check("space starts the battle", bs->battle().phase() == Phase::PlayerTurn);
    frame("8_ui_battle");
    p = tileXY(3, 10); click(p.x, p.y);
    frame("9_ui_selected");
    p = tileXY(3, 8); click(p.x, p.y);
    check("select + click moves unit", bs->battle().unitAt(3, 8) != nullptr);
    key(sf::Keyboard::Space);
    check("space ends turn", bs->battle().phase() == Phase::EnemyTurn);
    for (int i = 0; i < 40 && bs->battle().phase() == Phase::EnemyTurn; ++i) { screen_->update(0.6f); }
    check("enemy turn finishes by itself", bs->battle().phase() == Phase::PlayerTurn && bs->battle().round() == 2);
    std::cout << "demo finished\n";
}
