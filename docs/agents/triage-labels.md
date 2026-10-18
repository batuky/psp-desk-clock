# Triage Labels

The skills speak in terms of five canonical triage roles. This file maps those roles to the actual label strings used in this repo's issue tracker.

| Label in mattpocock/skills | Label in our tracker | Meaning                                  |
| -------------------------- | -------------------- | ---------------------------------------- |
| `needs-triage`             | `needs-triage`       | Maintainer needs to evaluate this issue  |
| `needs-info`               | `needs-info`         | Waiting on reporter for more information |
| `ready-for-agent`          | `ready-for-agent`    | Fully specified, ready for an AFK agent  |
| `ready-for-human`          | `ready-for-human`    | Requires human implementation            |
| `wontfix`                  | `wontfix`            | Will not be actioned                     |

## Other labels

Labels that are not triage roles but that issues here carry. Apply them yourself when filing or triaging; do not ask which to use.

| Label  | Meaning                                                                                              |
| ------ | ---------------------------------------------------------------------------------------------------- |
| `v1`   | The version being built now. Work on this repo's current code (features, refactors, fixes) is `v1`.  |
| `v2`   | A later version: possible changes to the kernel, to be done only if needed. Not current work.        |

An issue about the current code gets `v1` plus a type label, for example `enhancement` or `bug`.

When a skill mentions a role (e.g. "apply the AFK-ready triage label"), use the corresponding label string from this table.

Edit the right-hand column to match whatever vocabulary you actually use.
