# Monocypher, as ERDFlow carries it

ERDFlow uses Monocypher for one thing: checking the Ed25519 signature on an
update before it is put in place (ADR-024). Nothing else in ERDFlow uses it.

| | |
| --- | --- |
| Upstream | Monocypher, by Loup Vaillant, Michael Savage, Fabio Scotoni and contributors — <https://monocypher.org> |
| Version | 4.0.2 |
| Taken from | <https://monocypher.org/download/monocypher-4.0.2.tar.gz> |
| Checked against | <https://monocypher.org/download/monocypher-4.0.2.tar.gz.sha512>, the SHA-512 Monocypher publishes, on 2026-10-23 |
| SHA-512 of the archive | `bf275d4c53ff94af6cdc723a4e002e9f080f4d1436c86c76bb37870b34807f1d7b32331d8ff8a1aeb369e946f3769021e03e63efac25b82efc5abf54dc084714` |
| Licence | Dual: CC0-1.0 or BSD-2-Clause, at the user's choice; ERDFlow takes it under CC0-1.0. See `LICENCE.md`, copied unchanged |
| Approved by | Zain, 2026-10-23 |

## The files, unmodified

| Here | In the archive |
| --- | --- |
| `monocypher.c`, `monocypher.h` | `src/` |
| `monocypher-ed25519.c`, `monocypher-ed25519.h` | `src/optional/` |
| `LICENCE.md` | top level |

They are byte-for-byte as in the archive. To check, unpack the archive and
compare each file with `cmp`. Do not change them; a newer Monocypher is taken
by replacing all of them from a newer, checked archive and updating this page.
