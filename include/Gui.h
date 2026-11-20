// Gui.h
// SFML front end for the hospital simulation. The window shows:
//   - a header with the clock, statistics and time-control buttons
//   - the waiting room in priority order, color coded green (0) to red (10)
//   - every doctor with their skill, shift and current patient
//   - an event log
//   - a right-hand panel with two tabs: a patient/doctor lookup that searches
//     the B-Trees, and a live drawing of both B-Trees level by level
//
// Controls: Space/N = next tick, H = +1 hour, A = auto-play, F = search box,
//           Up/Down = faster/slower auto-play, Tab = switch lookup/B-Tree view,
//           mouse wheel = scroll a panel.
// Works with SFML 2.5+ and SFML 3.x.
#pragma once

#include <SFML/Graphics.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "Hospital.h"

class HospitalGui {
public:
    explicit HospitalGui(Hospital& hospital);

    // Opens the window and runs until it is closed. Returns a process exit code.
    int run();

private:
    struct Box {
        float x, y, w, h;
        bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
        float right() const { return x + w; }
        float bottom() const { return y + h; }
    };
    enum class Tab { Lookup, Tree };

    // Input
    void handleKey(sf::Keyboard::Key key);
    void handleText(std::uint32_t unicode);
    void handleClick(float x, float y);
    void handleWheel(float x, float y, float delta);
    void advance(int ticks);
    void runLookup();

    // Drawing
    void draw();
    void drawHeader();
    void drawWaiting();
    void drawDoctors();
    void drawLog();
    void drawRightPanel();
    void drawLookup(const Box& area);
    void drawPatientCard(const Patient& p, float x, float y, float w);
    void drawDoctorCard(const Doctor& d, float x, float y, float w);
    void drawTreeView(const Box& area);
    float drawTreeLevels(const std::string& title, const std::vector<std::pair<int, std::vector<PersonKey>>>& nodes,
                         const Box& area, float y);

    // Drawing primitives
    sf::Text makeText(const std::string& s, unsigned size, sf::Color color) const;
    float textWidth(const std::string& s, unsigned size) const;
    void drawRect(const Box& b, sf::Color fill, sf::Color outline = sf::Color::Transparent, float thickness = 0.f);
    void drawText(const std::string& s, float x, float y, unsigned size, sf::Color color, float maxWidth = 0.f);
    void drawTextRight(const std::string& s, float right, float y, unsigned size, sf::Color color);
    void drawTextCentered(const std::string& s, float cx, float y, unsigned size, sf::Color color);
    void drawPanel(const Box& b, const std::string& title, const std::string& note = "");
    void drawButton(const Box& b, const std::string& label, bool active);
    bool isHighlighted(const PersonKey& key) const;

    Hospital& hospital_;
    sf::RenderWindow window_;
    sf::Font font_;
    std::string fontPath_;

    // Layout (fixed window size)
    Box btnStep_, btnHour_, btnAuto_;
    Box waitingBox_, doctorsBox_, logBox_, rightBox_;
    Box tabLookup_, tabTree_, queryBox_;

    // State
    Tab tab_ = Tab::Lookup;
    bool autoPlay_ = false;
    float autoSeconds_ = 0.5f;   // seconds between ticks while auto-play is on (Up/Down keys)
    sf::Clock autoClock_;
    sf::Clock blinkClock_;
    std::string query_;
    bool queryFocused_ = false;
    bool swallowNextText_ = false;
    bool hasLookup_ = false;
    LookupResult lookup_;

    // Scroll positions (rows for lists, pixels for the tree view)
    int scrollWaiting_ = 0;
    int scrollDoctors_ = 0;
    int scrollLog_ = 0;
    int scrollCards_ = 0;
    float scrollTree_ = 0.f;
};
