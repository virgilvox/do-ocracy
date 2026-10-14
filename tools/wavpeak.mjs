// Peak and RMS of a 16-bit WAV from WAV=... node tools/shot.mjs, and how many
// samples sit at full scale (the limiter should keep that at zero).
//
//   node tools/wavpeak.mjs build/night.wav [max_peak_dbfs]
//
// Prints the overall peak and RMS in dBFS, the loudest second, and CLIP n when
// any sample reaches 32767. Exits non zero on a clip or a peak over the limit.
import { readFileSync } from 'node:fs';

const [path, limitArg] = process.argv.slice(2);
const b = readFileSync(path);
const data = b.indexOf('data', 12);
const n = (b.length - data - 8) >> 1;
const db = (v) => (v > 0 ? 20 * Math.log10(v) : -Infinity).toFixed(1);
let peak = 0, sum = 0, clip = 0, secPeak = 0, secAt = 0, cur = 0;
const perSec = 48000 * 2;
for (let i = 0; i < n; i++) {
  const s = b.readInt16LE(data + 8 + i * 2);
  const a = Math.abs(s) / 32767;
  if (Math.abs(s) >= 32767) clip++;
  if (a > peak) peak = a;
  if (a > cur) cur = a;
  sum += a * a;
  if ((i + 1) % perSec === 0) { if (cur > secPeak) { secPeak = cur; secAt = (i + 1) / perSec - 1; } cur = 0; }
}
const rms = Math.sqrt(sum / n);
console.log(`${path}: ${(n / perSec).toFixed(1)} s, peak ${db(peak)} dBFS, rms ${db(rms)} dBFS, loudest second ${secAt} s`);
const limit = limitArg !== undefined ? Number(limitArg) : -0.1;
if (clip) console.log(`CLIP ${clip} samples at full scale`);
if (20 * Math.log10(peak) > limit) console.log(`PEAK over ${limit} dBFS`);
process.exit(clip || 20 * Math.log10(peak) > limit ? 1 : 0);
