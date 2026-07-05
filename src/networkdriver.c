typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

#define RTL_TSADO 0x20
#define RTL_TSDO  0x10
#define RX_BUF_SIZE 8192
#define RTL_REG_RBSTART 0x30
#define RTL_REG_COMMAND 0x30
#define RTL_REG_IMR     0x37
#define RTL_REG_ISR     0x3C
#define RTL_REG_RCR     0x44
#define RTL_REG_CONFIG1 0x52

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

// Функция чтения байта из порта (Input Byte)
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t packet_buffer[1514];
uint8_t rx_buffer[RX_BUF_SIZE + 16];
int is_online = 0;

void rtl8139_send_packet(uint16_t io_base, void* packet_data, uint32_t size) {
    uint8_t* ptr = (uint8_t*)packet_data;
    for(uint32_t i = 0; i < size; i++) {
        packet_buffer[i] = ptr[i];
    }

    outl(io_base + RTL_TSADO, (uint32_t)&packet_buffer);
    outl(io_base + RTL_TSDO, size & 0xFFF);
}

void init_inet(uint16_t io_base) {
    outb(io_base + RTL_REG_CONFIG1, 0x00);

    outb(io_base + RTL_REG_COMMAND, 0x10);
    while ((inb(io_base + RTL_REG_COMMAND) & 0x10) != 0) {

    }

    outl(io_base + RTL_REG_RBSTART, (uint32_t)&rx_buffer);

    outw(io_base + RTL_REG_IMR, 0x0005);

    outl(io_base + RTL_REG_RCR, 0x0000008F);

    outb(io_base + RTL_REG_COMMAND, 0x0C);
    is_online = 1;
}
