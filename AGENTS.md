# Agent Instructions

Read [.specify/memory/constitution.md](.specify/memory/constitution.md) first, then
[CLAUDE.md](CLAUDE.md) for the shared project instructions.

Codex uses the existing [.claude/skills](.claude/skills) Spec Kit skills directly.
Explicitly open the relevant speckit-*/SKILL.md and follow it; do not generate a
second skill collection under .agents. Specifications are shared in [specs](specs).

Follow specify → clarify → plan → tasks → analyze → implement. Existing operator
decisions carry through these phases. The website and all six guides live in docs
and belong to feature [002](specs/002-guide-site/spec.md).
Complete design and tooling first; write the guide content with the operator next.
Public launch requires the separate content-launch decision. Keep website work
separate from the game addon.
