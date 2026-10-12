#include "net.h"
#include "wasmcart.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

static char url[128], room[8];
static int sock = -1;
static bool online;
static float retry_in;
static int fails;
static bool named;   /* the room code is fixed after the first WELCOME */

static void dial(void) {
  sock = wc_peer_open(url, (unsigned)strlen(url));
  online = false;
  char m[160];
  snprintf(m, sizeof m, "net: dial %s -> %d", url, sock);
  wc_log(m, (unsigned)strlen(m));
  if (sock < 0) { retry_in = 10.f; fails++; }
}

void net_init(const char *relay_url, const char *rm) {
  snprintf(url, sizeof url, "%s", relay_url);
  snprintf(room, sizeof room, "%s", rm);
  dial();
}

bool net_online(void) { return online; }

void net_update(float dt) {
  if (sock >= 0) return;
  retry_in -= dt;
  if (retry_in <= 0) dial();
}

/* ── CLASP binary frames ─────────────────────────────────────────── */

static uint8_t out[1024];

static int put_str(uint8_t *p, const char *s) {
  int n = (int)strlen(s);
  if (n > 400) n = 400;
  p[0] = (uint8_t)(n >> 8); p[1] = (uint8_t)n;
  memcpy(p + 2, s, (size_t)n);
  return n + 2;
}

static void send_payload(int len) {
  if (sock < 0) return;
  out[0] = 0x53;            /* 'S' */
  out[1] = 0x01;            /* QoS fire, no timestamp, binary encoding */
  out[2] = (uint8_t)(len >> 8); out[3] = (uint8_t)len;
  wc_peer_send(sock, out, (unsigned)(len + 4));
}

static void send_hello(void) {
  uint8_t *p = out + 4; int n = 0;
  p[n++] = 0x01;            /* HELLO */
  p[n++] = 3;               /* protocol version */
  p[n++] = 0xE0;            /* features: param, event, stream */
  n += put_str(p + n, "do-ocracy-cart");
  p[n++] = 0; p[n++] = 0;   /* no token */
  send_payload(n);
}

static void send_subscribe(void) {
  char pat[64];
  snprintf(pat, sizeof pat, "/hsx/%s/in/**", room);
  uint8_t *p = out + 4; int n = 0;
  p[n++] = 0x10;            /* SUBSCRIBE */
  p[n++] = 0; p[n++] = 0; p[n++] = 0; p[n++] = 1;  /* id 1 */
  n += put_str(p + n, pat);
  p[n++] = 0xFF;            /* all signal types */
  p[n++] = 0;               /* no options */
  send_payload(n);
}

void net_send(const char *pid, const char *msg) {
  if (!online) return;
  char addr[64];
  snprintf(addr, sizeof addr, "/hsx/%s/out/%s", room, pid ? pid : "all");
  uint8_t *p = out + 4; int n = 0;
  p[n++] = 0x20;            /* PUBLISH */
  p[n++] = 1 << 5;          /* signal: event */
  n += put_str(p + n, addr);
  p[n++] = 1;               /* one value */
  p[n++] = 0x08;            /* string */
  n += put_str(p + n, msg);
  send_payload(n);
}

static void on_publish(const uint8_t *p, int len) {
  /* flags, address, value indicator, value type, value */
  if (len < 5) return;
  int i = 2;   /* [0] type, [1] flags */
  int alen = (p[i] << 8) | p[i + 1]; i += 2;
  if (i + alen + 2 > len) return;
  char addr[96];
  int an = alen < 95 ? alen : 95;
  memcpy(addr, p + i, (size_t)an); addr[an] = 0;
  i += alen;
  if (p[i++] != 1) return;
  if (p[i++] != 0x08) return;               /* strings only */
  if (i + 2 > len) return;
  int slen = (p[i] << 8) | p[i + 1]; i += 2;
  if (i + slen > len) return;
  char msg[160];
  int sn = slen < 159 ? slen : 159;
  memcpy(msg, p + i, (size_t)sn); msg[sn] = 0;

  char prefix[32];
  snprintf(prefix, sizeof prefix, "/hsx/%s/in/", room);
  size_t pl = strlen(prefix);
  if (strncmp(addr, prefix, pl) != 0) return;
  const char *pid = addr + pl;
  if (!*pid || strlen(pid) > 24) return;
  game_on_phone(pid, msg);
}

static void on_frame(const uint8_t *p, int len) {
  if (len < 1) return;
  switch (p[0]) {
    case 0x02: {   /* WELCOME: version, features, time u64, session string, ... */
      if (!named && len > 13) {
        int sl = (p[11] << 8) | p[12];
        char session[80];
        int n = sl < 79 ? sl : 79;
        if (13 + n <= len) { memcpy(session, p + 13, (size_t)n); session[n] = 0; snprintf(room, sizeof room, "%s", game_room_from_session(session)); }
        named = true;
      }
      online = true; fails = 0; send_subscribe();
      { char m[48]; snprintf(m, sizeof m, "net: relay welcomed us, room %s", room); wc_log(m, (unsigned)strlen(m)); }
      break;
    }
    case 0x20: on_publish(p, len); break;                               /* PUBLISH */
    case 0x41: { uint8_t *q = out + 4; q[0] = 0x42; send_payload(1); break; } /* PING -> PONG */
    default: break;
  }
}

__attribute__((export_name("wc_peer_on_connect")))
void wc_peer_on_connect(int32_t id, const char *name, uint32_t name_len) {
  (void)name; (void)name_len;
  if (id == sock) { WC_LOG("net: socket open"); send_hello(); }
}

__attribute__((export_name("wc_peer_on_message")))
void wc_peer_on_message(int32_t id, const uint8_t *data, uint32_t len) {
  if (id != sock) return;
  uint32_t off = 0;
  while (off + 4 <= len) {
    if (data[off] != 0x53) return;
    int flags = data[off + 1];
    int plen = (data[off + 2] << 8) | data[off + 3];
    uint32_t hdr = (flags & 0x20) ? 12 : 4;
    if (off + hdr + (uint32_t)plen > len) return;
    on_frame(data + off + hdr, plen);
    off += hdr + (uint32_t)plen;
  }
}

static void lost(int32_t id) {
  if (id != sock) return;
  sock = -1; online = false;
  fails++;
  retry_in = fails < 4 ? 3.f : 15.f;
}

__attribute__((export_name("wc_peer_on_disconnect")))
void wc_peer_on_disconnect(int32_t id) { lost(id); }

__attribute__((export_name("wc_peer_on_error")))
void wc_peer_on_error(int32_t id) { lost(id); }
