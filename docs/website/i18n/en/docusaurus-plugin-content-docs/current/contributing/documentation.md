# Documentation Organization

Organize the manual by developer tasks and keep design history in maintenance
records. Running an Agent should not require reading dozens of ADRs.

## Information Architecture

| Kind | Directory | Goal |
|---|---|---|
| Tutorial | getting-started/ | Complete steps from setup to a runnable result |
| How-to | guides/ | Assembly, failure and lifecycle for real tasks |
| Reference | api/ | Functions, fields, ownership, zero values and errors |
| Platform guide | platforms/ | Dependencies, SDK limits, resources and acceptance |
| Explanation | architecture.md, arch/ | Modules, data flow, memory and invariants |
| Maintenance | development/, adr/ | Implementation facts, alternatives and rationale; Chinese only |

The organization follows [Diataxis](https://diataxis.fr/) and draws on
[Zephyr's platform documentation](https://docs.zephyrproject.org/latest/) and
[libcurl's API reference](https://curl.se/libcurl/c/), without copying their text
or architecture.

## Current Contracts and History

The manual follows current source, not planned phases. Never present future
features as available or use draft labels instead of clear implemented contracts.
ADRs retain historical states/alternatives; logs retain dates and never promote
one Host pass to a universal platform guarantee.

Authority order: public headers, implementation/tests -> manual -> historical
proposals. Correct contradictions and add tests rather than ask users to guess.

## Links, Examples and Translations

- Use relative .md links within docs; include every user page in navigation.
- Link source with GitHub URLs, never developer-machine paths.
- Format API identifiers/types as code to avoid MDX parsing problems.
- Label complete programs, integration functions and pseudocode accurately.
- Compile/run the Host quickstart; platform fragments are not complete BSPs.
- Update guides, references, switches and architecture with API changes.
- English manual files mirror Chinese IDs under `website/i18n/en/docusaurus-plugin-content-docs/current/`.
- Keep executable example blocks identical across locales; checks validate English coverage, links and examples.
- ADRs/development logs are not translated; mark maintenance navigation as Chinese material.
- Use document-root links such as `api/context.md` across translated/fallback content; `../` links resolve only in the physical source tree.

See [site maintenance commands](https://github.com/Scintela/cAGENT/blob/main/docs/README.md).
