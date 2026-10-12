// A fake phone: joins a room over the CLASP relay the way web/join.html does,
// then walks around and presses buttons. For testing the cart's netcode.
//
//   node tools/phone-sim.mjs ROOM [name] [seconds]
import { Clasp } from '@clasp-to/core';
const [room, name = 'SIMPHONE', secs = '12'] = process.argv.slice(2);
const pid = 'sim' + Math.random().toString(36).slice(2, 8);
const c = new Clasp('wss://relay.clasp.to', { name: 'do-ocracy-phone-sim' });
await c.connect();
let welcomed = false;
c.on(`/hsx/${room}/out/**`, (v, a) => {
  const to = a.split('/').pop();
  if (to !== pid && to !== 'all') return;
  if (v[0] === 'W') welcomed = true;
  if (v[0] !== 'P' || Math.random() < 0.2) console.log('phone got', to === 'all' ? '(all)' : '(me)', v);
});
const send = (m) => c.emit(`/hsx/${room}/in/${pid}`, m);
const t0 = Date.now();
const join = setInterval(() => !welcomed && send('J' + name), 1000);
send('J' + name);
const tick = setInterval(() => {
  if (!welcomed) return;
  const t = (Date.now() - t0) / 1000;
  const x = Math.round(Math.cos(t) * 100), y = Math.round(Math.sin(t) * 100);
  const b = (Math.floor(t * 2) % 4 === 0 ? 1 : 0) | (t > 6 && t < 6.3 ? 4 : 0);
  send(`I${b},${x},${y}`);
}, 100);
setTimeout(() => { send('L'); clearInterval(join); clearInterval(tick); console.log('welcomed:', welcomed); process.exit(welcomed ? 0 : 1); }, Number(secs) * 1000);
