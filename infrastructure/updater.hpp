// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include <QByteArray>
#include <QLockFile>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

namespace erdflow::infrastructure {

// Whether an installed ERDFlow keeps itself up to date (ADR-024): on unless
// somebody has turned it off in Settings.
inline constexpr char automatic_updates_key[] = "updates/automatic";
bool automatic_updates_on();

// The key every release's SHA256SUMS is signed with, the public half of the
// one the release workflow holds (secret ERDFLOW_UPDATE_SIGNING_KEY).
QByteArray release_public_key();

// What the packages for the computer this is running on are called, as their
// file names say it: windows-x86_64, macos-arm64, macos-x86_64, linux-x86_64.
// Empty where no package is made.
QString package_platform();

// Whether version `offered` comes after `running`, both written 1.2.3.
bool is_newer(const QString& offered, const QString& running);

// Whether `signature` is the Ed25519 signature of `message` by `public_key`.
bool signed_by(const QByteArray& message, const QByteArray& signature, const QByteArray& public_key);

// A newer ERDFlow for this computer, as a release's SHA256SUMS offers it.
struct UpdateOffer {
    QString version;
    QString file;
    QByteArray sha256; // hexadecimal, as SHA256SUMS writes it
};

// The release's offer, once its SHA256SUMS is proven to be signed by
// `public_key`: nothing where the signature fails, where no package is for
// `platform`, or where the version is not newer than `running`.
std::optional<UpdateOffer> read_offer(const QByteArray& sums, const QByteArray& signature,
                                      const QByteArray& public_key, const QString& platform,
                                      const QString& running);

// What the Windows installer is run with to put an update in place with
// nothing shown. Asked to open ERDFlow again afterwards (ADR-025), it is told
// so in a word of ERDFlow's own, which the installer answers by starting the
// version it has just installed; without the word it starts nothing, as before.
QStringList silent_install_arguments(bool open_again);

// Looks for a newer release while ERDFlow is open and makes it ready, then puts
// it in place when ERDFlow closes, so the next start is the new version. Only
// a copy installed from a package does this, and only while automatic updates
// are on; anything that goes wrong is let go without a word, and the copy that
// is running is never touched until it has closed.
class Updater {
public:
    explicit Updater(QString running_version);
    ~Updater();
    Updater(const Updater&) = delete;
    Updater& operator=(const Updater&) = delete;

    // Starts looking in the background; returns at once.
    void look();
    // Called as ERDFlow quits: puts a ready update in place.
    void put_in_place();

    // Update and Restart (ADR-025; Zain, 2026-11-12). The update is still
    // fetched silently and still put in place as ERDFlow closes; these let the
    // window say that one is ready, and close and open ERDFlow again on
    // request instead of waiting for it to be closed.
    //
    // The version fetched and waiting to be put in place, or nothing: where
    // none is waiting, in a copy that does not update itself, and while
    // automatic updates are off.
    [[nodiscard]] QString waiting() const;
    // Has `told` called each time a look this ERDFlow made for an update has
    // ended, whatever came of it, so that whatever shows what is waiting can
    // ask again.
    void on_looked(std::function<void()> told);
    // Whether another ERDFlow is open. An update waits for the last one to
    // close, so it cannot be put in place from this one while there is.
    [[nodiscard]] bool others_open();
    // Whether the next closing is to open ERDFlow again once the update is in
    // place. Off unless asked, and asked only for the closing that Update and
    // Restart makes; taken back if that closing is refused.
    void open_again(bool again);

    // The same, told where it is installed and where it keeps what it
    // fetches rather than finding them out. For tests, which have no
    // installed ERDFlow to update.
    Updater(QString running_version, QString where_installed, QString keeps_in);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace erdflow::infrastructure
