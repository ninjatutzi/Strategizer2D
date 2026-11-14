#pragma once
#include <deque>

#include "App.hpp"
#include "Battle.hpp"

class BattleScreen : public Screen {
public:
    BattleScreen(App& a, const Mission& m);
    void event(const sf::Event& e, sf::Vector2f mouse) override;
    void update(float dt) override;
    void draw(sf::Vector2f mouse) override;

    // used by the scripted screenshot demo
    Battle& battle() { return battle_; }
    void select(int unitId);
    void selectBuilding(int idx);
    void openShop(bool open) { shopOpen_ = open; }
    void setHover(int tx, int ty) { hx_ = tx; hy_ = ty; }
    void setDeployDef(const UnitDef* d) { deployDef_ = d; }
    void toggleWeapon(int i);

private:
    struct Popup { std::string text; sf::Vector2f pos; sf::Color color; float age = 0; };
    struct Shot { sf::Vector2f from, to; sf::Color color; float age = 0; };

    // geometry
    sf::Vector2f tileOrigin(int tx, int ty) const;
    sf::Vector2f tileCenter(int tx, int ty) const;
    bool tileAt(sf::Vector2f p, int& tx, int& ty) const;

    // input
    void onLeftClick(sf::Vector2f m);
    void onRightClick(sf::Vector2f m);
    void onKey(sf::Keyboard::Key k);
    void clickBoard(int tx, int ty);
    void endTurn();
    void startBattle();
    void refreshSelection();
    void afterAction();
    void selectNextReady();

    // drawing
    void drawTopBar();
    void drawBoard(sf::Vector2f mouse);
    void drawTile(int tx, int ty);
    void drawBuilding(const Building& b);
    void drawBadge(float x, float y, char letter, sf::Color fill, bool plus);
    void drawUnits();
    void drawEffects();
    void drawPanel(sf::Vector2f mouse);
    void drawSidebar(sf::Vector2f mouse);
    void drawBuildingCard(const Building& b, float x, float y, float w, sf::Vector2f mouse);
    void drawShop(sf::Vector2f mouse);
    bool onShopClick(sf::Vector2f m);
    void drawDeployPanel(sf::Vector2f mouse);
    void drawBattlePanel(sf::Vector2f mouse);
    void drawUnitCard(const UnitDef& def, const Unit* inst, float x, float y, float w, bool weaponRows,
                      sf::Vector2f mouse);
    void drawEndOverlay(sf::Vector2f mouse);
    void drawBar(float x, float y, float w, float h, float frac, sf::Color c);

    void consumeEvents();
    void log(const std::string& s, sf::Color c = col::TextDim);
    const Unit* selected() const { return battle_.unitById(selectedId_); }
    Building* selBuilding() { return battle_.buildingByIndex(selectedBuilding_); }
    const Building* selBuildingC() const { return selectedBuilding_ >= 0 ? &battle_.buildings()[selectedBuilding_] : nullptr; }

    const Mission& mission_;
    Battle battle_;
    const UnitDef* deployDef_ = nullptr;
    int selectedId_ = -1;
    int selectedBuilding_ = -1;  // index into battle_.buildings() (owned buildings can act / recruit)
    bool shopOpen_ = false;
    int shopScroll_ = 0;
    int selectedWeapon_ = -1;   // -1 = no weapon selected (no range shown)
    std::vector<int> reach_;     // reachable tiles of the selected unit
    int hx_ = -1, hy_ = -1;      // hovered tile
    float aiTimer_ = 0;
    float deployScroll_ = 0;
    bool finishedHandled_ = false;

    std::vector<Popup> popups_;
    std::vector<Shot> shots_;
    std::deque<std::pair<std::string, sf::Color>> log_;

    Button startBtn_, endBtn_, menuBtn_, resultMenu_, resultRetry_;
    std::vector<sf::FloatRect> weaponRects_;   // clickable weapon rows of the selected unit
    std::vector<sf::FloatRect> deployRects_;
    sf::FloatRect recruitRect_{};              // 'buy units' row of the selected building card
    bool hasRecruitRect_ = false;
    std::vector<std::pair<sf::FloatRect, int>> shopBuyRects_;   // buy buttons -> unit index
    sf::FloatRect shopBox_{}, shopCloseRect_{};
};
