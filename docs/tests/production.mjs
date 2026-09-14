import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdir } from 'node:fs/promises';
import { join } from 'node:path';
import { chromium } from 'playwright';
import { fixtureSlug, fixtureText } from './fixtures.mjs';
import chapters from '../src/data/chapters.json' with { type: 'json' };

const base = '/lite-lobby-ar/';
const home = (locale) => `${base}${locale === 'ru' ? 'ru/' : ''}`;
const route = (locale, article) => `${home(locale)}${article ? `${fixtureSlug}/` : ''}`;
const guideOrder = ['create-mission', 'example-mission', 'git', 'triad-tactics'];
const anchors = [...guideOrder];
const guidePages = new Map([...chapters.hubs, ...chapters.chapters].map((entry) => [entry.slug, entry]));

async function chapterTitle(page, locale, slug) {
  assert.equal((await page.locator('h1').textContent()).trim(), guidePages.get(slug).title[locale], `Unexpected ${locale} chapter title: ${slug}`);
}

async function noOverflow(page) {
  const dimensions = await page.evaluate(() => ({
    content: document.documentElement.scrollWidth,
    viewport: window.innerWidth,
  }));
  assert.ok(dimensions.content <= dimensions.viewport + 1, `Horizontal page overflow: ${JSON.stringify(dimensions)}`);
}

async function visibleControl(page, selector) {
  const control = page.locator(`${selector}:visible`);
  assert.equal(await control.count(), 1, `Expected one visible control: ${selector}`);
  const bounds = await control.boundingBox();
  assert.ok(bounds && bounds.width > 0 && bounds.x >= 0 && bounds.x + bounds.width <= page.viewportSize().width + 1, `Control is clipped: ${selector}`);
  return control;
}

async function openMenu(page) {
  if (page.viewportSize().width >= 800) return;
  const sidebar = page.locator('#starlight__sidebar');
  if (!(await sidebar.evaluate((element) => element.matches(':popover-open')))) {
    await page.locator('button[popovertarget="starlight__sidebar"]').click();
  }
}

async function closeMenu(page) {
  if (page.viewportSize().width >= 800) return;
  const sidebar = page.locator('#starlight__sidebar');
  if (await sidebar.evaluate((element) => element.matches(':popover-open'))) {
    await page.locator('button[popovertarget="starlight__sidebar"]').click();
  }
}

async function mobileMenu(page) {
  const viewport = page.viewportSize();
  const trigger = page.locator('button[popovertarget="starlight__sidebar"]');
  const menu = page.locator('#starlight__sidebar');
  if (viewport.width >= 800) {
    assert.ok(!(await trigger.isVisible()), 'Mobile menu button should be hidden on desktop');
    return;
  }
  const button = await visibleControl(page, 'button[popovertarget="starlight__sidebar"]');
  const buttonBounds = await button.boundingBox();
  const search = page.locator('site-search button[data-open-modal]');
  const searchBounds = await search.boundingBox();
  const titleBounds = await page.locator('a.site-title').boundingBox();
  const isHomeMenu = await menu.evaluate((element) => element.classList.contains('ll-mobile-menu'));
  async function navbarGeometry() {
    const centers = [];
    for (const [name, target] of [['search', search], ['menu', button]]) {
      const bounds = await target.boundingBox();
      assert.ok(bounds && Math.abs(bounds.width - 44) <= 1 && Math.abs(bounds.height - 44) <= 1, `Mobile ${name} needs a 44px touch target`);
      const icon = target.locator('svg:visible');
      assert.equal(await icon.count(), 1, `Mobile ${name} should display one icon`);
      const glyph = await icon.boundingBox();
      assert.ok(glyph && Math.abs(glyph.width - 20) <= 1 && Math.abs(glyph.height - 20) <= 1, `Mobile ${name} needs a 20px glyph`);
      const centerX = glyph.x + glyph.width / 2;
      const centerY = glyph.y + glyph.height / 2;
      assert.ok(Math.abs(centerX - bounds.x - bounds.width / 2) <= 1 && Math.abs(centerY - bounds.y - bounds.height / 2) <= 1, `Mobile ${name} glyph should be centered within its target`);
      centers.push(centerY);
    }
    assert.ok(Math.abs(centers[0] - centers[1]) <= 1, 'Mobile search and menu glyphs should align vertically');
  }
  await navbarGeometry();
  assert.ok((await button.textContent()).trim(), 'Mobile menu button needs an accessible name');
  assert.ok(searchBounds && titleBounds && searchBounds.x >= titleBounds.x + titleBounds.width && searchBounds.x + searchBounds.width <= buttonBounds.x, 'Mobile navbar controls overlap');
  assert.ok(!(await menu.evaluate((element) => element.matches(':popover-open'))), 'Mobile menu should initially be closed');
  assert.equal(await page.locator('starlight-theme-select select:visible, starlight-lang-select select:visible').count(), 0, 'Mobile preferences should be inside the closed menu');
  await button.focus();
  await page.keyboard.press('Enter');
  await page.waitForFunction(() => document.querySelector('#starlight__sidebar')?.matches(':popover-open'));
  await navbarGeometry();
  await page.keyboard.press('Tab');
  assert.ok(await menu.evaluate((element) => element.contains(document.activeElement)), 'Keyboard navigation should enter the open menu');
  const socialBounds = [];
  for (const name of ['Discord', 'GitHub', 'Telegram']) {
    const link = menu.getByRole('link', { name, exact: true });
    assert.ok(await link.isVisible(), `${name} should be available in the mobile menu`);
    await link.focus();
    const bounds = await link.boundingBox();
    assert.ok(bounds && bounds.x >= 0 && bounds.x + bounds.width <= viewport.width + 1 && bounds.y >= 0 && bounds.y + bounds.height <= viewport.height + 1, `${name} is clipped in the mobile menu`);
    if (isHomeMenu) {
      assert.ok(Math.abs(bounds.width - 44) <= 1 && Math.abs(bounds.height - 44) <= 1, `${name} needs a 44px mobile touch target`);
      const glyph = await link.locator('svg:visible').boundingBox();
      assert.ok(glyph && Math.abs(glyph.width - 20) <= 1 && Math.abs(glyph.height - 20) <= 1, `${name} needs a 20px mobile glyph`);
      assert.ok(Math.abs(glyph.x + glyph.width / 2 - bounds.x - bounds.width / 2) <= 1 && Math.abs(glyph.y + glyph.height / 2 - bounds.y - bounds.height / 2) <= 1, `${name} glyph should be centered within its target`);
      socialBounds.push(bounds);
    }
  }
  if (isHomeMenu) {
    const popup = await menu.boundingBox();
    const rem = await page.locator('html').evaluate((element) => parseFloat(getComputedStyle(element).fontSize));
    assert.ok(popup && popup.width <= 14 * rem + 1, 'Homepage mobile menu should stay within its compact 14rem width');
    socialBounds.sort((left, right) => left.x - right.x);
    const gaps = socialBounds.slice(1).map((bounds, index) => bounds.x - socialBounds[index].x - socialBounds[index].width);
    assert.ok(gaps.every((gap) => gap >= 0) && Math.abs(gaps[0] - gaps[1]) <= 1, 'Mobile social targets should have equal gaps without overlap');
    assert.ok(socialBounds.every((bounds) => Math.abs(bounds.y - socialBounds[0].y) <= 1), 'Mobile social targets should share one horizontal row');
    const groupCenter = (socialBounds[0].x + socialBounds[2].x + socialBounds[2].width) / 2;
    assert.ok(Math.abs(groupCenter - popup.x - popup.width / 2) <= 1, 'Mobile social targets should be centered in the popup');
  }
  for (const selector of ['starlight-theme-select select', 'starlight-lang-select select']) {
    const control = await visibleControl(page, selector);
    await control.focus();
    const bounds = await control.boundingBox();
    assert.ok(bounds && bounds.y >= 0 && bounds.y + bounds.height <= viewport.height + 1, 'Mobile preference is clipped vertically');
  }
  await page.keyboard.press('Escape');
  await page.waitForFunction(() => !document.querySelector('#starlight__sidebar')?.matches(':popover-open'));
  assert.ok(await button.evaluate((element) => element === document.activeElement), 'Escape should return focus to the menu button');
  await page.keyboard.press('Enter');
  await page.waitForFunction(() => document.querySelector('#starlight__sidebar')?.matches(':popover-open'));
  await page.mouse.click(titleBounds.x + titleBounds.width + 8, buttonBounds.y + buttonBounds.height / 2);
  await page.waitForFunction(() => !document.querySelector('#starlight__sidebar')?.matches(':popover-open'));
  await page.setViewportSize({ ...viewport, width: 799 });
  await openMenu(page);
  await noOverflow(page);
  await page.setViewportSize({ ...viewport, width: 800 });
  await page.waitForFunction(() => !document.querySelector('#starlight__sidebar')?.matches(':popover-open'));
  assert.ok(!(await trigger.isVisible()), 'Menu button should hide at the desktop breakpoint');
  await visibleControl(page, 'starlight-theme-select select');
  await visibleControl(page, 'starlight-lang-select select');
  await noOverflow(page);
  await page.setViewportSize({ ...viewport, width: 799 });
  assert.ok(await trigger.isVisible(), 'Menu button should return below the desktop breakpoint');
  assert.ok(!(await menu.evaluate((element) => element.matches(':popover-open'))), 'Returning to mobile should keep the menu closed');
  assert.equal(await page.locator('starlight-theme-select select:visible, starlight-lang-select select:visible').count(), 0, 'Returning to mobile should keep preferences hidden until the menu opens');
  await page.setViewportSize(viewport);
}

async function localeAndTheme(page, locale, article) {
  await openMenu(page);
  const theme = await visibleControl(page, 'starlight-theme-select select');
  await theme.focus();
  assert.ok(await theme.evaluate((element) => element === document.activeElement), 'Theme selector is not keyboard focusable');
  for (const value of ['dark', 'light']) {
    await theme.selectOption(value);
    await page.waitForFunction((expected) => document.documentElement.dataset.theme === expected, value);
  }

  for (const destination of [locale === 'ru' ? 'en' : 'ru', locale]) {
    await openMenu(page);
    const selector = await visibleControl(page, 'starlight-lang-select select');
    await selector.focus();
    assert.ok(await selector.evaluate((element) => element === document.activeElement), 'Language selector is not keyboard focusable');
    const destinationPath = route(destination, article);
    const option = await selector.locator('option').evaluateAll((options, path) => options.find((item) => new URL(item.value, location.href).pathname === path)?.value, destinationPath);
    assert.ok(option, `No language option points to ${destinationPath}`);
    await selector.selectOption(option);
    await page.waitForURL((url) => url.pathname === destinationPath);
    await page.waitForFunction((expected) => document.documentElement.lang === expected, destination);
    assert.equal(await page.locator('html').getAttribute('data-theme'), 'light', 'Theme did not survive language navigation');
  }
  await closeMenu(page);
}

async function navigation(page, locale, article) {
  if (!article) await page.goto(new URL(`${home(locale)}#create-mission`, page.url()).href);
  if (article) await openMenu(page);
  const homeLink = article
    ? page.locator(`.sidebar-pane a[href="${home(locale)}"]`).first()
    : page.locator(`a.site-title[href="${home(locale)}"]`);
  assert.ok(await homeLink.isVisible(), 'Homepage navigation is unavailable');
  await homeLink.focus();
  await page.keyboard.press('Enter');
  await page.waitForURL((url) => url.pathname === home(locale) && !url.hash);
  if (article) {
    await page.goto(new URL(route(locale, true), page.url()).href);
    const desktopLink = page.locator('starlight-toc a[href="#details"]:visible');
    let link = desktopLink;
    if (!(await desktopLink.count())) {
      const summary = page.locator('mobile-starlight-toc summary:visible');
      await summary.focus();
      await page.keyboard.press('Enter');
      link = page.locator('mobile-starlight-toc a[href="#details"]:visible');
    }
    await link.focus();
    await page.keyboard.press('Enter');
    await page.waitForURL((url) => url.pathname === route(locale, true) && url.hash === '#details');
    const target = await page.locator('#details').boundingBox();
    assert.ok(target && target.y >= 0 && target.y < page.viewportSize().height, 'Contents link did not bring the article section into view');
  }
  await page.goto(new URL(route(locale, article), page.url()).href);
  await page.keyboard.press('Tab');
  const focusedHref = await page.evaluate(() => document.activeElement?.getAttribute('href'));
  assert.equal(focusedHref, '#_top', 'First keyboard stop should be the skip-to-content link');
  await page.keyboard.press('Enter');
  await page.waitForURL((url) => url.hash === '#_top');
}

async function homepageLayout(page) {
  const list = page.locator('.ll-guide-list');
  const entries = list.locator('article.ll-guide-entry');
  assert.equal(await list.count(), 1, 'Homepage must present one guide list');
  assert.equal(await entries.count(), guideOrder.length, 'Homepage must present exactly four guide entries');
  assert.deepEqual(await entries.locator('h2').evaluateAll((headings) => headings.map((heading) => heading.id)), guideOrder, 'Homepage guides must follow the agreed order');
  assert.equal(await page.locator('.sidebar, .sidebar-pane, starlight-toc, mobile-starlight-toc').count(), 0, 'Homepage should omit article navigation');
  assert.equal(await page.locator('#starlight__sidebar.ll-mobile-menu[popover]').count(), 1, 'Homepage needs its compact mobile menu');
  assert.equal(await page.locator('.ll-home-controls, .ll-mobile-socials').count(), 0, 'Homepage should not duplicate menu controls in the content');
  const locale = await page.locator('html').getAttribute('lang');
  for (const slug of ['create-mission', 'example-mission', 'git']) {
    assert.equal(await page.locator(`#${slug} > a`).getAttribute('href'), `${home(locale)}${slug}/`, `${slug} must link to its localized guide`);
  }
  assert.deepEqual(await page.locator('.ll-section-pages a').evaluateAll((links) => links.map((link) => link.getAttribute('href'))),
    ['setup', 'requirements'].map((slug) => `${home(locale)}triad-tactics/${slug}/`));
  for (const id of anchors) assert.equal(await page.locator(`[id="${id}"]`).count(), 1, `Missing or repeated guide heading: ${id}`);
  const bounds = await entries.evaluateAll((elements) => elements.map((element) => {
    const { x, y, width, height } = element.getBoundingClientRect();
    return { x, y, width, height };
  }));
  const titles = [];
  for (const [index, box] of bounds.entries()) {
    assert.ok(box.width > 0 && box.height > 0, `Guide entry ${index + 1} is empty or hidden`);
    const title = await entries.nth(index).locator('h2').boundingBox();
    assert.ok(title && title.x >= box.x && title.x + title.width <= box.x + box.width + 1 && title.y + title.height <= box.y + box.height, `Guide title exceeds entry ${index + 1}`);
    titles.push(title);
    assert.ok(Math.abs(box.x - bounds[0].x) < 1 && Math.abs(box.width - bounds[0].width) < 1, 'Guide entries should share one column');
    assert.ok(Math.abs(title.x - titles[0].x) < 1, 'Guide titles should share their left alignment');
    if (index) assert.ok(box.y >= bounds[index - 1].y + bounds[index - 1].height - 1, 'Guide entries overlap');
  }
  const width = page.viewportSize().width;
  if (width >= 1440) {
    for (const box of bounds.slice(0, 3)) assert.ok(Math.abs(box.height - bounds[0].height) < 1, 'Desktop guide entries without subheadings should have equal heights');
    assert.ok(Math.abs(bounds[0].x - (width - bounds[0].x - bounds[0].width)) < 2, 'Desktop guide list should be centered');
  }
  await noOverflow(page);
}

async function search(page, locale) {
  if (page.viewportSize().width >= 1024) {
    const field = await page.locator('site-search button[data-open-modal]').boundingBox();
    const title = await page.locator('a.site-title').boundingBox();
    const controls = await page.locator('header .right-group').boundingBox();
    assert.ok(field && title && controls, 'Desktop header controls must be visible');
    assert.ok(Math.abs(field.x + field.width / 2 - page.viewportSize().width / 2) <= 1, 'Desktop search should be centered in the viewport');
    assert.ok(field.x >= title.x + title.width && field.x + field.width <= controls.x, 'Desktop search overlaps neighboring controls');
  }
  const query = (await page.locator('#create-mission').textContent()).trim();
  if (page.viewportSize().width < 800) await page.locator('site-search button[data-open-modal]').click();
  else await page.keyboard.press('Control+k');
  const dialog = page.locator('site-search dialog');
  await dialog.waitFor({ state: 'visible' });
  const input = dialog.locator('.pagefind-ui__search-input');
  await input.fill(query);
  const result = dialog.locator('.pagefind-ui__result-link').first();
  await result.waitFor({ state: 'visible', timeout: 20_000 });
  const results = await dialog.locator('.pagefind-ui__result-link').evaluateAll((links) => links.map((link) => link.href));
  assert.ok(results.length > 0, `No ${locale} search results for ${query}`);
  for (const href of results) {
    const pathname = new URL(href).pathname;
    assert.ok(pathname.startsWith(home(locale)), `Search result is outside ${locale}: ${href}`);
    if (locale === 'en') assert.ok(!pathname.startsWith(home('ru')), `Russian result in English search: ${href}`);
  }
  await input.press('Escape');
  await dialog.waitFor({ state: 'hidden' });
  await page.locator('site-search button[data-open-modal]').click();
  await input.fill(query);
  await result.waitFor({ state: 'visible', timeout: 20_000 });
  const destination = new URL(await result.getAttribute('href'), page.url());
  await result.click();
  await page.waitForURL((url) => url.pathname === destination.pathname && url.hash === destination.hash);
  await dialog.waitFor({ state: 'hidden' });
  assert.equal(await page.locator('html').getAttribute('lang'), locale, 'Search result changed the page language');
}

async function missionNavigation(page, locale) {
  await page.goto(new URL(home(locale), page.url()).href);
  const missionLink = page.locator('#create-mission > a');
  await missionLink.focus();
  await page.keyboard.press('Enter');
  await page.waitForURL((url) => url.pathname === `${home(locale)}create-mission/`);
  await chapterTitle(page, locale, 'create-mission');
  assert.equal(await page.locator('.sl-markdown-content #reading-paths').count(), 1, 'The overview needs both learning routes');
  await page.locator('.sl-markdown-content a[href="project/"]').click();
  await page.waitForURL((url) => url.pathname === `${home(locale)}create-mission/project/`);
  assert.equal(await page.locator('.sl-markdown-content h2#preparation').count(), 1, 'Preparation belongs in the project chapter');
  await noOverflow(page);
  await openMenu(page);
  const destination = locale === 'ru' ? 'en' : 'ru';
  const destinationPath = `${home(destination)}create-mission/project/`;
  const selector = await visibleControl(page, 'starlight-lang-select select');
  const option = await selector.locator('option').evaluateAll((options, path) => options.find((item) => new URL(item.value, location.href).pathname === path)?.value, destinationPath);
  assert.ok(option, `No language option points to ${destinationPath}`);
  await selector.focus();
  await selector.selectOption(option);
  await page.waitForURL((url) => url.pathname === destinationPath);
  assert.equal(await page.locator('html').getAttribute('lang'), destination);
  await chapterTitle(page, destination, 'create-mission/project');
  await noOverflow(page);
  await openMenu(page);
  const homeLink = page.locator(`.sidebar-pane a[href="${home(destination)}"]`).first();
  assert.ok(await homeLink.isVisible(), 'The mission guide needs a visible homepage link');
  await homeLink.focus();
  await page.keyboard.press('Enter');
  await page.waitForURL((url) => url.pathname === home(destination) && !url.hash);
  await page.goto(new URL(home(locale), page.url()).href);
}

async function chapterJourneys(page, locale, resultsDirectory) {
  const origin = new URL(page.url()).origin;
  const path = (slug) => `${home(locale)}${slug}/`;
  async function follow(selector, slug) {
    await page.locator(selector).first().click();
    await page.waitForURL((url) => url.pathname === path(slug));
    if (guidePages.has(slug)) await chapterTitle(page, locale, slug);
    await noOverflow(page);
  }
  async function sidebarChapter(slug) {
    await openMenu(page);
    const link = page.locator(`.sidebar-pane a[href="${path(slug)}"]`);
    assert.equal(await link.count(), 1, `Expected one sidebar entry for ${slug}`);
    const ancestors = await link.evaluate((element) => {
      const all = [...document.querySelectorAll('.sidebar-pane details')];
      const indices = [];
      for (let parent = element.parentElement; parent; parent = parent.parentElement) {
        if (parent.tagName === 'DETAILS') indices.unshift(all.indexOf(parent));
      }
      return indices;
    });
    for (const index of ancestors) {
      const branch = page.locator('.sidebar-pane details').nth(index);
      if (!(await branch.evaluate((element) => element.open))) await branch.locator(':scope > summary').click();
    }
    assert.equal((await link.textContent()).trim(), guidePages.get(slug).title[locale]);
    await link.click();
    await page.waitForURL((url) => url.pathname === path(slug));
    await closeMenu(page);
    await chapterTitle(page, locale, slug);
    await noOverflow(page);
  }
  async function sectionInSameChapter(slug, anchor) {
    const marker = `${slug}:${anchor}`;
    await page.evaluate((value) => { window.__llCourseChapter = value; }, marker);
    let link = page.locator(`starlight-toc a[href="#${anchor}"]:visible`);
    if (!(await link.count())) {
      const summary = page.locator('mobile-starlight-toc summary:visible');
      if (!(await summary.locator('..').evaluate((element) => element.open))) await summary.click();
      link = page.locator(`mobile-starlight-toc a[href="#${anchor}"]:visible`);
    }
    assert.equal(await link.count(), 1, `The chapter contents must include ${anchor}`);
    await link.click();
    await page.waitForURL((url) => url.pathname === path(slug) && url.hash === `#${anchor}`);
    assert.equal(await page.evaluate(() => window.__llCourseChapter), marker, 'A chapter subsection must not load another document');
    const target = await page.locator(`.sl-markdown-content [id="${anchor}"]`).boundingBox();
    assert.ok(target && target.y >= 0 && target.y < page.viewportSize().height, `Contents link did not reach ${anchor}`);
  }

  await page.goto(`${origin}${path('create-mission')}`);
  for (const id of ['preparation-part', 'characters-part', 'mission-part']) {
    assert.equal(await page.locator(`.sl-markdown-content [id="${id}"]`).count(), 1, `Missing course part: ${id}`);
  }
  for (const chapter of chapters.chapters) {
    const entry = page.locator(`.sidebar-pane a[href="${path(chapter.slug)}"]`);
    assert.equal(await entry.count(), 1, `Missing sidebar chapter: ${chapter.slug}`);
    assert.equal((await entry.textContent()).trim(), chapter.title[locale], `Unexpected sidebar chapter title: ${chapter.slug}`);
  }
  for (const retired of [
    'character-prefabs', 'character-prefabs/characters', 'character-prefabs/groups',
    'character-prefabs/characters/base', 'character-prefabs/characters/radios',
    'create-mission/conditions', 'create-mission/finishing',
  ]) {
    assert.equal(await page.locator(`.sidebar-pane a[href="${path(retired)}"]`).count(), 0, `A former walkthrough remains in the sidebar: ${retired}`);
  }

  // One reading route runs through all eight chapters, without intermediate topic hubs.
  for (const slug of chapters.sequence.slice(1)) {
    await follow('.pagination-links a[rel="next"]', slug);
    if (slug === 'create-mission/base-character') {
      assert.equal(await page.locator('.sl-markdown-content #base-outfit').count(), 1, 'The base chapter should include dressing the character');
      assert.equal(await page.locator('.sl-markdown-content #wristwatch').count(), 1, 'Finish the shared personal equipment in the same chapter');
      for (const id of ['oda-base', 'comms-sergeant', 'detachment-commander']) {
        assert.equal(await page.locator(`.sl-markdown-content [id="${id}"]`).count(), 0, `A later character appears before the shared base is ready: ${id}`);
      }
      await sectionInSameChapter(slug, 'radio-setup');
    }
    if (slug === 'create-mission/character-variants') {
      for (const id of ['oda-base', 'comms-sergeant', 'detachment-commander']) {
        assert.equal(await page.locator(`.sl-markdown-content [id="${id}"]`).count(), 1, `Missing inherited character: ${id}`);
      }
      assert.equal(await page.locator('.sl-markdown-content #comms-role').count(), 0, 'Editor display should not interrupt the equipment chapter');
    }
    if (slug === 'create-mission/character-editor') {
      await sectionInSameChapter(slug, 'comms-role');
      await sectionInSameChapter(slug, 'group-prefab');
    }
  }
  assert.equal(await page.locator('.pagination-links a[rel="next"]').count(), 0, 'The final chapter should conclude the course');

  await page.goto(`${origin}${path('create-mission')}`);
  await follow('.sl-markdown-content a[href="base-character/"]', 'create-mission/base-character');
  await sidebarChapter('create-mission/groups-and-slots');
  await openMenu(page);
  const current = page.locator('.sidebar-pane a[aria-current="page"]');
  await current.scrollIntoViewIfNeeded();
  assert.ok(await current.isVisible(), 'The current chapter should remain visible in its course part');
  const parentLabels = await current.evaluate((link) => {
    const labels = [];
    for (let parent = link.parentElement; parent; parent = parent.parentElement) {
      if (parent.tagName === 'DETAILS') {
        if (!parent.open) return [];
        labels.unshift(parent.querySelector(':scope > summary').textContent.trim());
      }
    }
    return labels;
  });
  assert.deepEqual(parentLabels, [
    guidePages.get('create-mission').title[locale],
    chapters.parts.find(({ id }) => id === 'mission').title[locale],
  ], 'The current chapter should be nested under its course and part');
  await noOverflow(page);
  await page.screenshot({ path: join(resultsDirectory, `${locale}-course-sidebar-${page.viewportSize().width}.png`) });
  await closeMenu(page);
  const footerOrder = await page.evaluate(() => {
    const content = document.querySelector('.sl-markdown-content').getBoundingClientRect();
    const pagination = document.querySelector('.pagination-links').getBoundingClientRect();
    const help = document.querySelector('.ll-help').getBoundingClientRect();
    return { gap: pagination.top - content.bottom, follows: help.top >= pagination.bottom };
  });
  assert.ok(footerOrder.follows && footerOrder.gap >= 0 && footerOrder.gap <= 48,
    `Chapter pagination should follow the article before the help panel: ${JSON.stringify(footerOrder)}`);
  await follow('.pagination-links a[rel="prev"]', 'create-mission/character-editor');
  await follow('.pagination-links a[rel="next"]', 'create-mission/groups-and-slots');

  await page.goto(`${origin}${path('create-mission/character-variants')}`);
  for (const destination of [locale === 'ru' ? 'en' : 'ru', locale]) {
    await openMenu(page);
    const selector = await visibleControl(page, 'starlight-lang-select select');
    const destinationPath = `${home(destination)}create-mission/character-variants/`;
    const option = await selector.locator('option').evaluateAll((options, pathname) => options.find((item) => new URL(item.value, location.href).pathname === pathname)?.value, destinationPath);
    assert.ok(option, `No language option for the variants chapter: ${destinationPath}`);
    await selector.selectOption(option);
    await page.waitForURL((url) => url.pathname === destinationPath);
    assert.equal(await page.locator('html').getAttribute('lang'), destination);
    await chapterTitle(page, destination, 'create-mission/character-variants');
    await closeMenu(page);
  }

  // Old page and fragment bookmarks still reach the corresponding place in the course.
  for (const [guide, anchor, chapter, targetAnchor = anchor] of [
    ['create-mission', 'save-world', 'create-mission/world'],
    ['character-prefabs', '', 'create-mission', 'characters-part'],
    ['character-prefabs', 'group-members', 'create-mission/character-editor'],
    ['character-prefabs/characters', '', 'create-mission/base-character'],
    ['character-prefabs/groups', '', 'create-mission/character-editor', 'group-prefab'],
    ['create-mission/groups-and-slots', 'group-members', 'create-mission/character-editor'],
    ['character-prefabs/characters/radioman', 'backpack-radio', 'create-mission/character-variants'],
    ['character-prefabs/characters/radios', 'backpack-radio', 'create-mission/character-variants'],
    ['character-prefabs/characters/commander', 'commander-launcher', 'create-mission/character-variants'],
    ['character-prefabs/characters/base', 'oda-base', 'create-mission/character-variants'],
    ['character-prefabs/characters/radio-operator', 'comms-role', 'create-mission/character-editor'],
    ['character-prefabs/characters/additional-equipment', 'commander-role', 'create-mission/character-editor'],
    ['character-prefabs/characters/localization', 'localized-character-name', 'create-mission/character-editor'],
    ['create-mission/scenario', 'map-setup', 'create-mission/world'],
    ['create-mission/conditions', 'mission-timer', 'create-mission/objectives'],
    ['create-mission/finishing', '', 'create-mission/groups-and-slots', 'freeze-zone'],
    ['create-mission/finishing', 'publish-mission', 'create-mission/publishing'],
  ]) {
    await page.goto(`${origin}${path(guide)}?from=bookmark${anchor ? `#${anchor}` : ''}`);
    await page.waitForURL((url) => url.pathname === path(chapter) && url.hash === (targetAnchor ? `#${targetAnchor}` : ''));
    assert.equal(new URL(page.url()).search, '?from=bookmark', 'Legacy bookmarks should retain query parameters');
    assert.equal(await page.locator('html').getAttribute('lang'), locale, 'Legacy navigation should retain the language');
    await chapterTitle(page, locale, chapter);
    if (targetAnchor) assert.equal(await page.locator(`[id="${targetAnchor}"]`).count(), 1);
    await noOverflow(page);
  }
  await page.goto(`${origin}${path('create-mission')}`);
  await page.evaluate(() => { location.hash = 'save-world'; });
  await page.waitForURL((url) => url.pathname === path('create-mission/world') && url.hash === '#save-world');
  await page.goto(`${origin}${path('create-mission')}#preface`);
  await page.waitForURL((url) => url.pathname === path('create-mission') && url.hash === '#reading-paths');
  await page.evaluate(() => window.scrollTo(0, 0));
  await page.screenshot({ path: join(resultsDirectory, `${locale}-mission-overview-${page.viewportSize().width}.png`), fullPage: true });
  await follow('.sl-markdown-content a[href="../example-mission/"]', 'example-mission');
  await follow('.sl-markdown-content a[href="../create-mission/project/#missing-addon-dependencies"]', 'create-mission/project');
  await page.goto(`${origin}${home(locale)}`);
}
async function articleContent(page, locale, inventory) {
  const text = fixtureText[locale];
  assert.equal(await page.locator('h1').textContent(), text.title);
  assert.equal(await page.locator('#starlight__sidebar').count(), 1, 'Article sidebar is missing');
  for (const id of ['preparation', 'details', 'screenshots']) assert.equal(await page.locator(`h2#${id}`).count(), 1, `Article section is missing: ${id}`);
  const body = page.locator('.sl-markdown-content');
  assert.ok((await body.textContent()).includes(text.intro), 'Temporary article notice is missing');
  assert.equal(await body.locator('ul > li').count(), 3);
  assert.equal(await body.locator('table th').count(), 2);
  assert.equal(await body.locator('table tbody tr').count(), 2);
  assert.ok(await body.getByRole('complementary', { name: text.noteTitle }).isVisible(), 'Native article aside is missing');
  for (const [name, alt, caption] of [
    ['create-mission/051.png', text.smallAlt, text.smallCaption],
    ['create-mission/078.png', text.largeAlt, text.largeCaption],
  ]) {
    const img = page.getByAltText(alt, { exact: true });
    await img.scrollIntoViewIfNeeded();
    await img.evaluate((element) => element.decode());
    assert.equal(await img.getAttribute('loading'), 'lazy');
    const dimensions = await img.evaluate((element) => ({
      rendered: element.getBoundingClientRect().width,
      natural: element.naturalWidth,
      srcset: element.getAttribute('srcset'),
    }));
    assert.ok(dimensions.rendered > 0 && dimensions.natural > 0, `Screenshot did not load: ${name}`);
    assert.ok(dimensions.srcset?.length, `Screenshot has no responsive variants: ${name}`);
    const figure = img.locator('xpath=ancestor::figure[1]');
    assert.ok((await figure.locator('figcaption').textContent()).includes(caption));
    const original = figure.getByRole('link', { name: text.originalLabel, exact: true });
    for (const link of await figure.getByRole('link').all()) {
      assert.equal(await link.getAttribute('target'), '_blank', 'Opening a screenshot must keep the guide open');
      assert.match(await link.getAttribute('rel'), /\bnoopener\b/);
      assert.match(await link.getAttribute('rel'), /\bnoreferrer\b/);
    }
    const response = await page.request.get(new URL(await original.getAttribute('href'), page.url()).href);
    assert.ok(response.ok(), `Original screenshot is unavailable: ${name}`);
    const bytes = await response.body();
    const expected = inventory.find((entry) => entry.path === name);
    assert.equal(bytes.byteLength, expected.bytes, `Original screenshot size changed: ${name}`);
    assert.equal(createHash('sha256').update(bytes).digest('hex'), expected.sha256, `Original screenshot bytes changed: ${name}`);
    assert.ok(dimensions.rendered <= bytes.readUInt32BE(16) + 1, `Screenshot was enlarged beyond its original width: ${name}`);
  }
  assert.equal(await page.locator('.expressive-code pre code').textContent(), 'const screenshotValidation = true;');
  await noOverflow(page);
}

async function captureThemes(page, { locale, article, width, resultsDirectory }) {
  for (const theme of ['light', 'dark']) {
    await openMenu(page);
    const selector = await visibleControl(page, 'starlight-theme-select select');
    await selector.selectOption(theme);
    await page.waitForFunction((expected) => document.documentElement.dataset.theme === expected, theme);
    await closeMenu(page);
    await page.evaluate(() => window.scrollTo(0, 0));
    await noOverflow(page);
    assert.equal(await page.locator('main footer').count(), 0, 'Pages without native footer content should not render an empty footer');
    const trailingSpace = await page.evaluate(() => document.querySelector('main').getBoundingClientRect().bottom - document.querySelector('.ll-help').getBoundingClientRect().bottom);
    assert.ok(trailingSpace >= -1 && trailingSpace <= 32, `Trailing space after the help panel exceeds its 32px budget: ${trailingSpace}px`);
    await page.screenshot({ path: join(resultsDirectory, `${locale}-${article ? 'article' : 'home'}-${width}-${theme}.png`), fullPage: true });
  }
}

export async function validateBrowser({ origin, inventory, resultsDirectory, fixtures = false }) {
  const browser = await chromium.launch();
  try {
    await mkdir(resultsDirectory, { recursive: true });
    for (const width of [390, 768, 1440, 1920]) {
      for (const locale of ['en', 'ru']) {
        const context = await browser.newContext({ viewport: { width, height: 960 }, reducedMotion: 'reduce' });
        const page = await context.newPage();
        page.setDefaultTimeout(10_000);
        const failures = [];
        page.on('pageerror', (error) => failures.push(error.message));
        page.on('response', (response) => {
          if (response.url().startsWith(origin) && response.status() >= 400) failures.push(`${response.status()} ${response.url()}`);
        });
        try {
          await page.goto(`${origin}${route(locale, fixtures)}`);
          assert.equal(await page.locator('html').getAttribute('lang'), locale);
          await noOverflow(page);
          await mobileMenu(page);
          await localeAndTheme(page, locale, fixtures);
          await navigation(page, locale, fixtures);
          if (fixtures) {
            await articleContent(page, locale, inventory);
          } else {
            await homepageLayout(page);
            await search(page, locale);
            await missionNavigation(page, locale);
            if ([390, 1440].includes(width)) await chapterJourneys(page, locale, resultsDirectory);
          }
          await captureThemes(page, { locale, article: fixtures, width, resultsDirectory });
          assert.deepEqual(failures, [], `Browser failures in ${locale} at ${width}px`);
          console.log(`Validated ${fixtures ? 'article navigation/content/screenshots' : 'homepage list/search/mission navigation'} and language/theme controls: ${locale}, ${width}px`);
        } catch (error) {
          await page.screenshot({ path: join(resultsDirectory, `failure-${fixtures ? 'fixtures' : 'home'}-${locale}-${width}.png`), fullPage: true }).catch(() => {});
          throw error;
        } finally {
          await context.close();
        }
      }
    }
  } finally {
    await browser.close();
  }
}
