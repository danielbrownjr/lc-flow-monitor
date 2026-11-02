#include <inttypes.h>
#include <stdio.h>
/* IDF 5.5.1 riscv/csr.h has an inline unsigned-long -> int conversion.
 * Keep our strict conversion diagnostics; scope the vendor exception to headers. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include "driver/i2c.h"
#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#pragma GCC diagnostic pop
#include "board.h"
#include "sf04.h"

static sf04_status_t map_error(esp_err_t error)
{
    if (error == ESP_OK) return SF04_OK;
    if (error == ESP_ERR_NOT_FOUND) return SF04_NACK;
    if (error == ESP_ERR_TIMEOUT) return SF04_TIMEOUT;
    return SF04_BUS_ERROR;
}

static sf04_status_t write_command(void *context, const uint8_t *data, size_t length)
{
    (void)context;
    esp_err_t error = i2c_master_write_to_device(I2C_NUM_0, LC_SENSOR_ADDRESS, data, length, pdMS_TO_TICKS(LC_TIMEOUT_MS));
    sf04_status_t status = error == ESP_FAIL ? SF04_NACK : map_error(error);
    printf("t_ms=%" PRId64 " op=command cmd=0x%02x bytes=%u status=%s idf=%s\n",
           esp_timer_get_time() / 1000, (unsigned)data[0], (unsigned)length,
           sf04_status_name(status), esp_err_to_name(error));
    return status;
}

static sf04_status_t read_reply(void *context, uint8_t *data, size_t length, size_t *actual)
{
    *actual = 0;
    (void)context;
    esp_err_t error = i2c_master_read_from_device(I2C_NUM_0, LC_SENSOR_ADDRESS, data, length, pdMS_TO_TICKS(LC_TIMEOUT_MS));
    sf04_status_t status = error == ESP_FAIL ? SF04_NACK : map_error(error);
    /* IDF returns transaction status, not partial length. Publish bytes only on success. */
    if (error == ESP_OK) *actual = length;
    printf("t_ms=%" PRId64 " op=read bytes=%u status=%s idf=%s\n",
           esp_timer_get_time() / 1000, (unsigned)*actual,
           sf04_status_name(status), esp_err_to_name(error));
    return status;
}

void app_main(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    printf("t_ms=%" PRId64 " firmware=lc_flow_monitor version=%s idf=%s target=esp32c5 sha256=",
           esp_timer_get_time() / 1000, app->version, app->idf_ver);
    for (size_t i = 0; i < sizeof(app->app_elf_sha256); ++i) printf("%02x", (unsigned)app->app_elf_sha256[i]);
    printf("\n");
    if (!lc_board_ready()) {
        printf("t_ms=%" PRId64 " status=BLOCKED reason=wiring_or_power_unverified sda=%d scl=%d\n",
               esp_timer_get_time() / 1000, LC_SDA_GPIO, LC_SCL_GPIO);
        return;
    }
    i2c_config_t config = {
        .mode = I2C_MODE_MASTER, .sda_io_num = LC_SDA_GPIO, .scl_io_num = LC_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_DISABLE, .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = LC_I2C_HZ,
    };
    esp_err_t error = i2c_param_config(I2C_NUM_0, &config);
    if (error == ESP_OK) error = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    printf("t_ms=%" PRId64 " op=i2c_init status=%s idf=%s sda=%d scl=%d hz=%u\n",
           esp_timer_get_time() / 1000, sf04_status_name(map_error(error)),
           esp_err_to_name(error), LC_SDA_GPIO, LC_SCL_GPIO, LC_I2C_HZ);
    if (error != ESP_OK) return;
    /* Datasheet power-up time 120 ms. No reset or register writes. */
    vTaskDelay(pdMS_TO_TICKS(200));
    i2c_cmd_handle_t probe = i2c_cmd_link_create();
    if (!probe) {
        printf("t_ms=%" PRId64 " op=probe status=BUS_ERROR reason=no_memory\n", esp_timer_get_time() / 1000);
        ESP_ERROR_CHECK(i2c_driver_delete(I2C_NUM_0));
        return;
    }
    error = i2c_master_start(probe);
    if (error == ESP_OK) error = i2c_master_write_byte(probe, (uint8_t)(LC_SENSOR_ADDRESS << 1), true);
    if (error == ESP_OK) error = i2c_master_stop(probe);
    sf04_status_t probe_status = SF04_BUS_ERROR;
    if (error == ESP_OK) {
        error = i2c_master_cmd_begin(I2C_NUM_0, probe, pdMS_TO_TICKS(LC_TIMEOUT_MS));
        probe_status = error == ESP_FAIL ? SF04_NACK : map_error(error);
    }
    i2c_cmd_link_delete(probe);
    printf("t_ms=%" PRId64 " op=probe address=0x40 status=%s idf=%s\n",
           esp_timer_get_time() / 1000, error == ESP_OK ? "ACK" : sf04_status_name(probe_status),
           esp_err_to_name(error));
    if (error == ESP_OK) {
        sf04_transport_t transport = {.context = NULL, .write = write_command, .read = read_reply};
        char name[21];
        uint32_t serial;
        sf04_status_t status = sf04_read_identity(&transport, name, &serial);
        printf("t_ms=%" PRId64 " op=identity status=%s\n", esp_timer_get_time() / 1000,
               sf04_status_name(status));
        if (status == SF04_OK) {
            /* Print raw name bytes so sensor data cannot inject log records. */
            printf("t_ms=%" PRId64 " part_hex=", esp_timer_get_time() / 1000);
            for (size_t i = 0; i < 20; ++i) printf("%02x", (unsigned)(uint8_t)name[i]);
            printf(" serial_product=%" PRIu32 " crc=OK\n", serial);
        }
        /* Stop after transport failure; CRC/invalid data allows the independent
         * calibration check, but never a retry or guessed command. */
        if (status == SF04_OK || status == SF04_CRC_ERROR || status == SF04_INVALID) {
            sf04_calibration_t cal;
            status = sf04_read_calibration(&transport, &cal);
            printf("t_ms=%" PRId64 " op=calibration status=%s\n", esp_timer_get_time() / 1000,
                   sf04_status_name(status));
            if (status == SF04_OK)
                printf("t_ms=%" PRId64 " user_register=0x%04x field=%u scale=%u unit=%s unit_code=%u crc=OK\n",
                       esp_timer_get_time() / 1000, (unsigned)cal.user_register, (unsigned)cal.field,
                       (unsigned)cal.scale, sf04_unit_name(cal.unit), (unsigned)cal.unit);
        }
    }
    ESP_ERROR_CHECK(i2c_driver_delete(I2C_NUM_0));
    for (;;) {
        printf("t_ms=%" PRId64 " status=DIAGNOSTIC_COMPLETE acquisition=disabled\n", esp_timer_get_time() / 1000);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
