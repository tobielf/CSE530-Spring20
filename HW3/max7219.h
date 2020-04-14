/**
 * @file max7219.h
 * @brief max7219 device driver, working with the max7219 SPI driver.
 * @author Xiangyu Guo
 */
#ifndef __MAX7219_H__
#define __MAX7219_H__

/**
 * @brief register the device when module is initiated
 * @param *msg, the message array.
 * @param len, the length of the array.
 * @return 0 on success, otherwise failed.
 */
int max7219_send_msg(uint8_t*, int);

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