// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "dr_icons.hpp"

#include <QFile>

namespace erdflow::desktop {

const std::array<DrIcon, 16>& dr_icons() {
    // The keys and labels are Zain's (2026-10-14) and are not to be renamed:
    // the key is also the file under assets/dr-icons.
    static const std::array<DrIcon, 16> icons{{
        {"student", "Student"},
        {"course", "Course"},
        {"family", "Family"},
        {"identification_card", "Identification Card"},
        {"order", "Order"},
        {"product", "Product"},
        {"book", "Book"},
        {"project", "Project"},
        {"employee", "Employee"},
        {"person", "Person"},
        {"seat", "Seat"},
        {"task", "Task"},
        {"car", "Car"},
        {"mobile_line", "Mobile Line"},
        {"phone_ext", "Phone Ext."},
        {"desk", "Desk"},
    }};
    return icons;
}

std::optional<DrIcon> dr_icon(const QString& key) {
    for (const auto& icon : dr_icons())
        if (key == QLatin1String(icon.key)) return icon;
    return std::nullopt;
}

QString dr_icon_resource(const DrIcon& icon) {
    return QStringLiteral(":/erdflow/dr-icons/%1.png").arg(QLatin1String(icon.key));
}

QByteArray dr_icon_bytes(const DrIcon& icon) {
    QFile file(dr_icon_resource(icon));
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

} // namespace erdflow::desktop
