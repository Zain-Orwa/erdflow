# ADR-020 — Invitations and Guest Access

**Status:** Accepted

**Date:** 2026-09-19

**Decision Scope:** How a person comes to have access to a project they did not
create, what an invitation link is and is not, and how someone without an
account participates while remaining identifiable.

---

## 1. Context

[ADR-019](ADR-019-PRINCIPALS-OWNERSHIP-AND-PERMISSIONS.md) settles what a
Principal is and what a permission grant is. It does not say how a Principal
comes to exist in a project in the first place.

An owner wants to hand access to one person. That person may already have an
ERDFlow account, or may not. The owner wants to choose what they may do and for
how long, and to be able to change their mind.

The mechanism is a link, and a link is text. It can be copied, forwarded,
pasted into a group chat and screenshotted. Any design that depends on a URL
staying secret is already broken.

---

## 2. Decision

ERDFlow uses:

> **Opaque, single-use, expiring invitation tokens that are claim credentials
> only, exchanged once for an identity that carries all later access.**

---

## 3. Invitation Flow

```text
Owner or authorized member
          ↓
Create Invitation
          ↓
Select: recipient, permissions, expiry, security requirements
          ↓
Invite Service
          ↓
Generate cryptographically strong token
          ↓
Store token hash
          ↓
Return shareable URL
          ↓
Recipient opens the link
          ↓
Safe invitation landing page
          ↓
Claim identity
          ├── has an account → authenticated UserPrincipal
          └── no account     → GuestPrincipal created
          ↓
Verify recipient where required
          ↓
Consume invitation
          ↓
Create PermissionGrant / Membership
          ↓
Project preview becomes available
          ↓
All later access uses the Principal, never the URL
```

Example URL:

```text
https://erdflow.app/invite/<opaque-secure-token>
```

---

## 4. An Invitation Is a Claim Credential, Not Access

The link exists to be exchanged for an identity. It is not ongoing access to
the project.

```text
Invite link
     ↓
Claim
     ↓
Membership / Guest identity created
     ↓
Token consumed
     ↓
Future access uses the identity and its session
     ↓
The old URL no longer grants anything
```

> **Rule**
>
> Invitation credential ≠ membership credential.

---

## 5. Single-Use by Default

An invitation is single-use unless deliberately configured otherwise. Once
claimed, the token is spent. Someone who receives the URL afterwards is
refused.

> **Rule**
>
> Forwarding a claimed invitation must not provide access.

---

## 6. A Copied Link Should Be Useless, Not Uncopyable

It is impossible to stop a person copying text. ERDFlow therefore does not try
to make URLs unshareable; it makes unauthorized sharing ineffective.

The available controls:

- recipient binding,
- single redemption,
- short expiry,
- one-time passcode,
- owner approval before first join,
- account or email restriction.

> **Rule**
>
> Do not try to make URLs physically unshareable. Make unauthorized sharing
> ineffective.

---

## 7. Recipient Binding

Where the owner knows who the invitation is for, it is bound to them:

```text
Invite
├── expected_email
└── expected_account_id
```

At claim time the authenticated identity must match.

```text
Invitation for  maria@example.com
Forwarded to    john@example.com
Result          access denied
```

Recipient-bound invitations are preferred for sensitive projects.

---

## 8. No Project Content Before a Successful Claim

Holding an unclaimed invitation URL does not entitle anyone to project data.

Before the claim, the landing page may show only invitation metadata:

```text
You have been invited to ERDFlow

Project:      University Model
Invited by:   Zain
Access:       Comment
Expires:      20 Sep 2026
```

After identity verification or guest claim, project preview becomes available.

> **Rule**
>
> Invitation metadata may be visible before a claim. Project content is visible
> only after a successful one.

---

## 9. The Token Is Opaque

The URL carries a random token and nothing else. It never carries
authorization data.

```text
Bad:        /invite?project=123&role=admin
Preferred:  /invite/SecureRandomOpaqueToken
```

The server resolves the token to the invitation record.

> **Rule**
>
> Never trust authorization information supplied by the URL.

---

## 10. Invitation Record

```text
Invite
├── invite_id
├── project_id
├── token_hash
├── intended_principal / email
├── permission_set
├── created_by
├── created_at
├── expires_at
├── max_uses
├── use_count
├── claimed_by
├── claimed_at
├── revoked_at
└── status
```

A secure hash of the token is stored rather than the token itself wherever
practical, so that a leak of the invitation store is not a leak of live
invitations.

---

## 11. Expiry and Revocation

Invitations expire. Defaults may be offered at 24, 48 or 72 hours, or a custom
duration; sensitive projects use shorter ones.

An authorized user may revoke an invitation before it is claimed, after which
the URL is invalid.

> **Rule**
>
> An unused invitation link must not remain valid forever, and an invitation
> must be cancellable.

---

## 12. Guest Claims

For someone without an account:

```text
Invite link
      ↓
Guest claim screen
      ↓
Display name
      ↓
Optional email verification / one-time passcode
      ↓
GuestPrincipal created
      ↓
Permissions applied
      ↓
Project opened
```

A guest is not anonymous. Per
[ADR-019 §12](ADR-019-PRINCIPALS-OWNERSHIP-AND-PERMISSIONS.md), every guest
receives a stable project-scoped identity and every guest mutation carries its
`GuestPrincipalId`.

---

## 13. Optional Strict Security

A project may require more without every ordinary project paying for it:

- recipient email required,
- one-time use,
- short expiry,
- email one-time passcode,
- passcode,
- owner approval before first join,
- device or network anomaly checks where appropriate,
- stricter rate limits.

> **Rule**
>
> Security may become stronger without forcing maximum friction on every
> project.

---

## 14. Rate Limiting

Tokens and passcodes are short secrets, and short secrets attract guessing. The
system defends against token guessing, passcode brute force and repeated claim
attempts with attempt limits, cooldowns, rate limiting, temporary blocking and
logging of suspicious activity.

> **Rule**
>
> A short secret is never an unlimited-try credential.

---

## 15. Audit

Invitation activity is part of a project's security history. Recorded: who
created an invitation and when, the intended recipient, the permission level,
when it was opened and claimed, who claimed it, security-relevant failed or
rejected claims, revocation and expiry.

---

## 16. What Is Built Now

Nothing. An invitation requires an account system, a hosted project store and a
server that can hold and resolve tokens — none of which exist. This belongs to
Phase 56 and Phase 57.

It is recorded now because the decisions here constrain the account system's
shape, and because the guest identity requirement reaches back into
attribution, which ADR-019 asks the command path to carry.

Open implementation decisions:

- token length and format;
- whether guest identities survive across projects or are strictly
  project-scoped;
- whether a "share with my team" multi-use invitation exists alongside the
  single-use default, and what it costs in traceability.

---

## 17. Alternatives Considered

**A — Long-lived shareable project links.** One link, anyone who has it is in.
Easy, and it makes revocation meaningless, attribution impossible and
forwarding undetectable. Rejected.

**B — Authorization encoded in the URL.** Removes a lookup, and lets anyone
edit their own permissions. Rejected per §9.

**C — Anonymous guests.** No claim step at all. Cheapest, and it produces
"Unknown changed project" in the history of a collaborative tool. Rejected per
§12.

**D — Accounts required for everyone.** Clean identity, and it refuses the
common case of one person needing to look at one diagram once. Rejected in
favour of guests with real identities.

---

## 18. Consequences

**Positive.** A leaked link is close to worthless: it is opaque, single-use,
expiring, revocable and optionally bound to one person. Every participant,
including guests, is identifiable. Invitation activity is auditable.

**Costs.** A claim step stands between the link and the project, which is one
more screen than "click and you are in". Guests must supply at least a display
name. The invitation store is real infrastructure with its own lifecycle,
hashing and rate limiting.

---

## 19. Relationship to Other ADRs

- **ADR-019** — what the claim produces: a Principal and a grant.
- **ADR-016** — project organisation, which the hosted case extends.
- **Architecture §41** — security boundaries, of which this is one.

---

## 20. Final Principle

> An invitation is a one-time ticket, not a key. It is exchanged at the door
> for an identity, and the identity is what opens anything afterwards.
