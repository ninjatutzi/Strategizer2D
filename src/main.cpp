#include <SFML/Graphics.hpp>
#include <filesystem>
#include <iostream>
#include <string>

#include "App.hpp"
#include "Data.hpp"

// Shows a data error in a window (a double-clicked game has no visible console) and waits until it is closed.
static void showError(const std::string& root, const std::string& message) {
    std::cerr << message << "\n";
    sf::Font font;
    bool ok = false;
    for (const std::string& p : {root + "/assets/fonts/font.ttf", std::string("assets/fonts/font.ttf"),
                                 std::string("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
                                 std::string("C:/Windows/Fonts/segoeui.ttf")})
        if (!p.empty() && std::filesystem::exists(p) && font.loadFromFile(p)) { ok = true; break; }
    if (!ok) return;

    sf::RenderWindow win(sf::VideoMode(1000, 440), "Strategizer2D - Fehler in den Spieldaten", sf::Style::Titlebar | sf::Style::Close);
    win.setFramerateLimit(30);
    // simple word wrap
    std::vector<std::string> lines;
    std::string cur;
    sf::Text probe("", font, 16);
    auto flush = [&] { if (!cur.empty()) lines.push_back(cur); cur.clear(); };
    for (size_t i = 0, start = 0; i <= message.size(); ++i) {
        if (i == message.size() || message[i] == ' ') {
            std::string word = message.substr(start, i - start);
            start = i + 1;
            std::string trial = cur.empty() ? word : cur + " " + word;
            probe.setString(sf::String::fromUtf8(trial.begin(), trial.end()));
            if (!cur.empty() && probe.getLocalBounds().width > 940) { flush(); cur = word; }
            else cur = trial;
        }
    }
    flush();
    while (win.isOpen()) {
        sf::Event e;
        while (win.pollEvent(e))
            if (e.type == sf::Event::Closed || (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::Escape)) win.close();
        win.clear(sf::Color(30, 22, 24));
        auto put = [&](const std::string& s, float y, unsigned size, sf::Color c) {
            sf::Text t(sf::String::fromUtf8(s.begin(), s.end()), font, size);
            t.setPosition(30, y);
            t.setFillColor(c);
            win.draw(t);
        };
        put("Das Spiel kann nicht starten: Fehler in den Spieldaten (data/*.csv)", 20, 22, sf::Color(255, 120, 100));
        float y = 70;
        for (auto& l : lines) { put(l, y, 16, sf::Color(240, 240, 240)); y += 24; }
        put("Bitte die genannte Datei und Zeile korrigieren und das Spiel neu starten.   (Esc = schließen)", 395, 14, sf::Color(170, 170, 170));
        win.display();
    }
}

int main(int argc, char** argv) {
    std::string shotDir;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--screenshots" && i + 1 < argc) shotDir = argv[++i];

    std::string root = findGameRoot(argv[0]);
    if (root.empty()) {
        showError("", "Die Spieldateien (data/units.csv) wurden nicht gefunden. Starte das Spiel aus dem Projektordner.");
        return 1;
    }
    GameData data;
    try {
        data.load(root);
    } catch (const std::exception& e) {
        showError(root, e.what());
        return 1;
    }
    App app(data, root);
    app.run(shotDir);
    return 0;
}
