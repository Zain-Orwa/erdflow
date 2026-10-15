// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "dr_icon_picker.hpp"

#include "dr_icons.hpp"

#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace erdflow::desktop {
namespace {

// Four to a row, so the seventeen make four full rows and a fifth holding
// the last (Zain, 2026-10-15), with nothing to scroll.
constexpr int columns = 4;
// Large enough to tell a family from a person and a desk from a seat; the
// pictures are detailed and lose their point drawn as small as a glyph.
constexpr int picture = 64;
// Wide enough for the longest name, "Identification Card", beneath its
// picture without being cut short.
constexpr QSize cell(132, 100);

} // namespace

DrIconPicker::DrIconPicker(QWidget* parent) : QDialog(parent) {
    setObjectName("drIconPicker");
    setWindowTitle("Dr.icons");
    // A tool window that does not come forward when it opens, so the diagram
    // keeps the keyboard and its commands while icons are being placed.
    setWindowFlags(Qt::Tool);
    setAttribute(Qt::WA_ShowWithoutActivating);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    grid_host_ = new QWidget(this);
    auto* grid = new QGridLayout(grid_host_);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(4);
    int index = 0;
    for (const auto& icon : dr_icons()) {
        const auto label = QString::fromUtf8(icon.label);
        auto* button = new QToolButton(grid_host_);
        button->setObjectName(QStringLiteral("drIcon_%1").arg(QLatin1String(icon.key)));
        button->setIcon(QIcon(dr_icon_resource(icon)));
        button->setIconSize(QSize(picture, picture));
        button->setText(label);
        button->setToolTip(label);
        button->setAccessibleName(label);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setFixedSize(cell);
        button->setAutoRaise(true);
        // Refusing focus keeps the keyboard with the diagram.
        button->setFocusPolicy(Qt::NoFocus);
        connect(button, &QToolButton::clicked, this, [this, key = QString::fromLatin1(icon.key)] {
            if (on_chosen) on_chosen(key);
        });
        grid->addWidget(button, index / columns, index % columns);
        ++index;
    }
    layout->addWidget(grid_host_);

    destination_ = new QLabel(QStringLiteral("Each goes as a picture on the diagram, or on the schema while it is in "
                                             "front, where the pointer last was. Drag it beside what it illustrates."),
                              this);
    destination_->setObjectName("drIconDestination");
    destination_->setWordWrap(true);
    layout->addWidget(destination_);
    layout->addStretch(1);
}

void DrIconPicker::set_theme(const Theme& colors) {
    grid_host_->setStyleSheet(QStringLiteral(
        "QToolButton { border: 1px solid transparent; border-radius: 4px; background: transparent;"
        " color: %1; padding: 2px; margin: 0px; }"
        "QToolButton:hover { background: %2; border-color: %3; }"
        "QToolButton:pressed { background: %4; }")
        .arg(colors.text.name(), hover_surface(colors).name(), colors.border.name(), colors.accent.name()));
    destination_->setStyleSheet(QStringLiteral("QLabel#drIconDestination { color: %1; font-size: 11px; }")
                                    .arg(colors.muted.name()));
}

} // namespace erdflow::desktop
