// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "application/project_store.hpp"

#include <QByteArray>
#include <QString>

namespace erdflow::infrastructure {

class QtIdGenerator final : public application::IdGenerator {
public:
    domain::Uuid next() override;
};

QString uuid_text(domain::Uuid value);

class ErdxProjectStore final : public application::ProjectStore {
public:
    static constexpr qsizetype max_file_bytes = 8 * 1024 * 1024;
    application::LoadResult load(const std::string& location) override;
    application::SaveResult save(const std::string& location, const domain::Project& project) override;
    application::EncodeResult project_bytes(const domain::Project& project) override;
    application::LoadResult project_from_bytes(const std::string& bytes) override;
    static QByteArray encode(const domain::Project& project);
    static application::LoadResult decode(const QByteArray& bytes);
};

} // namespace erdflow::infrastructure
