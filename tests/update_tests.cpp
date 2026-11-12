// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "infrastructure/updater.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

#if defined(Q_OS_MACOS)
#include <unistd.h>
#endif

using namespace erdflow::infrastructure;

namespace {
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(std::string(__func__) + ":" + std::to_string(__LINE__) + ": " #condition); } while (false)

// A SHA256SUMS as a release publishes it, signed with a key made for these
// tests alone (2026-10-23); never the release key.
const QByteArray sums =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa  ERDFlow-0.3.0-linux-x86_64.AppImage\n"
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb  ERDFlow-0.3.0-macos-arm64.dmg\n"
    "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc  ERDFlow-0.3.0-macos-x86_64.dmg\n"
    "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd  ERDFlow-0.3.0-windows-x86_64-setup.exe\n";
const QByteArray test_key = QByteArray::fromHex("50bcf45adc4163a04f538f0b086e025e7d8d96b2c6515b438ed68bdd522dae6d");
const QByteArray test_signature = QByteArray::fromHex(
    "20b80f1ac77e56a1d21fe303a6018cbcc0c2d042fea8097e83af6cf3fb8bac1edddf2aefaac164e3d0473a0b5778e38b2faad87e77e140e61356bd2399876c01");

void ed25519_matches_the_standard() {
    // RFC 8032, section 7.1, test 1: the empty message.
    const auto key = QByteArray::fromHex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    const auto signature = QByteArray::fromHex(
        "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
    CHECK(signed_by(QByteArray(), signature, key));
    CHECK(!signed_by(QByteArray("x"), signature, key));
    CHECK(!signed_by(QByteArray(), signature.left(63), key));
    CHECK(!signed_by(QByteArray(), signature, key.left(31)));
}

void the_release_key_is_a_key() {
    CHECK(release_public_key().size() == 32);
    CHECK(!signed_by(sums, test_signature, release_public_key()));
}

void versions_compare_by_number() {
    CHECK(is_newer("0.3.0", "0.2.0"));
    CHECK(is_newer("0.10.0", "0.9.1"));
    CHECK(is_newer("1.0.0", "0.99.99"));
    CHECK(!is_newer("0.2.0", "0.2.0"));
    CHECK(!is_newer("0.1.9", "0.2.0"));
    CHECK(!is_newer("", "0.2.0"));
    CHECK(!is_newer("0.3.0", ""));
}

void a_signed_newer_release_is_offered_for_this_computer() {
    const auto mac = read_offer(sums, test_signature, test_key, "macos-arm64", "0.2.0");
    CHECK(mac && mac->version == "0.3.0" && mac->file == "ERDFlow-0.3.0-macos-arm64.dmg");
    CHECK(mac->sha256 == QByteArray(64, 'b'));
    const auto windows = read_offer(sums, test_signature, test_key, "windows-x86_64", "0.2.0");
    CHECK(windows && windows->file == "ERDFlow-0.3.0-windows-x86_64-setup.exe" && windows->sha256 == QByteArray(64, 'd'));
    const auto linux_image = read_offer(sums, test_signature, test_key, "linux-x86_64", "0.2.0");
    CHECK(linux_image && linux_image->file == "ERDFlow-0.3.0-linux-x86_64.AppImage");
    CHECK(read_offer(sums, test_signature, test_key, "macos-x86_64", "0.2.0")->sha256 == QByteArray(64, 'c'));
}

void nothing_is_offered_unless_signed_newer_and_made_for_here() {
    // Not newer than what is running.
    CHECK(!read_offer(sums, test_signature, test_key, "macos-arm64", "0.3.0"));
    CHECK(!read_offer(sums, test_signature, test_key, "macos-arm64", "0.4.0"));
    // No package for this computer.
    CHECK(!read_offer(sums, test_signature, test_key, "", "0.2.0"));
    CHECK(!read_offer(sums, test_signature, test_key, "linux-arm64", "0.2.0"));
    // A signature that does not hold: another key, a changed list, none at all.
    CHECK(!read_offer(sums, test_signature, release_public_key(), "macos-arm64", "0.2.0"));
    auto changed = sums;
    changed[0] = 'e';
    CHECK(!read_offer(changed, test_signature, test_key, "macos-arm64", "0.2.0"));
    CHECK(!read_offer(sums, QByteArray(), test_key, "macos-arm64", "0.2.0"));
}

void names_outside_the_package_shape_are_never_taken() {
    // Signed with the test key, so it is the names that are refused: a path
    // out of the update's folder, a name that is not a package, a hash not
    // written as SHA256SUMS writes one.
    const QByteArray list =
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb  ERDFlow-0.3.0-macos-arm64/../../evil.dmg\n"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb  ../ERDFlow-0.3.0-macos-arm64.dmg\n"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb  ERDFlow-0.3.0-macos-arm64.dmg.sh\n"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB  ERDFlow-0.3.0-macos-arm64.dmg\n";
    const auto signature = QByteArray::fromHex(
        "4eb5546d15fa31e5cfacde1becf82e616dc49ea47514a0be00c364d5c7d85dce664a8fa65ca832892f5d74dabfe1bfa5535be67aaf8ddcbc8da1a8f922c00805");
    CHECK(signed_by(list, signature, test_key));
    CHECK(!read_offer(list, signature, test_key, "macos-arm64", "0.2.0"));
}

void this_computer_has_a_package_name() {
#if defined(Q_PROCESSOR_X86_64) || defined(Q_OS_MACOS)
    CHECK(!package_platform().isEmpty());
#endif
    CHECK(!package_platform().contains('/'));
}

// Update and Restart (ADR-025; 2026-11-12). What is waiting is remembered in
// the settings, under the two names ADR-024 gives them. The settings these
// tests use are a file in a folder of the run's own (see main), so nothing an
// ERDFlow installed on this computer remembers is read or disturbed.
QString settings_folder;

void stage(const QString& version, const QString& path) {
    QSettings settings;
    settings.setValue("updates/stagedVersion", version);
    settings.setValue("updates/stagedPath", path);
}

void forget() {
    QSettings settings;
    settings.remove("updates/stagedVersion");
    settings.remove("updates/stagedPath");
    settings.remove(automatic_updates_key);
}

void write(const QString& path, const QByteArray& words, bool program = false) {
    CHECK(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(words) == words.size());
    file.close();
    if (program)
        CHECK(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
}

QByteArray read(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Waits for what another process is doing, for no longer than it could take.
template <class Done>
bool before_long(Done done) {
    QElapsedTimer clock;
    clock.start();
    while (!done()) {
        if (clock.elapsed() > 30000) return false;
        QThread::msleep(25);
    }
    return true;
}

void what_is_waiting_is_said_only_while_it_can_be_put_in_place() {
    QTemporaryDir place;
    CHECK(place.isValid());
    const auto keeps = place.filePath("update");
    const auto fetched = keeps + "/ERDFlow-9.9.9-test.dmg";
    write(fetched, "a package");
    forget();
    const Updater installed("9.9.8", place.filePath("ERDFlow.app"), keeps);
    CHECK(installed.waiting().isEmpty());
    stage("9.9.9", fetched);
    CHECK(installed.waiting() == "9.9.9");
    // Not newer than the ERDFlow that is running.
    stage("9.9.8", fetched);
    CHECK(installed.waiting().isEmpty());
    stage("9.9.7", fetched);
    CHECK(installed.waiting().isEmpty());
    // Its files gone.
    stage("9.9.9", keeps + "/gone.dmg");
    CHECK(installed.waiting().isEmpty());
    // Automatic updates turned off, and on again.
    stage("9.9.9", fetched);
    QSettings().setValue(automatic_updates_key, false);
    CHECK(installed.waiting().isEmpty());
    QSettings().setValue(automatic_updates_key, true);
    CHECK(installed.waiting() == "9.9.9");
    // A copy that was not installed has none, whatever is remembered.
    const Updater built("9.9.8", QString(), keeps);
    CHECK(built.waiting().isEmpty());
    forget();
}

void a_look_that_finds_one_waiting_says_so_and_fetches_nothing() {
    // Only a computer a package is made for, with a curl to fetch it, looks.
    if (package_platform().isEmpty()) return;
#if defined(Q_OS_WIN)
    if (!QFileInfo::exists(qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows")) + "\\System32\\curl.exe"))
        return;
#elif !defined(Q_OS_MACOS)
    if (QStandardPaths::findExecutable(QStringLiteral("curl")).isEmpty()) return;
#endif
    QTemporaryDir place;
    CHECK(place.isValid());
    const auto keeps = place.filePath("update");
    const auto fetched = keeps + "/ERDFlow-999.0.1-test.dmg";
    write(fetched, "a package");
    forget();
    stage("999.0.1", fetched);
    Updater updater("999.0.0", place.filePath("ERDFlow.app"), keeps);
    int told = 0;
    QString waiting;
    updater.on_looked([&] {
        ++told;
        waiting = updater.waiting();
    });
    CHECK(told == 0);
    // One already waiting is kept as it is, so the look ends there and then.
    updater.look();
    CHECK(told == 1 && waiting == "999.0.1");
    CHECK(read(fetched) == "a package");
    forget();
}

void another_erdflow_open_is_noticed() {
    QTemporaryDir place;
    CHECK(place.isValid());
    const auto keeps = place.filePath("update");
    CHECK(QDir().mkpath(keeps));
    Updater one("9.9.8", place.filePath("ERDFlow.app"), keeps);
    CHECK(!one.others_open());
    // Every ERDFlow that is open holds a lock of its own there.
    QLockFile another(keeps + "/running-1.lock");
    another.setStaleLockTime(0);
    CHECK(another.tryLock(0));
    CHECK(one.others_open());
    another.unlock();
    CHECK(!one.others_open());
}

void the_installer_is_told_to_open_erdflow_again_only_when_asked() {
    // As ERDFlow 0.3.0 and 0.3.1 run it, and as closing still does.
    const QStringList quietly{"/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CLOSEAPPLICATIONS",
                              "/NORESTARTAPPLICATIONS"};
    CHECK(silent_install_arguments(false) == quietly);
    CHECK(silent_install_arguments(true) == quietly + QStringList{"/RELAUNCH=1"});
}

#if defined(Q_OS_MACOS)
// An ERDFlow that is closing, as a process of its own: the update is put in
// place only once the process that asked for it has gone.
int close_as_erdflow(const QStringList& told) {
    Updater closing("9.9.8", told[3], told[4]);
    closing.open_again(told[5] == "again");
    closing.put_in_place();
    return 0;
}

// What closing an installed ERDFlow comes to, in a folder laid out as an
// installed copy and its fetched update are, with an `open` of the test's
// own, first on the closing ERDFlow's PATH, which writes down what it was
// asked to open in place of opening it.
struct Closed {
    QByteArray application; // what the installed application is afterwards
    bool fetched_kept = false;
    QByteArray opened;      // what `open` was asked, or nothing
    bool left_beside = false;
};

Closed close_an_installed_erdflow(bool again, bool swap_can_be_made) {
    QTemporaryDir place;
    CHECK(place.isValid());
    const auto applications = place.filePath("Applications");
    const auto installed = applications + "/ERDFlow.app";
    const auto keeps = place.filePath("update");
    const auto fetched = keeps + "/ERDFlow.app";
    const auto program = QStringLiteral("/Contents/MacOS/ERDFlow");
    write(installed + program, "old", true);
    write(fetched + program, "new", true);
    const auto opened = place.filePath("opened");
    write(place.filePath("tools/open"), "#!/bin/sh\nprintf '%s\\n' \"$@\" > \"" + opened.toUtf8() + "\"\n", true);
    forget();
    stage("9.9.9", fetched);
    QSettings().sync();
    // A folder that cannot be written to is one the new application cannot
    // be copied into, so the swap is never begun.
    if (!swap_can_be_made)
        CHECK(QFile::setPermissions(applications, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    QProcess closing;
    auto surroundings = QProcessEnvironment::systemEnvironment();
    surroundings.insert("PATH", place.filePath("tools") + ":" + surroundings.value("PATH"));
    closing.setProcessEnvironment(surroundings);
    closing.start(QCoreApplication::applicationFilePath(),
                  {"--close-as-erdflow", settings_folder, installed, keeps, again ? "again" : "closed"});
    CHECK(closing.waitForFinished(30000) && closing.exitStatus() == QProcess::NormalExit && closing.exitCode() == 0);
    // The shell that makes the swap outlives the ERDFlow that started it. Its
    // last act is opening ERDFlow where that was asked -- waited for until
    // what `open` was asked is written whole -- and otherwise putting away
    // the application it fetched.
    const QByteArray asked = "-n\n" + installed.toUtf8() + "\n";
    const bool ended = before_long([&] { return again ? read(opened) == asked : !QFileInfo::exists(fetched); });
    if (!again) QThread::msleep(300); // long enough for an `open` nobody asked for to have been seen
    QFile::setPermissions(applications, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    CHECK(ended);
    Closed closed;
    closed.application = read(installed + program);
    closed.fetched_kept = QFileInfo::exists(fetched);
    closed.opened = read(opened);
    closed.left_beside = QFileInfo::exists(installed + ".new") || QFileInfo::exists(installed + ".old");
    CHECK(closed.opened.isEmpty() || closed.opened == asked);
    forget();
    return closed;
}

void closing_puts_the_update_in_place_and_opens_erdflow_again_only_when_asked() {
    // Closed as it always has been: the new application where the old one
    // was, what was fetched cleared away, and nothing opened.
    const auto quietly = close_an_installed_erdflow(false, true);
    CHECK(quietly.application == "new" && !quietly.fetched_kept && !quietly.left_beside && quietly.opened.isEmpty());
    // Update and Restart: exactly the same, and then ERDFlow is opened.
    const auto again = close_an_installed_erdflow(true, true);
    CHECK(again.application == "new" && !again.fetched_kept && !again.left_beside && !again.opened.isEmpty());
    // Where the new one cannot be put in place, the old one is left as it
    // was and is what is opened: asked to restart, ERDFlow always comes back.
    if (::geteuid() != 0) {
        const auto kept = close_an_installed_erdflow(true, false);
        CHECK(kept.application == "old" && kept.fetched_kept && !kept.left_beside && !kept.opened.isEmpty());
    }
}
#elif !defined(Q_OS_WIN)
void closing_puts_the_new_appimage_in_place_and_runs_it_only_when_asked() {
    for (const bool again : {false, true}) {
        QTemporaryDir place;
        CHECK(place.isValid());
        const auto installed = place.filePath("Applications/ERDFlow.AppImage");
        const auto keeps = place.filePath("update");
        const auto fetched = keeps + "/ERDFlow-9.9.9-linux-x86_64.AppImage";
        const auto ran = place.filePath("ran");
        // A program in place of each AppImage, which writes down which it is.
        const auto says = [&](const char* which) {
            return QByteArray("#!/bin/sh\necho ") + which + " > \"" + ran.toUtf8() + "\"\n";
        };
        write(installed, says("old"), true);
        write(fetched, says("new"), true);
        forget();
        stage("9.9.9", fetched);
        Updater closing("9.9.8", installed, keeps);
        closing.open_again(again);
        closing.put_in_place();
        // As it always has been: the new one where the old one was, and what
        // was fetched cleared away.
        CHECK(read(installed) == says("new"));
        CHECK(!QFileInfo::exists(fetched) && QSettings().value("updates/stagedVersion").toString().isEmpty());
        if (again) {
            // Update and Restart: and then it is run.
            CHECK(before_long([&] { return read(ran) == "new\n"; }));
        } else {
            QThread::msleep(300); // long enough for a run nobody asked for to have been seen
            CHECK(!QFileInfo::exists(ran));
        }
    }
    forget();
}
#endif

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("ERDFlowTests");
    QCoreApplication::setApplicationName("ERDFlowTests-Update");
    // The settings are a file in a folder of this run's own, so that what is
    // staged here is never an installed ERDFlow's, and nothing is left behind.
    // A closing ERDFlow started by a test is told the folder of the run that
    // started it.
    const auto told = QCoreApplication::arguments();
    const bool closing = told.size() == 6 && told[1] == "--close-as-erdflow";
    QTemporaryDir own;
    settings_folder = closing ? told[2] : own.path();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_folder);
#if defined(Q_OS_MACOS)
    if (closing) return close_as_erdflow(told);
#endif
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"Ed25519 matches RFC 8032", ed25519_matches_the_standard},
        {"the release key is a key", the_release_key_is_a_key},
        {"versions compare by number", versions_compare_by_number},
        {"a signed, newer release is offered for this computer", a_signed_newer_release_is_offered_for_this_computer},
        {"nothing is offered unless signed, newer and made for here", nothing_is_offered_unless_signed_newer_and_made_for_here},
        {"names outside the package shape are never taken", names_outside_the_package_shape_are_never_taken},
        {"this computer has a package name", this_computer_has_a_package_name},
        {"what is waiting is said only while it can be put in place",
         what_is_waiting_is_said_only_while_it_can_be_put_in_place},
        {"a look that finds one waiting says so, and fetches nothing",
         a_look_that_finds_one_waiting_says_so_and_fetches_nothing},
        {"another ERDFlow open is noticed", another_erdflow_open_is_noticed},
        {"the installer is told to open ERDFlow again only when asked",
         the_installer_is_told_to_open_erdflow_again_only_when_asked},
#if defined(Q_OS_MACOS)
        {"closing puts the update in place, and opens ERDFlow again only when asked",
         closing_puts_the_update_in_place_and_opens_erdflow_again_only_when_asked},
#elif !defined(Q_OS_WIN)
        {"closing puts the new AppImage in place, and runs it only when asked",
         closing_puts_the_new_appimage_in_place_and_runs_it_only_when_asked},
#endif
    };
    std::size_t failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    return failures == 0 ? 0 : 1;
}
