# Site and Authoring Contract

## Routes

The origin and project prefix are defined in docs/astro.config.mjs. English uses
the prefix root and Russian adds ru/. Each active route has both language versions.
Only published content pages are routes; discarded page layouts have no redirects.

## Homepage

| Anchor | English title | Russian title | Destination |
| --- | --- | --- | --- |
| create-mission | Create a mission | Как создать миссию | create-mission/ |
| example-mission | Open an existing mission | Как открыть существующую миссию | example-mission/ |
| git | Git and Arma Reforger | Git и Arma Reforger | git/ |
| triad-tactics | Missions for Triad Tactics | Миссии для Triad Tactics | setup and requirements child links |

The child routes are triad-tactics/setup/ and triad-tactics/requirements/.
There is no intermediate Triad Tactics landing page or API entry.

The homepage title is Lite Lobby guide / Руководство Lite Lobby. One centered list
contains four aligned semantic entries; headings own the four anchors. The first
three rows link to guides. The Triad Tactics row contains its two child links.
Project introduction precedes the list; help follows it. Neither belongs in the list.
The homepage has no article sidebar or on-page contents panel.

## Course and supporting material

create-mission/ is the introduction to a single course. Its three parts group eight
chapters, in the sequence stored in docs/src/data/chapters.json. Parts do not add
landing pages. The sidebar follows course → parts → chapters. H2 and H3 headings
provide on-page sections, and native pagination follows the course sequence.

The course covers project creation, world setup, a shared character base, armed
variants, character/group editor registration, playable units, objectives and
publication. The introduction recommends reading chapters in order and opening the
finished example alongside them. Git remains optional and is explained through
restoring a working version and saving time.

The separate Triad Tactics sidebar group contains setup and requirements. Additional
material contains example-mission/, git/, character-prefabs/planning/ and
character-prefabs/prefab-operations/. Each supplementary page starts with a localized
link back to the course contents and has no previous/next pagination.

Pages explain who needs them and when. Keep context when linking to another chapter;
do not duplicate its procedure. Crosslinks stay in the current language. Triad
Shooting Star links to its repository when mentioned as the mission example;
filenames, field values and screenshot labels remain literal.

## Presentation and accessibility

Use plain technical-book prose and concrete verbs. Define unfamiliar terms where
they are introduced. Preserve established community terms such as «префаб» and
«фризтайм», and literal UI labels such as Arma Reforger Tools. Omit filler, generic
encouragement, narration comments and unfinished content.

Article pages retain native sidebar and contents navigation. On wide screens the
contents panel is bounded to 16rem; the search field is centered from 64rem upward.
Below 50rem, the homepage menu exposes native language/theme selectors and community
links through a compact popover. Escape, outside clicks and crossing to desktop
close it. Search remains visible. Touch controls have 44px targets with 20px icons.
Community URLs come from Astro configuration; external social actions open a new tab
with appropriate rel attributes. Both themes retain readable contrast.

The project introduction retains attribution, source and APL links. The help panel
uses the configured Discord invite, follows content in normal flow and is excluded
from search. An empty native footer is omitted. Pagination precedes help on course
pages. Short pages do not gain unnecessary scrolling from viewport-filling layout.

## Figures

LL_Screenshot requires src, alt, caption and originalLabel. Source images are
imported from assets/screenshots/<topic>/; both languages share them. Topic folders
and descriptive filenames use lowercase kebab-case; capture numbers have three digits.
Renaming preserves source bytes and updates imports and inventory together.

Images use constrained responsive layout, intrinsic dimensions and lazy loading;
small images do not upscale. Both the image and caption action open the unchanged
original in a new tab and have the same accessible name. Organizational diagrams
use their figcaption as the accessible figure name.

## Maintenance and publication

[Quickstart](../quickstart.md) defines toolchain policy and commands. Validation must
pass for normal output and isolated fixtures, including links, locale boundaries,
images, search and responsive controls. Temporary content never enters publication
and is cleaned after failure or cancellation.

Relevant main pushes publish only after validation succeeds. Pull requests validate
without deploying. Manual runs support validation alone or publication of main.
Failure captures are retained by CI; the Pages artifact contains only normal dist.
