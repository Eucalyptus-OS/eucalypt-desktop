#include "input.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define KBD_DEVICE "/dev/event0"
#define MSE_DEVICE "/dev/event1"

#define MAX_EVENTS 64
#define MAX_KEYS   32

static int g_kbd = -1;
static int g_mse = -1;
static mouse_state_t g_mouse;
static key_event_t g_keys[MAX_KEYS];
static int g_key_count;

int input_init(void) {
    if (g_kbd >= 0 && g_mse >= 0) return 0;

    g_kbd = open(KBD_DEVICE, O_RDONLY | O_NONBLOCK);
    if (g_kbd < 0) return -errno;
    g_mse = open(MSE_DEVICE, O_RDONLY | O_NONBLOCK);
    if (g_mse < 0) {
        int e = errno;
        close(g_kbd);
        g_kbd = -1;
        return -e;
    }

    memset(&g_mouse, 0, sizeof(g_mouse));
    g_key_count = 0;
    return 0;
}

void input_release(void) {
    if (g_kbd >= 0) close(g_kbd);
    if (g_mse >= 0) close(g_mse);
    g_kbd = g_mse = -1;
}

static void apply_mouse(const struct input_event *e) {
    if (e->type == EV_REL) {
        if (e->code == REL_X) {
            g_mouse.dx += e->value;
            g_mouse.x += e->value;
        } else if (e->code == REL_Y) {
            g_mouse.dy += e->value;
            g_mouse.y += e->value;
        }
    } else if (e->type == EV_KEY) {
        if (e->value)
            g_mouse.buttons |= 1 << (e->code - BTN_LEFT);
        else
            g_mouse.buttons &= ~(1 << (e->code - BTN_LEFT));
    }
}

static int drain(int fd, int is_mouse) {
    int total = 0;
    for (;;) {
        struct input_event e;
        ssize_t n = read(fd, &e, sizeof(e));
        if (n == 0) break;
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) break;
            return -errno;
        }
        if (n < (ssize_t)sizeof(e)) break;

        if (e.type == EV_SYN) continue;

        if (is_mouse) {
            apply_mouse(&e);
        } else if (e.type == EV_KEY && g_key_count < MAX_KEYS) {
            g_keys[g_key_count].code = e.code;
            g_keys[g_key_count].pressed = e.value;
            g_key_count++;
        }
        total++;
    }
    return total;
}

int input_poll(void) {
    if (g_kbd < 0 || g_mse < 0) return -EBADF;

    g_key_count = 0;
    g_mouse.dx = 0;
    g_mouse.dy = 0;
    g_mouse.wheel = 0;

    int n = drain(g_kbd, 0);
    if (n < 0) return n;
    int total = n;
    n = drain(g_mse, 1);
    if (n < 0) return n;
    return total + n;
}

int input_keys(key_event_t *out, int max) {
    int n = g_key_count < max ? g_key_count : max;
    for (int i = 0; i < n; i++) out[i] = g_keys[i];
    return n;
}

const mouse_state_t *input_mouse(void) { return &g_mouse; }
