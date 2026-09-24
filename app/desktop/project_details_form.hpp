// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "app/desktop/theme.hpp"

#include <QString>
#include <QWidget>

#include <functional>

class QAbstractButton;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpacerItem;

namespace erdflow::desktop {

// What a project is called, where it goes, and what it is for.
//
// Asked on the home screen beside the route rather than in a dialog after it,
// so somebody can see what they chose while they name it, and change their
// mind about either without losing the other.
//
// Only the name is asked at first (Zain, 2026-09-24, ADR-022 section 9.18).
// Where the project goes, what it is for and whether it gets a folder all have
// working defaults, so they wait under More options, and a quiet line under
// the name always says where the project will be made.
class ProjectDetailsForm final : public QWidget {
public:
    explicit ProjectDetailsForm(QWidget* parent = nullptr);

    void wear(ThemeId id);

    // Where a location comes from. Held as a question rather than answered
    // here, because answering it means opening the platform's own folder
    // chooser -- which stops and waits for somebody, and so cannot be part of
    // anything that has to run without one.
    std::function<QString(const QString& start)> ask_where;
    // Somebody asked for the project to be made, and everything needed is
    // filled in. A form that is not filled in never calls this.
    std::function<void()> create;
    std::function<void()> cancel;
    // How a project's name becomes the name of its file and folder. That rule
    // belongs to whatever writes the file; the form only uses it to say where
    // the project will go. Without it, the name is shown as typed.
    std::function<QString(const QString& name)> file_name_for;
    // More options was opened or closed, so the page may need to fit again.
    std::function<void()> reshaped;

    [[nodiscard]] QString project_name() const;
    [[nodiscard]] QString location() const;
    [[nodiscard]] QString description() const;
    [[nodiscard]] bool wants_own_folder() const;

    void set_project_name(const QString& value);
    void set_location(const QString& value);
    void set_description(const QString& value);
    void set_wants_own_folder(bool on);
    // What the Create button says: the chosen card's title, so it names what
    // will be made.
    void set_action(const QString& words);
    [[nodiscard]] QString action() const;
    // Whether Location, Description and the folder choice are shown.
    void show_more(bool on);
    [[nodiscard]] bool showing_more() const;
    // The folder the project will be made in, as the line under the name
    // shows it, whole rather than shortened to fit.
    [[nodiscard]] QString saved_to() const;
    // Report a creation failure beside the form, where validation failures
    // already appear. This keeps file-system errors actionable without a
    // modal dialog taking the user away from the values they need to amend.
    // A failure about where the project goes opens More options, so the
    // field it is about can be seen.
    void show_error(const QString& message, bool about_location = false);
    void clear_error();
    // Puts the caret in the name, the first thing the form asks.
    void focus_name();
    // Tighter, for a window too short to hold the Home screen otherwise: the
    // gaps close up and the description comes down to a line and a little.
    // Fields and buttons keep their size.
    void set_compact(bool on);

    // Whether everything required has been given. A name and a location are
    // required; a description is not, and says so on its own label.
    [[nodiscard]] bool complete() const;
    // What is missing, in words, or nothing where nothing is.
    [[nodiscard]] QString what_is_missing() const;

private:
    void build();
    void follow_the_form();
    void follow_the_path();

    ThemeId theme_ = ThemeId::Azure;
    QLineEdit* name_ = nullptr;
    QLineEdit* where_ = nullptr;
    QPlainTextEdit* about_ = nullptr;
    QCheckBox* own_folder_ = nullptr;
    QLabel* saved_to_ = nullptr;
    QAbstractButton* more_ = nullptr;
    QWidget* options_ = nullptr;
    QPushButton* browse_ = nullptr;
    QPushButton* create_ = nullptr;
    QPushButton* cancel_ = nullptr;
    QLabel* trouble_ = nullptr;
    QSpacerItem* rule_gap_ = nullptr;
    QSpacerItem* row_gap_ = nullptr;
    QSpacerItem* actions_gap_ = nullptr;
    bool compact_ = false;
};

} // namespace erdflow::desktop
