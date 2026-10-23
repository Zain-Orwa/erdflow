// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "infrastructure/updater.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

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

} // namespace

int main() {
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"Ed25519 matches RFC 8032", ed25519_matches_the_standard},
        {"the release key is a key", the_release_key_is_a_key},
        {"versions compare by number", versions_compare_by_number},
        {"a signed, newer release is offered for this computer", a_signed_newer_release_is_offered_for_this_computer},
        {"nothing is offered unless signed, newer and made for here", nothing_is_offered_unless_signed_newer_and_made_for_here},
        {"names outside the package shape are never taken", names_outside_the_package_shape_are_never_taken},
        {"this computer has a package name", this_computer_has_a_package_name},
    };
    std::size_t failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    return failures == 0 ? 0 : 1;
}
