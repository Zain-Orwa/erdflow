# ADR-019 — Principals, Ownership and Permissions

**Status:** Accepted

**Date:** 2026-09-19

**Decision Scope:** Who an actor is, what ownership means, how permission is
granted and taken back, what is protected and what is free, and how all of it is
attributed and audited.

---

## 1. Context

Product section 50 records that ERDFlow will have accounts, that several people
will work in one project, and that several AI agents will work across accounts
and projects. None of it is built. All of it is intended.

That future needs answers to questions the desktop application has so far been
able to avoid:

```text
who is acting
what may they do
who decided that
for how long
can it be taken back
what did they actually do
```

The immediate trigger is conversion. Converting a Conceptual model into a
Relational Schema replaces something other people rely on, and the natural
first instinct — "only the owner may convert, and anyone else must ask the
owner for ownership" — is wrong in two separate ways. It protects the wrong
operation, and it pays for a small, frequent request with the largest and least
reversible thing a project has.

This ADR settles the identity and authority model. It does not build it.

---

## 2. Decision

ERDFlow uses:

> **One Principal abstraction, ownership separated from permission, and
> protection placed on shared truth rather than on exploration.**

Three decisions follow from that and are taken here:

1. Every actor — human, guest, AI agent — is a **Principal**, and there is one
   authorization system for all of them.
2. **Ownership** and **permission** are different concepts with different
   lifetimes, and neither is ever spent to buy the other.
3. Generating and previewing a candidate is **free** to any participant who may
   read the project. Applying a candidate to shared state is **protected**.

---

## 3. Every Actor Is a Principal

```text
Principal
├── User
├── Guest
└── Agent
```

All authorization works against a `PrincipalId`. There is no separate
authorization path for humans, a second one for guests, and a third one for AI.

> **Rule**
>
> Do not create separate authorization architectures for humans, guests and
> agents.

The reason is not elegance. Three parallel systems would diverge, and the one
used least would be the one with the hole in it.

---

## 4. Principal Identity Is Stable

Every actor that participates in project changes has a stable identity:

```text
UserPrincipal
GuestPrincipal
AgentPrincipal
```

Stable identity is what makes permissions, attribution, audit, revocation,
history and conflict resolution possible at all. It is the same argument that
ADR-001 makes for model elements, applied to the people and programs acting on
the model.

> **Rule**
>
> No meaningful project mutation is attributed to "Unknown".

---

## 5. Ownership Is Not Permission

| | Ownership | Permission |
| --- | --- | --- |
| Scope | the whole project | a named capability |
| Frequency | rare | ordinary |
| Lifetime | persists until transferred | may expire or be revoked |
| Granularity | one holder | many holders, different sets |
| Purpose | project governance | getting work done |

Ownership answers *whose project is this*. Permission answers *what may this
principal do*.

> **Rule**
>
> Ownership ≠ Permission. Never transfer ownership because someone needs
> temporary authority.

A request to perform one operation is answered with a permission — either a
one-time approval of that specific pending action, or an ongoing grant that
remains revocable. It is never answered by moving the ownership slot.

---

## 6. Ownership Transfer Is Its Own Workflow

Transfer is explicit, deliberate and rare. It must never occur as a side effect
of:

```text
a conversion request
an edit request
an invitation
guest access
agent access
```

---

## 7. Permission Grants

A grant is a record with a lifecycle, not a boolean:

```text
PermissionGrant
├── grant_id
├── principal_id
├── project_id
├── permission_set
├── scope
├── granted_by
├── granted_at
├── expires_at
├── revoked_at
└── status
```

Grants support:

- one-time,
- session-based,
- until a specific date and time,
- ongoing until revoked.

"Session" must be given one concrete meaning with a hard upper bound before
this is implemented, or session-based becomes indistinguishable from permanent
for anyone who never closes the application. That definition is an open
implementation decision, recorded in §16.

---

## 8. Revocation Exists From Day One

Every permission is revocable. Contractors finish, guests stop needing access,
people change teams, an agent stops being useful, a security concern appears.

> **Rule**
>
> "Granted once" must not mean "granted forever".

Revocation cannot be retrofitted cheaply: it requires every grant to have an
identity and a status from the first version. That is why the grant record
above is specified now even though nothing reads it yet.

---

## 9. Expiry Is Forward-Looking

When a permission expires or is revoked, future actions are refused. Past
legitimate actions are not reversed.

```text
Monday    user converts the schema under a valid grant
Tuesday   the grant expires
          → the conversion remains part of the project's history
          → the user cannot convert again
```

> **Rule**
>
> Permission expiry is forward-looking. It is not an undo.

---

## 10. What Is Protected

The protected operation is the one that changes what other people see.

```text
Generate Candidate          free
Preview Candidate           free
Compare with current state  free
─────────────────────────────────
Apply to Shared Schema      authorized
Replace Shared Schema       authorized
```

Flow:

```text
Generate Candidate
        ↓
Preview
        ↓
Review Differences
        ↓
Apply to Shared Schema
        ↓
Authorization Check
├── Yes → Apply
└── No  → Request Permission
```

> **Rule**
>
> Permission protects shared truth, not harmless candidate generation.

A permission request must not block the requester. They keep working and keep
their candidate while the request is outstanding.

---

## 11. Preview Is Available at Every Authorized Level

Preview is a baseline capability of every legitimate participant:

```text
Viewer       → preview allowed
Commenter    → preview allowed
Editor       → preview allowed
Guest        → preview allowed
Custom role  → preview allowed
AI Agent     → preview allowed with project-read access
```

Preview means inspecting the project, generating safe candidate
representations, inspecting potential conversions, and inspecting differences
and proposed changes.

Preview never replaces the shared schema, changes shared project truth, deletes
data, or bypasses a permission.

> **Rule**
>
> An authorized participant may evaluate the consequences of a change without
> receiving the authority to apply it.

---

## 12. Guest Does Not Mean Anonymous

A guest participates without a full ERDFlow account, and still receives a
project-scoped identity:

```text
GuestPrincipal
├── guest_id
├── display_name
├── project_id
├── created_from_invite
├── created_at
└── status
```

Good audit line:

```text
Guest "Maria" renamed Entity "Student".
```

Unacceptable audit line:

```text
Unknown changed project.
```

> **Rule**
>
> Participation without an account is allowed. Anonymous mutation is not.

A guest may hold View, Comment, Edit or a custom set. Edit is not
administrative authority: applying to the shared schema, managing members,
deleting the project, transferring ownership, creating invitations and changing
security policy remain separate capabilities.

---

## 13. AI Agents Are Principals With Their Own Grants

An agent is a Principal like any other:

```text
AgentPrincipal
├── agent_id
├── display_name
├── provider
├── created_by / invited_by
├── project_id
└── status
```

Three levels:

**READ** — inspect the project, diagrams, validation and readiness; generate
explanations. No mutation.

**SUGGEST** — everything in Read, plus conversion previews, comments, proposals
and proposed corrections. No direct application of high-impact changes.

**ACT** — execute Application Commands within an explicit grant, and no further.

> **Rule**
>
> Default AI access is Suggest.

### Agents do not inherit their inviter's permissions

```text
Wrong:   Zain is Owner  →  Zain's agent acts with Owner authority

Right:   Zain is Owner  →  Zain grants the agent Read + Preview + Suggest
                        →  the agent has exactly that
```

Every `AgentPrincipal` receives its own `PermissionGrant`. This is what keeps
"the owner's agent replaced the schema overnight" distinguishable from "the
owner replaced the schema overnight".

### Agent actions are attributed to the agent

```text
SchemaReviewer AI proposed 3 relational changes.
Invited by: Zain
Result: proposal created; shared schema unchanged.
```

Not:

```text
Zain changed schema
```

> **Rule**
>
> The inviter and the actor are different identities.

This is consistent with Architecture section 42 and with the standing decision
that AI is provider-agnostic and drives the application through the Editor's
own commands: AI proposes, and a human decides high-impact application.

---

## 14. Audit

Important actions produce audit events:

```text
AuditEvent
├── event_id
├── project_id
├── principal_id
├── action_type
├── target_id
├── timestamp
├── result
└── metadata
```

Covering at least: element created or changed, conversion preview generated,
conversion decision changed, shared schema replaced, invitation created or
revoked, guest joined, permission granted or revoked, agent added, agent
proposal created, ownership transferred.

> **Rule**
>
> Collaboration requires attribution and history.

---

## 15. Local Files Cannot Enforce This

A person who controls an `.erdx` file can modify it outside ERDFlow. Per
ADR-005 and ADR-016 a project is a real file in a real folder, and that is
deliberate.

```text
Local project        →  application-level collaboration rules; advisory
Hosted project       →  server-enforced authorization
```

For hosted collaboration:

```text
Client
   ↓
Authenticated Principal
   ↓
Server Authorization
   ↓
Application Operation
   ↓
Project State
```

> **Rule**
>
> Disabled buttons are UX. Server authorization is security.
>
> Do not present a local editable file as offering the same protection as an
> authenticated hosted project.

This is stated plainly so that no part of the product implies a guarantee the
local case cannot keep.

---

## 16. What Is Built Now

Nothing in this ADR is implemented, and most of it cannot be until accounts and
a hosted project store exist — Phases 56 and 57.

One seam is worth taking early, because it is cheap now and expensive later:
**a mutation records which principal issued it.** Every project mutation
already flows through an Application command (Architecture section 32, ADR-006),
so the command path is the one place that has to learn about principals.
Adding an author to a command later would touch every command that exists.

Deliberately **not** built early:

- an authorization interface with a permissive local implementation, because
  until the Relational Schema workspace exists there is nothing to protect and
  the shape of the check would be guessed rather than known;
- grant storage, invitation storage and audit storage, which are server
  concerns.

Open implementation decisions:

- the concrete definition of "session" and its upper bound;
- whether a project may have more than one owner, and how ownership is
  recovered if a sole owner's account is lost;
- the exact permission set names and their granularity.

---

## 17. Alternatives Considered

**A — Ownership as the only authority.** Simple, and wrong: every ordinary
request becomes a transfer of the whole project, and nothing is revocable.

**B — Gate conversion itself rather than application.** Blocks exploration,
makes a collaborator wait for an owner in order to learn anything, and protects
an operation that harms nobody. Rejected in favour of §10.

**C — Separate models for users, guests and agents.** Three systems to keep
correct, and the least-used one becomes the weakest. Rejected per §3.

**D — Agents inherit their inviter's authority.** Convenient, and it destroys
attribution exactly where attribution matters most. Rejected per §13.

**E — Permissions without revocation.** Cheaper to build, impossible to add
later without reworking every grant. Rejected per §8.

---

## 18. Consequences

**Positive.** One authorization path. Exploration stays free, so collaboration
does not depend on an owner being awake. Attribution survives agents and
guests. Revocation and expiry exist from the first version rather than being
retrofitted.

**Costs.** A grant is a record with a lifecycle, which is more than a boolean.
Attribution must be carried through the command path before there is a visible
reason for it. The local and hosted cases offer genuinely different guarantees,
and the product has to say so rather than blur it.

---

## 19. Relationship to Other ADRs

- **ADR-001** — stable identity, which grants and audit events depend on.
- **ADR-005 / ADR-016** — the project as a real file, which is why §15 exists.
- **ADR-006** — the command path that carries the acting principal.
- **ADR-009** — the readiness gate, which is separate from this authorization
  gate and comes first.
- **ADR-010** — three-way review, which protects manual downstream work; this
  ADR protects who may apply the result.
- **ADR-020** — how a principal comes to exist in a project.

---

## 20. Final Principle

> Seeing what a change would do should be easy. Changing what everyone else
> relies on should be deliberate, attributed, and revocable.
