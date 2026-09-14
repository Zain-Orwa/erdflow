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
    static QByteArray encode(const domain::Project& project);
    static application::LoadResult decode(const QByteArray& bytes);
};

} // namespace erdflow::infrastructure
