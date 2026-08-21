// Assemble timestamped frames into a smooth 30fps MP4 with true real-time pacing.
// Run: node assemble.js
const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const ffmpeg = require('ffmpeg-static');

const framesDir = path.join(__dirname, 'frames');
const stamps = fs.readFileSync(path.join(framesDir, 'stamps.txt'), 'utf8')
  .trim().split(/\r?\n/).map(Number);

// ffconcat with per-frame durations from the recorded timestamps
let list = 'ffconcat version 1.0\n';
for (let i = 0; i < stamps.length; i++) {
  const dur = i < stamps.length - 1 ? (stamps[i + 1] - stamps[i]) / 1000 : 0.5;
  list += `file 'f${String(i).padStart(5, '0')}.png'\nduration ${dur.toFixed(4)}\n`;
}
// concat demuxer quirk: repeat last file so its duration is honored
list += `file 'f${String(stamps.length - 1).padStart(5, '0')}.png'\n`;
fs.writeFileSync(path.join(framesDir, 'list.ffconcat'), list);

const out = path.join(__dirname, '..', '..', 'marketing', 'walkthrough.mp4');
const r = spawnSync(ffmpeg, [
  '-y', '-f', 'concat', '-safe', '0', '-i', path.join(framesDir, 'list.ffconcat'),
  '-vf', 'fps=30,format=yuv420p',
  '-c:v', 'libx264', '-crf', '19', '-preset', 'medium',
  '-movflags', '+faststart', out
], { stdio: ['ignore', 'pipe', 'pipe'] });

if (r.status !== 0) { console.error(r.stderr.toString().slice(-2000)); process.exit(1); }
const size = (fs.statSync(out).size / 1e6).toFixed(1);
const total = ((stamps[stamps.length - 1]) / 1000 + 0.5).toFixed(1);
console.log(`OK ${out} — ${size} MB, ~${total}s, ${stamps.length} source frames`);
