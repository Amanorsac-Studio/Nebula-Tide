#!/usr/bin/env node
// Nebula Tide — chord vocabulary + pad-mapping generator
//
// Every Nebula Tide pad is a sus2 voicing: root, major 2nd, perfect 5th.
// It deliberately has NO third, which is why one pad serves both the major
// and the minor chord on that root (and most of their extensions).
//
// This script derives, for all 12 roots x the full chord vocabulary, which of
// the 12 pads should sound. It emits:
//   docs/chords/nebula-chord-map.json   machine-readable (engine + Nebula Forge)
//   docs/chords/CHORD-MAP.md            human-readable reference
//   src/ChordMap.h                      generated C++ table for the engine
//
// Run:  node tools/chords/generate.js

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');

// Nebula Tide's key naming (matches keynames::display in PluginProcessor.h)
const NOTE = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B'];

// Interval -> scale-degree label, for describing what a pad note does over a chord
const DEGREE = ['root', 'b9', '9', '#9', '3', '11', '#11', '5', 'b13', '13', 'b7', 'maj7'];

// ── The chord vocabulary ──────────────────────────────────────────────────
// intervals are semitones from the chord root; values above 11 are compound
// (e.g. 14 = 9th, 21 = 13th) and are reduced mod 12 for analysis.
const QUALITIES = [
  // triads and dyads
  { id: 'maj',      sym: '',         name: 'Major',                  iv: [0, 4, 7],                  fam: 'Triads' },
  { id: 'min',      sym: 'm',        name: 'Minor',                  iv: [0, 3, 7],                  fam: 'Triads' },
  { id: 'sus2',     sym: 'sus2',     name: 'Suspended 2nd',          iv: [0, 2, 7],                  fam: 'Triads' },
  { id: 'sus4',     sym: 'sus4',     name: 'Suspended 4th',          iv: [0, 5, 7],                  fam: 'Triads' },
  { id: 'five',     sym: '5',        name: 'Power chord',            iv: [0, 7],                     fam: 'Triads' },
  { id: 'dim',      sym: 'dim',      name: 'Diminished',             iv: [0, 3, 6],                  fam: 'Triads' },
  { id: 'aug',      sym: 'aug',      name: 'Augmented',              iv: [0, 4, 8],                  fam: 'Triads' },

  // sixths
  { id: '6',        sym: '6',        name: 'Major 6th',              iv: [0, 4, 7, 9],               fam: 'Sixths' },
  { id: 'm6',       sym: 'm6',       name: 'Minor 6th',              iv: [0, 3, 7, 9],               fam: 'Sixths' },
  { id: '69',       sym: '6/9',      name: 'Six-nine',               iv: [0, 4, 7, 9, 14],           fam: 'Sixths' },
  { id: 'm69',      sym: 'm6/9',     name: 'Minor six-nine',         iv: [0, 3, 7, 9, 14],           fam: 'Sixths' },

  // sevenths
  { id: 'maj7',     sym: 'maj7',     name: 'Major 7th',              iv: [0, 4, 7, 11],              fam: 'Sevenths' },
  { id: 'dom7',     sym: '7',        name: 'Dominant 7th',           iv: [0, 4, 7, 10],              fam: 'Sevenths' },
  { id: 'm7',       sym: 'm7',       name: 'Minor 7th',              iv: [0, 3, 7, 10],              fam: 'Sevenths' },
  { id: 'mMaj7',    sym: 'm(maj7)',  name: 'Minor-major 7th',        iv: [0, 3, 7, 11],              fam: 'Sevenths' },
  { id: '7sus4',    sym: '7sus4',    name: 'Dominant 7 sus4',        iv: [0, 5, 7, 10],              fam: 'Sevenths' },
  { id: '7sus2',    sym: '7sus2',    name: 'Dominant 7 sus2',        iv: [0, 2, 7, 10],              fam: 'Sevenths' },
  { id: 'm7b5',     sym: 'm7b5',     name: 'Half-diminished',        iv: [0, 3, 6, 10],              fam: 'Sevenths' },
  { id: 'dim7',     sym: 'dim7',     name: 'Diminished 7th',         iv: [0, 3, 6, 9],               fam: 'Sevenths' },

  // ninths
  { id: 'add9',     sym: 'add9',     name: 'Added 9th',              iv: [0, 4, 7, 14],              fam: 'Ninths' },
  { id: 'madd9',    sym: 'm(add9)',  name: 'Minor added 9th',        iv: [0, 3, 7, 14],              fam: 'Ninths' },
  { id: 'maj9',     sym: 'maj9',     name: 'Major 9th',              iv: [0, 4, 7, 11, 14],          fam: 'Ninths' },
  { id: '9',        sym: '9',        name: 'Dominant 9th',           iv: [0, 4, 7, 10, 14],          fam: 'Ninths' },
  { id: 'm9',       sym: 'm9',       name: 'Minor 9th',              iv: [0, 3, 7, 10, 14],          fam: 'Ninths' },
  { id: 'mMaj9',    sym: 'm(maj9)',  name: 'Minor-major 9th',        iv: [0, 3, 7, 11, 14],          fam: 'Ninths' },
  { id: '9sus4',    sym: '9sus4',    name: 'Dominant 9 sus4',        iv: [0, 5, 7, 10, 14],          fam: 'Ninths' },
  { id: 'm9b5',     sym: 'm9b5',     name: 'Half-dim, natural 9',    iv: [0, 3, 6, 10, 14],          fam: 'Ninths' },

  // elevenths
  { id: '11',       sym: '11',       name: 'Dominant 11th',          iv: [0, 7, 10, 14, 17],         fam: 'Elevenths' },
  { id: 'm11',      sym: 'm11',      name: 'Minor 11th',             iv: [0, 3, 7, 10, 14, 17],      fam: 'Elevenths' },
  { id: 'maj7#11',  sym: 'maj7#11',  name: 'Lydian major 7th',       iv: [0, 4, 7, 11, 18],          fam: 'Elevenths' },
  { id: 'maj9#11',  sym: 'maj9#11',  name: 'Lydian major 9th',       iv: [0, 4, 7, 11, 14, 18],      fam: 'Elevenths' },
  { id: '7#11',     sym: '7#11',     name: 'Lydian dominant',        iv: [0, 4, 7, 10, 14, 18],      fam: 'Elevenths' },

  // thirteenths
  { id: '13',       sym: '13',       name: 'Dominant 13th',          iv: [0, 4, 7, 10, 14, 21],      fam: 'Thirteenths' },
  { id: 'maj13',    sym: 'maj13',    name: 'Major 13th',             iv: [0, 4, 7, 11, 14, 21],      fam: 'Thirteenths' },
  { id: 'm13',      sym: 'm13',      name: 'Minor 13th',             iv: [0, 3, 7, 10, 14, 21],      fam: 'Thirteenths' },
  { id: '13#11',    sym: '13#11',    name: 'Dominant 13 #11',        iv: [0, 4, 7, 10, 14, 18, 21],  fam: 'Thirteenths' },

  // altered dominants
  { id: '7b5',      sym: '7b5',      name: 'Dominant 7 flat 5',      iv: [0, 4, 6, 10],              fam: 'Altered' },
  { id: '7#5',      sym: '7#5',      name: 'Dominant 7 sharp 5',     iv: [0, 4, 8, 10],              fam: 'Altered' },
  { id: '7b9',      sym: '7b9',      name: 'Dominant 7 flat 9',      iv: [0, 4, 7, 10, 13],          fam: 'Altered' },
  { id: '7#9',      sym: '7#9',      name: 'Dominant 7 sharp 9',     iv: [0, 4, 7, 10, 15],          fam: 'Altered' },
  { id: '7b13',     sym: '7b13',     name: 'Dominant 7 flat 13',     iv: [0, 4, 7, 10, 20],          fam: 'Altered' },
  { id: '13b9',     sym: '13b9',     name: 'Dominant 13 flat 9',     iv: [0, 4, 7, 10, 13, 21],      fam: 'Altered' },
  { id: '7alt',     sym: '7alt',     name: 'Altered dominant',       iv: [0, 4, 10, 13, 15, 20],     fam: 'Altered' },
  { id: 'maj7#5',   sym: 'maj7#5',   name: 'Lydian augmented',       iv: [0, 4, 8, 11],              fam: 'Altered' },
];

// ── The mapping rule ──────────────────────────────────────────────────────
// A sus2 pad on root R supplies R (root), R+2 (the 9th) and R+7 (the 5th).
// It therefore fits any chord on R whose 5th is perfect and whose 9th is
// natural. Everything else needs a substitute pad -- or, for dim7, nothing.
function padFor(q) {
  const pcs = new Set(q.iv.map(i => ((i % 12) + 12) % 12));
  const maj3 = pcs.has(4);
  const b7 = pcs.has(10);
  const P5 = pcs.has(7);
  const b5 = pcs.has(6) && !P5;
  const s5 = pcs.has(8) && !P5;
  const b9 = pcs.has(1);
  const s9 = pcs.has(3) && maj3;   // a b3 is only a #9 when a major 3rd is also present

  if (q.id === 'dim7')
    return { off: null, why: 'Fully diminished — symmetrical, so every added note is a b9 against some chord tone. No sus2 pad fits; hold the previous pad through it.' };

  if (b5 && maj3 && b7)
    return { off: 2, why: 'b5 dominant — the pad a whole step up gives 9, 3 and 13, avoiding the 5th entirely.' };

  if (b5)
    return { off: 3, why: 'Flat 5 clashes with the pad 5th — the pad a minor 3rd up gives b3, 11 and b7.' };

  if (s5)
    return b7
      ? { off: 1, why: 'Altered dominant — the pad a semitone up (the tritone sub) gives b9, #9 and #5.' }
      : { off: 4, why: 'Augmented — the pad a major 3rd up gives 3, #11 and maj7 (Lydian augmented).' };

  if (b9 || s9)
    return { off: 1, why: 'Altered 9th clashes with the pad 9th — the pad a semitone up gives b9, #9 and b13.' };

  return { off: 0, why: 'Perfect 5th and natural 9th — the pad on the chord root fits directly.' };
}

// Describe what the pad's three notes do over the chord. The label depends on
// the chord: an Eb over Cm is a b3, but over C7 the same note is a #9.
function roles(chordRoot, padRoot, q) {
  const pcs = new Set(q.iv.map(i => ((i % 12) + 12) % 12));
  return [0, 2, 7].map(n => {
    const iv = (((padRoot + n) - chordRoot) % 12 + 12) % 12;
    if (iv === 3) return pcs.has(3) && !pcs.has(4) ? 'b3' : '#9';
    if (iv === 6) return pcs.has(6) && !pcs.has(7) ? 'b5' : '#11';
    if (iv === 8) return pcs.has(8) && !pcs.has(7) ? '#5' : 'b13';
    return DEGREE[iv];
  });
}

// ── Build ─────────────────────────────────────────────────────────────────
const chords = [];
for (let root = 0; root < 12; root++) {
  for (const q of QUALITIES) {
    const p = padFor(q);
    const pad = p.off === null ? null : (root + p.off) % 12;
    chords.push({
      root,
      rootName: NOTE[root],
      quality: q.id,
      symbol: NOTE[root] + q.sym,
      qualityName: q.name,
      family: q.fam,
      intervals: q.iv,
      pad,
      padName: pad === null ? 'HOLD' : NOTE[pad],
      padOffset: p.off,
      padRoles: pad === null ? [] : roles(root, pad, q),
      note: p.why,
    });
  }
}

const rules = QUALITIES.map(q => {
  const p = padFor(q);
  return { quality: q.id, symbol: q.sym, name: q.name, family: q.fam, intervals: q.iv, padOffset: p.off, why: p.why };
});

const shared = {};
for (const c of chords) {
  if (c.pad === null) continue;
  (shared[c.padName] ||= []).push(c.symbol);
}

const out = {
  generated: new Date().toISOString().slice(0, 10),
  padVoicing: 'sus2 — root, major 2nd, perfect 5th (no third)',
  notes: NOTE,
  degreeLabels: DEGREE,
  rules,
  chords,
  padCoverage: shared,
};

const jsonDir = path.join(ROOT, 'docs', 'chords');
fs.mkdirSync(jsonDir, { recursive: true });
fs.writeFileSync(path.join(jsonDir, 'nebula-chord-map.json'), JSON.stringify(out, null, 2));

// ── Markdown reference ────────────────────────────────────────────────────
const L = [];
L.push('# Nebula Tide — chord vocabulary and pad map', '');
L.push('_Generated by `tools/chords/generate.js`. Do not edit by hand._', '');
L.push('Every pad is a **sus2** voicing — root, major 2nd, perfect 5th, and deliberately **no third**.');
L.push('That missing third is what lets one pad serve both the major and the minor chord on the same');
L.push('root, plus most of their extensions. This file lists every chord in the vocabulary and the pad');
L.push('that sounds under it.', '');

L.push('## The rule in one line', '');
L.push('> A pad on root R fits any chord on R that has a **perfect 5th** and a **natural 9th**.');
L.push('> The third does not matter. Everything else needs a substitute pad.', '');

L.push('## Substitutions', '');
L.push('| Chord family | Pad | Interval | What the pad gives you |');
L.push('|---|---|---|---|');
const famRows = [
  ['Everything with a perfect 5th and natural 9th', 'Chord root', 'unison', 'root · 9 · 5'],
  ['Diminished triad, m7b5, m9b5', 'Minor 3rd up', '+3', 'b3 · 11 · b7'],
  ['Dominant 7b5', 'Whole step up', '+2', '9 · 3 · 13'],
  ['Augmented, maj7#5', 'Major 3rd up', '+4', '3 · #11 · maj7'],
  ['Altered dominants — 7b9, 7#9, 7#5, 7alt, 13b9', 'Semitone up', '+1', 'b9 · #9 · b13'],
  ['Diminished 7th', '**Hold previous**', '—', 'no sus2 pad fits'],
];
for (const r of famRows) L.push('| ' + r.join(' | ') + ' |');
L.push('');

L.push('## What each pad covers', '');
L.push('The number of chords that resolve onto each pad. This is the authoring view — when you record');
L.push('or choose the C pad, these are the chords it will be heard under.', '');
L.push('| Pad | Chords served |');
L.push('|---|---|');
for (const n of NOTE) L.push(`| **${n}** | ${(shared[n] || []).length} |`);
L.push('');

L.push('## Full map, key by key', '');
for (let root = 0; root < 12; root++) {
  L.push(`### ${NOTE[root]}`, '');
  L.push('| Chord | Quality | Pad | Pad plays |');
  L.push('|---|---|---|---|');
  for (const c of chords.filter(c => c.root === root)) {
    const pad = c.pad === null ? '**HOLD**' : `**${c.padName}**`;
    const play = c.pad === null ? '—' : c.padRoles.join(' · ');
    L.push(`| ${c.symbol} | ${c.qualityName} | ${pad} | ${play} |`);
  }
  L.push('');
}

fs.writeFileSync(path.join(jsonDir, 'CHORD-MAP.md'), L.join('\n'));

// ── Generated C++ table ───────────────────────────────────────────────────
const H = [];
H.push('#pragma once');
H.push('// GENERATED by tools/chords/generate.js — do not edit by hand.');
H.push('//');
H.push('// Chord-follow mapping. Each pad is a sus2 voicing (root, 2nd, 5th) with no');
H.push('// third, so one pad serves both the major and minor chord on that root.');
H.push('// padOffset is added to the chord root (mod 12) to get the pad to sound;');
H.push('// kHoldPad means no sus2 pad fits and the previous pad should be held.');
H.push('');
H.push('namespace chordmap');
H.push('{');
H.push('    static constexpr int kHoldPad = -1;');
H.push('');
H.push('    struct Quality');
H.push('    {');
H.push('        const char* id;');
H.push('        const char* symbol;');
H.push('        const char* name;');
H.push('        int   padOffset;      // semitones above the chord root, or kHoldPad');
H.push('        int   numIntervals;');
H.push('        int   intervals[8];   // semitones from the chord root');
H.push('    };');
H.push('');
H.push(`    static constexpr int numQualities = ${rules.length};`);
H.push('    static constexpr Quality qualities[numQualities] =');
H.push('    {');
for (const r of rules) {
  const iv = r.intervals.concat(Array(8 - r.intervals.length).fill(0));
  const off = r.padOffset === null ? 'kHoldPad' : String(r.padOffset);
  H.push(`        { "${r.quality}", "${r.symbol}", "${r.name}", ${off}, ${r.intervals.length}, { ${iv.join(', ')} } },`);
}
H.push('    };');
H.push('');
H.push('    // Pad index (0=C..11=B) for a chord, or kHoldPad to hold the previous pad.');
H.push('    inline int padForChord (int chordRoot, int qualityIndex) noexcept');
H.push('    {');
H.push('        if (qualityIndex < 0 || qualityIndex >= numQualities) return kHoldPad;');
H.push('        const int off = qualities[qualityIndex].padOffset;');
H.push('        if (off == kHoldPad) return kHoldPad;');
H.push('        return (((chordRoot + off) % 12) + 12) % 12;');
H.push('    }');
H.push('}');
H.push('');

fs.writeFileSync(path.join(ROOT, 'src', 'ChordMap.h'), H.join('\n'));

// ── Nebula Forge data ─────────────────────────────────────────────────────
// The preset editor shows, on each of the 12 pad slots, exactly which chords
// that pad will be heard under — so the sound can be chosen knowing the job.
const byPad = {};
for (const n of NOTE) byPad[n] = { count: 0, chords: [] };
for (const c of chords) {
  if (c.pad === null) continue;
  byPad[c.padName].count++;
  byPad[c.padName].chords.push(c.symbol);
}
// The chords each pad serves on its OWN root — the common case, listed first.
const ownRoot = {};
for (const n of NOTE) ownRoot[n] = [];
for (const c of chords) {
  if (c.pad !== null && c.padName === c.rootName) ownRoot[c.rootName].push(c.symbol);
}

fs.writeFileSync(
  path.join(ROOT, 'tools', 'forge', 'chord-data.js'),
  'window.NEBULA_CHORDS = ' +
    JSON.stringify({ notes: NOTE, byPad, ownRoot, rules: famRows }, null, 1) +
    ';\n'
);

console.log('wrote tools/forge/chord-data.js');
console.log(`chords:     ${chords.length} (${QUALITIES.length} qualities x 12 roots)`);
console.log(`hold cases: ${chords.filter(c => c.pad === null).length}`);
console.log('wrote docs/chords/nebula-chord-map.json');
console.log('wrote docs/chords/CHORD-MAP.md');
console.log('wrote src/ChordMap.h');
