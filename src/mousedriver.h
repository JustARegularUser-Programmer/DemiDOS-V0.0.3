#pragma once

typedef struct {
    int x;
    int y;
    unsigned int left_but;
    unsigned int righ_but;
} mouse_state_t;

void init_mouse(unsigned int screen_w, unsigned int screen_h);
unsigned char mouse_update(mouse_state_t* state);
