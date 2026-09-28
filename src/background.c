// Honestly I think this is weird but I feel like I should have a function to set the background in here

#include "fb.h"

void set_background(uint32_t color) {
    fb_clear(color);
    fb_present();
}