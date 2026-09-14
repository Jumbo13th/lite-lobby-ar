import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { chromium } from 'playwright';

export async function validateScreenshotNavigation(origin) {
  const original = await readFile(new URL('../src/assets/screenshots/create-mission/109.png', import.meta.url));
  const browser = await chromium.launch();
  try {
    const context = await browser.newContext();
    const page = await context.newPage();
    for (const locale of ['ru/', '']) {
      await page.goto(`${origin}/lite-lobby-ar/${locale}create-mission/groups-and-slots/`);
      const figure = page.locator('figure').filter({ has: page.locator('a.original[href*="/109."]') });
      await figure.locator('img').scrollIntoViewIfNeeded();
      await figure.locator('img').evaluate((image) => image.decode());
      for (const selector of ['a.original', 'figcaption a']) {
        const link = figure.locator(selector);
        const href = new URL(await link.getAttribute('href'), page.url()).href;
        // Fetching with */* misses Astro's base-path guard for HTML navigation.
        const [response, popup] = await Promise.all([
          context.waitForEvent('response', {
            predicate: (response) => response.url() === href && response.request().isNavigationRequest(),
          }),
          page.waitForEvent('popup'),
          link.click(),
        ]);
        await popup.waitForLoadState();
        assert.equal(response.status(), 200, `Original-image navigation failed: ${href}`);
        assert.match(response.headers()['content-type'], /^image\/png/);
        assert.deepEqual(await response.body(), original, 'Navigation must return the original PNG bytes');
        assert.ok(await popup.locator('img').evaluate((image) => image.naturalWidth > 0));
        await popup.close();
      }
    }
    console.log(`Original screenshot navigation passed in both locales: ${origin}`);
  } finally {
    await browser.close();
  }
}
