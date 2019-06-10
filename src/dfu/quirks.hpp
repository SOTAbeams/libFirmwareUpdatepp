#ifndef DFU_QUIRKS_H
#define DFU_QUIRKS_H

#include <cstdint>

#define QUIRK_POLLTIMEOUT  (1<<0)
#define QUIRK_FORCE_DFU11  (1<<1)

/* Fallback value, works for OpenMoko */
#define DEFAULT_POLLTIMEOUT  5

uint16_t get_quirks(uint16_t vendor, uint16_t product, uint16_t bcdDevice);

#endif /* DFU_QUIRKS_H */
