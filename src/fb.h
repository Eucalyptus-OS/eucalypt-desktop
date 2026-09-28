#pragma once

#include <stdint.h>

// Matches the kernel's fb0 ioctl payload (kernel/src/fs/devfs.c).
struct fb_info_user {
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint32_t bpp;
    uint32_t pad;
};

// Opens /dev/fb0 and queries its geometry. Allocates a full-screen shadow
// buffer, because the driver's read/write ignore the file offset: every write
// blits from the start of the framebuffer, so partial updates are not possible.
int fb_init(void);
void fb_release(void);

int fb_width(void);
int fb_height(void);
int fb_bpp(void);

// Writable shadow buffer, one uint32_t per pixel, row-major and unpadded.
uint32_t *fb_buffer(void);

// Blits the whole shadow buffer to the framebuffer.
void fb_present(void);
