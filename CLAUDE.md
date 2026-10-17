## Branching

- Default branch is `development`. `main` only receives work that is verified to run properly, and only the owner merges into it.
- Never commit straight to `development` or `main`. Create a branch named per [Conventional Branch](https://conventionalbranch.org/) (`feature/`, `bugfix/`, `hotfix/`, `release/`, `chore/`, then lowercase words joined by hyphens, e.g. `feature/month-calendar`), push it, then open a PR into `development`.

## Agent skills

### Issue tracker

Issues are tracked in GitHub Issues (`batuky/psp-desk-clock`) via the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

Default vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `GLOSSARY.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
