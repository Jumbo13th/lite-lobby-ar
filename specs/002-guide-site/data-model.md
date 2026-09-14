# Content Model: Lite Lobby Guide Site

## Localized homepage

English and Russian each own one MDX homepage. Stable ASCII heading anchors identify
the five guide entries in the order defined in contracts/site.md.
The API heading has scripting and website-integration subheadings. Both locales expose
the same seven anchors. Each page supplies five topic titles and two compact API audience
labels to the shared homepage component.
The guide order, stable IDs, and small title icons are shared.
These are informational list entries without destination URLs until real pages exist.
The mission, example, and Git entries are the three active guide links in both
locales. Character/group creation belongs to the mission course rather than a
separate homepage entry. Remaining entries have no destination yet.

## Guide relationships

The mission-creation guide owns the project-to-playable-mission sequence. It links to
opening the example and GitHub Desktop through ordinary preface paragraphs. The
example is the recommended way to learn, reflecting the authors' experience. Neither workflow is
required to continue. Character/group prefab creation is part of the required
course sequence; Triad Tactics or API setup is linked where applicable. Supporting guides own their
detailed procedures. Canonical Russian titles are recorded in contracts/site.md.
The main guide owns the repository/addon-folder explanation and supplied screenshot;
the Git introduction links back to that section.

## Screenshot figure

The figure receives an imported Astro image, nonempty localized alternative text,
nonempty localized caption, and a localized original-link label. Its source metadata
provides dimensions and the original URL. Generated variants are build artifacts.

## Asset inventory

The shared tests/screenshots.json manifest maps each current relative path to its
original path, byte length and SHA-256 hash. It covers all 144 source PNGs, including
the original 113 course captures. Both locales use the same source files.
Screenshots live in docs/src/assets/screenshots/<topic>/, using lowercase kebab-case
topic folders and three-digit capture numbers. Supplementary captures with descriptive
names use kebab-case. Source bytes must remain unchanged during path migrations.

## Build artifact

During the design stage, normal output contains the two localized homepages,
framework 404 output, generated search index, and only required assets. Completed
guide routes join this output during content authoring. Temporary browser fixtures
use separate output and are removed after validation. No database or API is added.

## Chapter navigation (T097; replaces the T080–T096 page hierarchy)

The only overview is create-mission. Each chapter has a stable slug, RU/EN titles,
this overview as its parent, and sequential neighbors. Course chapters have a
number and a part. Three ordered parts group the chapters
(1–2, 3–5, 6–8) without adding landing pages. Retired routes have a
default destination and an old-anchor map; active pages forward only moved anchors.
The reading sequence is one introduction plus eight chapters. Their on-page sections
contain the steps; no parallel tutorial/reference copy is stored. Supporting pages
are outside the required sequence and link back to the main contents. The existing
unit-planning and prefab-operations pages also use create-mission as their parent,
without a part or sequential neighbors. No figure-count limit defines a chapter.
Shared metadata drives sidebar and structural checks. Explicit frontmatter links
define the single course reading path. A map of old heading ids
to canonical chapters retains existing links, shared between locales. Hub hash
forwarding respects the active locale and site base.
