#include "gui_client.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
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

static const char *CTL_PREFIX  = "eucalypt-gui-ctl";
static const char *SLOT_PREFIX = "eucalypt-gui-slot-";

static int      g_shm_fd = -1;
static gui_ctl_t *g_ctl = NULL;
static gui_slot_t *g_slot = NULL;    // this client's mailbox
static int      g_slot_index = -1;
static gui_window_t *g_owned[GUI_MAX_WINDOWS];
static int      g_owned_count = 0;

static int shm_ioctl(int fd, unsigned long req, void *arg) {
    return (int)__do_syscall_ret(__do_syscall3(SYS_IOCTL, (scw)fd, (scw)req, (scw)arg));
}

static void *shm_map(int fd, uint64_t index, size_t len) {
    return (void *)__do_syscall_ret(__do_syscall6(
        SYS_MMAP, 0, (scw)len, PROT_READ | PROT_WRITE, MAP_SHARED, (scw)fd,
        (scw)(index * 4096)));
}

// Look a region up by name. Returns 0 and fills the query on success.
static int shm_query(const char *name, shm_query_t *q) {
    memset(q, 0, sizeof(*q));
    strncpy(q->name, name, SHM_NAME_LEN - 1);
    if (shm_ioctl(g_shm_fd, SHM_IOC_QUERY, q) != 0) return -1;
    return q->found ? 0 : -1;
}

// Publish a request and spin until the compositor has filled in the reply.
static int submit(gui_request_t *req, gui_reply_t *reply) {
    if (!g_slot) return -ENOTCONN;

    uint32_t seq = g_slot->req_seq;
    // Wait for any request still in flight so we never overwrite a live slot.
    while (seq & 1u) { __asm__ volatile("pause"); seq = g_slot->req_seq; }

    g_slot->req     = *req;
    g_slot->reply   = (gui_reply_t){0};
    g_slot->req_seq = seq + 1;          // odd: request published

    while (g_slot->req_seq & 1u) __asm__ volatile("pause");

    *reply = g_slot->reply;
    return reply->status;
}

int gui_connect(void) {
    if (g_slot) return 0;               // already connected

    g_shm_fd = open(SHM_DEVICE, O_RDWR);
    if (g_shm_fd < 0) return -errno;

    shm_query_t q;
    if (shm_query(CTL_PREFIX, &q) != 0) {
        printf("gui: compositor not running (no %s)\n", CTL_PREFIX);
        close(g_shm_fd);
        g_shm_fd = -1;
        return -ENOENT;
    }

    g_ctl = (gui_ctl_t *)shm_map(g_shm_fd, q.index, sizeof(gui_ctl_t));
    if ((long)g_ctl < 0) {
        g_ctl = NULL;
        return -EINVAL;
    }

    // The compositor creates the region before it stamps the magic, so a client
    // that races ahead of startup sees a zeroed block. Wait for it rather than
    // rejecting a compositor that is merely still booting.
    for (int tries = 0; tries < 200; tries++) {
        if (g_ctl->magic == GUI_CTL_MAGIC) break;
        usleep(1000);
    }
    if (g_ctl->magic != GUI_CTL_MAGIC) {
        printf("gui: bad control block (magic=%08x, want %08x)\n",
               g_ctl->magic, GUI_CTL_MAGIC);
        g_ctl = NULL;
        return -EINVAL;
    }

    // Claim the first mailbox nobody owns. This is a plain read-then-write with
    // no atomic compare-and-swap, which is sound here only because clients run
    // on the same single CPU as the compositor; a real SMP bring-up would need
    // a lock bit or an atomic CAS in the kernel.
    for (int i = 0; i < GUI_MAX_CLIENTS; i++) {
        char name[SHM_NAME_LEN];
        snprintf(name, sizeof(name), "%s%d", SLOT_PREFIX, i);
        if (shm_query(name, &q) != 0) continue;

        gui_slot_t *s = (gui_slot_t *)shm_map(g_shm_fd, q.index, sizeof(gui_slot_t));
        if ((long)s < 0) continue;
        if (s->owner != 0) continue;

        s->owner = (uint32_t)getpid();
        g_slot = s;
        g_slot_index = i;
        return 0;
    }

    printf("gui: all %d client slots are in use\n", GUI_MAX_CLIENTS);
    return -EAGAIN;
}

// Windows are individually allocated and tracked by pointer, so destroying one
// never shuffles the rest and invalidates pointers the caller still holds.
static gui_window_t *track(uint32_t id, uint32_t *pixels, int w, int h,
                           int x, int y, const char *title) {
    if (g_owned_count >= GUI_MAX_WINDOWS) return NULL;
    gui_window_t *win = (gui_window_t *)malloc(sizeof(gui_window_t));
    if (!win) return NULL;
    memset(win, 0, sizeof(*win));
    win->id     = id;
    win->pixels = pixels;
    win->w      = w;
    win->h      = h;
    win->x      = x;
    win->y      = y;
    strncpy(win->title, title, GUI_TITLE_LEN - 1);
    win->title[GUI_TITLE_LEN - 1] = 0;
    g_owned[g_owned_count++] = win;
    return win;
}

gui_window_t *gui_window_create(const char *title, int x, int y, int w, int h) {
    if (!g_slot) return NULL;

    gui_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = GUI_REQ_CREATE_WINDOW;
    req.x = x; req.y = y; req.w = w; req.h = h;
    strncpy(req.title, title ? title : "", GUI_TITLE_LEN - 1);

    gui_reply_t reply;
    if (submit(&req, &reply) != 0) return NULL;
    if (reply.window_id == 0) return NULL;

    // The compositor tells us which region backs this window, so the two sides
    // never exchange a raw address.
    void *px = shm_map(g_shm_fd, reply.shm_index, (size_t)reply.shm_size);
    if ((long)px < 0) return NULL;

    return track(reply.window_id, (uint32_t *)px, w, h, x, y, title ? title : "");
}

int gui_window_damage(gui_window_t *win, int x, int y, int w, int h) {
    if (!g_slot || !win) return -EINVAL;
    gui_request_t req;
    memset(&req, 0, sizeof(req));
    req.type     = GUI_REQ_DAMAGE;
    req.window_id = win->id;
    req.x = x; req.y = y; req.w = w; req.h = h;
    gui_reply_t reply;
    return submit(&req, &reply);
}

int gui_window_move(gui_window_t *win, int x, int y) {
    if (!g_slot || !win) return -EINVAL;
    gui_request_t req;
    memset(&req, 0, sizeof(req));
    req.type     = GUI_REQ_MOVE_WINDOW;
    req.window_id = win->id;
    req.x = x; req.y = y;
    gui_reply_t reply;
    int rc = submit(&req, &reply);
    if (rc == 0) { win->x = x; win->y = y; }
    return rc;
}

int gui_window_set_title(gui_window_t *win, const char *title) {
    if (!g_slot || !win) return -EINVAL;
    gui_request_t req;
    memset(&req, 0, sizeof(req));
    req.type     = GUI_REQ_SET_TITLE;
    req.window_id = win->id;
    strncpy(req.title, title ? title : "", GUI_TITLE_LEN - 1);
    gui_reply_t reply;
    int rc = submit(&req, &reply);
    if (rc == 0) {
        strncpy(win->title, title ? title : "", GUI_TITLE_LEN - 1);
        win->title[GUI_TITLE_LEN - 1] = 0;
    }
    return rc;
}

int gui_window_destroy(gui_window_t *win) {
    if (!g_slot || !win) return -EINVAL;
    gui_request_t req;
    memset(&req, 0, sizeof(req));
    req.type     = GUI_REQ_DESTROY_WINDOW;
    req.window_id = win->id;
    gui_reply_t reply;
    int rc = submit(&req, &reply);
    if (rc == 0) {
        for (int i = 0; i < g_owned_count; i++) {
            if (g_owned[i] != win) continue;
            for (int j = i; j + 1 < g_owned_count; j++) g_owned[j] = g_owned[j + 1];
            g_owned_count--;
            break;
        }
        free(win);
    }
    return rc;
}

int gui_poll_event(gui_event_t *out) {
    if (!g_slot) return 0;
    // Odd evt_seq means the compositor published an event. Reading it and
    // writing the next value back acknowledges it, which is what lets the
    // compositor send another one -- without the write-back its parity check
    // would stay odd and every later event would be dropped.
    uint32_t seq = g_slot->evt_seq;
    if ((seq & 1u) == 0) return 0;

    *out = g_slot->evt;
    g_slot->evt_seq = seq + 1;   // even: acknowledged
    return 1;
}

int gui_window_count(void) { return g_owned_count; }
