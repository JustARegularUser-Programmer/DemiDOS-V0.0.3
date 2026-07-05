 /* =================================================
  * Finally...
  * After a day...
  * Internet...
  * that means...
  * DEMIDOS V0.0.3!!!
  * I ADDED INTERNET, GRAPHICS (MORE), BROWSER (W.I.P)
  * =================================================
 */

#pragma once

typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus;
    uint8_t slot;
    int is_wifi;
} RealNetCard;

extern RealNetCard detected_cards[8];
extern int total_cards;

void scan_real_pci(void);
