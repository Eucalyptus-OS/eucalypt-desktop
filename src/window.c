// This is my first attempt at writing a window manager.

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

struct window {
    int x;
    int y;
    int width;
    int height;
    int z_index;
    bool focused;
    bool visible;
    struct window *next;
};

static struct window *windows = NULL;

// Initializing the window manager
int init_manager() {
    // Clear the screen
    printf("\033[2J\033[H");

    windows = malloc(sizeof(struct window));
    if (!windows) {
        fprintf(stderr, "Failed to allocate memory for windows\n");
        return -1;
    }
    windows->next = NULL;
    return 0;
}

struct window *create_window(int x, int y, int width, int height) {
    struct window *new_window = malloc(sizeof(struct window));
    if (!new_window) {
        fprintf(stderr, "Failed to allocate memory for new window\n");
        return NULL;
    }
    new_window->x = x;
    new_window->y = y;
    new_window->width = width;
    new_window->height = height;
    new_window->z_index = 0;
    new_window->focused = false;
    new_window->visible = true;
    new_window->next = windows;
    windows = new_window;
    return new_window;
}