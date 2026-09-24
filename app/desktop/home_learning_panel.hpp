// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "app/desktop/theme.hpp"

#include <QSize>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <array>
#include <cstddef>
#include <functional>

class QAction;
class QAbstractButton;
class QLabel;

namespace erdflow::desktop {

struct HomeLearningTopic {
    const char* title;
    const char* description;
    // The drawing in the line-art set, by file name.
    const char* icon;
};

inline constexpr std::size_t home_learning_topic_count = 4;

[[nodiscard]] const std::array<HomeLearningTopic, home_learning_topic_count>&
home_learning_topics();

// No Open an example project link: the sidebar's Examples row is where an
// example is opened (Zain, 2026-09-24, ADR-022 section 9.20).
enum class HomeLearningLink { ViewTutorials };
inline constexpr std::size_t home_learning_link_count = 1;

struct HomeLearningLinkDefinition {
    HomeLearningLink link;
    const char* object_name;
    const char* action_object_name;
    const char* label;
    const char* icon;
};

[[nodiscard]] const std::array<HomeLearningLinkDefinition, home_learning_link_count>&
home_learning_links();

// The 306 px companion panel on the right side of Home. Its lessons are
// informational; its link is a real button backed by a QAction, so it takes
// focus, answers Space and Return, and is read out as what it is.
//
// Its spacing gives way as the window gets shorter, so the whole panel is
// always on screen: the Home screen is one page and is never scrolled.
class HomeLearningPanel final : public QWidget {
public:
    using Callback = std::function<void()>;

    explicit HomeLearningPanel(QWidget* parent = nullptr);

    void wear(ThemeId id);

    [[nodiscard]] QAction* action(HomeLearningLink link) const;
    [[nodiscard]] QAbstractButton* button(HomeLearningLink link) const;
    void set_callback(HomeLearningLink link, Callback callback);

    std::function<void(HomeLearningLink)> activated;

    // Text accessors make the canonical copy verifiable without inspecting
    // rendered pixels or reaching into private child widgets.
    [[nodiscard]] QString top_phrase() const;
    [[nodiscard]] QString learn_more_heading() const;
    [[nodiscard]] QString footer_phrase() const;
    [[nodiscard]] QStringList topic_titles() const;
    [[nodiscard]] QStringList topic_descriptions() const;
    [[nodiscard]] QStringList link_labels() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    [[nodiscard]] static std::size_t index_of(HomeLearningLink link);
    [[nodiscard]] int wave_height() const;

    ThemeId theme_ = ThemeId::Azure;
    QLabel* phrase_ = nullptr;
    QWidget* rule_ = nullptr;
    QLabel* heading_ = nullptr;
    std::array<QLabel*, home_learning_topic_count> topic_icons_{};
    std::array<QLabel*, home_learning_topic_count> topic_titles_{};
    std::array<QLabel*, home_learning_topic_count> topic_bodies_{};
    std::array<QAbstractButton*, home_learning_link_count> buttons_{};
    std::array<QAction*, home_learning_link_count> actions_{};
    std::array<Callback, home_learning_link_count> callbacks_{};
};

} // namespace erdflow::desktop
