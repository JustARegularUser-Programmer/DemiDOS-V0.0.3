#pragma once

typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

extern int is_online;

void init_inet(uint16_t io_base);
void rtl8139_send_packet(uint16_t io_base, void* packet_data, uint32_t size);
