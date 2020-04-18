/**
 * @file max7219.h
 * @brief max7219 device driver, working with the max7219 SPI driver.
 * @author Xiangyu Guo
 */
#ifndef __MAX7219_H__
#define __MAX7219_H__

/**
 * @brief config the max7219 device with given chip select pin.
 * @note we assume that every time a new chip select corresponding to a new
 * LED Matrix, so it needs to be initilized.
 * @param cs_pin, the chip select pin for latching the data.
 */
void max7219_device_config(int);

/**
 * @brief send the pattern message to the MAX7219 through SPI bus.
 * @param *msg, the message array.
 * @param len, the length of the array.
 * @param cs_pin, the chip select pin for latching the data.
 * @return 0 on success, otherwise failed.
 */
int max7219_send_msg(uint8_t*, int, int);

/**
 * @brief register the device when module is initiated
 * @return 0 on success, otherwise failed.
 */
int max7219_device_init(void);

/**
 * @brief remove the device when module is removed
 */
void max7219_device_exit(void);

#endif