// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include <QString>

namespace erdflow::desktop
{
    // The folder an installed package keeps ERDFlow's licence and notice files
    // in, found from the folder the program runs from: Contents/Resources/
    // Licenses in ERDFlow.app, licenses beside bin on Windows, share/doc/erdflow
    // beside bin in the Linux AppImage (packaging/packaging.cmake). Empty for a
    // build that was not installed from a package.
    [[nodiscard]] QString installed_licences_folder(const QString &application_folder);

    // What Help -> Licences says: ERDFlow's own licence, the Qt it uses and the
    // terms it uses Qt under, and where the licence files are.
    [[nodiscard]] QString licences_text(const QString &erdflow_version, const QString &qt_version,
                                        const QString &folder);
}
