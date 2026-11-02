#include "sf04.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned writes, reads;
    sf04_status_t write_status, read_status;
    int truncated, corrupt, invalid;
    uint8_t field;
    uint8_t commands[4][3];
    size_t lengths[4];
} fake_t;
static sf04_status_t write_fake(void *context, const uint8_t *data, size_t length)
{
    fake_t *f = context;
    assert(f->writes < 4 && length <= 3);
    memcpy(f->commands[f->writes], data, length);
    f->lengths[f->writes++] = length;
    return f->write_status;
}
static sf04_status_t read_fake(void *context, uint8_t *data, size_t length, size_t *actual)
{
    fake_t *f = context;
    ++f->reads;
    if (f->read_status != SF04_OK) return f->read_status;
    memset(data, 0, length);
    for (size_t i = 0; i < length / 3; ++i) {
        uint16_t word = 0;
        if (f->commands[f->writes - 1][0] == 0xe3) word = (uint16_t)((unsigned)f->field << 4);
        else if (f->commands[f->writes - 1][1] == 0x2e) word = 0x4c47;
        else if (f->commands[f->writes - 1][1] == 0x2f) word = i == 0 ? 0x1234 : 0x5678;
        else word = i == 0 ? (f->invalid ? 0 : 5) : 2116;
        data[3 * i] = (uint8_t)(word >> 8);
        data[3 * i + 1] = (uint8_t)word;
        data[3 * i + 2] = sf04_crc8(data + 3 * i, 2);
    }
    if (f->corrupt) data[length - 1] ^= 1;
    *actual = f->truncated ? length - 1 : length;
    return SF04_OK;
}
static sf04_transport_t transport(fake_t *f)
{
    sf04_transport_t bus = {f, write_fake, read_fake};
    return bus;
}
int main(void)
{
    /* Independent published SF04 example: user register 0E 00 -> 6D. */
    const uint8_t vector[] = {0x0e, 0x00, 0x6d};
    const uint8_t zeros[] = {0, 0};
    assert(sf04_crc8(vector, 2) == 0x6d);
    assert(sf04_crc8(zeros, 2) == 0);
    uint16_t word = 99;
    assert(sf04_decode_words(vector, 3, &word, 1) == SF04_OK && word == 0x0e00);
    assert(sf04_decode_words(vector, 2, &word, 1) == SF04_TRUNCATED);
    const uint8_t bad[] = {0x0e, 0x00, 0x6c};
    word = 99;
    assert(sf04_decode_words(bad, 3, &word, 1) == SF04_CRC_ERROR && word == 99);
    const uint8_t bad_second[] = {0x0e, 0x00, 0x6d, 0x0e, 0x00, 0x6c};
    uint16_t words[] = {99, 99};
    assert(sf04_decode_words(bad_second, 6, words, 2) == SF04_CRC_ERROR && words[0] == 99);
    assert(sf04_signed(0) == 0 && sf04_signed(0x7fff) == 32767);
    assert(sf04_signed(0x8000) == -32768 && sf04_signed(0xffff) == -1);
    sf04_calibration_t cal = {0, 0, 5, 2116}; /* Datasheet CF0 fixture, never runtime fallback. */
    double flow = 42;
    assert(sf04_convert(5000, &cal, &flow) == SF04_OK && flow == 1000.0);
    assert(sf04_convert(60536, &cal, &flow) == SF04_OK && flow == -1000.0);
    cal.scale = 0;
    flow = 42;
    assert(sf04_convert(1, &cal, &flow) == SF04_INVALID && flow == 42);
    cal.scale = UINT16_MAX; /* Scale is documented as uint16_t, not a reserved value. */
    assert(sf04_validate_calibration(&cal) == SF04_OK);
    cal.scale = 5; cal.unit = 0;
    assert(sf04_validate_calibration(&cal) == SF04_INVALID);
    cal.unit = 2116; cal.field = 5;
    assert(sf04_validate_calibration(&cal) == SF04_INVALID);
    cal.field = 1;
    assert(sf04_validate_calibration(&cal) == SF04_INVALID);
    for (uint8_t field = 0; field < 8; ++field) {
        fake_t fake = {.field = field};
        sf04_transport_t bus = transport(&fake);
        sf04_status_t status = sf04_read_calibration(&bus, &cal);
        if (field > 4) {
            assert(status == SF04_INVALID && fake.writes == 1 && cal.scale == 0);
        } else {
            assert(status == SF04_OK && cal.field == field && cal.scale == 5 && cal.unit == 2116);
            const unsigned address = 0x2b6U + 0x300U * field;
            assert(fake.lengths[0] == 1 && fake.commands[0][0] == 0xe3);
            assert(fake.lengths[1] == 3 && fake.commands[1][0] == 0xfa);
            assert(fake.commands[1][1] == address >> 4 && fake.commands[1][2] == (uint8_t)(address << 4));
        }
    }
    const sf04_status_t failures[] = {SF04_NACK, SF04_TIMEOUT, SF04_BUS_ERROR};
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        fake_t fake = {.write_status = failures[i]};
        sf04_transport_t bus = transport(&fake);
        assert(sf04_read_calibration(&bus, &cal) == failures[i] && fake.reads == 0 && cal.scale == 0);
        fake = (fake_t){.read_status = failures[i]};
        assert(sf04_read_calibration(&bus, &cal) == failures[i] && fake.writes == 1 && cal.scale == 0);
    }
    for (unsigned i = 0; i < 3; ++i) {
        fake_t fake = {.truncated = i == 0, .corrupt = i == 1, .invalid = i == 2};
        sf04_transport_t bus = transport(&fake);
        const sf04_status_t expected[] = {SF04_TRUNCATED, SF04_CRC_ERROR, SF04_INVALID};
        assert(sf04_read_calibration(&bus, &cal) == expected[i] && cal.scale == 0);
    }
    fake_t fake = {0};
    sf04_transport_t bus = transport(&fake);
    char name[21]; uint32_t serial;
    assert(sf04_read_identity(&bus, name, &serial) == SF04_OK);
    assert(memcmp(name, "LGLG", 4) == 0 && name[20] == 0 && serial == 0x12345678);
    assert(fake.commands[0][1] == 0x2e && fake.commands[0][2] == 0x80);
    assert(fake.commands[1][1] == 0x2f && fake.commands[1][2] == 0x80);
    fake = (fake_t){.corrupt = 1};
    assert(sf04_read_identity(&bus, name, &serial) == SF04_CRC_ERROR && name[0] == 0 && serial == 0);
    assert(sf04_read_calibration(NULL, &cal) == SF04_INVALID);
    puts("PASS: CRC, signed limits, +/- standard conversion, calibration fields 0..7, units, metadata commands, truncation, NACK, timeout, bus error, CRC failure");
    return 0;
}
