#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace erdflow::domain {

struct Uuid {
    std::array<std::uint8_t, 16> bytes{};
    auto operator<=>(const Uuid&) const = default;
    [[nodiscard]] bool valid() const;
};

template<class Tag> struct Id {
    Uuid value;
    auto operator<=>(const Id&) const = default;
};
using ProjectId = Id<struct ProjectTag>;
using EntityId = Id<struct EntityTag>;
using AttributeId = Id<struct AttributeTag>;
using RelationshipId = Id<struct RelationshipTag>;
using SpecializationId = Id<struct SpecializationTag>;
using ParticipantId = Id<struct ParticipantTag>;
using PictureId = Id<struct PictureTag>;
using NoteId = Id<struct NoteTag>;
using CommentId = Id<struct CommentTag>;
// A specialization is a placed element of its own: it carries the ISA triangle
// on the canvas and the constraints that decide how it converts to relations.
// A picture and a note are placed elements too, though not database objects:
// see Picture below for what that means.
using ElementRef = std::variant<EntityId, AttributeId, RelationshipId, SpecializationId, PictureId, NoteId>;
// An attribute belongs to an entity, a relationship, or a composite attribute;
// never to a specialization, which owns no data of its own.
using AttributeOwner = ElementRef;
// A connector is drawn from the record that creates it, so it is identified by
// that record rather than by its own identity: an attribute's ownership link,
// or one relationship participant. Connectors are therefore never orphaned.
using ConnectorRef = std::variant<AttributeId, ParticipantId>;

// A colour the user has chosen for one element's surface, overriding the one
// its theme would give it. Stored as channels rather than as text so that a
// document carries a colour rather than a string that has to be parsed and
// might not be one.
struct Colour {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    auto operator<=>(const Colour&) const = default;
};

struct Point {
    double x = 0;
    double y = 0;
    auto operator<=>(const Point&) const = default;
};

// How the user has shaped a connector by hand. The bend is a signed
// perpendicular offset in canvas units, used only while the line has no
// waypoints of its own.
//
// The anchors, when present, pin where the line meets each shape. They name the
// two ends of the drawn line rather than two kinds of element: owner_anchor is
// the end the line is drawn from -- an attribute link's owning element, or a
// participant link's relationship -- and child_anchor is the far end, being the
// attribute or the entity. Each is a direction in radians from that shape's
// centre rather than a point, so the join keeps its place on the outline when
// the shape is moved or resized, and either end can be pinned on its own.
struct Connector {
    double offset = 0;
    std::optional<double> owner_anchor;
    std::optional<double> child_anchor;
    // Points the line is routed through, in canvas coordinates and in the order
    // they are met walking from the connector's source to its target. A
    // connector with any of these is routed through them and its single bend is
    // no longer consulted: the waypoints say everything about its shape.
    std::vector<Point> waypoints;
    [[nodiscard]] bool pinned() const { return owner_anchor.has_value() || child_anchor.has_value(); }
    [[nodiscard]] bool routed() const { return !waypoints.empty(); }
    [[nodiscard]] bool automatic() const { return offset == 0 && !pinned() && !routed(); }
    auto operator<=>(const Connector&) const = default;
};

struct Rect {
    double x = 0;
    double y = 0;
    double width = 160;
    double height = 80;
    auto operator<=>(const Rect&) const = default;
};

enum class AttributeKind { Normal, Key, Composite, Multivalued, Derived };

// How much a conceptual model is asked to say about itself.
//
// Basic is the diagram as it is drawn and taught: shapes, names and the
// notation. Convertible is the same model asked to carry what turning it into
// tables will need. It is one model either way -- switching modes changes what
// is shown and what is asked for, never what anything is or what identity it
// has, so a diagram can be drawn in Basic and finished in Convertible without
// being rebuilt.
enum class ConceptualMode { Basic, Convertible };

// A portable type, chosen without naming a database. Text(100) becomes
// VARCHAR(100) on one engine and NVARCHAR(100) on another, and that choice
// belongs to the physical stage rather than to this one.
//
// Unset is a real answer and the one every attribute starts with: in Basic it
// is simply not asked, and in Convertible it is the question still open.
enum class LogicalType { Unset, Text, Integer, Decimal, Boolean, Date, DateTime, Binary, Uuid };
enum class Cardinality { One, Many };
enum class Participation { Partial, Total };
// Whether an instance of the supertype may belong to more than one subtype,
// and whether it must belong to at least one. Together these choose the
// relational mapping strategy when the model is converted.
enum class Disjointness { Disjoint, Overlapping };
enum class Completeness { Partial, Total };
// Which way the hierarchy was read. It is not only provenance: the ISA triangle
// points at the supertype when generalising and at the subtypes when
// specialising, so the direction is part of the notation and is stored.
enum class Inheritance { Generalization, Specialization };

struct Entity {
    EntityId id;
    std::string name;
    std::string description;
    // What this becomes in the schema's own words. A description says what the
    // thing means to a reader; a comment is written for the database, and is
    // what a generated table or column carries as its COMMENT. They are kept
    // apart because they are read by different audiences and one of them
    // travels out of ERDFlow entirely.
    std::string comment;
    // A weak entity has no key of its own: it is identified through an
    // identifying relationship with its owner, and its key attribute is only
    // a partial key. Drawn with a double border.
    bool weak = false;
    auto operator<=>(const Entity&) const = default;
};
struct Attribute {
    AttributeId id;
    std::string name;
    std::string description;
    std::string comment;
    AttributeKind kind = AttributeKind::Normal;
    std::optional<AttributeOwner> owner;
    // What Convertible mode asks for, and Basic mode leaves alone. Every one of
    // these is stored whichever mode is on: a model drawn in Basic and finished
    // in Convertible must not lose what it was told in between, and a model
    // shown in Basic must not quietly forget what it already knows.
    LogicalType logical_type = LogicalType::Unset;
    // How long or how precise, where the type takes a number: Text(100). Zero
    // is unspecified, which is what a type that takes no number always is.
    std::uint32_t length = 0;
    // Whether this is part of what identifies a row, whether it must be filled
    // in, and whether no two rows may share it. Kept apart from the attribute's
    // Chen kind: a key oval says how the diagram draws it, these say what the
    // table will enforce, and the two are set at different stages by different
    // people.
    bool identifier = false;
    bool required = false;
    bool unique = false;
    auto operator<=>(const Attribute&) const = default;
};
// A participant attaches to an entity, or to an associative relationship that
// is acting as one. Only an associative relationship may be a target.
using ParticipantTarget = std::variant<EntityId, RelationshipId>;

struct Participant {
    ParticipantId id;
    ParticipantTarget target;
    Cardinality maximum = Cardinality::Many;
    Participation participation = Participation::Partial;
    std::string role;
    // Whether this side's constraints are drawn on the line. The constraints
    // themselves are unaffected: the model still holds them and anything that
    // reasons about the relationship still reads them. This only says that the
    // diagram is to show a bare connection at this end, which is how an ERD is
    // often drawn when only one side is being made a point of.
    bool show_constraints = true;
    auto operator<=>(const Participant&) const = default;
};
struct Relationship {
    RelationshipId id;
    std::string name;
    std::string description;
    std::string comment;
    // An associative relationship carries its own identity and may participate
    // in further relationships. It is drawn as a diamond inside a rectangle.
    bool associative = false;
    // An identifying relationship is the one through which a weak entity is
    // identified. Drawn as a double diamond. A relationship is identifying or
    // associative, never both.
    bool identifying = false;
    std::vector<Participant> participants;
    auto operator<=>(const Relationship&) const = default;
};
// The three kinds a relationship can be, read off its two flags.
enum class RelationshipKind { Regular, Identifying, Associative };
[[nodiscard]] RelationshipKind relationship_kind(const Relationship& relationship);
struct Specialization {
    SpecializationId id;
    std::string name;
    std::string description;
    Inheritance direction = Inheritance::Specialization;
    // Absent while the triangle has been placed but not yet connected. Like a
    // relationship with too few participants, that is work in progress.
    std::optional<EntityId> supertype;
    std::vector<EntityId> subtypes;
    Disjointness constraint = Disjointness::Disjoint;
    Completeness completeness = Completeness::Partial;
    auto operator<=>(const Specialization&) const = default;
};
// Visual aids placed on the canvas. Neither is a database object: nothing is
// converted from them, no attribute belongs to them and no line may join them,
// and validation asks no more of them than that they are well formed. They are
// part of the drawing, though, so they are placed, moved, coloured, saved and
// undone exactly as everything else on it is.
//
// A picture holds the encoded bytes of an image, PNG or JPEG, as the file gave
// them. The domain cannot decode an image and does not try: decoding is the
// presentation's business, and the domain only refuses bytes that could not be
// one, or more of them than a project file can hold.
struct Picture {
    PictureId id;
    std::string name;
    std::string description;
    std::vector<std::uint8_t> image;
    auto operator<=>(const Picture&) const = default;
};
// A note is free text: its name is drawn as a title and its description as
// the text beneath, which is why it has the same two fields as everything else.
struct Note {
    NoteId id;
    std::string name;
    std::string description;
    // A plain note is one character standing on its own: it is drawn as the
    // character alone, with no card, no border and no title, the way an emoji
    // sits in a line of chat. It is still a note in every other way, so it is
    // moved, coloured, copied, deleted and undone like one.
    bool plain = false;
    auto operator<=>(const Note&) const = default;
};
// The paper a diagram is drawn on. Theme leaves the canvas the plain colour
// its palette gives it; the others lay a ruling or a picture over that colour,
// as strongly as the strength says. It is part of how the diagram looks rather
// than of what it means, but it travels with the document: a diagram drawn on
// graph paper should open on graph paper.
enum class BackgroundStyle { Theme, Squares, Lines, Dots, Image };
struct Background {
    BackgroundStyle style = BackgroundStyle::Theme;
    // How much of the ruling or picture shows through, in percent. A hundred
    // is full strength and nothing is invisible below it that was not already.
    std::uint8_t strength = 100;
    // The encoded picture, PNG or JPEG, for the Image style and empty for the
    // rest. Held the same way a placed picture is: bytes as the file gave them.
    std::vector<std::uint8_t> image;
    auto operator<=>(const Background&) const = default;
};

// Which of an element's two written fields a comment is pinned into. These are
// the only two: everything on the diagram carries a name and a description, and
// nothing else it carries is prose a reader would want to remark on.
enum class TextField { Name, Description };

// A comment pinned to part of what somebody wrote rather than to the whole of
// an element: the second word of a name, a sentence in a description.
//
// The range is counted in characters rather than in bytes, so it means the same
// thing however the text is stored and however the presentation layer indexes
// it. A zero length is a caret between two characters, which is what a remark
// about a missing word is pinned to.
struct TextAnchor {
    ElementRef owner;
    TextField field = TextField::Name;
    std::uint32_t begin = 0;
    std::uint32_t length = 0;
    auto operator<=>(const TextAnchor&) const = default;
};

// What a comment is pinned to: a whole element, one of the lines between them,
// or a range of the text inside an element's name or description.
using CommentTarget = std::variant<ElementRef, ConnectorRef, TextAnchor>;

// A remark somebody has left on the diagram while reviewing it. This is not the
// Note element, which is a card placed on the canvas and is part of the
// drawing; and it is not an element's description, which documents the model
// and travels forward into the schema. A comment is about the work rather than
// part of it: it is pinned to things rather than placed, it can be pinned to
// several things at once so one remark covers a whole area, and it can be put
// away without being deleted.
//
// It records no author yet, because there are no accounts to name one. The
// format is versioned and additive, so an author is added when accounts are,
// without disturbing anything written here.
struct Comment {
    CommentId id;
    std::string text;
    // Everything this one remark is pinned to. At least one, because a comment
    // pinned to nothing could never be found again.
    std::vector<CommentTarget> targets;
    // Put away without being deleted: the mark stays on the things it is
    // pinned to, and the text stops appearing when they are pointed at.
    bool hidden = false;
    auto operator<=>(const Comment&) const = default;
};

struct Project {
    ProjectId id;
    std::string name = "Untitled";
    std::map<EntityId, Entity> entities;
    std::map<AttributeId, Attribute> attributes;
    std::map<RelationshipId, Relationship> relationships;
    std::map<SpecializationId, Specialization> specializations;
    std::map<PictureId, Picture> pictures;
    std::map<NoteId, Note> notes;
    // Review remarks, kept apart from the drawing because that is what they
    // are: a comment is pinned to the model rather than placed on the canvas,
    // so it has no layout, no colour and no transparency of its own.
    std::map<CommentId, Comment> comments;
    // What this model is being asked to say about itself. It travels with the
    // document, because what a model was told is part of the model rather than
    // part of how somebody happened to be looking at it.
    ConceptualMode mode = ConceptualMode::Basic;
    std::map<ElementRef, Rect> layout;
    // How the user has shaped each connector they have touched. An absent entry
    // means the connector is bent and joined entirely automatically.
    std::map<ConnectorRef, Connector> connectors;
    // Surface colours the user has chosen. An absent entry means the element is
    // drawn in whatever colour the active theme gives its kind, which is what
    // keeps a document that has never been recoloured following the theme.
    std::map<ElementRef, Colour> colours;
    // How see-through each element's surface is, in percent: 0 is solid and
    // 100 leaves only the outline and whatever lies behind. It applies to
    // whichever colour the surface has, chosen or the theme's, which is why
    // it is kept apart from the colour. An absent entry means solid.
    std::map<ElementRef, std::uint8_t> transparency;
    // The paper the diagram is drawn on.
    Background background;
    auto operator<=>(const Project&) const = default;
};

enum class Severity { Error, Warning, Info };
struct Issue {
    Severity severity;
    std::string code;
    std::string message;
    std::optional<ElementRef> element;
    bool blocks_save = false;
};

[[nodiscard]] Uuid uuid(const ElementRef& ref);
[[nodiscard]] bool exists(const Project& project, const ElementRef& ref);
// A picture or a note: on the canvas, but not in the database model. Nothing
// connects to one and no attribute belongs to one.
[[nodiscard]] bool is_figure(const ElementRef& ref);
// A connector exists while the record that draws it does: an owned attribute,
// or a participant still listed by its relationship.
[[nodiscard]] bool connector_exists(const Project& project, const ConnectorRef& ref);
// The element a participant attaches to, as a general element reference.
[[nodiscard]] ElementRef target_ref(const ParticipantTarget& target);
// Whether a comment's target still refers to something that is there.
[[nodiscard]] bool target_exists(const Project& project, const CommentTarget& target);
// The comments pinned to one element, one connector, or anything at all, in a
// stable order. A caller asking what to draw on a shape asks this.
[[nodiscard]] std::vector<CommentId> comments_on(const Project& project, const ElementRef& ref);
[[nodiscard]] std::vector<CommentId> comments_on_connector(const Project& project, const ConnectorRef& ref);
// How many characters a piece of text holds, which is what a text anchor's
// range is counted in.
[[nodiscard]] std::size_t character_count(const std::string& text);
[[nodiscard]] std::string name(const Project& project, const ElementRef& ref);
[[nodiscard]] std::string description(const Project& project, const ElementRef& ref);
[[nodiscard]] std::vector<Issue> validate(const Project& project);

inline constexpr std::size_t max_elements = 10000;
inline constexpr std::size_t max_name_bytes = 512;
inline constexpr std::size_t max_description_bytes = 16384;
inline constexpr double max_coordinate = 100000;
// A symbol is drawn as its character grown to fill its box, so the box is how
// big the character is. The smallest is still a mark that can be read beside a
// name; the largest heads a region of the diagram without becoming the paper
// it is drawn on. Between them a symbol can be enlarged and shrunk freely.
inline constexpr double min_symbol_size = 16;
inline constexpr double max_symbol_size = 4000;
// A picture's bytes, before the text encoding a project file gives them. Kept
// well inside the file limit, so a diagram can carry a few pictures and still
// have room for the model.
inline constexpr std::size_t max_image_bytes = 2U * 1024U * 1024U;
// A comment's own limits. The text is bounded like a description, because it
// is prose of the same kind; the number of things one remark may be pinned to
// is bounded like everything else that refers to elements.
// A logical length is a number a database will be given, so it is bounded well
// inside anything an engine would accept rather than left to be any number at
// all.
inline constexpr std::uint32_t max_logical_length = 1000000;
inline constexpr std::size_t max_comment_bytes = max_description_bytes;
inline constexpr std::size_t max_comment_targets = max_elements;
inline constexpr std::uint8_t max_transparency = 100;
inline constexpr std::uint8_t max_strength = 100;

} // namespace erdflow::domain
