// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/licences.hpp"

#include <QDir>
#include <QFileInfo>

// ERDFLOW_COPYRIGHT is the copyright line of LICENSE, given by CMakeLists.txt.

namespace erdflow::desktop
{
    QString installed_licences_folder(const QString &application_folder)
    {
        const QDir here(application_folder);
        for (const char *place : {"../Resources/Licenses", "../licenses", "../share/doc/erdflow"})
        {
            const QFileInfo licence(here.filePath(QString::fromLatin1(place) + "/LICENSE"));
            if (licence.isFile())
                return QDir::cleanPath(licence.absolutePath());
        }
        return {};
    }

    QString licences_text(const QString &erdflow_version, const QString &qt_version, const QString &folder)
    {
        QString text = QString("ERDFlow %1 is free software under the MIT licence.\n%2\n\n")
                           .arg(erdflow_version, QString::fromUtf8(ERDFLOW_COPYRIGHT));
        text += QString("It uses Qt %1, from The Qt Company Ltd. and other contributors, under the GNU "
                        "Lesser General Public License version 3. ERDFlow links to Qt dynamically, and "
                        "nothing in its licence limits your rights under the LGPL: you may replace the "
                        "Qt libraries with modified versions of your own.\n\n")
                    .arg(qt_version);
        text += "Its icons include Lucide and Feather icons under the ISC and MIT licences.\n\n";
        text += "It checks the signature on its updates with Monocypher, dedicated to the public domain "
                "under CC0-1.0.\n\n";
        if (folder.isEmpty())
            text += "This copy of ERDFlow was not installed from a package, so the licence files are not "
                    "beside it. They are in ERDFlow's source, under packaging/notices.";
        else
            text += QString("The licence texts, the notices of every component in this package, where to get "
                            "the source code of Qt and how to replace it are in:\n%1")
                        .arg(QDir::toNativeSeparators(folder));
        return text;
    }
}
