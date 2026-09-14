#pragma once

#include "application/editor.hpp"

namespace erdflow::application {

struct LoadResult {
    std::optional<domain::Project> project;
    std::string error;
    explicit operator bool() const { return project.has_value(); }
};
struct SaveResult {
    bool ok = false;
    std::string error;
    explicit operator bool() const { return ok; }
};
class ProjectStore {
public:
    virtual ~ProjectStore() = default;
    virtual LoadResult load(const std::string& location) = 0;
    virtual SaveResult save(const std::string& location, const domain::Project& project) = 0;
};

// The small initial persistence use cases are synchronous and bounded. They
// install only validated candidates and mark only the saved state as clean.
inline EditResult open_project(Editor& editor, ProjectStore& store, const std::string& location) {
    auto result = store.load(location);
    if (!result) return {false, result.error, {}, {}};
    return editor.replace_project(std::move(*result.project));
}
inline SaveResult save_project(Editor& editor, ProjectStore& store, const std::string& location) {
    const auto revision = editor.revision();
    auto result = store.save(location, editor.project());
    if (result) editor.mark_saved(revision);
    return result;
}

} // namespace erdflow::application
