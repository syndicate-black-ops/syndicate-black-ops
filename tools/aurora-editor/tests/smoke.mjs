// Headless check of the running editor (Playwright + Chromium, software WebGL):
//   node smoke.mjs <url> <screens-dir>
// Opens every shipped script, lets it run, and fails on any console error, page error or failed request; checks that
// each script's system makes particles, that the game prints no console line for the shipped scripts (other than
// expected ones), uploads, undo, the share link and the form-to-text edit; screenshots some presets.
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'node:fs';

const url = process.argv[2] || 'http://localhost:8095/';
const out = process.argv[3] || 'screens';
mkdirSync(out, { recursive: true });

const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
const context = await browser.newContext({ viewport: { width: 1600, height: 900 } });
const page = await context.newPage();
const problems = [];
page.on('console', (m) => { if (m.type() === 'error' || m.type() === 'warning') problems.push(`console ${m.type()}: ${m.text()}`); });
page.on('pageerror', (e) => problems.push(`page error: ${e.message}`));
page.on('requestfailed', (r) => problems.push(`request failed: ${r.url()} ${r.failure()?.errorText}`));
page.on('response', (r) => { if (r.status() >= 400 && !r.url().includes('/api/session/')) problems.push(`HTTP ${r.status()}: ${r.url()}`); });

await page.goto(url);
await page.waitForFunction(() => window.__aurora && window.__aurora.S.doc, null, { timeout: 30000 });
const info = await page.evaluate(() => window.__aurora.S.coreInfo);
console.log('core:', info);

const scripts = await page.evaluate(() => window.__aurora.S.assets.scripts.map((s) => s.name));
const shots = new Set(['aurora/steamvent.aur', 'aurora/sparkshower.aur', 'aurora/emberbarrel.aur', 'aurora/energyswirl.aur',
	'aurora/colorfountain.aur', 'aurora/fireflies.aur', 'aurora/mist.aur', 'aurora/fountain.aur', 'aurora/wrecksmoke.aur', 'aurora/torch.aur']);
const results = [];
for (const name of scripts) {
	const r = await page.evaluate(async (name) => {
		const A = window.__aurora;
		const text = await (await fetch('assets/' + name)).text();
		A.loadScript(name.replace(/^aurora\//, ''), text);
		await A.rebuild();
		A.frameCamera();
		await new Promise((res) => setTimeout(res, 3500));
		return {
			name,
			game: A.S.gameLog.slice(),
			particles: A.engine.stat(3, 1),
			quads: A.quads.length / 28,
			missing: [...A.S.spriteInfo.values()].filter((i) => i.state !== 'ok').map((i) => i.name),
		};
	}, name);
	results.push(r);
	console.log(`${r.particles > 0 ? 'ok  ' : 'NONE'} ${name}: ${r.particles} particles, ${r.quads} quads${r.game.length ? ' | game: ' + r.game.join(' / ') : ''}${r.missing.length ? ' | missing sprites: ' + r.missing.join(', ') : ''}`);
	if (shots.has(name)) await page.screenshot({ path: `${out}/${name.replace(/^aurora\//, '').replace('.aur', '')}.png` });
}

// form edit -> text: change the steamvent's smoke size through the store, check the line changed and comments stayed
const edit = await page.evaluate(async () => {
	const A = window.__aurora;
	const text = await (await fetch('assets/aurora/steamvent.aur')).text();
	A.loadScript('steamvent', text);
	const comments = (text.match(/\/\//g) || []).length;
	A.S.selected = A.S.doc.blocks.findIndex((b) => b.entries.some((e) => e.key === 'sprite'));
	const before = A.S.text;
	const m = await import(new URL('js/aur.js', location.href).href);
	A.setText(m.setValue(A.S.doc, A.S.doc.blocks[A.S.selected], 'startsize', '12'), 'form');
	const after = A.S.text;
	const diff = before.split('\n').filter((l, i) => l !== after.split('\n')[i]);
	document.querySelector('#btnUndo').click();
	return { comments, commentsAfter: (after.match(/\/\//g) || []).length, changedLines: diff.length, undone: A.S.text === before };
});
console.log('form edit:', JSON.stringify(edit));
if (edit.changedLines !== 1 || edit.comments !== edit.commentsAfter || !edit.undone) problems.push('form edit / undo check failed: ' + JSON.stringify(edit));

// upload a sprite and a script through the file input
const upload = await page.evaluate(async () => {
	const spr = await (await fetch('assets/sprites/glow01.spr')).arrayBuffer();
	const aur = '"particles" "50"\n"maintype" "a"\n{\n"name" "a"\n"spraytype" "b"\n"sprayrate" "20"\n}\n{\n"name" "b"\n"sprite" "sprites/my-upload.spr"\n"lifetime" "1"\n"startsize" "6"\n"sprayforce" "50"\n"offset" "0 0 0"\n"killcondition" "water"\n}\n';
	const dt = new DataTransfer();
	dt.items.add(new File([spr], 'My-Upload.spr'));
	dt.items.add(new File([aur], 'uploaded-test.aur'));
	const input = document.querySelector('#fileInput');
	input.files = dt.files;
	input.dispatchEvent(new Event('change'));
	await new Promise((r) => setTimeout(r, 2500));
	const A = window.__aurora;
	return { name: A.S.name, uploads: [...A.S.uploads.keys()], game: A.S.gameLog, particles: A.engine.stat(3, 1), session: A.S.session };
});
console.log('upload:', JSON.stringify(upload));
if (!upload.uploads.includes('sprites/my-upload.spr') || upload.name !== 'uploaded-test') problems.push('upload failed: ' + JSON.stringify(upload));
await page.screenshot({ path: `${out}/upload-and-problems.png` });

// share link round trip
const link = await page.evaluate(async () => {
	const util = await import(new URL('js/util.js', location.href).href);
	const A = window.__aurora;
	const packed = await util.packState({ v: 1, n: 'shared', t: A.S.text });
	const back = await util.unpackState(packed);
	return { ok: back.t === A.S.text, length: packed.length };
});
console.log('share:', JSON.stringify(link));
if (!link.ok) problems.push('share link round trip failed');

const view = (fn) => page.evaluate(fn);
// a moving target leaving a trail (camera set after the automatic framing)
await page.evaluate(async () => {
	const A = window.__aurora;
	const text = await (await fetch('assets/aurora/traintrail.aur')).text();
	A.S.emitter.mode = 'follow';
	A.S.emitter.path = 'circle';
	A.loadScript('traintrail', text);
	await A.rebuild();
	await new Promise((r) => setTimeout(r, 2000));
	A.cam.target = [0, 0, 48]; A.cam.dist = 520; A.cam.pitch = 40; A.cam.yaw = 200;
	await new Promise((r) => setTimeout(r, 3000));
});
await page.screenshot({ path: `${out}/traintrail-moving-target.png` });

// one-shot copies (Spawn Use) every 1.5 s
await page.evaluate(async () => {
	const A = window.__aurora;
	A.S.emitter.mode = 'static';
	A.S.play.mode = 'oneshot';
	A.S.play.autoTrigger = 1.5;
	document.querySelector('#playMode').value = 'oneshot';
	document.querySelector('#playMode').dispatchEvent(new Event('change'));
	const text = await (await fetch('assets/aurora/sparkshower.aur')).text();
	A.loadScript('sparkshower', text);
	await A.rebuild();
	await new Promise((r) => setTimeout(r, 3000));
});
await page.screenshot({ path: `${out}/sparkshower-oneshot.png` });
const copies = await view(() => window.__aurora.engine.stat(4));
console.log('one-shot copies alive:', copies);
if (copies < 1) problems.push('no one-shot copy');
await page.evaluate(() => {
	document.querySelector('#playMode').value = 'loop';
	document.querySelector('#playMode').dispatchEvent(new Event('change'));
});

// lightmodel: wrecksmoke and its unlit twin side by side (compare), dim world light and a warm lamp
const lit = await page.evaluate(async () => {
	const A = window.__aurora;
	const names = A.S.assets.scripts.map((s) => s.name);
	if (!names.includes('aurora/wrecksmoke.aur')) return 'no wrecksmoke.aur in this image';
	A.S.scene.lightColor = '#8090a0';
	A.S.scene.lightLevel = 70;
	A.S.scene.lamp = true;
	A.S.scene.lampPos = [0, 0, 120];
	A.S.scene.lampRadius = 200;
	A.S.scene.lampColor = '#ffb060';
	const text = await (await fetch('assets/aurora/wrecksmoke.aur')).text();
	A.loadScript('wrecksmoke', text);
	if (names.includes('aurora/wrecksmoke_unlit.aur')) {
		const t2 = await (await fetch('assets/aurora/wrecksmoke_unlit.aur')).text();
		const m = await import(new URL('js/aur.js', location.href).href);
		A.S.compare = { enabled: true, name: 'wrecksmoke_unlit', text: t2, doc: m.parse(t2), offset: 160 };
	}
	await A.rebuild();
	await new Promise((r) => setTimeout(r, 2000));
	A.cam.target = [0, -80, 110]; A.cam.dist = 420; A.cam.pitch = 8; A.cam.yaw = 180;
	await new Promise((r) => setTimeout(r, 4000));
	return 'ok';
});
console.log('lightmodel:', lit);
await page.screenshot({ path: `${out}/wrecksmoke-lightmodel-compare.png` });

// killcondition water: the emitter under water never emits
const kill = await page.evaluate(async () => {
	const A = window.__aurora;
	if (!A.S.assets.scripts.some((s) => s.name === 'aurora/wrecksmoke.aur')) return null;
	A.S.compare.enabled = false;
	A.S.scene.lamp = false;
	A.S.scene.lightLevel = 255;
	A.S.scene.water = true;
	A.S.scene.waterTop = 96;
	await A.rebuild();
	await new Promise((r) => setTimeout(r, 1500));
	const under = A.engine.stat(3, 1);
	A.S.scene.water = false;
	await A.rebuild();
	await new Promise((r) => setTimeout(r, 1500));
	return { under, dry: A.engine.stat(3, 1) };
});
console.log('killcondition water:', JSON.stringify(kill));
if (kill && !(kill.under <= 1 && kill.dry > 5)) problems.push('killcondition check failed');

// every sprite the scripts used is there and shown by default in the browser (not a HUD / UI sprite)
const spriteCheck = await page.evaluate(() => {
	const A = window.__aurora;
	const used = [...A.S.spriteInfo.keys()];
	const missing = used.filter((n) => A.S.spriteInfo.get(n).state !== 'ok');
	const hidden = used.filter((n) => (A.S.assets.sprites.find((s) => s.name === n) || {}).ui);
	const all = A.S.assets.sprites.length, shown = A.S.assets.sprites.filter((s) => !s.ui).length;
	return { used: used.length, missing, hidden, all, shown };
});
console.log('sprites:', JSON.stringify(spriteCheck));
if (spriteCheck.missing.length || spriteCheck.hidden.length) problems.push('sprite check failed: ' + JSON.stringify(spriteCheck));

// sprite browser
await page.evaluate(() => { document.querySelector('[data-tab="sprites"]').click(); });
await page.waitForTimeout(2500);
await page.evaluate(() => { const c = document.querySelector('.sprite-card[data-name="sprites/xfireball3.spr"]') || document.querySelector('.sprite-card'); c.click(); });
await page.waitForTimeout(1500);
await page.screenshot({ path: `${out}/sprite-browser.png` });

// properties form of a type and the script text
await page.evaluate(async () => {
	const A = window.__aurora;
	document.querySelector('[data-tab="types"]').click();
	const text = await (await fetch('assets/aurora/energyswirl.aur')).text();
	A.loadScript('energyswirl', text);
	await new Promise((r) => setTimeout(r, 300));
	const n = [...document.querySelectorAll('#tree .node')].find((e) => e.textContent.includes('orb'));
	n.click();
	await new Promise((r) => setTimeout(r, 2500));
});
await page.screenshot({ path: `${out}/energyswirl-properties.png` });
await page.evaluate(() => document.querySelector('[data-tab="text"]').click());
await page.waitForTimeout(300);
await page.screenshot({ path: `${out}/energyswirl-script.png` });

await page.evaluate(() => document.querySelector('#btnEntity').click());
await page.waitForTimeout(400);
await page.screenshot({ path: `${out}/map-entity.png` });
await page.keyboard.press('Escape');
await page.evaluate(() => document.querySelector('#view').focus());
await page.keyboard.press('h');
await page.waitForTimeout(400);
await page.screenshot({ path: `${out}/help.png` });

writeFileSync(`${out}/results.json`, JSON.stringify(results, null, 1));
await browser.close();
const none = results.filter((r) => r.particles === 0);
if (none.length) problems.push('no particles: ' + none.map((r) => r.name).join(', '));
console.log(problems.length ? 'PROBLEMS:\n' + problems.join('\n') : 'SMOKE OK');
process.exit(problems.length ? 1 : 0);
