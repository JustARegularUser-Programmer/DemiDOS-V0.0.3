#include "mousedriver.h"

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void p2_wait(unsigned int type) {
    unsigned int timeout = 100000;
    if (type == 0) {
        while (timeout-- && (inb(0x64) & 1) == 0);
    } else {
        while (timeout-- && (inb(0x64) & 2));
    }
}

static void mouse_write(unsigned char write) {
    p2_wait(1);
    outb(0x64, 0xD4);
    p2_wait(1);
    outb(0x60, write);
}

static unsigned char mouse_read() {
    p2_wait(0);
    return inb(0x60);
}

static int curr_x = 0;
static int curr_y = 0;
static unsigned int max_w = 1024;
static unsigned int max_h = 768;
static unsigned char mouse_cycle = 0;
static unsigned char mouse_packet[3];

void init_mouse(unsigned int screen_w, unsigned int screen_h) {
    max_w = screen_w;
    max_h = screen_h;
    curr_x = screen_w / 2;
    curr_y = screen_h / 2;

    unsigned char status;

    p2_wait(1);
    outb(0x64, 0x20);
    p2_wait(0);
    status = inb(0x60) | 2;

    p2_wait(1);
    outb(0x64, 0x60);
    p2_wait(1);
    outb(0x60, status);

    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF4);
    mouse_read();
}

unsigned char mouse_update(mouse_state_t* state) {
    if ((inb(0x64) & 1) && (inb(0x64) & 0x20)) {
        mouse_packet[mouse_cycle] = inb(0x60);
        mouse_cycle++;
        if (mouse_cycle == 3) {
            mouse_cycle = 0;

            state->left_but = mouse_packet[0] & 1;
            state->righ_but = (mouse_packet[0] & 2) >> 1;

            // ИСПРАВЛЕНО: Меняем signed char на полноценный int!
            int rel_x = (signed char)mouse_packet[1];
            int rel_y = (signed char)mouse_packet[2];

            if (mouse_packet[0] & 0x10) {
                rel_x |= 0xFFFFFF00; // Теперь это сработает идеально!
            }
            if (mouse_packet[0] & 0x20) {
                rel_y |= 0xFFFFFF00; // Теперь это сработает идеально!
            }

            curr_x += rel_x;
            curr_y -= rel_y;

            if (curr_x < 0) curr_x = 0;
            if (curr_y < 0) curr_y = 0;
            if (curr_x >= (int)max_w - 8) curr_x = max_w - 8;
            if (curr_y >= (int)max_h - 8) curr_y = max_h - 8;

            state->x = curr_x;
            state->y = curr_y;

            return 1;
        }
    }
    return 0;
}
