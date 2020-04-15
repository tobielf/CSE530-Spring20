/**
 * @file hcsr_sysfs.h
 * @brief Sysfs interface for the HCSR driver.
 * 
 * @author Xiangyu Guo
 */
#ifndef __HCSR_SYSFS_H__
#define __HCSR_SYSFS_H__

#include <linux/device.h>

ssize_t hcsr_distance_show(struct device *dev,
                           struct device_attribute *attr,
                           char *buf);

static DEVICE_ATTR(distance, S_IRUSR, hcsr_distance_show, NULL);

ssize_t hcsr_trigger_show(struct device *dev,
                          struct device_attribute *attr,
                          char *buf);
ssize_t hcsr_trigger_store(struct device *dev,
                           struct device_attribute *attr,
                           const char *buf,
                           size_t count);

static DEVICE_ATTR(trigger, S_IRUSR | S_IWUSR, hcsr_trigger_show, hcsr_trigger_store);

ssize_t hcsr_echo_show(struct device *dev,
                       struct device_attribute *attr,
                       char *buf);
ssize_t hcsr_echo_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf,
                        size_t count);

static DEVICE_ATTR(echo, S_IRUSR | S_IWUSR, hcsr_echo_show, hcsr_echo_store);                                                               

ssize_t hcsr_samples_show(struct device *dev,
                          struct device_attribute *attr,
                          char *buf);
ssize_t hcsr_samples_store(struct device *dev,
                           struct device_attribute *attr,
                           const char *buf,
                           size_t count);

static DEVICE_ATTR(number_samples, S_IRUSR | S_IWUSR, hcsr_samples_show, hcsr_samples_store);

ssize_t hcsr_interval_show(struct device *dev,
                           struct device_attribute *attr,
                           char *buf);
ssize_t hcsr_interval_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf,
                            size_t count);

static DEVICE_ATTR(sampling_period, S_IRUSR | S_IWUSR, hcsr_interval_show, hcsr_interval_store);

ssize_t hcsr_enable_show(struct device *dev,
                         struct device_attribute *attr,
                         char *buf);

ssize_t hcsr_enable_store(struct device *dev,
                          struct device_attribute *attr,
                          const char *buf,
                          size_t count);

static DEVICE_ATTR(enable, S_IRUSR | S_IWUSR, hcsr_enable_show, hcsr_enable_store);

static struct attribute *hcsr_attrs[] = {
        &dev_attr_distance.attr,
        &dev_attr_trigger.attr,
        &dev_attr_echo.attr,
        &dev_attr_number_samples.attr,
        &dev_attr_sampling_period.attr,
        &dev_attr_enable.attr,
        NULL,
};

static const struct attribute_group hcsr_group = {
        .attrs = hcsr_attrs,
};

static const struct attribute_group *hcsr_groups[] = {
        &hcsr_group,
        NULL
};

#endif