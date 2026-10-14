// Reads a save region dumped by SAVE=... node tools/shot.mjs and checks it.
//
//   node tools/accept/check-save.mjs build/save.bin [field=value ...]
//
// Fields: mode night_len bots chaos phones music sfx layout help nights.
// Exits non zero when the magic, version or crc is wrong or a field differs.
import { readFileSync } from 'node:fs';
import { crc32 } from 'node:zlib';

const [path, ...want] = process.argv.slice(2);
const b = readFileSync(path);
const magic = b.readUInt32LE(0), version = b.readUInt32LE(4), crc = b.readUInt32LE(8);
const body = b.subarray(12);
const got = {
  mode: b[12], night_len: b[13], bots: b[14], chaos: b[15], phones: b[16],
  music: b[17], sfx: b[18], layout: b[19], help: b[20], nights: b.readUInt32LE(28),
};
const bad = [];
if (magic !== 0x434f4f44) bad.push(`magic 0x${magic.toString(16)}`);
if (version !== 1) bad.push(`version ${version}`);
if ((crc32(body) >>> 0) !== crc) bad.push('crc does not verify');
for (const w of want) {
  const [k, v] = w.split('=');
  if (!(k in got)) bad.push(`unknown field ${k}`);
  else if (got[k] !== Number(v)) bad.push(`${k} ${got[k]}, want ${v}`);
}
console.log(`save ${path}: ${Object.entries(got).map(([k, v]) => `${k} ${v}`).join(', ')}`);
if (bad.length) { console.log(`SAVE_BAD ${bad.join('; ')}`); process.exit(1); }
console.log('save ok');
