/* Phone players over CLASP, spoken directly by the cart.
 *
 * The cart dials wss://relay.clasp.to through wasmcart's wc_peer_open (the
 * manifest grants that one domain) and speaks CLASP's binary frames itself:
 * HELLO, SUBSCRIBE, and event PUBLISHes carrying short strings. No browser
 * wraps the cart; phones run web/controller.html and talk to the same relay.
 *
 * Addresses (ROOM is the 4 letter code on screen, PID the phone's own id):
 *   phone -> cart  /hsx/ROOM/in/PID    "J<name>"  join / still here
 *                                      "I<btn>,<x>,<y>"  input (btn bits 1 A, 2 B, 4 X; x,y -100..100)
 *                                      "L"  leaving
 *   cart -> phone  /hsx/ROOM/out/PID   "W<slot>|<rrggbb>|<name>"  welcome
 *                                      "S<clout>|<karma>|<holding>|<hint>"  status
 *                                      "Z<ms>"  buzz
 *                                      "F"  room full
 *   cart -> all    /hsx/ROOM/out/all   "P<phase>|<seconds left>|<clutter>|<vibe>"
 */
#ifndef HSX_NET_H
#define HSX_NET_H
#include <stdbool.h>

void net_init(const char *relay_url, const char *room);
void net_update(float dt);           /* reconnects; call once a frame */
bool net_online(void);
void net_send(const char *pid, const char *msg);   /* pid NULL = all */

/* implemented by the game */
void game_on_phone(const char *pid, const char *msg);
/* first contact with the relay: its session id is fresh entropy for the room
 * code (some hosts never seed the cart). Returns the room to subscribe to. */
const char *game_room_from_session(const char *session);

#endif
