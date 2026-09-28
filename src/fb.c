#include "fb.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <abi/syscalls.h>

typedef long scw;
extern long __do_syscall_ret(unsigned long);
extern scw __do_syscall3(long, scw, scw, scw);

#define FB_DEVICE "/dev/fb0"
#define FBIOCGETINFO 0x4600

// mlibc's eucalypt sysdep has no <sys/ioctl.h>; issue the syscall directly, the
// same way userland's evdev_test.c does.
static int fb_ioctl(int fd, unsigned long req, void *arg) {
    return (int)__do_syscall_ret(
        __do_syscall3(SYS_IOCTL, (scw)fd, (scw)req, (scw)arg));
}

static int g_fd = -1;
static struct fb_info_user g_info;
static uint32_t *g_shadow;
static size_t g_shadow_bytes;

int fb_init() {
    if (g_fd >= 0) return 0;

    g_fd = open(FB_DEVICE, O_RDWR);
    if (g_fd < 0) return -errno;

    if (fb_ioctl(g_fd, FBIOCGETINFO, &g_info) != 0) {
        int e = errno;
        close(g_fd);
        g_fd = -1;
        return -e;
    }

    if (g_info.bpp != 32) {
        close(g_fd);
        g_fd = -1;
        return -ENOTSUP;
    }

    g_shadow_bytes = (size_t)g_info.width * g_info.height * sizeof(uint32_t);
    g_shadow = malloc(g_shadow_bytes);
    if (!g_shadow) {
        close(g_fd);
        g_fd = -1;
        return -ENOMEM;
    }

    for (size_t i = 0; i < g_shadow_bytes / sizeof(uint32_t); i++)
        g_shadow[i] = 0;

    return 0;
}

void fb_release() {
    free(g_shadow);
    g_shadow = 0;
    g_shadow_bytes = 0;
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
}

int fb_width() { return (int)g_info.width; }
int fb_height() { return (int)g_info.height; }
int fb_bpp() { return (int)g_info.bpp; }

uint32_t *fb_buffer() { return g_shadow; }

void fb_present() {
    if (g_fd < 0 || !g_shadow) return;
    size_t done = 0;
    while (done < g_shadow_bytes) {
        ssize_t n = write(g_fd, (char *)g_shadow + done, g_shadow_bytes - done);
        if (n <= 0) return;
        done += (size_t)n;
    }
}

void fb_draw_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)g_info.width || y < 0 || y >= (int)g_info.height)
        return;
    g_shadow[y * g_info.width + x] = color;
}

void fb_clear(uint32_t color) {
    for (size_t i = 0; i < g_shadow_bytes / sizeof(uint32_t); i++)
        g_shadow[i] = color;
}
