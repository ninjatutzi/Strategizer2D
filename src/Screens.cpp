#include "Screens.hpp"

#include <iostream>

#include "BattleScreen.hpp"

// ------------------------------------------------------------------------- Menu
MenuScreen::MenuScreen(App& a) : Screen(a) {
    play_ = {{kScreenW / 2 - 140, 400, 280, 52}, "Kampagne spielen"};
    quit_ = {{kScreenW / 2 - 140, 470, 280, 52}, "Beenden"};
}

void MenuScreen::event(const sf::Event& e, sf::Vector2f mouse) {
    if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
        if (play_.contains(mouse)) app.goTo(std::make_unique<CampaignScreen>(app));
        else if (quit_.contains(mouse)) app.quit();
    } else if (e.type == sf::Event::KeyPressed) {
        if (e.key.code == sf::Keyboard::Enter) app.goTo(std::make_unique<CampaignScreen>(app));
        else if (e.key.code == sf::Keyboard::Escape) app.quit();
    }
}

void MenuScreen::draw(sf::Vector2f mouse) {
    const std::string title = "STRATEGIZER 2D";
    float w = app.textWidth(title, 64, true);
    app.text(title, (kScreenW - w) / 2, 170, 64, col::Text, true);
    const std::string sub = "Rundenbasierte Taktik auf dem Schlachtfeld";
    app.text(sub, (kScreenW - app.textWidth(sub, 20)) / 2, 255, 20, col::TextDim);

    // decorative row of units
    const float step = 80.f;
    float x = kScreenW / 2 - (static_cast<float>(app.data.units().size()) - 1) * step / 2;
    for (const auto& u : app.data.units()) {
        app.drawUnitIcon(u, {x, 340}, 64, col::Player);
        x += step;
    }
    app.button(play_, mouse);
    app.button(quit_, mouse);
}

// ------------------------------------------------------------------------- Campaigns
CampaignScreen::CampaignScreen(App& a) : Screen(a) { back_ = {{30, 740, 160, 40}, "< Zurück"}; }

sf::FloatRect CampaignScreen::cardRect(size_t i) const { return {150, 150.f + i * 130.f, 900, 110}; }

void CampaignScreen::event(const sf::Event& e, sf::Vector2f mouse) {
    if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
        if (back_.contains(mouse)) { app.goTo(std::make_unique<MenuScreen>(app)); return; }
        for (size_t i = 0; i < app.data.campaigns().size(); ++i)
            if (cardRect(i).contains(mouse)) {
                app.goTo(std::make_unique<MissionScreen>(app, app.data.campaigns()[i]));
                return;
            }
    } else if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::Escape)
        app.goTo(std::make_unique<MenuScreen>(app));
}

void CampaignScreen::draw(sf::Vector2f mouse) {
    app.text("Kampagne wählen", 150, 60, 40, col::Text, true);
    for (size_t i = 0; i < app.data.campaigns().size(); ++i) {
        const Campaign& c = app.data.campaigns()[i];
        auto r = cardRect(i);
        bool hot = r.contains(mouse);
        app.rect(r, hot ? sf::Color(55, 63, 80) : col::Panel, hot ? col::Accent : sf::Color(80, 88, 105), hot ? 2.f : 1.f);
        app.text(c.name, r.left + 24, r.top + 16, 28, col::Text, true);
        app.paragraph(c.description, r.left + 24, r.top + 56, 640, 16);
        auto ms = app.data.missionsOf(c.id);
        int done = 0;
        for (auto* m : ms) done += app.completed(m->id);
        app.text(std::to_string(done) + " / " + std::to_string(ms.size()) + " Missionen", r.left + r.width - 200,
                 r.top + 40, 18, done == (int)ms.size() ? col::Good : col::TextDim);
    }
    app.button(back_, mouse);
}

// ------------------------------------------------------------------------- Missions
MissionScreen::MissionScreen(App& a, const Campaign& c) : Screen(a), campaign_(c) {
    missions_ = a.data.missionsOf(c.id);
    back_ = {{30, 740, 160, 40}, "< Zurück"};
}

sf::FloatRect MissionScreen::cardRect(size_t i) const { return {150, 140.f + i * 110.f, 900, 96}; }

void MissionScreen::event(const sf::Event& e, sf::Vector2f mouse) {
    if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
        if (back_.contains(mouse)) { app.goTo(std::make_unique<CampaignScreen>(app)); return; }
        for (size_t i = 0; i < missions_.size(); ++i)
            if (cardRect(i).contains(mouse)) {
                try {
                    app.goTo(std::make_unique<BattleScreen>(app, *missions_[i]));
                } catch (const std::exception& ex) {
                    std::cerr << "cannot start mission: " << ex.what() << "\n";
                }
                return;
            }
    } else if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::Escape)
        app.goTo(std::make_unique<CampaignScreen>(app));
}

void MissionScreen::draw(sf::Vector2f mouse) {
    app.text(campaign_.name, 150, 50, 40, col::Text, true);
    app.text("Mission wählen", 150, 102, 16, col::TextDim);
    for (size_t i = 0; i < missions_.size(); ++i) {
        const Mission& m = *missions_[i];
        auto r = cardRect(i);
        bool hot = r.contains(mouse);
        app.rect(r, hot ? sf::Color(55, 63, 80) : col::Panel, hot ? col::Accent : sf::Color(80, 88, 105), hot ? 2.f : 1.f);
        app.text(std::to_string(i + 1) + ". " + m.name, r.left + 20, r.top + 10, 24, col::Text, true);
        if (app.completed(m.id)) app.text("\xE2\x9C\x93 abgeschlossen", r.left + r.width - 180, r.top + 14, 16, col::Good);
        std::string obj = m.objective == Objective::Capture ? "Ziel: Gebäude einnehmen" : "Ziel: Gegner vernichten";
        app.text(obj + "   |   Budget: " + std::to_string(m.budget) +
                     (m.maxTurns > 0 ? "   |   Max. Runden: " + std::to_string(m.maxTurns) : ""),
                 r.left + 20, r.top + 44, 15, col::Accent);
        app.paragraph(m.description, r.left + 20, r.top + 66, 860, 14);
    }
    app.button(back_, mouse);
}
