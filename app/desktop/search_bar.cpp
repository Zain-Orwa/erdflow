#include "search_bar.hpp"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QTimer>
#include <QToolButton>

namespace erdflow::desktop {
namespace {
// How long the typing has to pause before the diagram is filtered on it. Long
// enough that writing a word is one change rather than six, short enough that
// it never feels like waiting.
constexpr int settling_ms = 220;

struct KindEntry { SearchKind kind; const char* label; };
const std::vector<KindEntry>& kinds() {
    static const std::vector<KindEntry> list{
        {SearchKind::Everything, "All kinds"},
        {SearchKind::Entities, "Entities"},
        {SearchKind::Attributes, "Attributes"},
        {SearchKind::Relationships, "Relationships"},
        {SearchKind::Hierarchies, "Hierarchies"},
    };
    return list;
}
} // namespace

SearchBar::SearchBar(QWidget* parent) : QWidget(parent) {
    setObjectName("searchBar");
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(22, 6, 12, 6);
    layout->setSpacing(8);

    text_ = new QLineEdit(this);
    text_->setObjectName("searchText");
    text_->setPlaceholderText("Search the diagram by name");
    text_->setClearButtonEnabled(true);
    text_->setMinimumWidth(160);
    layout->addWidget(text_, 1);

    // Choosing a kind and typing nothing asks for every element of that kind,
    // which is how "show me only the entities" is asked for.
    kind_ = new QComboBox(this);
    kind_->setObjectName("searchKind");
    kind_->setToolTip("Look among one kind of element, or among all of them. "
                      "Choosing a kind and typing nothing shows every element of that kind.");
    for (const auto& entry : kinds()) kind_->addItem(QString::fromUtf8(entry.label),
                                                     static_cast<int>(entry.kind));
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
    // nobody can read is what made these two look like rival switches.
    auto* keeping_heading = menu->addAction("What to keep");
    keeping_heading->setObjectName("searchKeepHeading");
    keeping_heading->setEnabled(false);
    auto* keeping = new QActionGroup(menu);
    keeping->setExclusive(true);
    auto* only_matches = menu->addAction("Only what matches");
    only_matches->setObjectName("searchKeepMatches");
    only_matches->setCheckable(true);
    only_matches->setChecked(true);
    only_matches->setActionGroup(keeping);
    only_matches->setToolTip("Keep the elements the search found, and nothing else.");
    relatives_ = menu->addAction("What matches, and what it touches");
    relatives_->setObjectName("searchRelatives");
    relatives_->setCheckable(true);
    relatives_->setActionGroup(keeping);
    relatives_->setToolTip("Also keep what a match belongs to and what it is joined to: its attributes, the "
                           "relationships and hierarchies it takes part in, and the far side of those.");

    menu->addSeparator();
    auto* rest_heading = menu->addAction("What to do with the rest");
    rest_heading->setObjectName("searchRestHeading");
    rest_heading->setEnabled(false);
    auto* becoming = new QActionGroup(menu);
    becoming->setExclusive(true);
    auto* fade_rest = menu->addAction("Fade it");
    fade_rest->setObjectName("searchFadeRest");
    fade_rest->setCheckable(true);
    fade_rest->setChecked(true);
    fade_rest->setActionGroup(becoming);
    fade_rest->setToolTip("Leave the rest of the diagram faintly in place, so a match is seen where it sits.");
    hide_rest_ = menu->addAction("Hide it");
    hide_rest_->setObjectName("searchHideRest");
    hide_rest_->setCheckable(true);
    hide_rest_->setActionGroup(becoming);
    hide_rest_->setToolTip("Take the rest of the diagram away instead. A line goes with whichever of its "
                           "ends goes, so nothing is left hanging.");
    settings_->setMenu(menu);
    layout->addWidget(settings_);

    count_ = new QLabel(this);
    count_->setObjectName("searchCount");
    count_->setMinimumWidth(80);
    layout->addWidget(count_);

    auto* close = new QToolButton(this);
    close->setObjectName("searchClose");
    close->setText("✕");
    close->setToolTip("Close the search and put the whole diagram back.\tEsc");
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
