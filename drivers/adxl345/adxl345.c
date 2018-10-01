/*
 * Copyright (C) 2017 Mesotic SAS
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_adxl345
 * @{
 *
 * @file
 * @brief       Device driver implementation for the ADXL345 accelerometer (i2c only)
 *
 * @author      Dylan Laduranty <dylan.laduranty@mesotic.com>
 *
 * @}
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "assert.h"
#include "periph/i2c.h"
#include "adxl345.h"
#include "adxl345_regs.h"

#define ENABLE_DEBUG        (0)
#include "debug.h"

#define I2C_SPEED           (I2C_SPEED_NORMAL)
#define BUS                 (dev->i2c)
#define ADDR                (dev->addr)

int adxl345_init(adxl345_t *dev, const adxl345_params_t* params)
{
    uint8_t reg;

    assert(dev && params);

    /* get device descriptor */
    dev->params = (adxl345_params_t*)params;

    /* get scale_factor from full_res and range parameters */
    dev->scale_factor = (ADXL345_PARAM_FULL_RES ? ADXL345_PARAM_SCALE_FACTOR : (4 << dev->params->range));

    /* Acquire exclusive access */
    i2c_acquire(BUS);

    /* Initialize I2C interface */
    // if (i2c_init_master(BUS, I2C_SPEED) < 0) {
    //     i2c_release(BUS);
    //     DEBUG("[adxl345] init - error: unable to initialize I2C bus\n");
    //     return ADXL345_NOI2C;
    // }
    i2c_init(BUS);
    
    DEBUG("[adxl345] I2C bus %d initialized\n", BUS);

    /* test if the target device responds */
    i2c_read_reg(BUS, ADDR, ACCEL_ADXL345_CHIP_ID_REG, &reg, 0);
    if (reg != ACCEL_ADXL345_CHIP_ID) {
        i2c_release(BUS);
        DEBUG("[adxl345] init - error: invalid id value [0x%02x]\n", (int)reg);
        return ADXL345_NODEV;
    }
    /* configure the user offset */
    i2c_write_regs(BUS, ADDR, ACCEL_ADXL345_OFFSET_X, dev->params->offset, 3, 0);
    /* Basic device setup */
    reg = (ADXL345_PARAM_FULL_RES | dev->params->range);
    
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_DATA_FORMAT, reg, 0);
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_BW_RATE, dev->params->rate, 0);
    /* Put device in measure mode */
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_POWER_CTL, MEASURE_BIT, 0);

    /* Release the bus */
    i2c_release(BUS);

    DEBUG("[adxl345] init: successful\n");

    return ADXL345_OK;
}

void adxl345_read(const adxl345_t *dev, adxl345_data_t *data)
{
    int16_t result[3] = {};

    assert(dev && data);

    i2c_acquire(BUS);
    i2c_read_regs(BUS, ADDR, ACCEL_ADXL345_DATA_X0, (void *)result, 6, 0);
    i2c_release(BUS);
    
    if (byteorder_is_little_endian()) {
        for (int i = 0; i < 3; i++) {
            byteorder_swap((void *)&result[i], sizeof(result[i]));
        }
    }

    data->x = result[0] * ADXL345_PARAM_SCALE_FACTOR;
    data->y = result[1] * ADXL345_PARAM_SCALE_FACTOR;
    data->z = result[3] * ADXL345_PARAM_SCALE_FACTOR;
}

void adxl345_set_interrupt(const adxl345_t *dev)
{
    assert(dev);

    DEBUG("[adxl345] Update interruptions configuration\n");

    i2c_acquire(BUS);
    /* Set threshold */
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_THRESH_TAP, dev->interrupt.thres_tap, 0);
    /* Set Map */
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_INT_MAP, dev->interrupt.map, 0);
    /* Set Duration */
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_TAP_DUR, dev->interrupt.thres_dur, 0);
    /* Enable axes */
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_TAP_AXES, dev->interrupt.tap_axes, 0);
    /* Set source */
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_INT_SOURCE, dev->interrupt.source, 0);
    /* Set latent threshold */
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_TAP_LAT, dev->interrupt.thres_latent, 0);
    /* Set window threshold */
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_TAP_WIN, dev->interrupt.thres_window, 0);
    /* Set activity threshold */
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_THRESH_ACT, dev->interrupt.thres_act, 0);
    /* Set inactivity threshold */
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_THRESH_INACT, dev->interrupt.thres_inact, 0);
    /* Set inactivity time */
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_TIME_INACT, dev->interrupt.time_inact, 0);
    /* Set free-fall threshold */
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_THRESH_FF, dev->interrupt.thres_ff, 0);
    /* Set free-fall time */
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_TIME_FF, dev->interrupt.time_ff, 0);
    /* Set axis control */
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_ACT_INACT_CTL, dev->interrupt.act_inact, 0);
    /* Enable interrupt */
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_INT_ENABLE, dev->interrupt.enable, 0);

    /* Release the bus */
    i2c_release(BUS);
}

void adxl345_set_measure(const adxl345_t *dev)
{
    uint8_t reg;

    assert(dev);

    DEBUG("[adxl345] set device to measure mode\n");

    i2c_acquire(BUS);
    i2c_read_reg(BUS, ADDR, ACCEL_ADXL345_POWER_CTL, &reg, 0);
    reg |= MEASURE_BIT;
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_POWER_CTL, reg, 0);
    i2c_release(BUS);
}

void adxl345_set_standby(const adxl345_t *dev)
{
    uint8_t reg;

    assert(dev);

    DEBUG("[adxl345] set device to standby mode\n");

    i2c_acquire(BUS);
    i2c_read_reg(BUS, ADDR, ACCEL_ADXL345_POWER_CTL, &reg, 0);
    reg &= ~MEASURE_BIT;
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_POWER_CTL, reg, 0);
    i2c_release(BUS);
}

void adxl345_set_sleep(const adxl345_t *dev)
{
    uint8_t reg;

    assert(dev);

    DEBUG("[adxl345] set device to sleep mode\n");

    i2c_acquire(BUS);
    i2c_read_reg(BUS, ADDR, ACCEL_ADXL345_POWER_CTL, &reg, 0);
    reg |= SLEEP_BIT;
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_POWER_CTL, reg, 0);
    i2c_release(BUS);
}

void adxl345_set_autosleep(const adxl345_t *dev)
{
    uint8_t reg;

    assert(dev);

    DEBUG("[adxl345] set device to autosleep mode\n");

    i2c_acquire(BUS);
    i2c_read_reg(BUS, ADDR, ACCEL_ADXL345_POWER_CTL, &reg, 0);
    reg |= AUTOSLEEP_BIT;
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_POWER_CTL, reg, 0);
    i2c_release(BUS);
}

void adxl345_set_bandwidth_rate(const adxl345_t *dev, uint8_t bw_rate)
{
    uint8_t reg;

    assert(dev);

    DEBUG("[adxl345] set device rate to %d Hz\n", (int)bw_rate);

    i2c_acquire(BUS);
    i2c_read_reg(BUS, ADDR, ACCEL_ADXL345_BW_RATE, &reg, 0);
    reg |= bw_rate;
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_BW_RATE, reg, 0);
    i2c_release(BUS);
}

void adxl345_set_fifo_mode(const adxl345_t *dev, uint8_t mode,
                           uint8_t output, uint8_t value)
{
    uint8_t reg;

    assert(dev);

    DEBUG("[adxl345] set fifo mode to %d, output trigger to %d and trigger "
          "value to :%d\n", (int)mode, (int)output, (int)value);

    i2c_acquire(BUS);
    reg = ((mode << FIFO_MODE_POS) | (output << FIFO_TRIGGER_POS) | value);
    i2c_write_reg(BUS, ADDR, ACCEL_ADXL345_FIFO_CTL, reg, 0);
    i2c_release(BUS);
}
