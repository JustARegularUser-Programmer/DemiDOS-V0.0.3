#include "pci.h"
#define PCI_CONFIG_ADDRESS 0x0CF8
#define PCI_CONFIG_DATA    0x0CFC

static inline uint8_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

RealNetCard detected_cards[8];
int total_cards = 0;

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void scan_real_pci() {
    total_cards = 0;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t reg0 = pci_read_config(bus, slot, 0, 0);
            uint16_t vendor = reg0 & 0xFFFF;

            if (vendor != 0xFFFF && vendor != 0x0000) {
                uint16_t device = (reg0 >> 16) & 0xFFFF;
                uint32_t reg2 = pci_read_config(bus, slot, 0, 0x08);
                uint8_t class_id = (reg2 >> 24) & 0xFF;
                if (class_id == 0x02) {
                    detected_cards[total_cards].vendor_id = vendor;
                    detected_cards[total_cards].device_id = device;
                    detected_cards[total_cards].bus = bus;
                    detected_cards[total_cards].slot = slot;

                    if (vendor == 0x10EC) detected_cards[total_cards].is_wifi = 0;
                    else detected_cards[total_cards].is_wifi = 1;

                    total_cards++;
                    if (total_cards >= 8) return;
                }
            }
        }
    }
}
