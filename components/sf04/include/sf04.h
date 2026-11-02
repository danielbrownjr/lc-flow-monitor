#ifndef LC_SF04_H
#define LC_SF04_H
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SF04_OK, SF04_NACK, SF04_TIMEOUT, SF04_BUS_ERROR,
    SF04_TRUNCATED, SF04_CRC_ERROR, SF04_INVALID
} sf04_status_t;
/* Transport must use address 0x40, bounded timeout, and report actual RX length.
 * A write here is only a read command/address selection, never EEPROM data. */
typedef struct {
    void *context;
    sf04_status_t (*write)(void *, const uint8_t *, size_t);
    sf04_status_t (*read)(void *, uint8_t *, size_t, size_t *);
} sf04_transport_t;
typedef struct {
    uint16_t user_register;
    uint8_t field;
    uint16_t scale;
    uint16_t unit;
} sf04_calibration_t;
uint8_t sf04_crc8(const uint8_t *data, size_t length);
sf04_status_t sf04_decode_words(const uint8_t *reply, size_t length,
                                uint16_t *words, size_t count);
int16_t sf04_signed(uint16_t word);
const char *sf04_status_name(sf04_status_t status);
const char *sf04_unit_name(uint16_t unit);
sf04_status_t sf04_validate_calibration(const sf04_calibration_t *cal);
sf04_status_t sf04_convert(uint16_t raw, const sf04_calibration_t *cal, double *flow);
sf04_status_t sf04_read_calibration(const sf04_transport_t *bus, sf04_calibration_t *cal);
sf04_status_t sf04_read_identity(const sf04_transport_t *bus, char name[21], uint32_t *serial);
#endif
