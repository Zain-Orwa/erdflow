#pragma once

#include "domain/model.hpp"

#include <memory>
#include <string>
#include <utility>

namespace erdflow::application {

class IdGenerator {
public:
    virtual ~IdGenerator() = default;
    virtual domain::Uuid next() = 0;
};

struct EditResult {
    bool ok = true;
    std::string error;
    std::optional<domain::ElementRef> created;
    std::optional<domain::ParticipantId> participant;
    explicit operator bool() const { return ok; }
};

class Editor {
public:
    explicit Editor(IdGenerator& ids);
    ~Editor();
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    [[nodiscard]] const domain::Project& project() const;
    [[nodiscard]] bool dirty() const;
    [[nodiscard]] std::uint64_t revision() const;
    [[nodiscard]] bool can_undo() const;
    [[nodiscard]] bool can_redo() const;
    [[nodiscard]] std::string undo_label() const;
    [[nodiscard]] std::string redo_label() const;
    [[nodiscard]] std::size_t history_bytes() const;

    void new_project();
    EditResult replace_project(domain::Project project);
    void mark_saved(std::uint64_t revision);
    EditResult rename_project(std::string name);
    EditResult create_entity(std::string name, domain::Rect rect);
    EditResult create_attribute(std::string name, domain::Rect rect,
                                std::optional<domain::AttributeOwner> owner = {});
    EditResult create_relationship(std::string name, domain::Rect rect);
    EditResult rename(domain::ElementRef ref, std::string name);
    EditResult describe(domain::ElementRef ref, std::string description);
    EditResult set_attribute_kind(domain::AttributeId id, domain::AttributeKind kind);
    EditResult set_attribute_owner(domain::AttributeId id,
                                   std::optional<domain::AttributeOwner> owner);
    EditResult connect(domain::RelationshipId relationship, domain::EntityId entity);
    EditResult update_participant(domain::RelationshipId relationship,
                                   domain::ParticipantId participant,
                                   domain::Cardinality maximum,
                                   domain::Participation participation,
                                   std::string role);
    EditResult disconnect(domain::RelationshipId relationship, domain::ParticipantId participant);
    EditResult move(const std::map<domain::ElementRef, domain::Rect>& positions);
    // A connector carries one signed perpendicular bend. Passing no offset
    // restores automatic routing rather than storing a zero-length bend.
    EditResult bend_connector(domain::ConnectorRef ref, std::optional<double> offset);
    EditResult erase(const std::vector<domain::ElementRef>& elements,
                     const std::vector<std::pair<domain::RelationshipId, domain::ParticipantId>>& participants = {},
                     const std::vector<domain::AttributeId>& detached_attributes = {});
    EditResult duplicate(const std::vector<domain::ElementRef>& elements, double dx = 32, double dy = 32);
    EditResult undo();
    EditResult redo();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace erdflow::application
