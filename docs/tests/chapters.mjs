import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { join } from 'node:path';
import { load } from 'cheerio';
import chapters from '../src/data/chapters.json' with { type: 'json' };
import fragments from '../src/data/guide-fragments.json' with { type: 'json' };

const origin = 'https://jumbo13th.github.io';
const base = '/lite-lobby-ar/';
const course = ['project', 'world', 'base-character', 'character-variants', 'character-editor', 'groups-and-slots', 'objectives', 'publishing']
  .map((slug) => `create-mission/${slug}`);

export async function validateChapters(output) {
  const entries = [...chapters.hubs, ...chapters.chapters];
  const hubSlugs = new Set(chapters.hubs.map(({ slug }) => slug));
  assert.equal(new Set(entries.map(({ slug }) => slug)).size, entries.length, 'Chapter routes must be unique');
  assert.deepEqual(chapters.sequence, ['create-mission', ...course], 'The course must remain one introduction and eight consecutive chapters');
  assert.deepEqual(chapters.hubs.map(({ slug }) => slug), ['create-mission'], 'The guide must have one course overview');
  assert.deepEqual(chapters.chapters.filter(({ part }) => part).map(({ slug }) => slug), course,
    'The sidebar and course must list the same eight chapters');
  const screenshotsByLocale = [];

  for (const locale of ['en', 'ru']) {
    const prefix = `${base}${locale === 'ru' ? 'ru/' : ''}`;
    const pathFor = (slug) => `${prefix}${slug}/`;
    const pages = new Map(await Promise.all(entries.map(async ({ slug }) => [
      slug, load(await readFile(join(output, locale === 'ru' ? 'ru' : '', slug, 'index.html'), 'utf8')),
    ])));
    const linksTo = ($, selector, slug, target) => $(selector).toArray().some((node) =>
      new URL($(node).attr('href'), `${origin}${pathFor(slug)}`).pathname === pathFor(target));
    const screenshots = [];

    for (const { slug, title, parent } of entries) {
      const $ = pages.get(slug);
      assert.equal($('h1').text().trim(), title[locale], `Chapter title differs from navigation: ${locale}/${slug}`);
      assert.equal($('.sidebar-pane a[aria-current="page"]').attr('href'), pathFor(slug), `Sidebar lost the current chapter: ${locale}/${slug}`);
      const originals = $('.sl-markdown-content figure > a.original').toArray().map((node) => $(node).attr('href'));
      if (hubSlugs.has(slug)) assert.equal(originals.length, 0, `An overview must not duplicate the illustrated course: ${locale}/${slug}`);
      screenshots.push(...originals);
      if (parent) {
        assert.ok(hubSlugs.has(parent), `Missing parent overview for ${slug}`);
        assert.ok(linksTo($, '.sl-markdown-content a[href]', slug, parent), `Chapter has no link back to its overview: ${locale}/${slug}`);
        assert.ok(linksTo(pages.get(parent), '.sl-markdown-content a[href]', parent, slug), `Overview does not list its chapter: ${locale}/${slug}`);
      }
      const index = chapters.sequence.indexOf(slug);
      for (const [relation, offset] of [['prev', -1], ['next', 1]]) {
        const selector = `.pagination-links a[rel="${relation}"]`;
        const target = index < 0 ? undefined : chapters.sequence[index + offset];
        assert.equal($(selector).length, target ? 1 : 0, `Unexpected ${relation} link: ${locale}/${slug}`);
        if (target) assert.ok(linksTo($, selector, slug, target), `Reading sequence breaks at ${locale}/${slug} (${relation})`);
      }
    }
    // These are the illustrations in the completed guides before they were split.
    assert.equal(screenshots.length, 116, `An existing guide illustration was lost or duplicated: ${locale}`);
    assert.equal(new Set(screenshots).size, 116, `A screenshot was substituted with a duplicate: ${locale}`);
    screenshotsByLocale.push(screenshots.sort());

    // Chapters follow the work on one mission. Related settings are subsections
    // of those chapters, rather than competing page-by-page walkthroughs.
    const taskSections = {
      'create-mission/project': ['preparation', 'workbench-project', 'missing-addon-dependencies'],
      'create-mission/world': ['save-world', 'systems-layer', 'lite-lobby-game-mode', 'map-setup'],
      'create-mission/base-character': ['create-base-prefab', 'base-outfit', 'radio-setup', 'initial-inventory', 'wristwatch'],
      'create-mission/character-variants': ['oda-base', 'armor-and-webbing', 'primary-weapon', 'grenade-slots', 'oda-ammunition', 'flashlight', 'magazine-pouches', 'reserve-magazines', 'comms-sergeant', 'backpack-radio', 'comms-uniform', 'storage-after-changes', 'detachment-commander', 'commander-launcher', 'commander-binoculars'],
      'create-mission/character-editor': ['create-string-table', 'register-localization', 'commander-name', 'comms-role', 'localized-character-name', 'commander-role', 'override-character-catalog', 'test-characters', 'create-group-prefab', 'group-members', 'register-group', 'test-group'],
      'create-mission/groups-and-slots': ['playable-characters', 'group-callsigns', 'freeze-zone'],
      'create-mission/objectives': ['map-markings', 'mission-conditions', 'capture-area', 'capture-settings', 'critical-losses', 'mission-timer', 'supremacy', 'mission-briefing'],
      'create-mission/publishing': ['final-test', 'multiplayer-test', 'publish-mission'],
    };
    for (const [slug, ids] of Object.entries(taskSections)) {
      const $ = pages.get(slug);
      for (const id of ids) assert.equal($(`.sl-markdown-content [id="${id}"]`).length, 1, `Topic lost its section: ${locale}/${slug}#${id}`);
    }
    for (const id of ['oda-base', 'comms-sergeant', 'detachment-commander']) {
      assert.equal(pages.get('create-mission/base-character')(`.sl-markdown-content [id="${id}"]`).length, 0, `Base character prematurely introduces ${id}`);
    }
    for (const id of ['group-prefab', 'group-members', 'group-catalog']) {
      assert.equal(pages.get('create-mission/groups-and-slots')(`.sl-markdown-content [id="${id}"]`).length, 0,
        `Group creation must finish in the character part before world placement: ${id}`);
    }
    assert.equal(chapters.chapters.find(({ slug }) => slug === 'create-mission/character-editor').part, 'characters');
    assert.equal(pages.get('create-mission/character-editor')('.sl-markdown-content #playable-forces').length, 0,
      'World placement belongs in the mission part');
    for (const slug of ['base-character', 'character-variants']) {
      const content = pages.get(`create-mission/${slug}`)('.sl-markdown-content');
      for (const field of ['SCR_EditableCharacterComponent', 'Authored Labels', 'ROLE_RADIOOPERATOR', 'ROLE_LEADER']) {
        assert.ok(!content.text().includes(field), `Editor display setting belongs outside the equipment task: ${slug} (${field})`);
      }
    }
    for (const [slug, ordered] of [
      ['base-character', ['create-base-prefab', 'base-outfit', 'radio-setup', 'initial-inventory', 'wristwatch']],
      ['character-variants', ['oda-base', 'primary-weapon', 'oda-ammunition', 'comms-sergeant', 'backpack-radio', 'comms-uniform', 'storage-after-changes', 'detachment-commander']],
      ['character-editor', ['create-string-table', 'register-localization', 'localized-character-name', 'override-character-catalog', 'test-characters', 'create-group-prefab', 'group-members', 'register-group', 'test-group']],
      ['groups-and-slots', ['playable-forces', 'playable-characters', 'group-callsigns', 'freeze-zone']],
      ['objectives', ['capture-area', 'capture-settings', 'mission-briefing']],
    ]) {
      const $ = pages.get(`create-mission/${slug}`);
      const ids = $('.sl-markdown-content [id]').toArray().map((node) => $(node).attr('id'));
      const positions = ordered.map((id) => ids.indexOf(id));
      assert.ok(positions.every((position, index) => position >= 0 && (index === 0 || position > positions[index - 1])),
        `The chapter introduces a task before its preparation: ${locale}/${slug} (${ordered.join(' → ')})`);
    }
    const overview = pages.get('create-mission');
    for (const id of ['preparation-part', 'characters-part', 'mission-part']) {
      assert.equal(overview(`.sl-markdown-content [id="${id}"]`).length, 1, `The course overview needs its part heading: ${id}`);
    }
    const $ = pages.get('create-mission/project');
    const courseTitle = entries.find(({ slug }) => slug === 'create-mission').title[locale];
    const branch = $('.sidebar-pane summary').filter((_, node) => $(node).text().trim() === courseTitle).parent('details');
    assert.equal(branch.length, 1, 'The sidebar needs one main course branch');
    assert.equal(branch.find('details').length, 3, 'The course should be organized into three parts');
    const sidebarRoutes = branch.find('a[href]').toArray().map((node) => new URL($(node).attr('href'), origin).pathname);
    assert.deepEqual(sidebarRoutes, chapters.sequence.map(pathFor), 'The course sidebar must follow the reading sequence');
    for (const part of chapters.parts) {
      const partBranch = branch.find('summary').filter((_, node) => $(node).text().trim() === part.title[locale]).parent('details');
      assert.equal(partBranch.length, 1, `Missing course part: ${part.title[locale]}`);
      const members = chapters.chapters.filter((chapter) => chapter.part === part.id).map(({ slug }) => pathFor(slug));
      const actual = partBranch.find('a[href]').toArray().map((node) => new URL($(node).attr('href'), origin).pathname);
      assert.deepEqual(actual, members, `The part contains the wrong chapters: ${part.title[locale]}`);
    }
    for (const retiredHub of ['character-prefabs', 'character-prefabs/characters', 'character-prefabs/groups']) {
      assert.ok(!linksTo($, '.sidebar-pane a[href]', 'create-mission/project', retiredHub), `A former walkthrough still competes with the course: ${retiredHub}`);
    }

    for (const [guide, targets] of Object.entries(fragments)) {
      const $ = pages.get(guide) ?? load(await readFile(join(output, locale === 'ru' ? 'ru' : '', guide, 'index.html'), 'utf8'));
      const renderedMap = JSON.parse($('[data-ll-guide-fragments]').attr('data-ll-guide-fragments'));
      if (pages.has(guide)) assert.equal(targets[''], undefined, `An active chapter must not redirect on entry: ${guide}`);
      if (!pages.has(guide)) {
        assert.equal($('.sidebar-pane a[aria-current="page"]').length, 0, `Retired chapter should not appear in navigation: ${guide}`);
        assert.ok($('meta[name="robots"]').attr('content')?.includes('noindex'), `Retired chapter must not be indexed: ${guide}`);
        assert.equal($('.sl-markdown-content figure').length, 0, `Retired chapter duplicated content: ${guide}`);
        assert.ok(targets[''], `Retired chapter needs a default destination: ${guide}`);
      }
      for (const [id, destination] of Object.entries(targets)) {
        assert.equal(renderedMap[id], `${prefix}${destination}`, `Legacy link lost its language or base: ${locale}/${guide}#${id}`);
        const [route, anchor] = destination.split('#');
        const target = pages.get(route.replace(/\/$/, ''));
        assert.ok(target, `Legacy link points to an unknown chapter: ${destination}`);
        if (anchor) assert.ok(target('[id]').toArray().some((node) => target(node).attr('id') === anchor), `Legacy link points to a missing heading: ${destination}`);
      }
    }
  }
  assert.deepEqual(...screenshotsByLocale, 'The translations must retain the same guide illustrations');
  const supplementary = ['example-mission', 'git', 'character-prefabs/planning', 'character-prefabs/prefab-operations'];
  const activeRoutes = [...new Set([
    ...entries.map(({ slug }) => slug), ...supplementary,
    'triad-tactics/setup', 'triad-tactics/requirements',
  ])];
  for (const slug of activeRoutes) {
    const headings = [];
    for (const locale of ['', 'ru']) {
      const prefix = `/lite-lobby-ar/${locale ? 'ru/' : ''}`;
      const $ = load(await readFile(join(output, locale, slug, 'index.html'), 'utf8'));
      headings.push($('.sl-markdown-content h2, .sl-markdown-content h3').toArray().map((node) => $(node).attr('id')));
      const current = $('.sidebar-pane a[aria-current="page"]');
      assert.equal(current.length, 1, `Active page missing from sidebar: ${locale}/${slug}`);
      // The overview is intentionally called Introduction in the sidebar.
      if (slug !== 'create-mission') assert.equal(current.text().trim(), $('h1').text().trim(), `Sidebar title differs from page title: ${locale}/${slug}`);
      if (supplementary.includes(slug)) {
        const first = $('.sl-markdown-content').children().first();
        assert.equal(first.text().trim(), locale ? 'К оглавлению руководства' : 'Back to the guide contents', `Missing top return link: ${locale}/${slug}`);
        assert.equal(new URL(first.find('a').attr('href'), `https://example.test${prefix}${slug}/`).pathname, `${prefix}create-mission/`);
        assert.equal($('.pagination-links a').length, 0, `Supplementary page must not imply a chapter sequence: ${locale}/${slug}`);
      }
      if (slug === 'create-mission') {
        const related = $('#further-explanations').closest('.sl-heading-wrapper').next('ul');
        const destinations = related.find('a').toArray().map((node) => new URL($(node).attr('href'), `https://example.test${prefix}${slug}/`).pathname);
        assert.deepEqual(destinations, supplementary.map((page) => `${prefix}${page}/`), `Overview and sidebar supplementary lists differ: ${locale}`);
      }
    }
    assert.deepEqual(headings[0], headings[1], `Section anchors differ between translations: ${slug}`);
  }
  for (const [slug, numbers] of [
    ['git', Array.from({ length: 11 }, (_, index) => String(index + 1))],
    ['example-mission', ['07', '08', '09', '10', '11', '12', '13', '01', '02', '03', '04', '06']],
  ]) {
    const byLocale = [];
    for (const locale of ['', 'ru']) {
      const $ = load(await readFile(join(output, locale, slug, 'index.html'), 'utf8'));
      const originals = $('.sl-markdown-content figure > a.original').toArray().map((node) => $(node).attr('href'));
      assert.equal(new Set(originals).size, numbers.length, `Missing or repeated ${slug} illustration: ${locale}`);
      assert.deepEqual(originals.map((url) => url.split('/').pop().split('.')[0]), numbers.map((number) => number.padStart(3, '0')), `${slug} illustrations are out of order: ${locale}`);
      byLocale.push(originals);
      if (slug === 'example-mission') {
        const prefix = `/lite-lobby-ar/${locale ? 'ru/' : ''}`;
        assert.equal($('.sl-markdown-content h2').first().attr('id'), 'open-project', 'The GitHub example must come first');
        assert.equal($('.sl-markdown-content a[href="../create-mission/project/#missing-addon-dependencies"]').length, 2, 'Both examples must link to the existing addon recovery steps');
        for (const id of ['open-project', 'missing-dependencies', 'open-world']) assert.equal($(`[id="${id}"]`).length, 1);
        assert.ok($('.sl-markdown-content a[href="../create-mission/"]').length, `${prefix}${slug} must link to the mission course`);
      }
    }
    assert.deepEqual(byLocale[0], byLocale[1], `${slug} screenshots differ between locales`);
  }
  console.log(`Validated ${chapters.hubs.length} overviews, ${chapters.chapters.length} chapters, reading order, legacy destinations, and all course and supporting-guide illustrations in each language`);
}
