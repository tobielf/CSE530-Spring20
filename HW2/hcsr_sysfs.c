/**
 * @file hcsr_sysfs.c
 * @brief Sysfs implementation for the HCSR driver.
 * 
 * @author Xiangyu Guo
 */
#include <linux/miscdevice.h>

#include <linux/device.h>

#include "defs.h"
#include "hcsr_drv.h"
#include "hcsr_config.h"

/** ==========================================================================
 *                      Distance sysfs attribute
 *============================================================================*/
ssize_t hcsr_distance_show(struct device *dev,
                           struct device_attribute *attr,
                           char *buf) {
        hcsr_dev_t *devp = dev_get_drvdata(dev);
        ssize_t status;

        // get most recently measurement.
        status = sprintf(buf, "%d\n", atomic_read(&devp->settings.most_recent));

        return status;
}

/** ==========================================================================
 *                      Trigger sysfs attribute
 *============================================================================*/
ssize_t hcsr_trigger_show(struct device *dev,
                          struct device_attribute *attr,
                          char *buf) {
        hcsr_dev_t *devp = dev_get_drvdata(dev);

        return sprintf(buf, "%d\n", devp->settings.pins.trigger_pin);
}

ssize_t hcsr_trigger_store(struct device *dev,
                           struct device_attribute *attr,
                           const char *buf,
                           size_t count) {
        int pin;
        int status;
        hcsr_dev_t *devp = dev_get_drvdata(dev);
        status = sscanf(buf, "%d", &pin);
        // lock the device, make sure no measurement using the pin.
        if (hcsr_lock(devp))
                return -EBUSY;
        if (hcsr04_config_validate_pin(pin)) {
                hcsr_unlock(devp);
                return -EINVAL;
        }
        if (devp->settings.pins.trigger_pin != -1)
                hcsr04_fini_pins(devp->settings.pins.trigger_pin);

        devp->settings.pins.trigger_pin = -1;

        if (hcsr04_init_pins(pin, OUTPUT)) {
                hcsr_unlock(devp);
                return -EBUSY;
        }

        devp->settings.pins.trigger_pin = pin;
        // unlock the device, finish pin setting.
        hcsr_unlock(devp);
        return count;
}

/** ==========================================================================
 *                      Echo sysfs attribute
 *============================================================================*/
ssize_t hcsr_echo_show(struct device *dev,
                       struct device_attribute *attr,
                       char *buf) {
        hcsr_dev_t *devp = dev_get_drvdata(dev);

        return sprintf(buf, "%d\n", devp->settings.pins.echo_pin);
}

ssize_t hcsr_echo_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf,
                        size_t count) {
        int pin;
        int status;
        hcsr_dev_t *devp = dev_get_drvdata(dev);
        status = sscanf(buf, "%d", &pin);
        // lock the device, make sure no measurement using the pin.
        if (hcsr_lock(devp))
                return -EBUSY;
        if (hcsr04_config_validate_pin(pin) ||
            hcsr04_config_validate_echo(pin)) {
                hcsr_unlock(devp);
                return -EINVAL;
        }
        if (devp->settings.pins.echo_pin != -1)
                hcsr04_fini_pins(devp->settings.pins.echo_pin);

        // Remove isr and irq_no
        hcsr_isr_exit(devp);

        devp->settings.pins.echo_pin = -1;

        if (hcsr04_init_pins(pin, INPUT)) {
                hcsr_unlock(devp);
                return -EBUSY;
        }

        devp->settings.pins.echo_pin = pin;

        // Setup isr and irq_no
        status = hcsr_isr_init(devp);
        if (status < 0) {
                devp->irq_no = 0;
                hcsr04_fini_pins(devp->settings.pins.echo_pin);
                devp->settings.pins.echo_pin = -1;
                printk(KERN_ALERT "irq failed %d\n", status);
                hcsr_unlock(devp);
                return status;
        }

        // unlock the device, finish pin setting.
        hcsr_unlock(devp);
        return count;
}

/** ==========================================================================
 *                      Samples sysfs attribute
 *============================================================================*/
ssize_t hcsr_samples_show(struct device *dev,
                          struct device_attribute *attr,
                          char *buf) {
        hcsr_dev_t *devp = dev_get_drvdata(dev);

        return sprintf(buf, "%d\n", devp->settings.params.m - NUM_OF_OUTLIER);
}

ssize_t hcsr_samples_store(struct device *dev,
                           struct device_attribute *attr,
                           const char *buf,
                           size_t count) {
        int m;
        int status;
        hcsr_dev_t *devp = dev_get_drvdata(dev);
        status = sscanf(buf, "%d", &m);
        if (m < 1)
                return -EINVAL;
        // lock the device 
        if (hcsr_lock(devp))
                return -EBUSY;
        // update m
        if (hcsr_reallocate(devp, m + NUM_OF_OUTLIER)) {
                hcsr_unlock(devp);
                return -ENOMEM;
        }

        devp->settings.params.m = m + NUM_OF_OUTLIER;

        // unlock the device
        hcsr_unlock(devp);
        return count;
}

/** ==========================================================================
 *                      Interval sysfs attribute
 *============================================================================*/
ssize_t hcsr_interval_show(struct device *dev,
                           struct device_attribute *attr,
                           char *buf) {
        hcsr_dev_t *devp = dev_get_drvdata(dev);

        return sprintf(buf, "%d\n", devp->settings.params.delta);
}

ssize_t hcsr_interval_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf,
                            size_t count) {
        int val;
        hcsr_dev_t *devp = dev_get_drvdata(dev);

        sscanf(buf, "%d", &val);
        if (val < MIN_INTERVAL)
                return -EINVAL;
        // lock the device 
        if (hcsr_lock(devp))
                return -EBUSY;

        devp->settings.params.delta = val;

        // unlock the device
        hcsr_unlock(devp);
        return count;
}

/** ==========================================================================
 *                      Enable sysfs attribute
 *============================================================================*/
ssize_t hcsr_enable_show(struct device *dev,
                         struct device_attribute *attr,
                         char *buf) {
        hcsr_dev_t *devp = dev_get_drvdata(dev);

        return sprintf(buf, "%d\n", devp->settings.endless);
}

ssize_t hcsr_enable_store(struct device *dev,
                          struct device_attribute *attr,
                          const char *buf,
                          size_t count) {
        int val;
        hcsr_dev_t *devp = dev_get_drvdata(dev);

        sscanf(buf, "%d", &val);
        if (val) {
                // lock the device 
                if (hcsr_lock(devp))
                        return -EBUSY;
                devp->settings.endless = val;
                hcsr_new_task(devp);
        } else {
                devp->settings.endless = val;
        }
        return count;
}