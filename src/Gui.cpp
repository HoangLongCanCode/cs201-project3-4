// Gui.cpp
#include "Gui.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>

// ---------------------------------------------------------------------------
// SFML 2 / SFML 3 compatibility. SFML 3 changed event handling, sf::Text
// construction, font loading and sf::VideoMode. Everything that differs is
// kept in this block so the rest of the file is version independent.
// ---------------------------------------------------------------------------
#if SFML_VERSION_MAJOR >= 3
#define HOSPITAL_SFML3 1
#else
#define HOSPITAL_SFML3 0
#endif

namespace {

struct InputEvent {
    enum class Kind { Closed, KeyPressed, TextEntered, LeftClick, Wheel };
    Kind kind;
    sf::Keyboard::Key key = sf::Keyboard::Key::Unknown;
    std::uint32_t unicode = 0;
    float x = 0.f, y = 0.f;
    float delta = 0.f;
};

std::vector<InputEvent> pollEvents(sf::RenderWindow& window) {
    std::vector<InputEvent> out;
#if HOSPITAL_SFML3
    while (const std::optional<sf::Event> event = window.pollEvent()) {
        InputEvent e{InputEvent::Kind::Closed};
        if (event->is<sf::Event::Closed>()) {
            out.push_back(e);
        } else if (const auto* k = event->getIf<sf::Event::KeyPressed>()) {
            e.kind = InputEvent::Kind::KeyPressed;
            e.key = k->code;
            out.push_back(e);
        } else if (const auto* t = event->getIf<sf::Event::TextEntered>()) {
            e.kind = InputEvent::Kind::TextEntered;
            e.unicode = static_cast<std::uint32_t>(t->unicode);
            out.push_back(e);
        } else if (const auto* m = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (m->button == sf::Mouse::Button::Left) {
                e.kind = InputEvent::Kind::LeftClick;
                e.x = static_cast<float>(m->position.x);
                e.y = static_cast<float>(m->position.y);
                out.push_back(e);
            }
        } else if (const auto* w = event->getIf<sf::Event::MouseWheelScrolled>()) {
            e.kind = InputEvent::Kind::Wheel;
            e.delta = w->delta;
            e.x = static_cast<float>(w->position.x);
            e.y = static_cast<float>(w->position.y);
            out.push_back(e);
        }
    }
#else
    sf::Event event;
    while (window.pollEvent(event)) {
        InputEvent e{InputEvent::Kind::Closed};
        switch (event.type) {
            case sf::Event::Closed:
                out.push_back(e);
                break;
            case sf::Event::KeyPressed:
                e.kind = InputEvent::Kind::KeyPressed;
                e.key = event.key.code;
                out.push_back(e);
                break;
            case sf::Event::TextEntered:
                e.kind = InputEvent::Kind::TextEntered;
                e.unicode = event.text.unicode;
                out.push_back(e);
                break;
            case sf::Event::MouseButtonPressed:
                if (event.mouseButton.button == sf::Mouse::Button::Left) {
                    e.kind = InputEvent::Kind::LeftClick;
                    e.x = static_cast<float>(event.mouseButton.x);
                    e.y = static_cast<float>(event.mouseButton.y);
                    out.push_back(e);
                }
                break;
            case sf::Event::MouseWheelScrolled:
                e.kind = InputEvent::Kind::Wheel;
                e.delta = event.mouseWheelScroll.delta;
                e.x = static_cast<float>(event.mouseWheelScroll.x);
                e.y = static_cast<float>(event.mouseWheelScroll.y);
                out.push_back(e);
                break;
            default:
                break;
        }
    }
#endif
    return out;
}

sf::VideoMode makeVideoMode(unsigned w, unsigned h) {
#if HOSPITAL_SFML3
    return sf::VideoMode(sf::Vector2u(w, h));
#else
    return sf::VideoMode(w, h);
#endif
}

bool loadFont(sf::Font& font, const std::string& path) {
#if HOSPITAL_SFML3
    return font.openFromFile(path);
#else
    return font.loadFromFile(path);
#endif
}

float boundsRight(const sf::FloatRect& r) {
#if HOSPITAL_SFML3
    return r.position.x + r.size.x;
#else
    return r.left + r.width;
#endif
}

// ---------------------------------------------------------------------------
// Look and layout
// ---------------------------------------------------------------------------

constexpr unsigned kWidth = 1240;
constexpr unsigned kHeight = 720;
constexpr float kAutoSecondsDefault = 0.5f;   // starting auto-play speed
constexpr float kAutoSecondsMin = 0.1f;       // fastest
constexpr float kAutoSecondsMax = 2.0f;       // slowest
constexpr float kAutoSecondsStep = 0.1f;

const sf::Color kBg(24, 26, 32);
const sf::Color kPanel(36, 39, 48);
const sf::Color kRowAlt(42, 45, 56);
const sf::Color kEdge(60, 65, 78);
const sf::Color kText(232, 234, 238);
const sf::Color kMuted(150, 156, 170);
const sf::Color kAccent(96, 150, 245);
const sf::Color kGood(80, 190, 120);
const sf::Color kWarn(240, 175, 60);
const sf::Color kBad(235, 85, 85);
const sf::Color kButton(52, 57, 70);
const sf::Color kButtonHover(68, 74, 90);
const sf::Color kInput(28, 30, 38);
const sf::Color kCriticalRow(78, 36, 42);
const sf::Color kDark(22, 22, 26);

// Condition 0 is green, 5 amber, 10 red.
sf::Color conditionColor(int level) {
    level = std::max(0, std::min(10, level));
    const sf::Color green(70, 180, 100), amber(238, 182, 48), red(222, 60, 60);
    auto mix = [](sf::Color a, sf::Color b, float t) {
        auto ch = [t](std::uint8_t p, std::uint8_t q) {
            return static_cast<std::uint8_t>(static_cast<float>(p) + (static_cast<float>(q) - p) * t);
        };
        return sf::Color(ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b));
    };
    return level <= 5 ? mix(green, amber, static_cast<float>(level) / 5.f)
                      : mix(amber, red, static_cast<float>(level - 5) / 5.f);
}

// "Sam Lee" -> "S.Lee/34", short enough to fit many keys in a tree node.
std::string shortLabel(const PersonKey& key) {
    const std::string& n = key.name();
    std::size_t lastSpace = n.find_last_of(' ');
    std::string label = (lastSpace == std::string::npos) ? n : n.substr(0, 1) + "." + n.substr(lastSpace + 1);
    return label + "/" + std::to_string(key.age());
}

std::string findFont() {
    const char* candidates[] = {
        "assets/font.ttf",
        // Windows
        "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/tahoma.ttf",
        // macOS
        "/System/Library/Fonts/Supplemental/Arial.ttf", "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc", "/System/Library/Fonts/SFNS.ttf",
        // Linux
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf", "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf", "/usr/share/fonts/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf"};
    for (const char* path : candidates) {
        std::ifstream f(path, std::ios::binary);
        if (f.good()) return path;
    }
    return "";
}

void clampScroll(int& scroll, int total, int visible) {
    scroll = std::max(0, std::min(scroll, std::max(0, total - visible)));
}

}  // namespace

// ---------------------------------------------------------------------------
// Construction and main loop
// ---------------------------------------------------------------------------

HospitalGui::HospitalGui(Hospital& hospital)
    : hospital_(hospital),
      window_(makeVideoMode(kWidth, kHeight), "Nota Hospital - ER Simulation",
              sf::Style::Titlebar | sf::Style::Close) {
    window_.setFramerateLimit(60);

    const float W = static_cast<float>(kWidth), H = static_cast<float>(kHeight);
    btnAuto_ = {W - 16 - 140, 14, 140, 36};
    btnHour_ = {btnAuto_.x - 8 - 120, 14, 120, 36};
    btnStep_ = {btnHour_.x - 8 - 150, 14, 150, 36};

    waitingBox_ = {12, 72, 400, 380};
    doctorsBox_ = {424, 72, 380, 380};
    logBox_ = {12, 464, 792, H - 464 - 12};
    rightBox_ = {816, 72, W - 816 - 12, H - 72 - 12};

    tabLookup_ = {rightBox_.x + 12, rightBox_.y + 10, 120, 28};
    tabTree_ = {tabLookup_.right() + 8, rightBox_.y + 10, 120, 28};
    queryBox_ = {rightBox_.x + 12, rightBox_.y + 50, rightBox_.w - 24, 34};
}

int HospitalGui::run() {
    fontPath_ = findFont();
    if (fontPath_.empty() || !loadFont(font_, fontPath_)) {
        std::cerr << "Could not find a font. Copy any .ttf file to assets/font.ttf and try again.\n";
        return 1;
    }

    while (window_.isOpen()) {
        for (const InputEvent& e : pollEvents(window_)) {
            switch (e.kind) {
                case InputEvent::Kind::Closed: window_.close(); break;
                case InputEvent::Kind::KeyPressed: handleKey(e.key); break;
                case InputEvent::Kind::TextEntered: handleText(e.unicode); break;
                case InputEvent::Kind::LeftClick: handleClick(e.x, e.y); break;
                case InputEvent::Kind::Wheel: handleWheel(e.x, e.y, e.delta); break;
            }
        }
        if (autoPlay_ && autoClock_.getElapsedTime().asSeconds() >= autoSeconds_) {
            advance(1);
            autoClock_.restart();
        }
        draw();
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void HospitalGui::advance(int ticks) {
    hospital_.advance(ticks);
    scrollLog_ = 0;   // jump back to the newest events
    if (hospital_.finished()) autoPlay_ = false;
}

void HospitalGui::handleKey(sf::Keyboard::Key key) {
    using K = sf::Keyboard::Key;
    if (key == K::Tab) {
        tab_ = (tab_ == Tab::Lookup) ? Tab::Tree : Tab::Lookup;
        return;
    }
    if (queryFocused_) {   // typing is handled in handleText
        if (key == K::Escape) queryFocused_ = false;
        return;
    }
    if (key == K::Space || key == K::N || key == K::Right) advance(1);
    else if (key == K::H) advance(simtime::kTicksPerHour);
    // Up = faster auto-play, Down = slower (seconds between ticks)
    else if (key == K::Up) autoSeconds_ = std::max(kAutoSecondsMin, autoSeconds_ - kAutoSecondsStep);
    else if (key == K::Down) autoSeconds_ = std::min(kAutoSecondsMax, autoSeconds_ + kAutoSecondsStep);
    else if (key == K::A) {
        autoPlay_ = !autoPlay_;
        autoClock_.restart();
    } else if (key == K::F) {
        tab_ = Tab::Lookup;
        queryFocused_ = true;
        swallowNextText_ = true;   // the same key press also sends an 'f' text event
    }
}

void HospitalGui::handleText(std::uint32_t unicode) {
    if (swallowNextText_) {
        swallowNextText_ = false;
        if (unicode == 'f' || unicode == 'F') return;
    }
    if (!queryFocused_) return;
    if (unicode == 8) {   // backspace
        if (!query_.empty()) query_.pop_back();
    } else if (unicode == 13 || unicode == 10) {   // enter
        runLookup();
    } else if (unicode >= 32 && unicode < 127 && query_.size() < 60) {
        query_ += static_cast<char>(unicode);
    }
}

void HospitalGui::handleClick(float x, float y) {
    queryFocused_ = false;
    if (btnStep_.contains(x, y)) advance(1);
    else if (btnHour_.contains(x, y)) advance(simtime::kTicksPerHour);
    else if (btnAuto_.contains(x, y)) {
        autoPlay_ = !autoPlay_;
        autoClock_.restart();
    } else if (tabLookup_.contains(x, y)) tab_ = Tab::Lookup;
    else if (tabTree_.contains(x, y)) tab_ = Tab::Tree;
    else if (tab_ == Tab::Lookup && queryBox_.contains(x, y)) queryFocused_ = true;
}

void HospitalGui::handleWheel(float x, float y, float delta) {
    int rows = delta > 0 ? -1 : 1;
    if (waitingBox_.contains(x, y)) scrollWaiting_ += rows;
    else if (doctorsBox_.contains(x, y)) scrollDoctors_ += rows;
    else if (logBox_.contains(x, y)) scrollLog_ += rows * 3;
    else if (rightBox_.contains(x, y)) {
        if (tab_ == Tab::Lookup) scrollCards_ += rows;
        else scrollTree_ = std::max(0.f, scrollTree_ + rows * 40.f);
    }
}

void HospitalGui::runLookup() {
    lookup_ = hospital_.lookup(query_);
    hasLookup_ = true;
    scrollCards_ = 0;
}

// ---------------------------------------------------------------------------
// Drawing primitives
// ---------------------------------------------------------------------------

sf::Text HospitalGui::makeText(const std::string& s, unsigned size, sf::Color color) const {
    sf::String str = sf::String::fromUtf8(s.begin(), s.end());
#if HOSPITAL_SFML3
    sf::Text text(font_, str, size);
#else
    sf::Text text(str, font_, size);
#endif
    text.setFillColor(color);
    return text;
}

float HospitalGui::textWidth(const std::string& s, unsigned size) const {
    return boundsRight(makeText(s, size, kText).getLocalBounds());
}

void HospitalGui::drawRect(const Box& b, sf::Color fill, sf::Color outline, float thickness) {
    sf::RectangleShape r(sf::Vector2f(b.w, b.h));
    r.setPosition(sf::Vector2f(b.x, b.y));
    r.setFillColor(fill);
    if (thickness > 0.f) {
        r.setOutlineColor(outline);
        r.setOutlineThickness(thickness);
    }
    window_.draw(r);
}

void HospitalGui::drawText(const std::string& s, float x, float y, unsigned size, sf::Color color, float maxWidth) {
    std::string shown = s;
    if (maxWidth > 0.f && textWidth(shown, size) > maxWidth) {
        // Trim whole UTF-8 characters until "text..." fits.
        while (!shown.empty() && textWidth(shown + "...", size) > maxWidth) {
            while (!shown.empty() && (static_cast<unsigned char>(shown.back()) & 0xC0) == 0x80) shown.pop_back();
            if (!shown.empty()) shown.pop_back();
        }
        shown += "...";
    }
    sf::Text t = makeText(shown, size, color);
    t.setPosition(sf::Vector2f(x, y));
    window_.draw(t);
}

void HospitalGui::drawTextRight(const std::string& s, float right, float y, unsigned size, sf::Color color) {
    drawText(s, right - textWidth(s, size), y, size, color);
}

void HospitalGui::drawTextCentered(const std::string& s, float cx, float y, unsigned size, sf::Color color) {
    drawText(s, cx - textWidth(s, size) / 2.f, y, size, color);
}

void HospitalGui::drawPanel(const Box& b, const std::string& title, const std::string& note) {
    drawRect(b, kPanel, kEdge, 1.f);
    if (!title.empty()) drawText(title, b.x + 12, b.y + 8, 16, kText, b.w - 24);
    if (!note.empty()) drawTextRight(note, b.right() - 12, b.y + 11, 12, kMuted);
}

void HospitalGui::drawButton(const Box& b, const std::string& label, bool active) {
    sf::Vector2i m = sf::Mouse::getPosition(window_);
    bool hover = b.contains(static_cast<float>(m.x), static_cast<float>(m.y));
    drawRect(b, active ? kAccent : (hover ? kButtonHover : kButton), kEdge, 1.f);
    drawTextCentered(label, b.x + b.w / 2.f, b.y + b.h / 2.f - 10.f, 14, active ? kDark : kText);
}

bool HospitalGui::isHighlighted(const PersonKey& key) const {
    if (!hasLookup_) return false;
    for (const Patient* p : lookup_.patients)
        if (p->key() == key) return true;
    for (const Doctor* d : lookup_.doctors)
        if (d->key() == key) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Panels
// ---------------------------------------------------------------------------

void HospitalGui::draw() {
    window_.clear(kBg);
    drawHeader();
    drawWaiting();
    drawDoctors();
    drawLog();
    drawRightPanel();
    window_.display();
}

void HospitalGui::drawHeader() {
    drawText("Nota Hospital ER", 16, 8, 22, kText);
    std::string sub = hospital_.clock() + "   (" + std::to_string(simtime::kMinutesPerTick) + " min per tick)";
    drawText(sub, 16, 38, 14, kMuted);

    HospitalStats s = hospital_.stats();
    const float sx = 300;
    drawText("Waiting " + std::to_string(s.waiting) + "     In treatment " + std::to_string(s.inTreatment) +
                 "     Discharged " + std::to_string(s.discharged) + "     Not arrived " +
                 std::to_string(s.notYetArrived),
             sx, 12, 14, kText, btnStep_.x - sx - 10);
    const int avgWaitMinutes = static_cast<int>(s.averageWaitTicks * simtime::kMinutesPerTick + 0.5);
    std::string line2 = "Went critical " + std::to_string(s.wentCritical) + "     Avg wait " +
                        std::to_string(avgWaitMinutes) + "m     Doctors on shift " +
                        std::to_string(s.doctorsOnShift) + "/" + std::to_string(hospital_.doctors().size());
    if (hospital_.finished()) line2 += "     Day's patients all done";
    drawText(line2, sx, 36, 14, s.wentCritical > 0 ? kWarn : kMuted, btnStep_.x - sx - 10);

    drawButton(btnStep_, "Next tick  [Space]", false);
    drawButton(btnHour_, "+1 hour  [H]", false);
    // Show the current auto-play speed (seconds per tick) with one decimal.
    const int tenths = static_cast<int>(autoSeconds_ * 10.f + 0.5f);
    const std::string speed = std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "s";
    drawButton(btnAuto_, autoPlay_ ? "Auto: " + speed + " [A]" : "Auto: off  [A]", autoPlay_);
}

void HospitalGui::drawWaiting() {
    const Box& b = waitingBox_;
    const int now = hospital_.tick();
    std::vector<const Patient*> waiting = hospital_.waitingInPriorityOrder();

    const float rowH = 42.f, top = b.y + 36.f;
    const int visible = static_cast<int>((b.h - 40.f) / rowH);
    const int n = static_cast<int>(waiting.size());
    clampScroll(scrollWaiting_, n, visible);
    std::string note = (n > visible) ? "rows " + std::to_string(scrollWaiting_ + 1) + "-" +
                                           std::to_string(std::min(n, scrollWaiting_ + visible)) + " of " +
                                           std::to_string(n) + " (scroll)"
                                     : "seen top to bottom";
    drawPanel(b, "Waiting room (" + std::to_string(n) + ")", note);
    if (n == 0) drawText("No one is waiting.", b.x + 14, top + 6, 14, kMuted);

    for (int i = 0; i < visible && scrollWaiting_ + i < n; ++i) {
        const int idx = scrollWaiting_ + i;
        const Patient& p = *waiting[idx];
        const float y = top + i * rowH;
        drawRect({b.x + 6, y, b.w - 12, rowH - 4}, p.isCritical() ? kCriticalRow : (idx % 2 ? kRowAlt : kPanel));

        drawRect({b.x + 12, y + 5, 28, 28}, conditionColor(p.condition()));
        drawTextCentered(std::to_string(p.condition()), b.x + 26, y + 9, 15, kDark);

        drawText(std::to_string(idx + 1) + ". " + p.name() + ", " + std::to_string(p.age()), b.x + 50, y + 2, 15,
                 kText, 210);
        drawTextRight("waited " + simtime::formatDuration(p.ticksWaited(now)), b.right() - 14, y + 4, 13, kMuted);
        drawText(p.ailment(), b.x + 50, y + 21, 12, kMuted, 200);
        if (p.isCritical()) {
            drawTextRight("CRITICAL", b.right() - 14, y + 21, 12, kBad);
        } else {
            int left = p.ticksUntilCritical(now);
            drawTextRight("critical in " + simtime::formatDuration(left), b.right() - 14, y + 21, 12,
                          left <= 2 ? kWarn : kMuted);
        }
    }
}

void HospitalGui::drawDoctors() {
    const Box& b = doctorsBox_;
    const auto& doctors = hospital_.doctors();
    const int minute = simtime::minuteOfDay(hospital_.tick());

    const float rowH = 42.f, top = b.y + 36.f;
    const int visible = static_cast<int>((b.h - 40.f) / rowH);
    const int n = static_cast<int>(doctors.size());
    clampScroll(scrollDoctors_, n, visible);
    drawPanel(b, "Doctors", n > visible ? "scroll for more" : "");

    for (int i = 0; i < visible && scrollDoctors_ + i < n; ++i) {
        const Doctor& d = *doctors[scrollDoctors_ + i];
        const float y = top + i * rowH;
        const bool onShift = d.isOnShift(minute);
        sf::Color stripe = d.isBusy() ? kAccent : (onShift ? kGood : kEdge);
        drawRect({b.x + 6, y, b.w - 12, rowH - 4}, (scrollDoctors_ + i) % 2 ? kRowAlt : kPanel);
        drawRect({b.x + 6, y, 4, rowH - 4}, stripe);

        sf::Color nameColor = (onShift || d.isBusy()) ? kText : kMuted;
        drawText("Dr. " + d.name(), b.x + 18, y + 2, 15, nameColor, 200);

        // Skill as a 10-segment bar.
        const float barX = b.right() - 14 - 10 * 7.f;
        drawTextRight("skill " + std::to_string(d.skill()), barX - 6, y + 4, 12, kMuted);
        for (int s = 0; s < 10; ++s)
            drawRect({barX + s * 7.f, y + 7, 5, 10}, s < d.skill() ? (onShift ? kAccent : kMuted) : kEdge);

        std::string status;
        sf::Color statusColor;
        if (d.isBusy()) {
            const Patient& p = *d.currentPatient();
            status = (onShift ? "Treating " : "Finishing (shift over) ") + p.name() + ", level " +
                     std::to_string(p.condition()) + " - " + simtime::formatDuration(d.ticksRemaining()) + " left";
            statusColor = kAccent;
        } else if (onShift) {
            status = "Available";
            statusColor = kGood;
        } else {
            status = "Off shift";
            statusColor = kMuted;
        }
        drawText(d.shiftString(), b.x + 18, y + 21, 12, kMuted);
        drawText(status, b.x + 100, y + 21, 12, statusColor, b.w - 120);
    }
}

void HospitalGui::drawLog() {
    const Box& b = logBox_;
    const auto& log = hospital_.log();
    const float lineH = 18.f, top = b.y + 34.f;
    const int visible = static_cast<int>((b.h - 40.f) / lineH);
    const int n = static_cast<int>(log.size());
    clampScroll(scrollLog_, n, visible);
    drawPanel(b, "Event log (newest first)", "Space: next tick   H: +1 hour   A: auto   F: search   Tab: B-Tree view");

    for (int i = 0; i < visible; ++i) {
        int idx = n - 1 - (scrollLog_ + i);
        if (idx < 0) break;
        const std::string& line = log[idx];
        sf::Color c = kText;
        if (line.find("CRITICAL") != std::string::npos) c = kBad;
        else if (line.find("discharged") != std::string::npos) c = kGood;
        else if (line.find("shift") != std::string::npos) c = kMuted;
        else if (line.find("is seeing") != std::string::npos) c = kAccent;
        drawText(line, b.x + 12, top + i * lineH, 13, c, b.w - 24);
    }
}

void HospitalGui::drawRightPanel() {
    drawPanel(rightBox_, "");
    drawButton(tabLookup_, "Lookup", tab_ == Tab::Lookup);
    drawButton(tabTree_, "B-Trees", tab_ == Tab::Tree);
    drawTextRight("Tab to switch", rightBox_.right() - 12, rightBox_.y + 16, 12, kMuted);

    Box area{rightBox_.x + 12, rightBox_.y + 48, rightBox_.w - 24, rightBox_.h - 56};
    if (tab_ == Tab::Lookup) drawLookup(area);
    else drawTreeView(area);
}

void HospitalGui::drawLookup(const Box& area) {
    // Search box
    drawRect(queryBox_, kInput, queryFocused_ ? kAccent : kEdge, queryFocused_ ? 2.f : 1.f);
    if (query_.empty() && !queryFocused_) {
        drawText("Click here (or press F) and type a name", queryBox_.x + 10, queryBox_.y + 7, 14, kMuted);
    } else {
        bool caret = queryFocused_ && static_cast<int>(blinkClock_.getElapsedTime().asSeconds() * 2) % 2 == 0;
        drawText(query_ + (caret ? "|" : ""), queryBox_.x + 10, queryBox_.y + 7, 15, kText, queryBox_.w - 20);
    }
    float y = queryBox_.bottom() + 8;
    drawText("Name prefix: Sam      Exact: Sam Lee, 34, 1991-12-30", area.x, y, 12, kMuted, area.w);
    y += 24;

    if (!hasLookup_) {
        const char* lines[] = {
            "Patients are added to the patient B-Tree when they",
            "arrive; doctors are added when the data is loaded.",
            "",
            "A name prefix does an in-order range search, so",
            "everyone whose name starts with it is listed.",
            "",
            "\"name, age, date of birth\" is the full key and does",
            "a normal B-Tree search. The number of nodes the",
            "search visited is shown with the result.",
            "",
            "Press Enter to search. Matching keys are also",
            "highlighted in the B-Trees tab."};
        for (const char* l : lines) {
            drawText(l, area.x, y, 13, kMuted);
            y += 19;
        }
        return;
    }

    drawText(lookup_.message, area.x, y, 14, lookup_.ok ? kText : kBad, area.w);
    y += 20;
    if (lookup_.exact) {
        drawText("Patient tree: " + std::to_string(lookup_.patientNodesVisited) + " of " +
                     std::to_string(hospital_.patientTree().nodeCount()) + " nodes visited (height " +
                     std::to_string(hospital_.patientTree().height()) + ").  Doctor tree: " +
                     std::to_string(lookup_.doctorNodesVisited) + " visited.",
                 area.x, y, 12, kMuted, area.w);
        y += 18;
    }

    // Result cards: patients first, then doctors.
    const float cardH = 88.f, gap = 8.f;
    const int total = static_cast<int>(lookup_.patients.size() + lookup_.doctors.size());
    const int visible = std::max(1, static_cast<int>((area.bottom() - y - 4) / (cardH + gap)));
    clampScroll(scrollCards_, total, visible);
    if (total > visible)
        drawTextRight(std::to_string(scrollCards_ + 1) + "-" + std::to_string(std::min(total, scrollCards_ + visible)) +
                          " of " + std::to_string(total) + " (scroll)",
                      area.right(), y - 18, 12, kMuted);

    for (int i = 0; i < visible && scrollCards_ + i < total; ++i) {
        const int idx = scrollCards_ + i;
        const float cy = y + 4 + i * (cardH + gap);
        drawRect({area.x, cy, area.w, cardH}, kRowAlt, kEdge, 1.f);
        if (idx < static_cast<int>(lookup_.patients.size()))
            drawPatientCard(*lookup_.patients[idx], area.x, cy, area.w);
        else
            drawDoctorCard(*lookup_.doctors[idx - lookup_.patients.size()], area.x, cy, area.w);
    }
}

void HospitalGui::drawPatientCard(const Patient& p, float x, float y, float w) {
    const int now = hospital_.tick();
    drawRect({x + 10, y + 10, 26, 26}, conditionColor(p.condition()));
    drawTextCentered(std::to_string(p.condition()), x + 23, y + 13, 14, kDark);
    drawText("Patient: " + p.name(), x + 46, y + 6, 15, kText, w - 56);
    drawText("Age " + std::to_string(p.age()) + "   DOB " + p.dob() + "   " + p.ailment(), x + 46, y + 26, 12, kMuted,
             w - 56);

    std::string status;
    sf::Color c = kText;
    switch (p.status()) {
        case PatientStatus::Waiting:
            status = "Waiting " + simtime::formatDuration(p.ticksWaited(now)) +
                     (p.isCritical() ? " - CRITICAL"
                                     : " - critical in " + simtime::formatDuration(p.ticksUntilCritical(now)));
            c = p.isCritical() ? kBad : kWarn;
            break;
        case PatientStatus::InTreatment:
            status = "In treatment with Dr. " + p.doctor()->name();
            c = kAccent;
            break;
        case PatientStatus::Discharged:
            status = "Discharged " + simtime::formatClock(p.dischargeTick()) + " by Dr. " + p.doctor()->name();
            c = kGood;
            break;
        case PatientStatus::NotArrived:
            status = "Not arrived yet";
            c = kMuted;
            break;
    }
    drawText(status, x + 12, y + 46, 13, c, w - 24);
    std::string history = "Arrived " + simtime::formatClock(p.arrivalTick()) + " at level " +
                          std::to_string(p.initialCondition());
    if (p.wentCritical()) history += ", went critical while waiting";
    drawText(history, x + 12, y + 66, 12, kMuted, w - 24);
}

void HospitalGui::drawDoctorCard(const Doctor& d, float x, float y, float w) {
    const bool onShift = d.isOnShift(simtime::minuteOfDay(hospital_.tick()));
    drawRect({x + 10, y + 10, 26, 26}, onShift ? kGood : kEdge);
    drawTextCentered("Dr", x + 23, y + 13, 13, kDark);
    drawText("Doctor: Dr. " + d.name(), x + 46, y + 6, 15, kText, w - 56);
    drawText("Age " + std::to_string(d.age()) + "   DOB " + d.dob() + "   Skill " + std::to_string(d.skill()) + "/10",
             x + 46, y + 26, 12, kMuted, w - 56);
    std::string status = d.isBusy() ? "Treating " + d.currentPatient()->name() + " (" +
                                          simtime::formatDuration(d.ticksRemaining()) + " left)"
                                    : (onShift ? "Available" : "Off shift");
    drawText(status, x + 12, y + 46, 13, d.isBusy() ? kAccent : (onShift ? kGood : kMuted), w - 24);
    drawText("Shift " + d.shiftString() + "   Patients seen: " + std::to_string(d.patientsSeen()), x + 12, y + 66, 12,
             kMuted, w - 24);
}

// ---------------------------------------------------------------------------
// B-Tree view
// ---------------------------------------------------------------------------

void HospitalGui::drawTreeView(const Box& area) {
    // Copy each tree's nodes (level by level) so both trees use the same drawing code.
    std::vector<std::pair<int, std::vector<PersonKey>>> doctorNodes, patientNodes;
    hospital_.doctorTree().forEachNodeByLevel([&](int depth, const std::vector<Hospital::DoctorTree::Entry>& es) {
        std::vector<PersonKey> keys;
        for (const auto& e : es) keys.push_back(e.key);
        doctorNodes.push_back({depth, keys});
    });
    hospital_.patientTree().forEachNodeByLevel([&](int depth, const std::vector<Hospital::PatientTree::Entry>& es) {
        std::vector<PersonKey> keys;
        for (const auto& e : es) keys.push_back(e.key);
        patientNodes.push_back({depth, keys});
    });

    const auto& dt = hospital_.doctorTree();
    const auto& pt = hospital_.patientTree();
    float y = area.y - scrollTree_;
    y = drawTreeLevels("Doctor B-Tree (t = " + std::to_string(Hospital::kBTreeDegree) + "): " +
                           std::to_string(dt.size()) + " keys, " + std::to_string(dt.nodeCount()) + " nodes, height " +
                           std::to_string(dt.height()),
                       doctorNodes, area, y);
    y += 14;
    y = drawTreeLevels("Patient B-Tree (t = " + std::to_string(Hospital::kBTreeDegree) + "): " +
                           std::to_string(pt.size()) + " keys, " + std::to_string(pt.nodeCount()) + " nodes, height " +
                           std::to_string(pt.height()),
                       patientNodes, area, y);

    // Stop scrolling once the bottom of the drawing is in view.
    float contentBottom = y + scrollTree_;
    scrollTree_ = std::min(scrollTree_, std::max(0.f, contentBottom - area.bottom() + 10));
}

float HospitalGui::drawTreeLevels(const std::string& title,
                                  const std::vector<std::pair<int, std::vector<PersonKey>>>& nodes, const Box& area,
                                  float y) {
    auto visible = [&](float top, float h) { return top >= area.y && top + h <= area.bottom(); };
    const unsigned keySize = 11;
    const float nodeH = 20.f, pad = 5.f, sep = 9.f;

    if (visible(y, 20)) drawText(title, area.x, y, 13, kText, area.w);
    y += 22;
    if (nodes.empty() || nodes.front().second.empty()) {
        if (visible(y, 16)) drawText("(empty)", area.x, y, 12, kMuted);
        return y + 18;
    }

    int currentDepth = -1;
    float x = area.x;
    for (const auto& node : nodes) {
        if (node.first != currentDepth) {   // new level
            currentDepth = node.first;
            if (currentDepth > 0) y += nodeH + 6;
            if (visible(y, 14)) drawText("Level " + std::to_string(currentDepth), area.x, y, 11, kMuted);
            y += 15;
            x = area.x;
        }
        // Width of this node's keys.
        float w = pad;
        for (std::size_t i = 0; i < node.second.size(); ++i)
            w += textWidth(shortLabel(node.second[i]), keySize) + (i + 1 < node.second.size() ? sep : 0.f);
        w += pad;
        if (x > area.x && x + w > area.right()) {   // wrap to the next row
            x = area.x;
            y += nodeH + 4;
        }
        if (visible(y, nodeH)) {
            drawRect({x, y, w, nodeH}, kInput, kEdge, 1.f);
            float kx = x + pad;
            for (std::size_t i = 0; i < node.second.size(); ++i) {
                const PersonKey& k = node.second[i];
                std::string label = shortLabel(k);
                drawText(label, kx, y + 3, keySize, isHighlighted(k) ? kWarn : kText);
                kx += textWidth(label, keySize);
                if (i + 1 < node.second.size()) {
                    drawRect({kx + sep / 2.f - 0.5f, y + 3, 1, nodeH - 6}, kEdge);
                    kx += sep;
                }
            }
        }
        x += w + 6;
    }
    return y + nodeH + 4;
}
