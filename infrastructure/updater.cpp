// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "infrastructure/updater.hpp"

#include "third_party/monocypher/monocypher-ed25519.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QVersionNumber>

#include <filesystem>
#include <utility>

namespace erdflow::infrastructure {
namespace {

// Where the newest release's files are, whatever it is numbered.
constexpr char latest_release[] = "https://github.com/Zain-Orwa/erdflow/releases/latest/download/";
// The public half of the release signing key (ADR-024), made 2026-10-23.
constexpr char release_key_hex[] = "c79d6b56602cd9a6c9967948c7c9a7022bff3199fa8e994dd1a1ba40f465cf11";
// An update fetched and ready, waiting for ERDFlow to close.
constexpr char staged_version_key[] = "updates/stagedVersion";
constexpr char staged_path_key[] = "updates/stagedPath";

// The system's own curl fetches the files, so that ERDFlow carries no
// networking or TLS library of its own: macOS and Windows 10 and later always
// have it, and most Linux desktops do. Without it, nothing is fetched.
QString curl_program() {
#if defined(Q_OS_WIN)
    const auto system = qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows")) + "\\System32\\curl.exe";
    return QFileInfo::exists(system) ? system : QString();
#elif defined(Q_OS_MACOS)
    return QStringLiteral("/usr/bin/curl");
#else
    return QStandardPaths::findExecutable(QStringLiteral("curl"));
#endif
}

// What an update replaces: the application bundle on macOS, the AppImage on
// Linux, and the folder the installer chose on Windows. Empty for a copy that
// was not installed from a package -- one built from source, or one started
// from the disk image or from a folder it may not write to -- which is never
// updated.
#if defined(ERDFLOW_PACKAGED_BUILD)
constexpr bool packaged = true;
#else
constexpr bool packaged = false;
#endif

QString installed_at() {
    if constexpr (!packaged) {
        return {};
    } else {
        const QDir program(QCoreApplication::applicationDirPath());
#if defined(Q_OS_MACOS)
        const auto bundle = QDir(program.absoluteFilePath(QStringLiteral("../.."))).canonicalPath();
        if (!bundle.endsWith(QStringLiteral(".app")) || bundle.contains(QStringLiteral("/AppTranslocation/")))
            return {};
        return QFileInfo(QFileInfo(bundle).absolutePath()).isWritable() ? bundle : QString();
#elif defined(Q_OS_WIN)
        const auto root = QDir(program.absoluteFilePath(QStringLiteral(".."))).canonicalPath();
        return QFileInfo::exists(root + "/unins000.exe") ? root : QString();
#else
        const QFileInfo image(qEnvironmentVariable("APPIMAGE"));
        if (!image.isFile() || !QFileInfo(image.absolutePath()).isWritable()) return {};
        return image.absoluteFilePath();
#endif
    }
}

QByteArray file_sha256(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return {};
    return hash.result().toHex();
}

QByteArray read_all(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Removes what an update left in its own folder, and nothing outside it.
void remove_inside(const QString& folder, const QString& path) {
    if (path.isEmpty() || !QFileInfo(path).absoluteFilePath().startsWith(folder + '/')) return;
    if (QFileInfo(path).isDir()) QDir(path).removeRecursively();
    else QFile::remove(path);
}

} // namespace

bool automatic_updates_on() { return QSettings().value(automatic_updates_key, true).toBool(); }

QByteArray release_public_key() { return QByteArray::fromHex(release_key_hex); }

QString package_platform() {
#if defined(Q_OS_WIN) && defined(Q_PROCESSOR_X86_64)
    return QStringLiteral("windows-x86_64");
#elif defined(Q_OS_MACOS) && defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("macos-arm64");
#elif defined(Q_OS_MACOS) && defined(Q_PROCESSOR_X86_64)
    return QStringLiteral("macos-x86_64");
#elif defined(Q_OS_LINUX) && defined(Q_PROCESSOR_X86_64)
    return QStringLiteral("linux-x86_64");
#else
    return {};
#endif
}

bool is_newer(const QString& offered, const QString& running) {
    const auto a = QVersionNumber::fromString(offered);
    const auto b = QVersionNumber::fromString(running);
    return !a.isNull() && !b.isNull() && QVersionNumber::compare(a, b) > 0;
}

bool signed_by(const QByteArray& message, const QByteArray& signature, const QByteArray& public_key) {
    if (signature.size() != 64 || public_key.size() != 32) return false;
    return crypto_ed25519_check(reinterpret_cast<const uint8_t*>(signature.constData()),
                                reinterpret_cast<const uint8_t*>(public_key.constData()),
                                reinterpret_cast<const uint8_t*>(message.constData()),
                                static_cast<size_t>(message.size())) == 0;
}

std::optional<UpdateOffer> read_offer(const QByteArray& sums, const QByteArray& signature,
                                      const QByteArray& public_key, const QString& platform,
                                      const QString& running) {
    if (platform.isEmpty() || !signed_by(sums, signature, public_key)) return std::nullopt;
    // A line of SHA256SUMS: the hash, two spaces (or a space and an asterisk)
    // and the package's name, which says its version and what it is for. The
    // name becomes a file in the update's folder, so it is held to the shape
    // ERDFlow's own packages are named in and nothing else.
    static const QRegularExpression line(
        QStringLiteral(R"(^([0-9a-f]{64}) [ *](ERDFlow-(\d+\.\d+\.\d+)-([A-Za-z0-9_-]+)\.(?:dmg|exe|AppImage))$)"));
    for (const auto& text : QString::fromUtf8(sums).split('\n', Qt::SkipEmptyParts)) {
        const auto match = line.match(text.trimmed());
        if (!match.hasMatch()) continue;
        const auto made_for = match.captured(4);
        if (made_for != platform && made_for != platform + "-setup") continue;
        if (!is_newer(match.captured(3), running)) return std::nullopt;
        return UpdateOffer{match.captured(3), match.captured(2), match.captured(1).toLatin1()};
    }
    return std::nullopt;
}

struct Updater::Impl {
    QString running;
    QString installed = installed_at();
    QString folder = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/update";
    QString curl = curl_program();
    // Held for as long as this ERDFlow is open, so that the last one to close
    // is the one that puts an update in place.
    QLockFile here{folder + "/running-" + QString::number(QCoreApplication::applicationPid()) + ".lock"};
    // Held while fetching, so that two ERDFlows open together fetch only once.
    QLockFile fetching{folder + "/fetching.lock"};
    // The programs run in the background belong to this, and are stopped with it.
    QObject owner;

    explicit Impl(QString version) : running(std::move(version)) {
        // A lock is stale only when the ERDFlow holding it has gone, however
        // long ago it was taken.
        here.setStaleLockTime(0);
        fetching.setStaleLockTime(0);
    }

    // Runs a program in the background and says afterwards whether it worked.
    void run(const QString& program, const QStringList& arguments, std::function<void(bool)> then) {
        auto* process = new QProcess(&owner);
        process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
        QObject::connect(process, &QProcess::finished, &owner,
                         [process, then](int code, QProcess::ExitStatus status) {
                             process->deleteLater();
                             then(status == QProcess::NormalExit && code == 0);
                         });
        QObject::connect(process, &QProcess::errorOccurred, &owner, [process, then](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart) return;
            process->deleteLater();
            then(false);
        });
        process->start(program, arguments);
    }

    void fetch(const QString& url, const QString& into, int seconds, std::function<void(bool)> then) {
        run(curl,
            {QStringLiteral("--fail"), QStringLiteral("--silent"), QStringLiteral("--location"),
             QStringLiteral("--proto"), QStringLiteral("=https"), QStringLiteral("--proto-redir"), QStringLiteral("=https"),
             QStringLiteral("--retry"), QStringLiteral("2"), QStringLiteral("--connect-timeout"), QStringLiteral("15"),
             QStringLiteral("--max-time"), QString::number(seconds), QStringLiteral("--output"), into, url},
            std::move(then));
    }

    void forget_staged() {
        QSettings settings;
        remove_inside(folder, settings.value(staged_path_key).toString());
        settings.remove(staged_version_key);
        settings.remove(staged_path_key);
    }

    void stage(const QString& version, const QString& path) {
        QSettings settings;
        settings.setValue(staged_version_key, version);
        settings.setValue(staged_path_key, path);
    }

    void done() { fetching.unlock(); }

    // Makes the fetched package ready to be put in place: on macOS the
    // application is taken out of the disk image now, so that closing ERDFlow
    // has only to swap it in.
    void prepare(const UpdateOffer& offer, const QString& package) {
#if defined(Q_OS_MACOS)
        const auto mount = folder + "/mount";
        const auto app = folder + "/ERDFlow.app";
        QDir().mkpath(mount);
        remove_inside(folder, app);
        run(QStringLiteral("/usr/bin/hdiutil"),
            {QStringLiteral("attach"), QStringLiteral("-nobrowse"), QStringLiteral("-readonly"),
             QStringLiteral("-noautoopen"), QStringLiteral("-quiet"), QStringLiteral("-mountpoint"), mount, package},
            [this, offer, package, mount, app](bool attached) {
                if (!attached) {
                    QFile::remove(package);
                    return done();
                }
                run(QStringLiteral("/usr/bin/ditto"), {mount + "/ERDFlow.app", app},
                    [this, offer, package, mount, app](bool copied) {
                        run(QStringLiteral("/usr/bin/hdiutil"), {QStringLiteral("detach"), QStringLiteral("-quiet"), mount},
                            [this, offer, package, app, copied](bool) {
                                QFile::remove(package);
                                if (copied && QFileInfo(app + "/Contents/MacOS/ERDFlow").isExecutable())
                                    stage(offer.version, app);
                                else
                                    remove_inside(folder, app);
                                done();
                            });
                    });
            });
#else
#if !defined(Q_OS_WIN)
        QFile::setPermissions(package, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                           QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther |
                                           QFileDevice::ExeOther);
#endif
        stage(offer.version, package);
        done();
#endif
    }

    // Whether another ERDFlow is still open; the last to close updates.
    bool others_open() {
        const auto mine = QFileInfo(here.fileName()).fileName();
        for (const auto& name : QDir(folder).entryList({QStringLiteral("running-*.lock")}, QDir::Files)) {
            if (name == mine) continue;
            QLockFile other(folder + "/" + name);
            other.setStaleLockTime(0);
            if (!other.tryLock(0)) return true;
            other.unlock(); // its ERDFlow had gone; the lock is cleared away
        }
        return false;
    }
};

Updater::Updater(QString running_version) : impl_(std::make_unique<Impl>(std::move(running_version))) {}

Updater::~Updater() = default;

void Updater::look() {
    auto& d = *impl_;
    if (d.installed.isEmpty() || d.curl.isEmpty() || package_platform().isEmpty()) return;
    if (!QDir().mkpath(d.folder)) return;
    d.here.tryLock(0);
    if (!automatic_updates_on()) {
        d.forget_staged();
        return;
    }
    if (!d.fetching.tryLock(0)) return;
    // An update already waiting stays ready; one this ERDFlow already is, or
    // whose files have gone, is cleared away before looking again.
    const QSettings settings;
    const auto waiting = settings.value(staged_version_key).toString();
    if (!waiting.isEmpty()) {
        if (is_newer(waiting, d.running) && QFileInfo::exists(settings.value(staged_path_key).toString()))
            return d.done();
        d.forget_staged();
    }
    const auto sums = d.folder + "/SHA256SUMS";
    const auto signature = sums + ".ed25519";
    d.fetch(QString(latest_release) + "SHA256SUMS", sums, 60, [&d, sums, signature](bool fetched) {
        if (!fetched) return d.done();
        d.fetch(QString(latest_release) + "SHA256SUMS.ed25519", signature, 60, [&d, sums, signature](bool signed_too) {
            const auto offer = signed_too ? read_offer(read_all(sums), read_all(signature), release_public_key(),
                                                       package_platform(), d.running)
                                          : std::nullopt;
            QFile::remove(sums);
            QFile::remove(signature);
            if (!offer) return d.done();
            const auto package = d.folder + "/" + offer->file;
            const auto partial = package + ".part";
            d.fetch(QString(latest_release) + offer->file, partial, 1800, [&d, offer = *offer, package, partial](bool got) {
                if (!got || file_sha256(partial) != offer.sha256) {
                    QFile::remove(partial);
                    return d.done();
                }
                QFile::remove(package);
                if (!QFile::rename(partial, package)) {
                    QFile::remove(partial);
                    return d.done();
                }
                d.prepare(offer, package);
            });
        });
    });
}

void Updater::put_in_place() {
    auto& d = *impl_;
    if (d.installed.isEmpty() || !automatic_updates_on()) return;
    const QSettings settings;
    const auto version = settings.value(staged_version_key).toString();
    const auto staged = settings.value(staged_path_key).toString();
    if (version.isEmpty() || !is_newer(version, d.running) || !QFileInfo::exists(staged) || d.others_open()) return;
#if defined(Q_OS_MACOS)
    // The bundle cannot be swapped while this ERDFlow is still running from
    // it, so a shell waits for it to have closed, copies the new one in beside
    // the old, and only then trades them; if anything fails the old one is put
    // back as it was.
    static const auto script = QStringLiteral(
        "while kill -0 \"$1\" 2>/dev/null; do sleep 0.2; done\n"
        "[ -n \"$2\" ] && [ -n \"$3\" ] || exit 1\n"
        "rm -rf \"$3.new\" \"$3.old\"\n"
        "ditto \"$2\" \"$3.new\" || { rm -rf \"$3.new\"; exit 1; }\n"
        "if mv \"$3\" \"$3.old\"; then\n"
        "  if mv \"$3.new\" \"$3\"; then rm -rf \"$3.old\" \"$2\"; else mv \"$3.old\" \"$3\"; rm -rf \"$3.new\"; fi\n"
        "else rm -rf \"$3.new\"; fi\n");
    QProcess::startDetached(QStringLiteral("/bin/sh"),
                            {QStringLiteral("-c"), script, QStringLiteral("erdflow-update"),
                             QString::number(QCoreApplication::applicationPid()), staged, d.installed});
#elif defined(Q_OS_WIN)
    // The installer, run with nothing shown, puts the new version where the
    // old one was installed and does not start it.
    QProcess::startDetached(staged, {QStringLiteral("/VERYSILENT"), QStringLiteral("/SUPPRESSMSGBOXES"),
                                     QStringLiteral("/NORESTART"), QStringLiteral("/CLOSEAPPLICATIONS"),
                                     QStringLiteral("/NORESTARTAPPLICATIONS")});
#else
    // A running AppImage may be replaced: it keeps running from the file it
    // was started from. The new one is copied in beside it and renamed over it.
    namespace fs = std::filesystem;
    const fs::path target(QFile::encodeName(d.installed).toStdString());
    const auto beside = target.parent_path() / ("." + target.filename().string() + ".new");
    std::error_code error;
    fs::copy_file(fs::path(QFile::encodeName(staged).toStdString()), beside, fs::copy_options::overwrite_existing, error);
    if (!error) fs::permissions(beside, fs::perms(0755), error);
    if (!error) fs::rename(beside, target, error);
    if (error) fs::remove(beside, error);
    else d.forget_staged();
#endif
}

} // namespace erdflow::infrastructure
