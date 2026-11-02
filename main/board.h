#ifndef LC_BOARD_H
#define LC_BOARD_H
#include "sdkconfig.h"
/* LilyGO connector exposes IO11/TX and IO12/RX, 3V3, GND.
 * SDA/SCL depend on the actual splice and must not be inferred from its shape. */
#define LC_SDA_GPIO CONFIG_LC_SDA_GPIO
#define LC_SCL_GPIO CONFIG_LC_SCL_GPIO
#define LC_I2C_HZ 100000U
#define LC_SENSOR_ADDRESS 0x40U
#define LC_TIMEOUT_MS 100
static inline int lc_board_ready(void)
{
#ifdef CONFIG_LC_HARDWARE_VERIFIED
    return ((LC_SDA_GPIO == 11 && LC_SCL_GPIO == 12) ||
            (LC_SDA_GPIO == 12 && LC_SCL_GPIO == 11));
#else
    return 0;
#endif
}
#endif
