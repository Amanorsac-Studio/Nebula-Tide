// Nebula Forge — local preset authoring backend for Nebula Tide.
// Run: node tools/forge/server.js   →  http://localhost:8451
const http = require('http');
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..', '..');
const PRESETS = path.join(ROOT, 'presets');
const MANIFEST = path.join(PRESETS, 'manifest.json');
const PORT = 8451;

const KEYS = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B'];
const AUDIO_EXTS = ['.wav', '.mp3', '.ogg', '.flac', '.aiff'];

if (!fs.existsSync(PRESETS)) fs.mkdirSync(PRESETS, { recursive: true });

// NebulaConvert CLI (built with the app); enables auto WAV→FLAC on upload
const CONVERTER = [
  path.join(ROOT, 'build', 'NebulaConvert_artefacts', 'Release', 'NebulaConvert.exe'),
  path.join(ROOT, 'build', 'NebulaConvert_artefacts', 'NebulaConvert.exe')
].find(f => fs.existsSync(f)) || null;
if (!CONVERTER) console.warn('NebulaConvert.exe not found — uploads keep their original format');

function readManifest() {
  try { return JSON.parse(fs.readFileSync(MANIFEST, 'utf8')); }
  catch (_) { return { presets: [] }; }
}
function writeManifest(m) {
  fs.writeFileSync(MANIFEST, JSON.stringify(m, null, 2));
}
function safeBase(name) {
  return name.replace(/[^a-zA-Z0-9 _-]/g, '').trim().replace(/\s+/g, '_');
}
function findKeyFile(base, key) {
  for (const ext of AUDIO_EXTS) {
    const f = `${base}_${key}${ext}`;
    if (fs.existsSync(path.join(PRESETS, f))) return f;
  }
  return null;
}

// Convert any uploaded audio to FLAC, normalized to -16 dBFS RMS.
// Returns the new full path, or null if conversion was unavailable/failed
// (in which case the original file is kept as-is).
function normalizeToFlac(inputFull, flacFull) {
  if (!CONVERTER) return null;
  try {
    execFileSync(CONVERTER, [inputFull, flacFull + '.tmp.flac', '-n24'], { timeout: 300000 });
    fs.unlinkSync(inputFull);
    if (fs.existsSync(flacFull)) fs.unlinkSync(flacFull);
    fs.renameSync(flacFull + '.tmp.flac', flacFull);
    return flacFull;
  } catch (e) {
    console.error('normalize/convert failed, keeping original:', e.message);
    try { if (fs.existsSync(flacFull + '.tmp.flac')) fs.unlinkSync(flacFull + '.tmp.flac'); } catch (_) {}
    return null;
  }
}

const AUX_DIRS = { fx: path.join(PRESETS, 'fx'), textures: path.join(PRESETS, 'textures') };
for (const d of Object.values(AUX_DIRS)) if (!fs.existsSync(d)) fs.mkdirSync(d, { recursive: true });

function json(res, code, obj) {
  res.writeHead(code, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify(obj));
}

function collectBody(req, cb) {
  const chunks = [];
  req.on('data', c => chunks.push(c));
  req.on('end', () => cb(Buffer.concat(chunks)));
}

const server = http.createServer((req, res) => {
  const url = new URL(req.url, `http://localhost:${PORT}`);
  const p = url.pathname;

  // ── static UI ──
  if (req.method === 'GET' && (p === '/' || p === '/index.html')) {
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end(fs.readFileSync(path.join(__dirname, 'index.html')));
    return;
  }

  // ── list presets (manifest merged with files on disk) ──
  if (req.method === 'GET' && p === '/api/presets') {
    const m = readManifest();
    const out = m.presets.map(pr => {
      const base = safeBase(pr.name);
      const files = {};
      for (const k of KEYS) files[k] = findKeyFile(base, k);
      return { ...pr, files };
    });
    json(res, 200, { presets: out, keys: KEYS });
    return;
  }

  // ── serve an audio file for waveform / loop testing ──
  if (req.method === 'GET' && p === '/api/file') {
    const name = path.basename(url.searchParams.get('name') || '');
    const full = path.join(PRESETS, name);
    if (!fs.existsSync(full)) return json(res, 404, { error: 'not found' });
    res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
    fs.createReadStream(full).pipe(res);
    return;
  }

  // ── upload one key file: /api/upload?preset=Name&key=Eb&ext=.wav  (raw body) ──
  if (req.method === 'POST' && p === '/api/upload') {
    const preset = safeBase(url.searchParams.get('preset') || '');
    const key = url.searchParams.get('key');
    let ext = (url.searchParams.get('ext') || '.wav').toLowerCase();
    if (!preset || !KEYS.includes(key)) return json(res, 400, { error: 'bad preset/key' });
    if (!AUDIO_EXTS.includes(ext)) return json(res, 400, { error: 'bad extension' });
    collectBody(req, body => {
      // remove any previous file for this slot (maybe different extension)
      const old = findKeyFile(preset, key);
      if (old) fs.unlinkSync(path.join(PRESETS, old));
      let fname = `${preset}_${key}${ext}`;
      let full = path.join(PRESETS, fname);
      fs.writeFileSync(full, body);

      const done = normalizeToFlac(full, path.join(PRESETS, `${preset}_${key}.flac`));
      if (done) { fname = path.basename(done); full = done; }
      json(res, 200, { file: fname, bytes: fs.statSync(full).size });
    });
    return;
  }

  // ── save/update a preset's metadata ──
  if (req.method === 'POST' && p === '/api/preset') {
    collectBody(req, body => {
      let pr;
      try { pr = JSON.parse(body.toString('utf8')); } catch (_) { return json(res, 400, { error: 'bad json' }); }
      if (!pr.name || !safeBase(pr.name)) return json(res, 400, { error: 'name required' });
      const m = readManifest();
      const idx = m.presets.findIndex(x => safeBase(x.name) === safeBase(pr.name));
      const entry = {
        name: pr.name.trim(),
        colour: pr.colour || '#4fe3ff',
        fx: pr.fx || null,           // default FX star sound (filename)
        texture: pr.texture || null, // default texture star sound (filename)
        reverb: {
          type: pr.reverb?.type ?? 'hall',
          mix: +(pr.reverb?.mix ?? 0.4),
          size: +(pr.reverb?.size ?? 0.85),
          damp: +(pr.reverb?.damp ?? 0.45)
        }
      };
      if (idx >= 0) m.presets[idx] = entry; else m.presets.push(entry);
      writeManifest(m);
      json(res, 200, { ok: true });
    });
    return;
  }

  // ── delete a preset (metadata + all its key files) ──
  if (req.method === 'DELETE' && p === '/api/preset') {
    const name = url.searchParams.get('name') || '';
    const base = safeBase(name);
    const m = readManifest();
    m.presets = m.presets.filter(x => safeBase(x.name) !== base);
    writeManifest(m);
    for (const k of KEYS) {
      const f = findKeyFile(base, k);
      if (f) fs.unlinkSync(path.join(PRESETS, f));
    }
    json(res, 200, { ok: true });
    return;
  }

  // ── FX / texture sounds (global, preset-independent) ──
  if (req.method === 'GET' && p === '/api/aux') {
    const out = {};
    for (const [cat, dir] of Object.entries(AUX_DIRS))
      out[cat] = fs.readdirSync(dir)
        .filter(f => AUDIO_EXTS.includes(path.extname(f).toLowerCase()))
        .sort();
    json(res, 200, out);
    return;
  }

  if (req.method === 'GET' && p === '/api/aux/file') {
    const cat = url.searchParams.get('cat');
    if (!AUX_DIRS[cat]) return json(res, 400, { error: 'bad category' });
    const full = path.join(AUX_DIRS[cat], path.basename(url.searchParams.get('name') || ''));
    if (!fs.existsSync(full)) return json(res, 404, { error: 'not found' });
    res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
    fs.createReadStream(full).pipe(res);
    return;
  }

  // upload: /api/aux/upload?cat=fx&name=Clap&ext=.wav  (raw body)
  if (req.method === 'POST' && p === '/api/aux/upload') {
    const cat = url.searchParams.get('cat');
    const name = safeBase(url.searchParams.get('name') || '');
    let ext = (url.searchParams.get('ext') || '.wav').toLowerCase();
    if (!AUX_DIRS[cat]) return json(res, 400, { error: 'bad category' });
    if (!name) return json(res, 400, { error: 'name required' });
    if (!AUDIO_EXTS.includes(ext)) return json(res, 400, { error: 'bad extension' });
    collectBody(req, body => {
      // clear other-extension versions of the same sound
      for (const e of AUDIO_EXTS) {
        const dup = path.join(AUX_DIRS[cat], name + e);
        if (fs.existsSync(dup)) fs.unlinkSync(dup);
      }
      let full = path.join(AUX_DIRS[cat], name + ext);
      fs.writeFileSync(full, body);
      const done = normalizeToFlac(full, path.join(AUX_DIRS[cat], name + '.flac'));
      if (done) full = done;
      json(res, 200, { file: path.basename(full), bytes: fs.statSync(full).size });
    });
    return;
  }

  if (req.method === 'DELETE' && p === '/api/aux') {
    const cat = url.searchParams.get('cat');
    if (!AUX_DIRS[cat]) return json(res, 400, { error: 'bad category' });
    const full = path.join(AUX_DIRS[cat], path.basename(url.searchParams.get('name') || ''));
    if (fs.existsSync(full)) fs.unlinkSync(full);
    json(res, 200, { ok: true });
    return;
  }

  json(res, 404, { error: 'not found' });
});

server.listen(PORT, () => console.log(`Nebula Forge running → http://localhost:${PORT}`));
