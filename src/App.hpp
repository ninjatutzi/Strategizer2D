#pragma once
#include <SFML/Graphics.hpp>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "Data.hpp"

constexpr float kScreenW = 1200.f;
constexpr float kScreenH = 800.f;

namespace col {
const sf::Color Bg(30, 34, 42);
const sf::Color Panel(42, 47, 58);
const sf::Color PanelLight(58, 65, 80);
const sf::Color Text(235, 238, 242);
const sf::Color TextDim(150, 158, 172);
const sf::Color Player(70, 140, 235);
const sf::Color Enemy(225, 75, 65);
const sf::Color Accent(240, 190, 60);
const sf::Color Good(110, 200, 110);
} // namespace col

class App;

class Screen {
public:
    explicit Screen(App& a) : app(a) {}
    virtual ~Screen() = default;
    virtual void event(const sf::Event& e, sf::Vector2f mouse) = 0;
    virtual void update(float /*dt*/) {}
    virtual void draw(sf::Vector2f mouse) = 0;

protected:
    App& app;
};

struct Button {
    sf::FloatRect rect;
    std::string label;
    bool enabled = true;
    bool contains(sf::Vector2f p) const { return enabled && rect.contains(p); }
};

class App {
public:
    App(GameData& data, const std::string& root);

    // Runs until the window closes. If shotDir is non-empty, plays a scripted demo that
    // saves screenshots there and exits (used for automated checks).
    void run(const std::string& shotDir = "");

    void goTo(std::unique_ptr<Screen> s) { pending_ = std::move(s); hasPending_ = true; }
    void quit() { window.close(); }

    // ---- shared helpers ------------------------------------------------------------
    float text(const std::string& utf8, float x, float y, unsigned size = 16,
               sf::Color c = col::Text, bool bold = false);
    float textWidth(const std::string& utf8, unsigned size, bool bold = false);
    std::vector<std::string> wrap(const std::string& utf8, unsigned size, float maxW);
    float paragraph(const std::string& utf8, float x, float y, float maxW, unsigned size = 14,
                    sf::Color c = col::TextDim, float lineH = 0);
    void rect(sf::FloatRect r, sf::Color fill, sf::Color outline = sf::Color::Transparent, float th = 0);
    void button(const Button& b, sf::Vector2f mouse, bool highlighted = false);

    // Unit sprite from assets/units/ (nullptr if the file does not exist)
    const sf::Texture* unitTexture(const UnitDef& def);
    // Draws a unit icon (sprite or fallback shape) centred at c, fitting into a size x size box
    void drawUnitIcon(const UnitDef& def, sf::Vector2f c, float size, sf::Color teamColor, float alpha = 1.f);

    bool completed(const std::string& missionId) const { return done_.count(missionId) > 0; }
    void markCompleted(const std::string& missionId);

    sf::RenderWindow window;
    sf::Font font;
    sf::Font fontBold;
    GameData& data;
    std::string root;

private:
    void drawVersion();
    void applyLetterbox();
    void loadProgress();
    void runDemo(const std::string& dir);
    sf::Vector2f mapMouse(int x, int y) { return window.mapPixelToCoords({x, y}); }
    void screenshot(const std::string& path);

    std::unique_ptr<Screen> screen_, pending_;
    bool hasPending_ = false;
    std::map<std::string, std::unique_ptr<sf::Texture>> textures_;
    std::set<std::string> done_;
};
