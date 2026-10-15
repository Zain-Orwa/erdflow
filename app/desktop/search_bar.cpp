// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "search_bar.hpp"

#include "schema_view.hpp"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>

namespace erdflow::desktop {
namespace {
// How long the typing has to pause before the diagram is filtered on it. Long
// enough that writing a word is one change rather than six, short enough that
// it never feels like waiting.
constexpr int settling_ms = 220;

struct KindEntry { int kind; const char* label; };
const std::vector<KindEntry>& kinds(SearchSubject subject) {
    static const std::vector<KindEntry> diagram{
        {static_cast<int>(SearchKind::Everything), "All kinds"},
        {static_cast<int>(SearchKind::Entities), "Entities"},
        {static_cast<int>(SearchKind::Attributes), "Attributes"},
        {static_cast<int>(SearchKind::Relationships), "Relationships"},
        {static_cast<int>(SearchKind::Hierarchies), "Hierarchies"},
    };
    static const std::vector<KindEntry> schema{
        {static_cast<int>(SchemaSearchKind::Everything), "All kinds"},
        {static_cast<int>(SchemaSearchKind::Tables), "Tables"},
        {static_cast<int>(SchemaSearchKind::Columns), "Columns"},
        {static_cast<int>(SchemaSearchKind::Keys), "Keys"},
        {static_cast<int>(SchemaSearchKind::Relationships), "Relationships"},
    };
    return subject == SearchSubject::Schema ? schema : diagram;
}

// What the bar says, in the terms of what it searches: the diagram's elements,
// or the schema's tables and what they hold.
struct Words {
    const char* name;
    const char* placeholder;
    const char* kind_tip;
    const char* relatives_tip;
    const char* fade_tip;
    const char* hide_tip;
    const char* close_tip;
};
const Words& words(SearchSubject subject) {
    static const Words diagram{
        "searchBar",
        "Search the diagram by name",
        "Look among one kind of element, or among all of them. "
        "Choosing a kind and typing nothing shows every element of that kind.",
        "Also keep what a match belongs to and what it is joined to: its attributes, the "
        "relationships and hierarchies it takes part in, and the far side of those.",
        "Leave the rest of the diagram faintly in place, so a match is seen where it sits.",
        "Take the rest of the diagram away instead. A line goes with whichever of its "
        "ends goes, so nothing is left hanging.",
        "Close the search and put the whole diagram back.\tEsc",
    };
    static const Words schema{
        "schemaSearchBar",
        "Search the schema by name",
        "Look among tables, columns, keys or relationships, or among all of them. "
        "Choosing a kind and typing nothing shows every one of that kind.",
        "Also keep the tables a match is joined to by its foreign keys, both ways: the tables "
        "it refers to and the tables that refer to it.",
        "Leave the rest of the schema faintly in place, so a match is seen where it sits.",
        "Take the rest of the schema away instead, here and in a picture of it. A line goes "
        "with either of its tables, so nothing is left hanging.",
        "Close the search and put the whole schema back.\tEsc",
    };
    return subject == SearchSubject::Schema ? schema : diagram;
}
} // namespace

SearchBar::SearchBar(QWidget* parent, SearchSubject subject) : QWidget(parent) {
    const auto& said = words(subject);
    setObjectName(QString::fromUtf8(said.name));
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(22, 6, 12, 6);
    layout->setSpacing(8);

    text_ = new QLineEdit(this);
    own_text_ = text_;
    text_->setObjectName("searchText");
    text_->setPlaceholderText(QString::fromUtf8(said.placeholder));
    text_->setClearButtonEnabled(true);
    text_->setMinimumWidth(160);
    layout->addWidget(text_, 1);

    // Choosing a kind and typing nothing asks for every element of that kind,
    // which is how "show me only the entities" is asked for.
    kind_ = new QComboBox(this);
    kind_->setObjectName("searchKind");
    kind_->setToolTip(QString::fromUtf8(said.kind_tip));
    for (const auto& entry : kinds(subject)) kind_->addItem(QString::fromUtf8(entry.label), entry.kind);
    layout->addWidget(kind_);

    settings_ = new QToolButton(this);
    settings_->setObjectName("searchSettings");
    settings_->setText("Options");
    settings_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    settings_->setPopupMode(QToolButton::InstantPopup);
    // Two questions, not two switches. How much to keep is one, and what to do
    // with the rest is the other, and they are answered separately: keeping a
    // match's neighbours and taking the rest away is the clearest view of all,
    // so neither answer may rule the other out. Written as two sets of
    // alternatives, because within each question the choices really are
    // alternatives -- choosing one does cancel the other.
    auto* menu = new QMenu(settings_);
    menu->setObjectName("searchSettingsMenu");
    // Written as entries that cannot be chosen rather than as sections, which
    // some styles draw as a bare line with the words dropped -- and a heading
    // nobody can read is what made these two look like rival switches. The
    // Export menu's headings are written the same way, for the same reason.
    auto* keeping_heading = menu->addAction("What to keep");
    keeping_heading->setObjectName("searchKeepHeading");
    keeping_heading->setEnabled(false);
    auto* keeping = new QActionGroup(menu);
    keeping->setExclusive(true);
    auto* only_matches = menu->addAction("Only what matches");
    only_matches_ = only_matches;
    only_matches->setObjectName("searchKeepMatches");
    only_matches->setCheckable(true);
    only_matches->setChecked(true);
    only_matches->setActionGroup(keeping);
    only_matches->setToolTip(subject == SearchSubject::Schema ? "Keep the tables the search found, and nothing else."
                                                              : "Keep the elements the search found, and nothing else.");
    relatives_ = menu->addAction("What matches, and what it touches");
    relatives_->setObjectName("searchRelatives");
    relatives_->setCheckable(true);
    relatives_->setActionGroup(keeping);
    relatives_->setToolTip(QString::fromUtf8(said.relatives_tip));

    menu->addSeparator();
    auto* rest_heading = menu->addAction("What to do with the rest");
    rest_heading->setObjectName("searchRestHeading");
    rest_heading->setEnabled(false);
    auto* becoming = new QActionGroup(menu);
    becoming->setExclusive(true);
    auto* fade_rest = menu->addAction("Fade it");
    fade_rest_ = fade_rest;
    fade_rest->setObjectName("searchFadeRest");
    fade_rest->setCheckable(true);
    fade_rest->setChecked(true);
    fade_rest->setActionGroup(becoming);
    fade_rest->setToolTip(QString::fromUtf8(said.fade_tip));
    hide_rest_ = menu->addAction("Hide it");
    hide_rest_->setObjectName("searchHideRest");
    hide_rest_->setCheckable(true);
    hide_rest_->setActionGroup(becoming);
    hide_rest_->setToolTip(QString::fromUtf8(said.hide_tip));
    settings_->setMenu(menu);
    layout->addWidget(settings_);

    count_ = new QLabel(this);
    count_->setObjectName("searchCount");
    count_->setMinimumWidth(80);
    layout->addWidget(count_);

    auto* close = new QToolButton(this);
    close->setObjectName("searchClose");
    close->setText("✕");
    close->setToolTip(QString::fromUtf8(said.close_tip));
    layout->addWidget(close);

    // Typing settles before the diagram is filtered on it; everything else is
    // a deliberate choice and takes effect as it is made.
    settling_ = new QTimer(this);
    settling_->setSingleShot(true);
    settling_->setInterval(settling_ms);
    connect(settling_, &QTimer::timeout, this, [this] { if (on_changed) on_changed(); });
    connect(text_, &QLineEdit::textChanged, this, [this] { changed(false); });
    connect(text_, &QLineEdit::returnPressed, this, [this] { changed(true); });
    connect(kind_, &QComboBox::currentIndexChanged, this, [this] { changed(true); });
    connect(relatives_, &QAction::toggled, this, [this] { changed(true); });
    connect(hide_rest_, &QAction::toggled, this, [this] { changed(true); });
    connect(close, &QToolButton::clicked, this, [this] { if (on_closed) on_closed(); });
}

void SearchBar::changed(bool at_once) {
    if (at_once) {
        settling_->stop();
        if (on_changed) on_changed();
        return;
    }
    settling_->start();
}

QString SearchBar::text() const { return text_->text().trimmed(); }
int SearchBar::kind() const { return kind_->currentData().toInt(); }
bool SearchBar::with_relatives() const { return relatives_->isChecked(); }
bool SearchBar::hide_the_rest() const { return hide_rest_->isChecked(); }

void SearchBar::reset() {
    const QSignalBlocker kind(kind_);
    const QSignalBlocker keeping(relatives_);
    const QSignalBlocker rest(hide_rest_);
    kind_->setCurrentIndex(0);
    only_matches_->setChecked(true);
    fade_rest_->setChecked(true);
}

DiagramSearch SearchBar::search() const {
    DiagramSearch asked;
    asked.text = text_->text().trimmed();
    asked.kind = static_cast<SearchKind>(kind_->currentData().toInt());
    asked.with_relatives = relatives_->isChecked();
    asked.hide_the_rest = hide_rest_->isChecked();
    return asked;
}

void SearchBar::open() {
    show();
    text_->setFocus();
    text_->selectAll();
}

void SearchBar::look_for(const QString& text) {
    text_->setText(text);
    open();
    // Typed all at once rather than letter by letter, so there is nothing to
    // settle and the diagram is filtered straight away.
    changed(true);
}

void SearchBar::use_text(QLineEdit* field, bool typing_opens) {
    if (!field || field == text_) return;
    typing_opens_ = typing_opens;
    field->setText(text_->text());
    own_text_->hide();
    text_ = field;
    connect(field, &QLineEdit::textChanged, this, [this] {
        // Typing is asking, so the bar comes with it; clearing what was typed
        // leaves it where it is, as clearing its own box does.
        if (typing_opens_ && !isVisible() && !text_->text().isEmpty()) show();
        changed(false);
    });
    connect(field, &QLineEdit::returnPressed, this, [this] { changed(true); });
    field->installEventFilter(this);
}

bool SearchBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == text_ && text_ != own_text_ && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape && (typing_opens_ || isVisible())) {
        if (on_closed) on_closed();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void SearchBar::report(int found) {
    if (!search().looking()) { count_->clear(); return; }
    count_->setText(found == 0 ? QStringLiteral("Nothing found")
                   : found == 1 ? QStringLiteral("1 found")
                                : QStringLiteral("%1 found").arg(found));
}

void SearchBar::keyPressEvent(QKeyEvent* event) {
    // Escape puts the whole diagram back, wherever in the bar the caret is.
    if (event->key() == Qt::Key_Escape) {
        if (on_closed) on_closed();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace erdflow::desktop
