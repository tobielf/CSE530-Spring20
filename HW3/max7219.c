/**
 * @file max7219.c
 * @brief max7219 device driver, working with the max7219 SPI driver.
 * @author Xiangyu Guo
 */
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>

#include <linux/delay.h>

#include <linux/workqueue.h>

#include "gpio_config.h"

#define SPI_DIR         (24)                    /**< SPI Direction GPIO pin */
#define SPI_MUX1        (44)                    /**< SPI MUX1 GPIO pin */
#define SPI_MUX2        (72)                    /**< SPI MUX2 GPIO pin */

#define SCK_DIR         (30)                    /**< SCK Direction GPIO pin */
#define SCK_MUX1        (46)                    /**< SCK MUX1 GPIO pin */

#define NUM_OF_COLUMNS  (8)                     /**< Number of the columns in LED Matrix */

/** @reference https://datasheets.maximintegrated.com/en/ds/MAX7219-MAX7221.pdf 
 *  This document explained the details about how MAX7219 register works.
 */
#define DECODE_MODE     (0x09)                  /**< Decode mode register address */
#define SCAN_LIMIT      (0x0b)                  /**< Scan limit register address */
#define SHUTDOWN_MODE   (0x0c)                  /**< Shutdown mode register address */
#define TEST_DISPLAY    (0x0f)                  /**< Test display register address */

static uint8_t s_init_sequence[][2] = {
        {SHUTDOWN_MODE, 0x01},
        {SCAN_LIMIT, 0x07},
        {TEST_DISPLAY, 0x00},
        {DECODE_MODE, 0x00},
        {0xff, 0xff},
};

// [ToDo] for more than one device.
static struct spi_device *max7219_device = NULL;        /**< SPI device structure */
static struct workqueue_struct *max7219_wq = NULL;      /**< Work queue for one device */

typedef struct {
        struct work_struct display_work;
        int cs_pin;
        uint8_t led[NUM_OF_COLUMNS];
} display_work_t;

static struct spi_board_info max7219_info = {
        .modalias = "max7219-led",
        .chip_select = 1,
        .controller_data = NULL,
        .max_speed_hz = 1000000,
        .bus_num = 1,
        .mode = SPI_MODE_0,
};

#define BUFF_SIZE       (4096)                  /**< Buffer size */

/** per device structure */
struct max7219_dev {
        spinlock_t              spi_lock;
        struct spi_device       *spi;
        u8                      cs;

        /* TX/RX buffers are NULL unless this device is open (users > 0) */
        u8                      *tx_buffer;
        u32                     speed_hz;
};

struct spi_ioc_transfer {
        __u32           tx_buf;

        __u32           len;
        __u32           speed_hz;

        __u16           delay_usecs;
        __u8            bits_per_word;
        __u8            cs_change;
        __u8            tx_nbits;
};

static atomic_t available = ATOMIC_INIT(1);     /**< Singleton lock */
static struct max7219_dev      spidev;          /**< Per device objects */

/* ==================== Took from spidev.c ====================================*/

/*
 * We can't use the standard synchronous wrappers for file I/O; we
 * need to protect against async removal of the underlying spi_device.
 */
static void spidev_complete(void *arg)
{
        complete(arg);
}

static ssize_t
spidev_sync(struct max7219_dev *spidev, struct spi_message *message)
{
        DECLARE_COMPLETION_ONSTACK(done);
        int status;

        message->complete = spidev_complete;
        message->context = &done;

        spin_lock_irq(&spidev->spi_lock);
        if (spidev->spi == NULL)
                status = -ESHUTDOWN;
        else
                status = spi_async(spidev->spi, message);
        spin_unlock_irq(&spidev->spi_lock);

        if (status == 0) {
                wait_for_completion(&done);
                status = message->status;
                if (status == 0)
                        status = message->actual_length;
        }
        return status;
}

static int spidev_message(struct max7219_dev *spidev,
                struct spi_ioc_transfer *u_xfers, unsigned n_xfers)
{
        struct spi_message      msg;
        struct spi_transfer     *k_xfers;
        struct spi_transfer     *k_tmp;
        struct spi_ioc_transfer *u_tmp;
        unsigned                n, total;
        u8                      *tx_buf;
        int                     status = -EFAULT;

        spi_message_init(&msg);
        k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
        if (k_xfers == NULL)
                return -ENOMEM;

        /* Construct spi_message, copying any tx data to bounce buffer.
         * We walk the array of user-provided transfers, using each one
         * to initialize a kernel version of the same transfer.
         */
        tx_buf = spidev->tx_buffer;
        total = 0;
        for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
                        n;
                        n--, k_tmp++, u_tmp++) {
                k_tmp->len = u_tmp->len;

                total += k_tmp->len;
                /* Check total length of transfers.  Also check each
                 * transfer length to avoid arithmetic overflow.
                 */
                if (total > BUFF_SIZE || k_tmp->len > BUFF_SIZE) {
                        status = -EMSGSIZE;
                        goto done;
                }

                if (u_tmp->tx_buf) {
                        k_tmp->tx_buf = tx_buf;
                        strncpy(tx_buf, (void *)u_tmp->tx_buf, u_tmp->len);
                }

                tx_buf += k_tmp->len;

                k_tmp->cs_change = !!u_tmp->cs_change;
                k_tmp->tx_nbits = u_tmp->tx_nbits;
                k_tmp->bits_per_word = u_tmp->bits_per_word;
                k_tmp->delay_usecs = u_tmp->delay_usecs;
                k_tmp->speed_hz = u_tmp->speed_hz;
                if (!k_tmp->speed_hz)
                        k_tmp->speed_hz = spidev->speed_hz;

                spi_message_add_tail(k_tmp, &msg);
        }

        status = spidev_sync(spidev, &msg);
        if (status < 0)
                goto done;

        status = total;

done:
        kfree(k_xfers);
        return status;
}

/* ============================================================================*/

/**
 * @brief setup multiplexing for SPI
 * @return 0, on success, otherwise failure.
 */
static int max7219_setup(void);

/**
 * @brief teardown multiplexing for SPI
 */
static void max7219_teardown(void);

/**
 * @brief send out one MAX7219 format message on SPI bus.
 * @param address, D15-D8 stands for the register address.
 * @param data, D7-D0 stands for the register data.
 */
static void max7219_send_one_msg(uint8_t, uint8_t);

/**
 * @brief a work queue function to send the data.
 * @param work, the work structure.
 */
static void max7219_work_function(struct work_struct *);

static int max7219_setup(void) {
        int ret;
        ret = gpio_request_one(SPI_DIR, GPIOF_OUT_INIT_LOW, "MAX7219");
        ret = gpio_request_one(SPI_MUX1, GPIOF_OUT_INIT_HIGH, "MAX7219");
        ret = gpio_request(SPI_MUX2, "MAX7219");
        gpio_set_value_cansleep(SPI_MUX2, 0);

        ret = gpio_request_one(SCK_DIR, GPIOF_OUT_INIT_LOW, "MAX7219");
        ret = gpio_request_one(SCK_MUX1, GPIOF_OUT_INIT_HIGH, "MAX7219");

        return 0;
}

static void max7219_teardown(void) {
        gpio_free(SPI_DIR);
        gpio_free(SPI_MUX1);
        gpio_free(SPI_MUX2);

        gpio_free(SCK_DIR);
        gpio_free(SCK_MUX1);
}

static void max7219_send_one_msg(uint8_t address, uint8_t data) {
        uint8_t tx[2];
        struct spi_ioc_transfer tr = {
            .delay_usecs = 0,
            .speed_hz = 1000000,
            .bits_per_word = 8,
            //.cs_change = true,
        };

        tr.tx_buf = (unsigned long)tx;
        tr.len = 2;

        tx[0] = address;
        tx[1] = data;

        spidev_message(&spidev, &tr, 1);
}

static void max7219_work_function(struct work_struct *work) {
        // We are not using contianer_of here, since the struct layout.
        display_work_t *my_work = (display_work_t *)work;
        uint8_t i;
        int cs_gpio;

        cs_gpio = quark_gpio_shield_to_gpio(my_work->cs_pin);

        for (i = 0; i < NUM_OF_COLUMNS; i++) {
                gpio_set_value_cansleep(cs_gpio, 0);
                barrier();
                max7219_send_one_msg(i + 1, my_work->led[i]);
                barrier();
                gpio_set_value_cansleep(cs_gpio, 1);
                mdelay(10);
        }

        kfree(work);
}

/**
 * @brief send the pattern message to the MAX7219 through SPI bus.
 * @param *msg, the message array.
 * @param len, the length of the array.
 * @param cs_pin, the chip select pin for latching the data.
 * @return 0 on success, otherwise failed.
 */
int max7219_send_msg(uint8_t *msg, int length, int cs_pin) {
        display_work_t *work;

        work = (display_work_t *)kmalloc(sizeof(display_work_t), GFP_KERNEL);
        if (!work)
                return -ENOMEM;

        INIT_WORK((struct work_struct *)work, max7219_work_function);
        work->cs_pin = cs_pin;
        memcpy(work->led, msg, length);
        return queue_work(max7219_wq, (struct work_struct*)work);
}

/**
 * @brief config the max7219 device with given chip select pin.
 * @note we assume that every time a new chip select corresponding to a new
 * LED Matrix, so it needs to be initilized.
 * @param cs_pin, the chip select pin for latching the data.
 */
void max7219_device_config(int cs_pin) {
        int cs_gpio;
        int i;
        cs_gpio = quark_gpio_shield_to_gpio(cs_pin);

        for (i = 0; i < 4; ++i)
        {
                gpio_set_value_cansleep(cs_gpio, 0);
                barrier();
                max7219_send_one_msg(s_init_sequence[i][0], s_init_sequence[i][1]);
                barrier();
                gpio_set_value_cansleep(cs_gpio, 1);
                mdelay(10);
        }
}

/**
 * @brief register the device when module is initiated
 * @return 0 on success, otherwise failed.
 */
int max7219_device_init(void) {
        int ret;
        struct spi_master *master;

        // Setup the GPIO multiplexing
        if (max7219_setup())
                return -EFAULT;

        master = spi_busnum_to_master(max7219_info.bus_num);
        if (!master)
                return -ENODEV;

        // Avoid double init.
        if (max7219_device != NULL)
                return -EPERM;

        max7219_wq = create_workqueue("display_pattern");
        if (!max7219_wq)
                return -ENOMEM;

        max7219_device = spi_new_device(master, &max7219_info);
        put_device(&master->dev);
        if (!max7219_device) {
                destroy_workqueue(max7219_wq);
                max7219_wq = NULL;
                return -ENODEV;
        }

        max7219_device->bits_per_word = 8;
        max7219_device->max_speed_hz = 1000000;
        //max7219_device->cs_gpio = 40;

        ret = spi_setup(max7219_device);
        if (ret)
                spi_unregister_device(max7219_device);
        else
                printk(KERN_INFO "MAX7219 registered to SPI bus %u, chipselect %u\n", 
                        max7219_info.bus_num, max7219_info.chip_select);

        spin_lock_init(&spidev.spi_lock);
        spidev.spi = max7219_device;
        spidev.speed_hz = max7219_device->max_speed_hz;
        spidev.tx_buffer = kmalloc(BUFF_SIZE, GFP_KERNEL);
        if (!spidev.tx_buffer) {
                return -ENOMEM;
        }

        spi_set_drvdata(max7219_device, &spidev);

        return ret;
}

/**
 * @brief remove the device when module is removed
 */
void max7219_device_exit(void) {
        // Double free will be handled by spi_unregister_device, kfree, and gpio_free.
        spi_unregister_device(max7219_device);
        destroy_workqueue(max7219_wq);
        kfree(spidev.tx_buffer);
        max7219_teardown();
        printk(KERN_ALERT "Goodbye, unregister the device\n");
        spidev.tx_buffer = NULL;
        max7219_device = NULL;
        max7219_wq = NULL;
}