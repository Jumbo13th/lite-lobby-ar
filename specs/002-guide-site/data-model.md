# Content Model: Lite Lobby Guide Site

## Localized pages

English pages live at the content root; Russian counterparts live under ru/ with
the same relative filenames and section IDs. Each page owns its prose, localized
metadata and figure text. Language controls connect corresponding pages.

## Homepage entries

The homepage component receives localized titles for the ordered entries in
[the site contract](contracts/site.md#homepage), including the two Triad Tactics
child links. Both languages use the same stable heading IDs.

## Course metadata

docs/src/data/chapters.json contains:

- hubs: the introduction's slug, localized title and null parent;
- chapters: a slug, localized title and parent; course chapters also have a part;
- sequence: the introduction and ordered course chapter slugs;
- parts: ordered identifiers and localized labels used to group sidebar chapters.

Unit planning and prefab operations are supporting pages with the course as parent,
without a part or sequential neighbors. Numbering is part of chapter titles.
MDX previous/next links are validated against sequence. No legacy destination or
fragment mapping is stored.

## Screenshot figures and inventory

A figure receives imported ImageMetadata, nonempty localized alt, caption and
originalLabel strings. Metadata supplies intrinsic dimensions and the source URL;
responsive variants are generated artifacts shared by both languages.

tests/screenshots.json records each retained PNG's current topic-relative path,
original path, byte length and SHA-256 hash. Filenames use three-digit capture
numbers or descriptive kebab-case names. The inventory defines the expected files;
[spec.md](spec.md#acceptance-criteria) defines the accepted count.

## Build artifacts

Normal dist contains localized pages, the framework error page, search index,
sitemap and referenced assets. It contains no database, API or internal documents.
Validation uses separate fixture sources and .validation-dist, and stores captures
in .validation-results. Those temporary resources are not content records.
