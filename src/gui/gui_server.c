#include "gui_server.h"
#include "gui_protocol.h"
#include "cursor.h"

#include "fb.h"
#include "input.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <abi/mmap.h>
#include <abi/shm.h>
#include <abi/syscalls.h>

typedef long scw;
extern long __do_syscall_ret(unsigned long);
extern scw __do_syscall3(long, scw, scw, scw);
extern scw __do_syscall6(long, scw, scw, scw, scw, scw, scw);

#define SHM_DEVICE "/dev/shm"

static int shm_ioctl(int fd, unsigned long req, void *arg) {
    return (int)__do_syscall_ret(__do_syscall3(SYS_IOCTL, (scw)fd, (scw)req, (scw)arg));
}

static void *shm_map(int fd, uint64_t index, size_t len) {
    return (void *)__do_syscall_ret(__do_syscall6(
        SYS_MMAP, 0, (scw)len, PROT_READ | PROT_WRITE, MAP_SHARED, (scw)fd,
        (scw)(index * 4096)));
}

// Regions are named deterministically so a client can find one with the query
// ioctl and learn its mmap offset, rather than the two sides exchanging
// addresses across the process boundary.
static const char *CTL_PREFIX  = "eucalypt-gui-ctl";
static const char *SLOT_PREFIX = "eucalypt-gui-slot-";
static const char *WIN_PREFIX  = "eucalypt-gui-win-";
static const char *CURSOR_REGION = "eucalypt-gui-cursor";

#define WIN_FREE   0
#define WIN_NORMAL 1
#define WIN_CURSOR 2

typedef struct {
    uint32_t  state;         // WIN_FREE / WIN_NORMAL / WIN_CURSOR
    uint32_t  id;
    int       owner_slot;    // which client slot may address it
    int       x, y, w, h;
    uint64_t  shm_index;
    size_t    shm_size;
    uint32_t *pixels;        // mapped backing store; NULL if the region failed
    int       visible;
    int       has_alpha;     // skip pixels whose alpha byte is 0 (the cursor)
    char      title[GUI_TITLE_LEN];
} gui_window_t;

static gui_window_t g_windows[GUI_MAX_WINDOWS];
static int g_shm_fd = -1;
static gui_ctl_t *g_ctl = NULL;
static gui_slot_t *g_slots[GUI_MAX_CLIENTS];
static uint32_t g_screen_w, g_screen_h;
static uint32_t g_focus_window = 0;
static int g_next_window_id = 1;
static int g_prev_buttons = 0;

#define GUI_BG 0x00181820u   // flat dark background, no chrome

// Create a region of `bytes` and map it here. Returns the mapping, or NULL.
static void *make_region(const char *name, size_t bytes, uint64_t *out_index) {
    shm_create_t c;
    memset(&c, 0, sizeof(c));
    strncpy(c.name, name, SHM_NAME_LEN - 1);
    c.size = bytes;
    if (shm_ioctl(g_shm_fd, SHM_IOC_CREATE, &c) != 0) return NULL;

    void *p = shm_map(g_shm_fd, c.index, bytes);
    if ((long)p < 0) return NULL;
    if (out_index) *out_index = c.index;
    return p;
}

// Windows are drawn in table order, so a later entry sits on top of an earlier
// one. That makes the table double as the z-order, and the cursor is simply
// always composited last regardless of where it sits in the table.
static gui_window_t *alloc_window(void) {
    for (int i = 0; i < GUI_MAX_WINDOWS; i++)
        if (g_windows[i].state == WIN_FREE) return &g_windows[i];
    return NULL;
}

static gui_window_t *find_window(uint32_t id) {
    for (int i = 0; i < GUI_MAX_WINDOWS; i++)
        if (g_windows[i].state != WIN_FREE && g_windows[i].id == id)
            return &g_windows[i];
    return NULL;
}

static void send_event(int slot, const gui_event_t *ev) {
    if (slot < 0 || slot >= GUI_MAX_CLIENTS) return;
    gui_slot_t *s = g_slots[slot];
    if (!s) return;
    // Mirror of the request handshake, running the other way: an odd evt_seq
    // means an event is waiting for the client to acknowledge it. Dropping here
    // rather than queueing is deliberate -- the mailbox holds exactly one event,
    // and input is polled far faster than a client can drain it, so a queue
    // would grow without bound.
    if (s->evt_seq & 1u) return;
    s->evt = *ev;
    s->evt_seq = s->evt_seq + 1;   // odd: waiting for the client to acknowledge
}

// --- request handling ------------------------------------------------------

static void handle_request(int slot, gui_request_t *req, gui_reply_t *reply) {
    reply->status    = 0;
    reply->window_id = 0;
    reply->shm_index = 0;
    reply->shm_size  = 0;

    switch (req->type) {
        case GUI_REQ_CREATE_WINDOW: {
            int w = req->w  > 0 ? req->w  : 320;
            int h = req->h  > 0 ? req->h  : 200;
            if (w > (int)g_screen_w) w = (int)g_screen_w;
            if (h > (int)g_screen_h) h = (int)g_screen_h;

            gui_window_t *win = alloc_window();
            if (!win) { reply->status = -ENOMEM; break; }

            int id = g_next_window_id;
            char name[SHM_NAME_LEN];
            snprintf(name, sizeof(name), "%s%d", WIN_PREFIX, id);

            uint64_t idx = 0;
            uint32_t *px = (uint32_t *)make_region(name, (size_t)w * h * 4, &idx);
            if (!px) { reply->status = -ENOMEM; break; }

            for (size_t i = 0; i < (size_t)w * h; i++) px[i] = 0x00202020u;

            win->state      = WIN_NORMAL;
            win->id         = (uint32_t)g_next_window_id++;
            win->owner_slot = slot;
            win->x = req->x; win->y = req->y;
            win->w = w; win->h = h;
            win->shm_index  = idx;
            win->shm_size   = (size_t)w * h * 4;
            win->pixels     = px;
            win->visible    = 1;
            strncpy(win->title, req->title, GUI_TITLE_LEN - 1);
            win->title[GUI_TITLE_LEN - 1] = 0;

            reply->window_id = win->id;
            reply->shm_index = idx;
            reply->shm_size  = win->shm_size;
            break;
        }

        case GUI_REQ_DESTROY_WINDOW: {
            gui_window_t *w = find_window(req->window_id);
            if (!w || w->owner_slot != slot || w->state != WIN_NORMAL) {
                reply->status = -EINVAL; break;
            }
            if (g_focus_window == w->id) g_focus_window = 0;
            w->state  = WIN_FREE;
            w->pixels = NULL;
            break;
        }

        case GUI_REQ_MOVE_WINDOW: {
            gui_window_t *w = find_window(req->window_id);
            if (!w || w->owner_slot != slot || w->state != WIN_NORMAL) {
                reply->status = -EINVAL; break;
            }
            w->x = req->x;
            w->y = req->y;
            break;
        }

        case GUI_REQ_DAMAGE:
            // Informational for now: the compositor recomposites the entire
            // screen every frame, so there is no partial-update bookkeeping yet.
            break;

        case GUI_REQ_SET_TITLE: {
            gui_window_t *w = find_window(req->window_id);
            if (!w || w->owner_slot != slot) { reply->status = -EINVAL; break; }
            strncpy(w->title, req->title, GUI_TITLE_LEN - 1);
            w->title[GUI_TITLE_LEN - 1] = 0;
            break;
        }

        case GUI_REQ_QUIT:
            reply->status = -EPERM;   // a client does not get to stop the desktop
            break;

        default:
            reply->status = -EINVAL;
            break;
    }
}

static void service_slots(void) {
    if (!g_ctl) return;
    for (int i = 0; i < GUI_MAX_CLIENTS; i++) {
        gui_slot_t *s = g_slots[i];
        if (!s || s->owner == 0) continue;

        uint32_t seq = s->req_seq;
        if (seq & 1u) {                       // odd: a request is waiting
            gui_request_t req = s->req;
            gui_reply_t   reply;
            handle_request(i, &req, &reply);
            s->reply   = reply;
            s->req_seq = seq + 1;             // even again: the reply is ready
        }
    }
}

// --- input routing ---------------------------------------------------------

// Topmost normal window containing the point. The cursor window is skipped so
// the sprite never swallows a click meant for whatever is underneath it.
static gui_window_t *hit_test(int px, int py) {
    gui_window_t *best = NULL;
    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        gui_window_t *w = &g_windows[i];
        if (w->state != WIN_NORMAL || !w->visible) continue;
        if (px < w->x || py < w->y) continue;
        if (px >= w->x + w->w || py >= w->y + w->h) continue;
        best = w;      // later entries draw on top, so the last match wins
    }
    return best;
}

static void route_input(void) {
    const mouse_state_t *m = input_mouse();

    // Keys go to the focused window, or nowhere if nothing has focus.
    key_event_t keys[16];
    int nk = input_keys(keys, 16);
    for (int i = 0; i < nk; i++) {
        gui_window_t *fw = g_focus_window ? find_window(g_focus_window) : NULL;
        if (!fw) continue;
        gui_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type      = GUI_EVT_KEY;
        ev.window_id = fw->id;
        ev.button    = keys[i].code;
        ev.state     = keys[i].pressed;
        send_event(fw->owner_slot, &ev);
    }

    if (m->dx || m->dy) {
        gui_window_t *w = hit_test(m->x, m->y);
        if (w) {
            gui_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.type      = GUI_EVT_MOTION;
            ev.window_id = w->id;
            ev.x         = m->x - w->x;
            ev.y         = m->y - w->y;
            send_event(w->owner_slot, &ev);
        }
    }

    // input_mouse() exposes a level-triggered bitmask, so compare against the
    // previous frame to turn it into press/release edges. Without this a held
    // button would re-report every frame.
    for (int b = 0; b < 3; b++) {
        int bit = 1 << b;
        int held_now  = (m->buttons & bit) != 0;
        int held_prev = (g_prev_buttons & bit) != 0;
        if (held_now == held_prev) continue;

        gui_window_t *w = hit_test(m->x, m->y);
        if (w) {
            if (held_now && g_focus_window != w->id) {
                g_focus_window = w->id;
                gui_event_t fe;
                memset(&fe, 0, sizeof(fe));
                fe.type      = GUI_EVT_FOCUS;
                fe.window_id = w->id;
                fe.state     = 1;
                send_event(w->owner_slot, &fe);
            }
            gui_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.type      = GUI_EVT_BUTTON;
            ev.window_id = w->id;
            ev.x         = m->x - w->x;
            ev.y         = m->y - w->y;
            ev.button    = b;
            ev.state     = held_now;
            send_event(w->owner_slot, &ev);
        }
    }
    g_prev_buttons = m->buttons;
}

// --- compositing -----------------------------------------------------------

static void blit_window(gui_window_t *w) {
    if (!w->pixels || !w->visible) return;
    uint32_t *dst = fb_buffer();
    int sw = (int)g_screen_w, sh = (int)g_screen_h;

    for (int row = 0; row < w->h; row++) {
        int dy = w->y + row;
        if (dy < 0 || dy >= sh) continue;
        // Clip the source span to the part of the row that is actually on screen
        int sx0 = 0, sx1 = w->w;
        if (w->x < 0)     sx0 = -w->x;
        if (w->x + w->w > sw) sx1 = sw - w->x;
        if (sx0 >= sx1) continue;

        uint32_t *d = dst + (size_t)dy * sw + (w->x + sx0);
        const uint32_t *s = w->pixels + (size_t)row * w->w + sx0;
        int n = sx1 - sx0;
        if (w->has_alpha) {
            // Alpha 0 means "show what is already there", which is what lets the
            // cursor sit over windows without an opaque box around it.
            for (int i = 0; i < n; i++)
                if (s[i] >> 24) d[i] = s[i];
        } else {
            for (int i = 0; i < n; i++) d[i] = s[i];
        }
    }
}

static void composite(void) {
    uint32_t *dst = fb_buffer();
    size_t total = (size_t)g_screen_w * g_screen_h;

    // The screen is rebuilt from scratch every frame, which is what stops the
    // cursor leaving a trail: nothing persists between frames, so there is no
    // old footprint to erase.
    for (size_t i = 0; i < total; i++) dst[i] = GUI_BG;

    for (int i = 0; i < GUI_MAX_WINDOWS; i++)
        if (g_windows[i].state == WIN_NORMAL) blit_window(&g_windows[i]);

    // The cursor is a window like any other and goes through the same blit, but
    // it is composited last so it always sits on top, and its origin follows the
    // pointer. It is skipped by hit-testing, so it never steals a click.
    const mouse_state_t *m = input_mouse();
    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (g_windows[i].state != WIN_CURSOR) continue;
        g_windows[i].x = m->x;
        g_windows[i].y = m->y;
        blit_window(&g_windows[i]);
    }
}

// --- lifecycle -------------------------------------------------------------

int gui_server_window_count(void) {
    int n = 0;
    for (int i = 0; i < GUI_MAX_WINDOWS; i++)
        if (g_windows[i].state == WIN_NORMAL) n++;
    return n;
}

int gui_server_init(void) {
    memset(g_windows, 0, sizeof(g_windows));
    memset(g_slots, 0, sizeof(g_slots));

    if (fb_init() != 0) {
        printf("gui: fb_init failed\n");
        return -1;
    }
    g_screen_w = (uint32_t)fb_width();
    g_screen_h = (uint32_t)fb_height();

    if (input_init() != 0)
        printf("gui: input_init failed, continuing without input\n");

    g_shm_fd = open(SHM_DEVICE, O_RDWR);
    if (g_shm_fd < 0) {
        printf("gui: cannot open %s\n", SHM_DEVICE);
        return -1;
    }

    g_ctl = (gui_ctl_t *)make_region(CTL_PREFIX, sizeof(gui_ctl_t), NULL);
    if (!g_ctl) {
        printf("gui: cannot create/map control region\n");
        return -1;
    }
    memset(g_ctl, 0, sizeof(gui_ctl_t));
    g_ctl->magic      = GUI_CTL_MAGIC;
    g_ctl->slot_count = GUI_MAX_CLIENTS;

    for (int i = 0; i < GUI_MAX_CLIENTS; i++) {
        char name[SHM_NAME_LEN];
        snprintf(name, sizeof(name), "%s%d", SLOT_PREFIX, i);
        g_slots[i] = (gui_slot_t *)make_region(name, sizeof(gui_slot_t), NULL);
        if (g_slots[i]) memset(g_slots[i], 0, sizeof(gui_slot_t));
        else printf("gui: slot %d unavailable\n", i);
    }

    // The cursor is window 1, living in the same table as everything else, with
    // its own shared surface. The sprite is rasterised into that surface once at
    // startup; from then on it is composited by the ordinary window path, so a
    // client can replace it simply by drawing into the cursor window's region.
    gui_window_t *cur = alloc_window();
    if (cur) {
        cur->state      = WIN_CURSOR;
        cur->id         = (uint32_t)g_next_window_id++;
        cur->owner_slot = -1;          // no client owns the cursor
        cur->w = GUI_CURSOR_W;
        cur->h = GUI_CURSOR_H;
        cur->visible    = 1;
        cur->has_alpha  = 1;
        strncpy(cur->title, "cursor", GUI_TITLE_LEN - 1);

        size_t bytes = (size_t)cur->w * cur->h * sizeof(uint32_t);
        cur->pixels = make_region(CURSOR_REGION, bytes, &cur->shm_index);
        if (cur->pixels) {
            cur->shm_size = bytes;
            gui_cursor_blit(cur->pixels, cur->w, 0, 0);
        } else {
            printf("gui: cursor has no backing region, sprite will not draw\n");
        }
        if (g_ctl) g_ctl->cursor_window_id = cur->id;
    }

    printf("gui: compositor up, screen %ux%u, %d client slots\n",
           g_screen_w, g_screen_h, GUI_MAX_CLIENTS);
    return 0;
}

void gui_server_run(void) {
    while (1) {
        input_poll();
        route_input();
        service_slots();
        composite();
        fb_present();

        // Each pass pushes a full-screen frame, so spinning flat out would burn
        // the only CPU and starve every client. A short sleep caps the redraw
        // rate and leaves the scheduler room to run other processes.
        usleep(1000);
    }
}
