#include "sf04.h"
#include <string.h>

uint8_t sf04_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0; /* SF04, not SF06 (which starts at 0xff). */
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (uint8_t)((crc & 0x80U) ? ((unsigned)crc << 1) ^ 0x31U : (unsigned)crc << 1);
        }
    }
    return crc;
}

sf04_status_t sf04_decode_words(const uint8_t *reply, size_t length, uint16_t *words, size_t count)
{
    if (!reply || !words || count == 0 || count > SIZE_MAX / 3) return SF04_INVALID;
    if (length != count * 3) return SF04_TRUNCATED;
    /* Validate the whole reply before publishing any values. */
    for (size_t i = 0; i < count; ++i)
        if (sf04_crc8(reply + 3 * i, 2) != reply[3 * i + 2]) return SF04_CRC_ERROR;
    for (size_t i = 0; i < count; ++i)
        words[i] = (uint16_t)((uint16_t)reply[3 * i] << 8 | reply[3 * i + 1]);
    return SF04_OK;
}

int16_t sf04_signed(uint16_t word)
{
    int32_t value = word;
    if (word & 0x8000U) value -= 65536;
    return (int16_t)value;
}

const char *sf04_status_name(sf04_status_t status)
{
    switch (status) {
    case SF04_OK: return "OK";
    case SF04_NACK: return "NACK";
    case SF04_TIMEOUT: return "TIMEOUT";
    case SF04_BUS_ERROR: return "BUS_ERROR";
    case SF04_TRUNCATED: return "TRUNCATED";
    case SF04_CRC_ERROR: return "CRC_ERROR";
    case SF04_INVALID: return "INVALID";
    default: return "UNKNOWN_ERROR";
    }
}

const char *sf04_unit_name(uint16_t unit)
{
    switch (unit) {
    case 2115: return "nl/min";
    case 2116: return "ul/min";
    case 2117: return "ml/min";
    case 2100: return "ul/sec";
    case 2133: return "ml/h";
    default: return NULL;
    }
}

sf04_status_t sf04_validate_calibration(const sf04_calibration_t *cal)
{
    if (!cal || cal->field > 4 || cal->field != ((cal->user_register >> 4) & 7U) ||
        cal->scale == 0 || !sf04_unit_name(cal->unit))
        return SF04_INVALID;
    return SF04_OK;
}

sf04_status_t sf04_convert(uint16_t raw, const sf04_calibration_t *cal, double *flow)
{
    if (!flow || sf04_validate_calibration(cal) != SF04_OK) return SF04_INVALID;
    *flow = (double)sf04_signed(raw) / cal->scale;
    return SF04_OK;
}

static sf04_status_t exchange(const sf04_transport_t *bus, const uint8_t *command,
                             size_t command_length, uint16_t *words, size_t count)
{
    uint8_t reply[30];
    size_t actual = 0;
    if (!bus || !bus->write || !bus->read || count == 0 || count > sizeof(reply) / 3)
        return SF04_INVALID;
    sf04_status_t status = bus->write(bus->context, command, command_length);
    if (status != SF04_OK) return status;
    status = bus->read(bus->context, reply, count * 3, &actual);
    if (status != SF04_OK) return status;
    return sf04_decode_words(reply, actual, words, count);
}

static sf04_status_t eeprom_read(const sf04_transport_t *bus, uint16_t address,
                                 uint16_t *words, size_t count)
{
    /* Exactly command + left-aligned address; no EEPROM write data exists. */
    const uint8_t command[] = {0xfa, (uint8_t)(address >> 4), (uint8_t)(address << 4)};
    return exchange(bus, command, sizeof(command), words, count);
}

sf04_status_t sf04_read_calibration(const sf04_transport_t *bus, sf04_calibration_t *cal)
{
    if (!cal) return SF04_INVALID;
    memset(cal, 0, sizeof(*cal)); /* Failed reads never leave stale calibration. */
    sf04_calibration_t result = {0};
    const uint8_t command = 0xe3;
    sf04_status_t status = exchange(bus, &command, 1, &result.user_register, 1);
    if (status != SF04_OK) return status;
    result.field = (uint8_t)((result.user_register >> 4) & 7U);
    if (result.field > 4) return SF04_INVALID;
    const uint16_t address = (uint16_t)(0x2b6U + 0x300U * result.field);
    uint16_t words[2];
    status = eeprom_read(bus, address, words, 2);
    if (status != SF04_OK) return status;
    result.scale = words[0];
    result.unit = words[1];
    status = sf04_validate_calibration(&result);
    if (status == SF04_OK) *cal = result;
    return status;
}

sf04_status_t sf04_read_identity(const sf04_transport_t *bus, char name[21], uint32_t *serial)
{
    if (!name || !serial) return SF04_INVALID;
    name[0] = '\0';
    *serial = 0;
    uint16_t part[10], number[2];
    sf04_status_t status = eeprom_read(bus, 0x2e8, part, 10);
    if (status != SF04_OK) return status;
    status = eeprom_read(bus, 0x2f8, number, 2);
    if (status != SF04_OK) return status;
    for (size_t i = 0; i < 10; ++i) {
        name[2 * i] = (char)(part[i] >> 8);
        name[2 * i + 1] = (char)(part[i] & 0xffU);
    }
    name[20] = '\0';
    *serial = (uint32_t)number[0] << 16 | number[1];
    return SF04_OK;
}
