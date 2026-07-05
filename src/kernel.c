typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef uint32_t uintptr_t;
#include "mousedriver.h"
#include "networkdriver.h"
#include "pci.h"
#define MAX_WINDS 3
#define MAX_BUTTONS 5
#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768
#define TASKBAR_HEIGHT 40  // Высота панели задач в пикселях
#define COLOR_BLACK 0x00000000

char cli_input_buffer[64]; // Сделаем размер массива 64 байта
int cli_buffer_len = 0;
const char* cli_prompt = "DemiDOS> ";

typedef struct {
    int x, y;          // Координаты кнопки на экране
    int w, h;          // Размеры кнопки
    const char* label; // Текст на кнопке
    int app_type;      // Какое приложение открывать при клике
    int is_pressed;    // Флаг: зажата ли кнопка в данный момент
} TaskbarButton;
static const char scancode_to_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' '
};
TaskbarButton taskbar_buttons[MAX_BUTTONS];
int total_buttons = 0;
typedef struct {
    int x;
    int y;
    int w;
    int h;
    int is_close;
    int is_drag;
    const char* title;
    int is_active;
    int is_minimize; // 1 = окно свернуто, 0 = открыто
    uint32_t color;
} Window;
uint32_t shadow_buffer[1024 * 768];
Window windows[MAX_WINDS];
int totalwindows = 0;

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;

    uint32_t syms[4];

    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;

    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;

    uint32_t framebuffer_addr_high;
    uint32_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
} __attribute__((packed)) multiboot_info_t;


static const uint8_t font_8x8[256][8] = {
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['!'] = {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},
    ['"'] = {0x6C, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['#'] = {0x6C, 0x6C, 0xFE, 0x6C, 0xFE, 0x6C, 0x6C, 0x00},
    ['$'] = {0x18, 0x7E, 0xC0, 0x7C, 0x06, 0xFC, 0x18, 0x00},
    ['%'] = {0x00, 0xC6, 0xCC, 0x18, 0x30, 0x66, 0xC3, 0x00},
    ['&'] = {0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00},
    ['\'']= {0x30, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['('] = {0x18, 0x30, 0x60, 0x60, 0x60, 0x30, 0x18, 0x00},
    [')'] = {0x30, 0x18, 0x06, 0x06, 0x06, 0x18, 0x30, 0x00},
    ['*'] = {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},
    ['+'] = {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00},
    [','] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30},
    ['-'] = {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
    ['.'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    ['/'] = {0x00, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x00},
    ['0'] = {0x3C, 0x66, 0x6E, 0x7E, 0x76, 0x66, 0x3C, 0x00},
    ['1'] = {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    ['2'] = {0x3C, 0x66, 0x06, 0x0C, 0x30, 0x60, 0x7E, 0x00},
    ['3'] = {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
    ['4'] = {0x1C, 0x3C, 0x6C, 0xCC, 0xFE, 0x0C, 0x1E, 0x00},
    ['5'] = {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
    ['6'] = {0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00},
    ['7'] = {0x7E, 0x66, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00},
    ['8'] = {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
    ['9'] = {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00},
    [':'] = {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00},
    [';'] = {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00},
    ['<'] = {0x00, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x00, 0x00},
    ['='] = {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00},
    ['>'] = {0x00, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x00, 0x00},
    ['?'] = {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00},
    ['@'] = {0x3C, 0x66, 0x6E, 0x6E, 0x60, 0x62, 0x3C, 0x00},
    ['A'] = {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00},
    ['B'] = {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00},
    ['C'] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
    ['D'] = {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00},
    ['E'] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00},
    ['F'] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00},
    ['G'] = {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3E, 0x00},
    ['H'] = {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    ['I'] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    ['J'] = {0x06, 0x06, 0x06, 0x06, 0x06, 0x66, 0x3C, 0x00},
    ['K'] = {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00},
    ['L'] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00},
    ['M'] = {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00},
    ['N'] = {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00},
    ['O'] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['P'] = {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00},
    ['Q'] = {0x3C, 0x66, 0x66, 0x66, 0x6E, 0x3C, 0x0E, 0x00},
    ['R'] = {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00},
    ['S'] = {0x3E, 0x60, 0x60, 0x3C, 0x06, 0x06, 0x7C, 0x00},
    ['T'] = {0x7E, 0x5A, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['U'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['V'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    ['W'] = {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
    ['X'] = {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
    ['Y'] = {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00},
    ['Z'] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
    ['['] = {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00},
    ['\\']= {0x00, 0xC0, 0x60, 0x18, 0x0C, 0x06, 0x03, 0x00},
    [']'] = {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00},
    ['^'] = {0x10, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00},
    ['_'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},
    ['`'] = {0x30, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['a'] = {0x00, 0x00, 0x38, 0x06, 0x3E, 0x66, 0x3B, 0x00},
    ['b'] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00},
    ['c'] = {0x00, 0x00, 0x3C, 0x60, 0x60, 0x66, 0x3C, 0x00},
    ['d'] = {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00},
    ['e'] = {0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00},
    ['f'] = {0x1C, 0x30, 0x78, 0x30, 0x30, 0x30, 0x30, 0x00},
    ['g'] = {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x7C},
    ['h'] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['i'] = {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['j'] = {0x06, 0x00, 0x0E, 0x06, 0x06, 0x06, 0x06, 0x3C},
    ['k'] = {0x60, 0x60, 0x6C, 0x78, 0x70, 0x6C, 0x66, 0x00},
    ['l'] = {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['m'] = {0x00, 0x00, 0x66, 0x7F, 0xDB, 0xDB, 0xDB, 0x00},
    ['n'] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['o'] = {0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['p'] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60},
    ['q'] = {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06},
    ['r'] = {0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00},
    ['s'] = {0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00},
    ['t'] = {0x30, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x1C, 0x00},
    ['u'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3B, 0x00},
    ['v'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    ['w'] = {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x3E, 0x36, 0x00},
    ['x'] = {0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00},
    ['y'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x7C},
    ['z'] = {0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00},
    ['{'] = {0x0C, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0C, 0x00},
    ['|'] = {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},
    ['}'] = {0x30, 0x18, 0x18, 0x0C, 0x18, 0x18, 0x30, 0x00},
    ['~'] = {0x3B, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

// Переменная для защиты от "залипания" клавиш
static uint8_t last_scancode = 0;

void draw_rect(uint32_t* fb, uint32_t pitch_in_pixels, int x, int y, int w, int h, uint32_t color) {
    for (int curr_y = y; curr_y < y + h; curr_y++) {
        for (int curr_x = x; curr_x < x + w; curr_x++) {
            if (curr_x >= 0 && curr_x < 1024 && curr_y >= 0 && curr_y < 768) {
                fb[curr_y * pitch_in_pixels + curr_x] = color;
            }
        }
    }
}
int is_iopen = 0;
int net_status = 0;

// Замени на свои системные функции чтения/записи в порты, если они другие
void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Функция чтения байта из порта (Input Byte)
uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
uint8_t get_rtc_register(int reg) {
    outb(0x70, reg);      // было outportb
    return inb(0x71);    // было inportb
}

// Переводим кривой формат BCD из памяти CMOS в обычные числа Си
int bcd_to_bin(uint8_t bcd) {
    return ((bcd / 16) * 10) + (bcd % 16);
}

char get_ascii_key(void) {
    uint8_t status = inb(0x64);
    if ((status & 1) == 1 && (status & 0x20) == 0) {
        uint8_t scancode = inb(0x60);

        if (scancode >= 0x80) {
            last_scancode = 0;
            return 0;
        }

        if (scancode == last_scancode) {
            return 0;
        }

        last_scancode = scancode;

        if (scancode < 128) {
            return scancode_to_ascii[scancode];
        }
    }
    return 0;
}

void draw_char(uint32_t* fb, uint32_t pitch, int x, int y, char c, uint32_t color) {
    // Перебираем 8 строк символа
    for (int row = 0; row < 8; row++) {
        uint8_t bits = font_8x8[(unsigned char)c][row];

        // Перебираем 8 пикселей в строке (слева направо)
        for (int col = 0; col < 8; col++) {
            // ИСПРАВЛЕНО: проверяем биты слева направо (начиная со старшего бита 0x80)
            if (bits & (0x80 >> col)) {
                int scr_x = x + col;
                int scr_y = y + row;

                // Простая проверка границ экрана 1024x768 (БЕЗ переворотов Y!)
                if (scr_x >= 0 && scr_x < 1024 && scr_y >= 0 && scr_y < 768) {
                    fb[scr_y * pitch + scr_x] = color;
                }
            }
        }
    }
}

void spawn_window(const char* title, uint32_t color) {
    int free = -1;
    for (int i = 0; i < MAX_WINDS; i++) {
        if (windows[i].is_close == 1) {
            free = i;
            break;
        }
    }

    if (free == -1) return;

    windows[free].x = 200 + (free * 25);
    windows[free].y = 150 + (free * 20);
    windows[free].w = 300; // Ширина окна
    windows[free].h = 200; // Высота окна
    windows[free].color = color;
    windows[free].title = title;
    windows[free].is_close = 0;   // Открываем!
    windows[free].is_active = 1;  // Делаем активным
    windows[free].is_drag = 0;

    // Деактивируем все остальные окна, так как новое окно сейчас на переднем плане
    for (int j = 0; j < MAX_WINDS; j++) {
        if (j != free) {
            windows[j].is_active = 0;
        }
    }
    return; // Окно создано, выходим из функции
}

void draw_string(uint32_t* fb, uint32_t pitch, int x, int y, const char* str, uint32_t color) {
    while (*str) {
        draw_char(fb, pitch, x, y, *str, color);
        x += 8; // Сдвигаемся на 8 пикселей вправо для следующего символа
        str++;
    }
}

void init_taskbar(uint32_t screen_width, uint32_t screen_height) {
    int taskbar_y = screen_height - TASKBAR_HEIGHT; // 728

    taskbar_buttons[2].x = 10;
    taskbar_buttons[2].y = taskbar_y + 8; // Слегка приподнимаем внутри панели
    taskbar_buttons[2].w = 56;
    taskbar_buttons[2].h = 24;
    taskbar_buttons[2].label = "MENU";
    taskbar_buttons[2].app_type = 3; // Код для CLI
    taskbar_buttons[2].is_pressed = 0;

    // Кнопка 1: "LITE CLI"
    taskbar_buttons[0].x = 80;
    taskbar_buttons[0].y = taskbar_y + 8; // Слегка приподнимаем внутри панели
    taskbar_buttons[0].w = 80;
    taskbar_buttons[0].h = 24;
    taskbar_buttons[0].label = "LITE CLI";
    taskbar_buttons[0].app_type = 1; // Код для CLI
    taskbar_buttons[0].is_pressed = 0;

    // Кнопка 2: "TEST WIN"
    taskbar_buttons[1].x = 170;
    taskbar_buttons[1].y = taskbar_y + 8;
    taskbar_buttons[1].w = 80;
    taskbar_buttons[1].h = 24;
    taskbar_buttons[1].label = "NEW WIN";
    taskbar_buttons[1].app_type = 2; // Код для нового окна
    taskbar_buttons[1].is_pressed = 0;

    taskbar_buttons[3].x = 260;
    taskbar_buttons[3].y = taskbar_y + 8;
    taskbar_buttons[3].w = 80;
    taskbar_buttons[3].h = 24;
    taskbar_buttons[3].label = "INET";
    taskbar_buttons[3].app_type = 4;
    taskbar_buttons[3].is_pressed = 0;
    total_buttons = 4;
}

void draw_taskbar(uint32_t* buffer, uint32_t pitch, uint32_t screen_width, uint32_t screen_height, int hours, int minutes, int seconds) {
    int taskbar_y = screen_height - 40;

    // 1. Рисуем серую панель задач во всю ширину экрана
    draw_rect(buffer, pitch, 0, taskbar_y, screen_width, 40, 0x00CCCCCC);
    draw_rect(buffer, pitch, 0, taskbar_y, screen_width, 1, 0x00FFFFFF); // Белая рамка сверху

    // 2. Сначала рисуем наши системные кнопки ("LITE CLI" и "NEW WIN")
    for (int i = 0; i < total_buttons; i++) {
        TaskbarButton btn = taskbar_buttons[i];
        uint32_t btn_color = (btn.is_pressed == 1) ? 0x00777777 : 0x00E0E0E0;
        draw_rect(buffer, pitch, btn.x, btn.y, btn.w, btn.h, btn_color);
        draw_rect(buffer, pitch, btn.x, btn.y + btn.h - 1, btn.w, 1, 0x00444444);
        draw_rect(buffer, pitch, btn.x + btn.w - 1, btn.y, 1, btn.h, 0x00444444);

        if (btn.label != 0) {
            draw_string(buffer, pitch, btn.x + 8, btn.y + 8, btn.label, 0x00000000);
        }
    }

    // 3. ДИНАМИЧЕСКИЕ КНОПКИ ДЛЯ ОТКРЫТЫХ ОКОН
    // Начнем размещать кнопки окон чуть правее системных (например, с координаты X = 200)
    int current_btn_x = 340;

    for (int i = 0; i < MAX_WINDS; i++) {
        // Если окно закрыто насовсем — кнопку для него не выводим
        if (windows[i].is_close == 1) continue;

        int btn_w = 110; // Ширина кнопки окна на таскбаре
        int btn_h = 24;

        // Если окно сейчас активно на экране — делаем кнопку темнее, если свернуто — обычной серой
        uint32_t win_btn_color = (windows[i].is_active == 1) ? 0x00999999 : 0x00E0E0E0;

        // Рисуем кнопку для окна `i`
        draw_rect(buffer, pitch, current_btn_x, taskbar_y + 8, btn_w, btn_h, win_btn_color);
        draw_rect(buffer, pitch, current_btn_x, taskbar_y + 8 + btn_h - 1, btn_w, 1, 0x00444444);
        draw_rect(buffer, pitch, current_btn_x + btn_w - 1, taskbar_y + 8, 1, btn_h, 0x00444444);

        // Пишем имя окна прямо внутри этой кнопки на панели задач
        if (windows[i].title != 0) {
            draw_string(buffer, pitch, current_btn_x + 6, taskbar_y + 14, windows[i].title, 0x00000000);
        }

        current_btn_x += 120; // Сдвигаемся вправо для кнопки следующего окна
    }

    // 4. Сборка и вывод текста времени "00:00:00"
    char time_str[9];
    time_str[0] = '0' + (hours / 10);
    time_str[1] = '0' + (hours % 10);
    time_str[2] = ':';
    time_str[3] = '0' + (minutes / 10);
    time_str[4] = '0' + (minutes % 10);
    time_str[5] = ':';
    time_str[6] = '0' + (seconds / 10);
    time_str[7] = '0' + (seconds % 10);
    time_str[8] = '\0';

    int clock_x = screen_width - 85;
    int clock_y = screen_height - 40 + 14;
    draw_string(buffer, pitch, clock_x, clock_y, time_str, 0x00000000);

    uint8_t day = bcd_to_bin(get_rtc_register(0x07));
    uint8_t mon = bcd_to_bin(get_rtc_register(0x08));
    uint8_t yer = bcd_to_bin(get_rtc_register(0x09));

    char date_str[12];
    date_str[0] = (day / 10) + '0';
    date_str[1] = (day % 10) + '0';
    date_str[2] = '.';
    date_str[3] = (mon / 10) + '0';
    date_str[4] = (mon % 10) + '0';
    date_str[5] = '.';
    date_str[6] = (yer / 10) + '0';
    date_str[7] = (yer % 10) + '0';
    date_str[8] = '\0';

    int date_x = screen_width - 180;
    int date_y = screen_height - 25;
    draw_string(buffer, pitch, date_x, date_y, date_str, 0x00000000);
}

void draw_window(uint32_t* fb, uint32_t pitch, Window win) {
    if (win.is_close == 1 || win.is_minimize == 1) {
        return;
    }

    // 1. Рисуем основное тело окна
    draw_rect(fb, pitch, win.x, win.y, win.w, win.h, win.color);

    // 2. Определяем цвет шапки (ИСПРАВЛЕНО: == вместо =)
    uint32_t title_color = (win.is_active == 1) ? 0x000000AA : 0x00555555;

    // Рисуем шапку окна (высота 16 пикселей для аккуратного размещения текста 8x8)
    draw_rect(fb, pitch, win.x, win.y, win.w, 16, title_color);

    // 3. Выводим заголовок окна белым цветом (с отступом в 4 пикселя сверху и слева)
    if (win.title != 0) {
        draw_string(fb, pitch, win.x + 6, win.y + 4, win.title, 0x00FFFFFF);
    }

    // Кнопка ЗАКРЫТЬ (Крестик) - смещена на 18 пикселей от правого края
    int close_x = win.x + win.w - 18;
    int close_y = win.y + 2;
    draw_rect(fb, pitch, close_x, close_y, 12, 12, 0x00FF0000); // Красный квадрат
    draw_char(fb, pitch, close_x + 2, close_y + 2, 'X', 0x00FFFFFF);

    // ДОБАВЛЯЕМ КНОПКУ СВЕРНУТЬ (Минус) - смещаем левее крестика (на 34 пикселя от края)
    int min_x = win.x + win.w - 34;
    int min_y = win.y + 2;
    draw_rect(fb, pitch, min_x, min_y, 12, 12, 0x00777777); // Серый квадрат
    draw_char(fb, pitch, min_x + 2, min_y + 2, '_', 0x00FFFFFF);
    if (win.title != 0 && win.title[0] == 'D' && win.color == 0x00000000) {

        // Рисуем зелёное приглашение "DemiDOS> "
        // win.x + 10 (отступ слева), win.y + 24 (отступ вниз под синюю шапку)
        draw_string(fb, pitch, win.x + 10, win.y + 24, cli_prompt, 0x0000FF00);

        // Рисуем то, что пользователь уже успел набрать на клавиатуре
        if (cli_buffer_len > 0) {
            cli_input_buffer[cli_buffer_len] = '\0'; // Закрываем строку
            // Сдвигаем текст вправо на 72 пикселя (9 символов "DemiDOS> " * 8 пикселей = 72)
            draw_string(fb, pitch, win.x + 10 + 72, win.y + 24, cli_input_buffer, 0x00FFFFFF);
        }

        // Рисуем текстовый курсор '_' после набранного текста
        int cursor_x = win.x + 10 + 72 + (cli_buffer_len * 8);
        draw_char(fb, pitch, cursor_x, win.y + 24, '_', 0x00FFFFFF);
    }
}

int strcmp(const char* s1, const char* s2, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        if (s1[i] != s2[i]) {
            return 0;
        }
    }
    return 1;
}

void start_demidos_pro(uint32_t magic, void* mbi_ptr) {
    multiboot_info_t* mbi = (multiboot_info_t*)mbi_ptr;

    uint32_t width = SCREEN_WIDTH;
    uint32_t height = SCREEN_HEIGHT;
    uint32_t ppixels = SCREEN_WIDTH;
    uint32_t* fb = 0;

    // Перехват флага фреймбуфера Мультибута
    if (mbi != 0) {
        if (mbi->flags & (1 << 12)) {
            // ИСПРАВЛЕНО: используем framebuffer_addr вместо framebuffer_addr_low
            fb = (uint32_t*)(unsigned long)mbi->framebuffer_addr;
            if (mbi->framebuffer_width > 0)  width = mbi->framebuffer_width;
            if (mbi->framebuffer_height > 0) height = mbi->framebuffer_height;
            if (mbi->framebuffer_pitch > 0)  ppixels = mbi->framebuffer_pitch / 4;
        }
    }

    // Твой спасительный хардкод VBE, если GRUB вернул ноль
    if (fb == 0) {
        fb = (uint32_t*)0xFD000000;
    }


    // Закрываем все окна на старте
    for (int i = 0; i < MAX_WINDS; i++) {
        windows[i].is_close = 1;
    }

    // Запуск твоего драйвера мыши и панели задач
    init_mouse(width, height);
    init_taskbar(width, height);
    scan_real_pci();
    init_inet(0xC000);

    mouse_state_t mouse;
    mouse.x = width / 2;
    mouse.y = height / 2;

    int last_mouse_x = width / 2;
    int last_mouse_y = height / 2;

    int is_safe =0;
    if (mbi != 0 && mbi->flags & (1 << 2) && mbi->cmdline != 0) {
        char* args = (char*)(unsigned long)mbi->cmdline;
        for (int i = 0; args[i] != '\0'; i++) {
            if (args[i] == '-' && args[i+1] == 's' && args[i+2] == 'a' && args[i+3] == 'f' && args[i+4] == 'e') {
                is_safe = 1;
                break;
            }
        }
    }
    uint32_t bg_color = 0x0000FF;

    if (is_safe == 1) {
        bg_color = 0x000000;
    } else {
        bg_color = 0x0000FF;
    }

    int clock_timer = 0;
    int is_moped = 0;
    int hours = 0, minutes = 0, seconds = 0;

    int old_mouse_x = -1, old_mouse_y = -1, old_mouse_click = -1, old_seconds = -1;

    // Первичная очистка реального экрана fb
    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t j = 0; j < width; j++) {
            fb[i * ppixels + j] = bg_color;
        }
    }

    while ((inb(0x64) & 1) == 1) { inb(0x60); }

    // Главный бесконечный цикл операционной системы
    while (1) {
        clock_timer++;
        if (clock_timer >= 30) {
            clock_timer = 0;
            hours   = bcd_to_bin(get_rtc_register(4));
            minutes = bcd_to_bin(get_rtc_register(2));
            seconds = bcd_to_bin(get_rtc_register(0));
        }

        // Перехват нажатий кнопок на клавиатуре
        char incoming_char = get_ascii_key();

        if (incoming_char != 0) {
            int is_cli_active = 0;
            for (int i = 0; i < MAX_WINDS; i++) {
                if (windows[i].is_close == 0 && windows[i].is_minimize == 0 &&
                    windows[i].is_active == 1 && windows[i].color == 0x00000000) {
                    is_cli_active = 1;
                break;
                    }
            }
            if (is_cli_active == 1) {
                if (incoming_char == '\n') {
                    // Очистка строки по нажатию на Enter
                    if (cli_buffer_len == 6 && strcmp(cli_input_buffer, "verity", 6)) {
                        draw_string(fb, ppixels, 210, 180, "Hello!! Im Verity, you personal helper friend!\n", 0x0000FF00);
                        draw_string(fb, ppixels, 210, 180, "Ask me anything, i know everything!", 0x0000FF00);
                    }
                    cli_buffer_len = 0;
                } else if (incoming_char == '\b') {
                    if (cli_buffer_len > 0) {
                        cli_buffer_len--;
                        cli_input_buffer[cli_buffer_len] = '\0';
                    }
                } else {
                    if (cli_buffer_len < 25) {
                        cli_input_buffer[cli_buffer_len++] = incoming_char;
                    }
                }
            }
        }

        // === ПРАВИЛЬНЫЙ И БЫСТРЫЙ ОПРОС МЫШИ ===
        if (mouse_update(&mouse)) {

            // ЗАЩИТА ОТ УЛЁТА В МИНУС (Для беззнаковых uint32_t / unsigned int)
            // Если координата стала огромной (например, 4294967295 из-за ухода вверх/влево)
            // или реально превысила границы экрана — принудительно возвращаем её на край!
            if (mouse.x > width)  mouse.x = 0;
            if (mouse.y > height) mouse.y = 0;

            // Защита от вылета за правый и нижний края экрана
            if (mouse.x >= width)  mouse.x = width - 1;
            if (mouse.y >= height) mouse.y = height - 1;

            // Считаем дельту движения окон относительно прошлого кадра
            int delta_x = mouse.x - last_mouse_x;
            int delta_y = mouse.y - last_mouse_y;
            last_mouse_x = mouse.x;
            last_mouse_y = mouse.y;

            int taskbar_y = height - TASKBAR_HEIGHT;


            if (mouse.left_but) {
                if (mouse.y >= taskbar_y) {
                    // Кнопки панели задач
                    for (int b = 0; b < total_buttons; b++) {
                        TaskbarButton* btn = &taskbar_buttons[b];
                        if (mouse.x >= btn->x && mouse.x <= (btn->x + btn->w) &&
                            mouse.y >= btn->y && mouse.y <= (btn->y + btn->h)) {
                            btn->is_pressed = 1;

                        if (btn->app_type == 2) {
                            spawn_window("DemiDOS Application", 0x007F7F7F);
                            btn->is_pressed = 0;
                        }
                        if (btn->app_type == 1) {
                            spawn_window("DemiDOS LITE CLI", 0x00000000);
                            btn->is_pressed = 0;
                        }
                        if (btn->app_type == 3) {
                            is_moped = !is_moped;
                            btn->is_pressed = 0;
                        }
                        if (btn->app_type == 4) {
                            is_iopen = !is_iopen;
                            btn->is_pressed = 0;
                        }
                            }
                    }

                    int current_btn_x = 200;
                    for (int i = 0; i < MAX_WINDS; i++) {
                        if (windows[i].is_close == 1) continue;
                        int btn_w = 110;
                        if (mouse.x >= current_btn_x && mouse.x <= (current_btn_x + btn_w) &&
                            mouse.y >= (taskbar_y + 8) && mouse.y <= (taskbar_y + 8 + 24)) {

                            if (windows[i].is_minimize == 1) {
                                windows[i].is_minimize = 0;
                                for (int j = 0; j < MAX_WINDS; j++) windows[j].is_active = 0;
                                windows[i].is_active = 1;
                            } else if (windows[i].is_active == 1) {
                                windows[i].is_minimize = 1;
                                windows[i].is_active = 0;
                            } else {
                                for (int j = 0; j < MAX_WINDS; j++) windows[j].is_active = 0;
                                windows[i].is_active = 1;
                            }
                            break;
                            }
                            current_btn_x += 120;
                    }
                }
                else {
                    // Клик по окнам на рабочем столе
                    int alr_drag = 0;
                    for (int i = 0; i < MAX_WINDS; i++) {
                        if (windows[i].is_drag == 1) {
                            alr_drag = 1;
                            windows[i].x += delta_x;
                            windows[i].y += delta_y;
                            break;
                        }
                    }

                    if (alr_drag == 0) {
                        for (int i = MAX_WINDS - 1; i >= 0; i--) {
                            if (windows[i].is_close == 1 || windows[i].is_minimize == 1) continue;

                            int close_x = windows[i].x + windows[i].w - 18;
                            int close_y = windows[i].y + 4;
                            int min_x = windows[i].x + windows[i].w - 34;
                            int min_y = windows[i].y + 4;

                            // Клик по Закрыть
                            if (mouse.x >= close_x && mouse.x <= (close_x + 14) &&
                                mouse.y >= close_y && mouse.y <= (close_y + 14)) {
                                windows[i].is_close = 1;
                            windows[i].is_active = 0;
                            break;
                                }

                                // Клик по Свернуть
                                if (mouse.x >= min_x && mouse.x <= (min_x + 14) &&
                                    mouse.y >= min_y && mouse.y <= (min_y + 14)) {
                                    windows[i].is_minimize = 1;
                                windows[i].is_active = 0;
                                break;
                                    }

                                    // Клик по шапке + Z-Order
                                    if (mouse.x >= windows[i].x && mouse.x <= (windows[i].x + windows[i].w) &&
                                        mouse.y >= windows[i].y && mouse.y <= (windows[i].y + 16)) {
                                        windows[i].is_drag = 1;
                                    for (int j = 0; j < MAX_WINDS; j++) windows[j].is_active = 0;
                                    windows[i].is_active = 1;

                                        if (i < MAX_WINDS - 1) {
                                            Window temp = windows[i];
                                            for (int k = i; k < MAX_WINDS - 1; k++) windows[k] = windows[k + 1];
                                            windows[MAX_WINDS - 1] = temp;
                                            windows[MAX_WINDS - 1].is_drag = 1;
                                        }
                                        break;
                                        }
                        }
                    }
                }
            } else {
                for (int j = 0; j < MAX_WINDS; j++) windows[j].is_drag = 0;
                for (int b = 0; b < total_buttons; b++) taskbar_buttons[b].is_pressed = 0;
            }

            // Отрисовка кадра только при реальных изменениях состояния
            if (mouse.x != old_mouse_x || mouse.y != old_mouse_y || mouse.left_but != old_mouse_click || seconds != old_seconds) {
                old_mouse_x = mouse.x; old_mouse_y = mouse.y; old_mouse_click = mouse.left_but; old_seconds = seconds;

                for (uint32_t i = 0; i < height; i++) {
                    for (uint32_t j = 0; j < width; j++) {
                        shadow_buffer[i * ppixels + j] = bg_color;
                    }
                }

                // 2. Рисуем ВСЕ окна по очереди в цикле for
                for (int i = 0; i < MAX_WINDS; i++) {
                    draw_window(shadow_buffer, ppixels, windows[i]);
                } // <-- О НЕТ, ТУТ САТАНА!

                // 3. Рисуем панель задач ПОВЕРХ окон (уже вне цикла)
                draw_taskbar(shadow_buffer, ppixels, width, height, hours, minutes, seconds);

                // 4. Рисуем ОДИН курсор мыши ПОВЕРХ ВСЕГО (тоже вне цикла)
                draw_rect(shadow_buffer, ppixels, mouse.x, mouse.y, 8, 8, 0x00FFFFFF);
                if (is_safe == 1) {
                    draw_string(fb, ppixels, 20, 20, "DemiDOS Safe mode", 0x00FF00);
                    draw_string(fb, ppixels, width -136 -20, 20, "DemiDOS Safe mode", 0x00FF00);
                    draw_string(fb, ppixels, 20, height - 40 - 20, "DemiDOS Safe mode", 0x00FF00);
                    draw_string(fb, ppixels, width - 136- 20, height - 40 -20, "DemiDOS Safe mode", 0x00FF00);
                }

                // 5. Копируем финальный готовый кадр на реальный экран fb
                for (uint32_t i = 0; i < height; i++) {
                    for (uint32_t j = 0; j < width; j++) {
                        fb[i * ppixels + j] = shadow_buffer[i * ppixels + j];
                    }
                }
                for (uint32_t i = 0; i < height; i++) {
                    for (uint32_t j = 0; j < width; j++) {
                        fb[i * ppixels + j] = shadow_buffer[i * ppixels + j];
                    }
                }
            } // Скобка закрывает блок проверки "if (mouse.x != old_mouse_x || ...)"
        } // Скобка закрывает "if (mouse_moved == 1)" ИЛИ проверку мыши
        if (is_safe == 1) {
            draw_string(fb, ppixels, 20, 20, "DemiDOS Safe mode", 0x00FF00);
            draw_string(fb, ppixels, width -136 -20, 20, "DemiDOS Safe mode", 0x00FF00);
            draw_string(fb, ppixels, 20, height - 40 - 20, "DemiDOS Safe mode", 0x00FF00);
            draw_string(fb, ppixels, width - 136- 20, height - 40 -20, "DemiDOS Safe mode", 0x00FF00);
            draw_string(fb, ppixels, width -136 -20, 20, "DemiDOS Safe mode", 0x00FF00);
            draw_string(fb, ppixels, 20, height - 40 - 20, "DemiDOS Safe mode", 0x00FF00);
            draw_string(fb, ppixels, width - 136- 20, height - 40 -20, "DemiDOS Safe mode", 0x00FF00);
        }
        if (is_moped == 1) {
            int start_y = (height - 40) - 120;

            draw_rect(shadow_buffer, ppixels, 10, start_y, 120, 110, 0x00CCCCCC);
            draw_string(shadow_buffer, ppixels, 20, start_y + 10, "1. Verity AI", 0x00FFFF00);

            for (uint32_t i = 0; i < height; i++) {
                for (uint32_t j = 0; j < width; j++) {
                    fb[i * ppixels + j] = shadow_buffer[i * ppixels + j];
                }
            }
            for (uint32_t i = 0; i < height; i++) {
                for (uint32_t j = 0; j < width; j++) {
                    fb[i * ppixels + j] = shadow_buffer[i * ppixels + j];
                }
            }
        }
        if (is_iopen == 1) {
            int menu_x = 260;
            int menu_y = 420;
            int menu_w = 280;
            int menu_h = 130;

            draw_rect(shadow_buffer, ppixels, menu_x, menu_y, menu_w, menu_h, 0x00D3D3D3D);

            draw_rect(shadow_buffer, ppixels, menu_x, menu_y, menu_w, 1, COLOR_BLACK);
            draw_rect(shadow_buffer, ppixels, menu_x, menu_y + menu_h - 1, menu_w, 1, COLOR_BLACK);
            draw_rect(shadow_buffer, ppixels, menu_x, menu_y, 1, menu_h, COLOR_BLACK);
            draw_rect(shadow_buffer, ppixels, menu_x + menu_w - 1, menu_y, 1, menu_h, COLOR_BLACK);

            draw_string(shadow_buffer, ppixels, menu_x + 10, menu_y + 10, "PCI HARDWARE INTERNETS:", COLOR_BLACK);

            draw_rect(shadow_buffer, ppixels, menu_x + 10, menu_y + 28, menu_w - 20, 1, 0x00808080);
            for (uint32_t i = 0; i < height; i++) {
                for (uint32_t j = 0; j < width; j++) {
                    fb[i * ppixels + j] = shadow_buffer[i * ppixels + j];
                }
            }

        }
    }
}
