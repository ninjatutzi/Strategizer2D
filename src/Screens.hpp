#pragma once
#include "App.hpp"

class MenuScreen : public Screen {
public:
    explicit MenuScreen(App& a);
    void event(const sf::Event& e, sf::Vector2f mouse) override;
    void draw(sf::Vector2f mouse) override;

private:
    Button play_, quit_;
};

class CampaignScreen : public Screen {
public:
    explicit CampaignScreen(App& a);
    void event(const sf::Event& e, sf::Vector2f mouse) override;
    void draw(sf::Vector2f mouse) override;

private:
    sf::FloatRect cardRect(size_t i) const;
    Button back_;
};

class MissionScreen : public Screen {
public:
    MissionScreen(App& a, const Campaign& c);
    void event(const sf::Event& e, sf::Vector2f mouse) override;
    void draw(sf::Vector2f mouse) override;

private:
    sf::FloatRect cardRect(size_t i) const;
    const Campaign& campaign_;
    std::vector<const Mission*> missions_;
    Button back_;
};
