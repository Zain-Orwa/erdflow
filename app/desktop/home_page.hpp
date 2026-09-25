// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "app/desktop/app_top_bar.hpp"
#include "app/desktop/home_learning_panel.hpp"
#include "app/desktop/home_live_demo.hpp"
#include "app/desktop/home_sidebar.hpp"
#include "app/desktop/start_route_card.hpp"
#include "app/desktop/welcome_flow_illustration.hpp"
#include "app/desktop/theme.hpp"

#include <QWidget>

#include <functional>
#include <vector>

class QLabel;
class QScrollArea;
class QSpacerItem;

namespace erdflow::desktop {

// The screen ERDFlow opens on.
//
// A slim bar across the top, then three columns: what you can go to, what you
// can start, and what you can learn. It is a page of the application rather
// than a dialog, because a dialog is something you dismiss to get to the work
// and this is where the work is chosen.
//
// It is **one page**. Zain settled on 2026-09-23 that nothing on it is reached
// by scrolling: the cards and their Create buttons are all on screen at once,
// and where a window is short it is the spacing that gives way, then the
// illustration. The page ends with the cards (ADR-022 section 9.19). The centre still sits in a scroll area, but only as the last
// resort of a window shorter than anything ERDFlow opens at.
//
// Built from its own parts in its own file. `main_window.cpp` is already five
// thousand lines, and a home screen folded into it would be a sixth thousand
// nobody could find their way around.
class HomePage final : public QWidget {
public:
    explicit HomePage(QWidget* parent = nullptr);

    // The palette it draws in. Everything here asks the resolved tokens rather
    // than a Theme's own fields, so a theme that states nothing still gets a
    // complete answer. Restyles what is there rather than building it again,
    // so the choice of card survives a change of theme -- and a theme is tried
    // by hovering it, which would otherwise rebuild the page on every name
    // the pointer passed.
    void wear(ThemeId id);

    // Choosing a card and starting a project are two different acts, and are
    // kept apart. Pressing a card only moves the choice; nothing is created
    // and nowhere is gone to until the card's own + Create is pressed. A card
    // that started work the moment it was touched would make it impossible to
    // look at the cards and decide.
    std::function<void(StartRoute)> route_selected;
    // A card's + Create was pressed. A route that cannot yet be taken has its
    // button disabled with its card, so this is always one the application
    // can actually follow.
    std::function<void(StartRoute)> route_chosen;
    [[nodiscard]] StartRoute chosen_route() const { return chosen_; }

    // Which sidebar row is lit, by its position. For tests, which cannot read
    // pixels.
    [[nodiscard]] int chosen_row() const;
    [[nodiscard]] QStringList sidebar_labels() const;
    // The cards, in the order they are shown.
    [[nodiscard]] std::vector<StartRouteCard*> cards() const { return cards_; }
    // Between each card and the next, the sign of the way from one to the
    // other and back, in the cards' order.
    [[nodiscard]] std::vector<QWidget*> bridges() const { return bridges_; }
    // The live demos, one inside each card and in the same order.
    [[nodiscard]] std::vector<HomeLiveDemo*> demos() const { return demos_; }
    // The one clock the demos are played by.
    [[nodiscard]] HomeDemoClock* demo_clock() const { return demo_clock_; }
    // Plays the demos, or stands them still, finished, as a picture that has
    // to come out the same every time needs them. They start still where
    // reduced motion is asked for, and tick only while Home is showing.
    void set_demos_moving(bool on) { demo_clock_->set_moving(on); }
    [[nodiscard]] WelcomeFlowIllustration* hero() const { return hero_; }
    // Where each sidebar row and learning link goes belongs to whoever owns
    // the page, so the page hands its parts over rather than guessing.
    [[nodiscard]] HomeSidebar* sidebar() const { return sidebar_; }
    [[nodiscard]] HomeLearningPanel* learning() const { return learning_; }
    [[nodiscard]] AppTopBar* top_bar() const { return top_bar_; }
    // Whether the centre is being scrolled rather than shown whole. For tests:
    // at any size ERDFlow opens at, it must not be.
    [[nodiscard]] bool centre_needs_scrolling() const;
    [[nodiscard]] bool compact() const { return compact_; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void build_centre();
    void lay_out();
    void fit();
    void set_compact(bool on);
    void place_hero();
    [[nodiscard]] int spacing_saving() const;

    ThemeId theme_ = ThemeId::Azure;
    AppTopBar* top_bar_ = nullptr;
    HomeSidebar* sidebar_ = nullptr;
    QWidget* centre_ = nullptr;
    QScrollArea* centre_scroll_ = nullptr;
    HomeLearningPanel* learning_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* welcome_ = nullptr;
    QLabel* subtitle_ = nullptr;
    QLabel* section_ = nullptr;
    // Between each card and the next, the way on and the way back.
    std::vector<QWidget*> bridges_;
    QWidget* card_row_ = nullptr;
    WelcomeFlowIllustration* hero_ = nullptr;
    QWidget* hero_holder_ = nullptr;
    QWidget* hero_backdrop_ = nullptr;
    // The gaps between the centre's parts, which close up on a short window.
    QSpacerItem* top_space_ = nullptr;
    QSpacerItem* header_space_ = nullptr;
    QSpacerItem* greeting_space_ = nullptr;
    QSpacerItem* cards_space_ = nullptr;
    std::vector<StartRouteCard*> cards_;
    HomeDemoClock* demo_clock_ = nullptr;
    std::vector<HomeLiveDemo*> demos_;
    StartRoute chosen_ = StartRoute::Conceptual;
    // Whether the cards are drawn small to keep the page on one screen.
    bool compact_ = false;
};

} // namespace erdflow::desktop
