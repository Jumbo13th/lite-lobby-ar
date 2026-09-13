import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import { access, mkdir, open, readFile, readdir, rm, stat, unlink } from 'node:fs/promises';
import { createServer } from 'node:net';
import { dirname, isAbsolute, join, relative, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { preview } from 'astro';
import { load } from 'cheerio';
import { fixtureSlug, fixtureSource } from '../tests/fixtures.mjs';
import { validateBrowser } from '../tests/production.mjs';
import { validateChapters } from '../tests/chapters.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const output = join(root, 'dist');
const content = join(root, 'src', 'content', 'docs');
const temporaryOutput = join(root, '.validation-dist');
const resultsDirectory = join(root, '.validation-results');
const publicOrigin = 'https://jumbo13th.github.io';
const base = '/lite-lobby-ar/';
const expectedGuideOrder = ['create-mission', 'example-mission', 'git', 'triad-tactics', 'api'];
const expectedAnchors = [...expectedGuideOrder, 'scripting', 'website-integration'];
const children = new Set();
const temporarySources = [];
let ownsTemporaryOutput = false;

function insideRoot(path) {
  const resolved = resolve(path);
  const suffix = relative(root, resolved);
  assert.ok(suffix && !suffix.startsWith(`..${sep}`) && suffix !== '..' && !isAbsolute(suffix), `Path is outside the website directory: ${resolved}`);
  return resolved;
}

async function filesIn(directory) {
  const entries = await readdir(directory, { withFileTypes: true });
  const nested = await Promise.all(entries.map((entry) => entry.isDirectory() ? filesIn(join(directory, entry.name)) : [join(directory, entry.name)]));
  return nested.flat().sort();
}

function slash(path) {
  return path.split(sep).join('/');
}

function sha256(bytes) {
  return createHash('sha256').update(bytes).digest('hex');
}

async function snapshot(directory) {
  const entries = await Promise.all((await filesIn(directory)).map(async (path) => [slash(relative(directory, path)), sha256(await readFile(path))]));
  return Object.fromEntries(entries);
}

async function inventoryCheck() {
  const inventory = JSON.parse(await readFile(join(root, 'tests', 'screenshots.json'), 'utf8'));
  assert.equal(inventory.length, 113, 'The original inventory must contain 113 screenshots');
  const expectedNames = Array.from({ length: 113 }, (_, index) => `${String(index + 1).padStart(2, '0')}.png`).sort();
  assert.deepEqual(inventory.map((entry) => entry.name).sort(), expectedNames, 'The screenshot inventory has missing or repeated filenames');
  const assetDirectory = join(root, 'src', 'assets', 'guide');
  assert.deepEqual((await readdir(assetDirectory)).sort(), expectedNames, 'The shared screenshot directory differs from the original inventory');
  await Promise.all(inventory.map(async (entry) => {
    const bytes = await readFile(join(assetDirectory, entry.name));
    assert.equal(bytes.byteLength, entry.bytes, `Screenshot size changed: ${entry.name}`);
    assert.equal(sha256(bytes), entry.sha256, `Screenshot bytes changed: ${entry.name}`);
  }));
  console.log('Validated original names, lengths, and SHA-256 hashes for all 113 screenshots');
  return inventory;
}

async function localeCheck() {
  const sources = (await filesIn(content)).filter((path) => /\.mdx?$/.test(path)).map((path) => slash(relative(content, path)));
  const english = sources.filter((path) => !path.startsWith('ru/'));
  const russian = sources.filter((path) => path.startsWith('ru/')).map((path) => path.slice(3));
  assert.deepEqual(english, russian, 'Every English source page must have a matching Russian source page');
  assert.ok(english.includes('index.mdx') || english.includes('index.md'), 'The English homepage is missing');
  const titles = [];
  for (const locale of ['', 'ru/']) {
    const html = load(await readFile(join(output, locale, 'index.html'), 'utf8'));
    assert.equal(html('html').attr('lang'), locale ? 'ru' : 'en');
    for (const anchor of expectedAnchors) assert.equal(html(`[id="${anchor}"]`).length, 1, `Missing or repeated ${locale || 'English '}anchor: ${anchor}`);
    assert.equal(html('.ll-guide-list').length, 1, 'The homepage needs one guide list');
    assert.equal(html('.ll-guide-list article.ll-guide-entry').length, expectedGuideOrder.length, `The ${locale || 'English '}homepage needs five semantic guide entries`);
    assert.deepEqual(html('.ll-guide-list article.ll-guide-entry h2').toArray().map((heading) => html(heading).attr('id')), expectedGuideOrder, 'Homepage guides must follow the agreed order');
    for (const slug of ['create-mission', 'example-mission', 'git']) {
      assert.equal(html(`#${slug} > a`).attr('href'), `${base}${locale}${slug}/`, `${slug} must link to its localized guide`);
    }
    assert.equal(html('.ll-guide-entry:not(:has(#create-mission, #example-mission, #git))').find('a, button, [role="button"], [role="link"]').length, 0, 'Unwritten guides must not appear actionable');
    assert.equal(html('.sidebar, .sidebar-pane, starlight-toc, mobile-starlight-toc').length, 0, 'Article navigation entered a homepage');
    assert.equal(html('#starlight__sidebar.ll-mobile-menu[popover]').length, 1, 'The homepage needs its compact mobile menu');
    assert.equal(html('button[popovertarget="starlight__sidebar"]').length, 1, 'The homepage needs one navbar menu button');
    assert.equal(html('.ll-home-controls, .ll-mobile-socials').length, 0, 'Homepage content should not duplicate menu controls');
    titles.push(html('h1').text().trim());
  }
  assert.ok(titles[0] && titles[1] && titles[0] !== titles[1], 'The two homepages need localized titles');
  console.log(`Validated matching English/Russian sources, five ordered homepage entries, three guide links, and all ${expectedAnchors.length} anchors`);
}

function routeFor(relativePath) {
  return `${base}${relativePath === 'index.html' ? '' : relativePath.replace(/\/index\.html$/, '/')}`;
}

async function outputCheck(directory, { fixtures = false } = {}) {
  const files = await filesIn(directory);
  const paths = new Set(files.map((path) => slash(relative(directory, path))));
  const pages = new Map();
  for (const path of paths) {
    assert.ok(!/(^|\/)(?:\.claude|\.agents|\.specify|specs|src|tests|scripts|Lite Lobby)(\/|$)/i.test(path), `Internal project directory entered the website: ${path}`);
    assert.ok(!/\.(?:mdx?|astro|ts|et|ent|conf|meta|gproj|c|ps1)$/i.test(path), `Source or addon file entered the website: ${path}`);
    if (!fixtures) assert.ok(!path.includes(fixtureSlug) && !path.includes('.validation'), `Validation fixture entered normal output: ${path}`);
    if (path.endsWith('.html')) pages.set(path, load(await readFile(join(directory, path), 'utf8')));
  }

  function checkReference(reference, fromPath) {
    if (!reference || /^(?:data:|blob:|mailto:|tel:|javascript:)/i.test(reference)) return;
    const url = new URL(reference, `${publicOrigin}${routeFor(fromPath)}`);
    if (url.origin !== publicOrigin) return;
    assert.ok(url.pathname.startsWith(base), `Local URL is missing the GitHub Pages prefix in ${fromPath}: ${reference}`);
    let target = decodeURIComponent(url.pathname.slice(base.length));
    if (target === '' || target.endsWith('/')) target += 'index.html';
    else if (!paths.has(target) && paths.has(`${target}/index.html`)) target += '/index.html';
    assert.ok(paths.has(target), `Broken local link or asset in ${fromPath}: ${reference} (expected ${target})`);
    if (url.hash && pages.has(target)) {
      const anchor = decodeURIComponent(url.hash.slice(1));
      const document = pages.get(target);
      assert.ok(!anchor || document('[id]').toArray().some((node) => document(node).attr('id') === anchor) || document('a[name]').toArray().some((node) => document(node).attr('name') === anchor), `Broken anchor in ${fromPath}: ${reference}`);
    }
  }

  for (const [path, document] of pages) {
    for (const element of document('[href], [src], [poster], [srcset]').toArray()) {
      const node = document(element);
      // Starlight emits locale metadata for its special 404.html response, not separate 404 routes.
      if (path === '404.html' && element.tagName === 'link' && ['canonical', 'alternate'].includes(node.attr('rel'))) continue;
      for (const attribute of ['href', 'src', 'poster']) checkReference(node.attr(attribute), path);
      const srcset = node.attr('srcset');
      if (srcset && !srcset.trim().startsWith('data:')) {
        for (const candidate of srcset.split(',')) checkReference(candidate.trim().split(/\s+/)[0], path);
      }
    }
    for (const element of document('img').toArray()) assert.ok(document(element).attr('alt')?.trim(), `Image with missing or blank alternative text in ${path}`);
  }
  for (const path of paths) {
    if (!path.endsWith('.css')) continue;
    const css = await readFile(join(directory, path), 'utf8');
    for (const match of css.matchAll(/url\(\s*['"]?([^'"\s)]+)['"]?\s*\)/g)) checkReference(match[1], path);
  }
  if (!fixtures) {
    const assetSources = (await filesIn(join(root, 'src'))).filter((path) => /\.(?:mdx?|astro|[cm]?[jt]sx?)$/.test(path));
    const sourceText = (await Promise.all(assetSources.map((path) => readFile(path, 'utf8')))).join('\n');
    const referencedNames = new Set([...sourceText.matchAll(/\/(\d{2,3})\.png(?=["'`])/g)].map((match) => match[1]));
    for (const path of paths) {
      const name = path.match(/(?:^|\/)(\d{2,3})(?:\.[^/]+)?\.(?:png|webp|avif)$/i)?.[1];
      if (name) assert.ok(referencedNames.has(name), `An unused source screenshot entered normal output: ${path}`);
    }
  }
  console.log(`Validated ${pages.size} HTML pages, local links/anchors/assets, and ${fixtures ? 'fixture' : 'normal'} output exclusions`);
}

function astro(args) {
  const child = spawn(process.execPath, [join(root, 'node_modules', 'astro', 'bin', 'astro.mjs'), ...args], {
    cwd: root,
    shell: false,
    windowsHide: true,
    stdio: ['ignore', 'pipe', 'pipe'],
    env: { ...process.env, ASTRO_TELEMETRY_DISABLED: '1' },
  });
  children.add(child);
  let captured = '';
  for (const stream of [child.stdout, child.stderr]) stream.on('data', (chunk) => { captured = `${captured}${chunk}`.slice(-30_000); });
  const completed = new Promise((resolveChild, rejectChild) => {
    child.on('error', rejectChild);
    child.on('close', (code) => { children.delete(child); resolveChild(code); });
  });
  completed.catch(() => {});
  return { child, completed, log: () => captured };
}

async function runAstro(args) {
  const command = astro(args);
  const code = await command.completed;
  assert.equal(code, 0, `Astro ${args[0]} failed:\n${command.log()}`);
}

async function unusedPort() {
  const server = createServer();
  await new Promise((resolvePort, rejectPort) => {
    server.once('error', rejectPort);
    server.listen(0, '127.0.0.1', resolvePort);
  });
  const port = server.address().port;
  await new Promise((resolveClose, rejectClose) => server.close((error) => error ? rejectClose(error) : resolveClose()));
  return port;
}

async function withPreview(directory, action) {
  const port = await unusedPort();
  const origin = `http://127.0.0.1:${port}`;
  const server = await preview({ root, outDir: directory, server: { host: '127.0.0.1', port }, logLevel: 'error' });
  try {
    await action(origin);
  } finally {
    await server.stop();
  }
}

async function createFixtures() {
  const destinations = [join(content, `${fixtureSlug}.mdx`), join(content, 'ru', `${fixtureSlug}.mdx`)];
  for (const path of [...destinations, temporaryOutput]) {
    try { await access(path); } catch (error) { if (error.code === 'ENOENT') continue; throw error; }
    throw new Error(`Refusing to overwrite an existing validation path: ${path}. Inspect and remove stale validation files before retrying.`);
  }
  await mkdir(insideRoot(temporaryOutput));
  ownsTemporaryOutput = true;
  for (const [index, path] of destinations.entries()) {
    const file = await open(insideRoot(path), 'wx');
    temporarySources.push(path);
    try {
      await file.writeFile(fixtureSource(index ? 'ru' : 'en'));
    } finally {
      await file.close();
    }
  }
}

async function cleanup() {
  for (const child of children) child.kill();
  const results = await Promise.allSettled([
    ...temporarySources.map(async (path) => unlink(insideRoot(path))),
    ...(ownsTemporaryOutput ? [rm(insideRoot(temporaryOutput), { recursive: true, force: true, maxRetries: 3, retryDelay: 200 })] : []),
  ]);
  const errors = results.filter((result) => result.status === 'rejected').map((result) => result.reason);
  if (errors.length) throw new AggregateError(errors, 'Could not remove all temporary validation files');
}

async function main() {
  assert.ok((await stat(output)).isDirectory(), 'Run the normal production build before this validator');
  const inventory = await inventoryCheck();
  await localeCheck();
  await outputCheck(output);
  await validateChapters(output);
  const originalOutput = await snapshot(output);
  await withPreview(output, (origin) => validateBrowser({ origin, inventory, resultsDirectory }));
  await createFixtures();
  console.log('Building isolated article layout fixtures');
  await runAstro(['build', '--outDir', temporaryOutput]);
  await outputCheck(temporaryOutput, { fixtures: true });
  await withPreview(temporaryOutput, (origin) => validateBrowser({ origin, inventory, resultsDirectory, fixtures: true }));
  assert.deepEqual(await snapshot(output), originalOutput, 'Fixture validation modified the normal publication artifact');
}

try {
  await main();
} catch (error) {
  console.error(error);
  process.exitCode = 1;
} finally {
  try {
    await cleanup();
  } catch (error) {
    console.error(error);
    process.exitCode = 1;
  }
}
if (!process.exitCode) {
  await outputCheck(output);
  console.log('Website validation passed. Temporary fixtures and preview processes have been removed.');
}
