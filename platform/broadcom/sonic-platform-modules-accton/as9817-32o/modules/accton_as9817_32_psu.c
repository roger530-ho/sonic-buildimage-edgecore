/*
 * Copyright (C)  Roger Ho <roger530_ho@edge-core.com>
 * Based on:
 *    pca954x.c from Kumar Gala <galak@kernel.crashing.org>
 * Copyright (C) 2006
 *
 * Based on:
 *    pca954x.c from Ken Harrenstien
 * Copyright (C) 2004 Google, Inc. (Ken Harrenstien)
 *
 * Based on:
 *    i2c-virtual_cb.c from Brian Kuschak <bkuschak@yahoo.com>
 * and
 *    pca9540.c from Jean Delvare <khali@linux-fr.org>.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/stat.h>
#include <linux/sysfs.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/ipmi.h>
#include <linux/ipmi_smi.h>
#include <linux/platform_device.h>
#include <linux/string_helpers.h>
#include "accton_ipmi_intf.h"

#define DRVNAME "as9817_32_psu"
#define ACCTON_IPMI_NETFN 0x34
#define IPMI_PSU_READ_CMD 0x16
#define IPMI_PSU_MODEL_NAME_CMD 0x10
#define IPMI_PSU_SERIAL_NUM_CMD 0x11
#define IPMI_PSU_FAN_DIR_CMD 0x13
#define IPMI_PSU_INFO_CMD 0x20
#define IPMI_MODEL_SERIAL_LEN 32
#define IPMI_FAN_DIR_LEN 3

static ssize_t show_psu(struct device *dev, struct device_attribute *attr,
                            char *buf);
static ssize_t show_psu_info(struct device *dev, struct device_attribute *attr,
                            char *buf);
static ssize_t show_string(struct device *dev, struct device_attribute *attr,
                            char *buf);
static int as9817_32_psu_probe(struct platform_device *pdev);
static int as9817_32_psu_remove(struct platform_device *pdev);

enum psu_id {
    PSU_1,
    PSU_2,
    NUM_OF_PSU
};

enum psu_data_index {
    PSU_PRESENT = 0,
    PSU_TEMP_FAULT,
    PSU_POWER_GOOD_CPLD,
    PSU_POWER_GOOD_PMBUS,
    PSU_OVER_VOLTAGE,
    PSU_OVER_CURRENT,
    PSU_POWER_ON,
    PSU_VIN0,
    PSU_VIN1,
    PSU_VIN2,
    PSU_VOUT0,
    PSU_VOUT1,
    PSU_VOUT2,
    PSU_IIN0,
    PSU_IIN1,
    PSU_IIN2,
    PSU_IOUT0,
    PSU_IOUT1,
    PSU_IOUT2,
    PSU_PIN0,
    PSU_PIN1,
    PSU_PIN2,
    PSU_PIN3,
    PSU_POUT0,
    PSU_POUT1,
    PSU_POUT2,
    PSU_POUT3,
    PSU_TEMP1_0,
    PSU_TEMP1_1,
    PSU_TEMP2_0,
    PSU_TEMP2_1,
    PSU_TEMP3_0,
    PSU_TEMP3_1,
    PSU_FAN0,
    PSU_FAN1,
    PSU_VOUT_MODE,
    PSU_STATUS_COUNT,
    PSU_MODEL = 0,
    PSU_SERIAL = 0,
    PSU_TEMP1_MAX0 = 2,
    PSU_TEMP1_MAX1,
    PSU_TEMP1_MIN0,
    PSU_TEMP1_MIN1,
    PSU_TEMP2_MAX0,
    PSU_TEMP2_MAX1,
    PSU_TEMP2_MIN0,
    PSU_TEMP2_MIN1,
    PSU_TEMP3_MAX0,
    PSU_TEMP3_MAX1,
    PSU_TEMP3_MIN0,
    PSU_TEMP3_MIN1,
    PSU_VIN_UPPER_CRIT0,
    PSU_VIN_UPPER_CRIT1,
    PSU_VIN_UPPER_CRIT2,
    PSU_VIN_MAX0,
    PSU_VIN_MAX1,
    PSU_VIN_MAX2,
    PSU_VIN_MIN0,
    PSU_VIN_MIN1,
    PSU_VIN_MIN2,
    PSU_VIN_LOWER_CRIT0,
    PSU_VIN_LOWER_CRIT1,
    PSU_VIN_LOWER_CRIT2,
    PSU_VOUT_MAX0,
    PSU_VOUT_MAX1,
    PSU_VOUT_MAX2,
    PSU_VOUT_MIN0,
    PSU_VOUT_MIN1,
    PSU_VOUT_MIN2,
    PSU_IIN_MAX0,
    PSU_IIN_MAX1,
    PSU_IIN_MAX2,
    PSU_IOUT_MAX0,
    PSU_IOUT_MAX1,
    PSU_IOUT_MAX2,
    PSU_PIN_MAX0,
    PSU_PIN_MAX1,
    PSU_PIN_MAX2,
    PSU_PIN_MAX3,
    PSU_POUT_MAX0,
    PSU_POUT_MAX1,
    PSU_POUT_MAX2,
    PSU_POUT_MAX3,
    PSU_INFO_COUNT
};

struct ipmi_psu_resp_data {
    unsigned char status[PSU_STATUS_COUNT];
    unsigned char info[PSU_INFO_COUNT];
    char serial[IPMI_MODEL_SERIAL_LEN+1];
    char model[IPMI_MODEL_SERIAL_LEN+1];
    char fandir[IPMI_FAN_DIR_LEN+1];
};

struct as9817_32_psu_data {
    struct platform_device *pdev;
    struct device   *hwmon_dev;
    struct mutex update_lock;
    char valid;                    /* != 0 if registers are valid */
    unsigned long last_updated;    /* In jiffies */
    struct ipmi_data ipmi;
    struct ipmi_psu_resp_data ipmi_resp;
    unsigned char ipmi_tx_data[2];
};

struct platform_device *pdev[2] = {NULL};

static struct platform_driver as9817_32_psu_driver = {
    .probe = as9817_32_psu_probe,
    .remove = as9817_32_psu_remove,
    .driver = {
        .name = DRVNAME,
        .owner = THIS_MODULE,
    },
};

#define PSU_PRESENT_ATTR_ID(index) PSU##index##_PRESENT
#define PSU_POWERGOOD_ATTR_ID(index) PSU##index##_POWER_GOOD
#define PSU_VIN_ATTR_ID(index) PSU##index##_VIN
#define PSU_VOUT_ATTR_ID(index) PSU##index##_VOUT
#define PSU_IIN_ATTR_ID(index) PSU##index##_IIN
#define PSU_IOUT_ATTR_ID(index) PSU##index##_IOUT
#define PSU_PIN_ATTR_ID(index) PSU##index##_PIN
#define PSU_POUT_ATTR_ID(index) PSU##index##_POUT
#define PSU_MODEL_ATTR_ID(index) PSU##index##_MODEL
#define PSU_SERIAL_ATTR_ID(index) PSU##index##_SERIAL
#define PSU_TEMP1_INPUT_ATTR_ID(index) PSU##index##_TEMP1_INPUT
#define PSU_TEMP2_INPUT_ATTR_ID(index) PSU##index##_TEMP2_INPUT
#define PSU_TEMP3_INPUT_ATTR_ID(index) PSU##index##_TEMP3_INPUT
#define PSU_FAN_INPUT_ATTR_ID(index) PSU##index##_FAN_INPUT
#define PSU_FAN_DIR_ATTR_ID(index) PSU##index##_FAN_DIR

#define PSU_TEMP1_INPUT_MAX_ATTR_ID(index) PSU##index##_TEMP1_INPUT_MAX
#define PSU_TEMP1_INPUT_MIN_ATTR_ID(index) PSU##index##_TEMP1_INPUT_MIN
#define PSU_TEMP2_INPUT_MAX_ATTR_ID(index) PSU##index##_TEMP2_INPUT_MAX
#define PSU_TEMP2_INPUT_MIN_ATTR_ID(index) PSU##index##_TEMP2_INPUT_MIN
#define PSU_TEMP3_INPUT_MAX_ATTR_ID(index) PSU##index##_TEMP3_INPUT_MAX
#define PSU_TEMP3_INPUT_MIN_ATTR_ID(index) PSU##index##_TEMP3_INPUT_MIN
#define PSU_VIN_MAX_ATTR_ID(index) PSU##index##_VIN_MAX
#define PSU_VIN_MIN_ATTR_ID(index) PSU##index##_VIN_MIN
#define PSU_VIN_UPPER_CRIT_ATTR_ID(index) PSU##index##_VIN_UPPER_CRIT
#define PSU_VIN_LOWER_CRIT_ATTR_ID(index) PSU##index##_VIN_LOWER_CRIT
#define PSU_VOUT_MAX_ATTR_ID(index) PSU##index##_VOUT_MAX
#define PSU_VOUT_MIN_ATTR_ID(index) PSU##index##_VOUT_MIN
#define PSU_IIN_MAX_ATTR_ID(index) PSU##index##_IIN_MAX
#define PSU_IOUT_MAX_ATTR_ID(index) PSU##index##_IOUT_MAX
#define PSU_PIN_MAX_ATTR_ID(index) PSU##index##_PIN_MAX
#define PSU_POUT_MAX_ATTR_ID(index) PSU##index##_POUT_MAX

#define PSU_ATTR(psu_id) \
    PSU_PRESENT_ATTR_ID(psu_id), \
    PSU_POWERGOOD_ATTR_ID(psu_id), \
    PSU_VIN_ATTR_ID(psu_id), \
    PSU_VOUT_ATTR_ID(psu_id), \
    PSU_IIN_ATTR_ID(psu_id), \
    PSU_IOUT_ATTR_ID(psu_id), \
    PSU_PIN_ATTR_ID(psu_id), \
    PSU_POUT_ATTR_ID(psu_id), \
    PSU_MODEL_ATTR_ID(psu_id), \
    PSU_SERIAL_ATTR_ID(psu_id), \
    PSU_TEMP1_INPUT_ATTR_ID(psu_id), \
    PSU_TEMP2_INPUT_ATTR_ID(psu_id), \
    PSU_TEMP3_INPUT_ATTR_ID(psu_id), \
    PSU_FAN_INPUT_ATTR_ID(psu_id), \
    PSU_FAN_DIR_ATTR_ID(psu_id), \
    PSU_TEMP1_INPUT_MAX_ATTR_ID(psu_id), \
    PSU_TEMP1_INPUT_MIN_ATTR_ID(psu_id), \
    PSU_TEMP2_INPUT_MAX_ATTR_ID(psu_id), \
    PSU_TEMP2_INPUT_MIN_ATTR_ID(psu_id), \
    PSU_TEMP3_INPUT_MAX_ATTR_ID(psu_id), \
    PSU_TEMP3_INPUT_MIN_ATTR_ID(psu_id), \
    PSU_VIN_MAX_ATTR_ID(psu_id), \
    PSU_VIN_MIN_ATTR_ID(psu_id), \
    PSU_VIN_UPPER_CRIT_ATTR_ID(psu_id), \
    PSU_VIN_LOWER_CRIT_ATTR_ID(psu_id), \
    PSU_VOUT_MAX_ATTR_ID(psu_id), \
    PSU_VOUT_MIN_ATTR_ID(psu_id), \
    PSU_IIN_MAX_ATTR_ID(psu_id), \
    PSU_IOUT_MAX_ATTR_ID(psu_id), \
    PSU_PIN_MAX_ATTR_ID(psu_id), \
    PSU_POUT_MAX_ATTR_ID(psu_id)

enum as9817_32_psu_sysfs_attrs {
    /* psu attributes */
    PSU_ATTR(1),
    PSU_ATTR(2),
    NUM_OF_PSU_ATTR,
    NUM_OF_PER_PSU_ATTR = (NUM_OF_PSU_ATTR/NUM_OF_PSU)
};

/* psu attributes */
#define DECLARE_PSU_SENSOR_DEVICE_ATTR(index) \
    static SENSOR_DEVICE_ATTR(psu##index##_present,    S_IRUGO, show_psu, NULL, \
                                PSU##index##_PRESENT); \
    static SENSOR_DEVICE_ATTR(psu##index##_power_good, S_IRUGO, show_psu, NULL,\
                                PSU##index##_POWER_GOOD); \
    static SENSOR_DEVICE_ATTR(psu##index##_vin, S_IRUGO, show_psu, NULL, \
                                PSU##index##_VIN); \
    static SENSOR_DEVICE_ATTR(psu##index##_vout, S_IRUGO, show_psu, NULL, \
                                PSU##index##_VOUT); \
    static SENSOR_DEVICE_ATTR(psu##index##_iin, S_IRUGO, show_psu, NULL, \
                                PSU##index##_IIN); \
    static SENSOR_DEVICE_ATTR(psu##index##_iout, S_IRUGO, show_psu, NULL, \
                                PSU##index##_IOUT); \
    static SENSOR_DEVICE_ATTR(psu##index##_pin, S_IRUGO, show_psu, NULL, \
                                PSU##index##_PIN); \
    static SENSOR_DEVICE_ATTR(psu##index##_pout, S_IRUGO, show_psu, NULL, \
                                PSU##index##_POUT); \
    static SENSOR_DEVICE_ATTR(psu##index##_model, S_IRUGO, show_string, NULL, \
                                PSU##index##_MODEL); \
    static SENSOR_DEVICE_ATTR(psu##index##_serial, S_IRUGO, show_string, NULL,\
                                PSU##index##_SERIAL);\
    static SENSOR_DEVICE_ATTR(psu##index##_temp1_input, S_IRUGO, show_psu,NULL,\
                                PSU##index##_TEMP1_INPUT); \
    static SENSOR_DEVICE_ATTR(psu##index##_temp2_input, S_IRUGO, show_psu,NULL,\
                                PSU##index##_TEMP2_INPUT); \
    static SENSOR_DEVICE_ATTR(psu##index##_temp3_input, S_IRUGO, show_psu,NULL,\
                                PSU##index##_TEMP3_INPUT); \
    static SENSOR_DEVICE_ATTR(psu##index##_fan1_input, S_IRUGO, show_psu, NULL,\
                                PSU##index##_FAN_INPUT); \
    static SENSOR_DEVICE_ATTR(psu##index##_fan_dir, S_IRUGO, show_string, NULL,\
                                PSU##index##_FAN_DIR); \
    static SENSOR_DEVICE_ATTR(psu##index##_temp1_input_max, S_IRUGO, \
                        show_psu_info, NULL, PSU##index##_TEMP1_INPUT_MAX); \
    static SENSOR_DEVICE_ATTR(psu##index##_temp1_input_min, S_IRUGO, \
                        show_psu_info, NULL, PSU##index##_TEMP1_INPUT_MIN); \
    static SENSOR_DEVICE_ATTR(psu##index##_temp2_input_max, S_IRUGO, \
                        show_psu_info, NULL, PSU##index##_TEMP2_INPUT_MAX); \
    static SENSOR_DEVICE_ATTR(psu##index##_temp2_input_min, S_IRUGO, \
                        show_psu_info, NULL, PSU##index##_TEMP2_INPUT_MIN); \
    static SENSOR_DEVICE_ATTR(psu##index##_temp3_input_max, S_IRUGO, \
                        show_psu_info, NULL, PSU##index##_TEMP3_INPUT_MAX); \
    static SENSOR_DEVICE_ATTR(psu##index##_temp3_input_min, S_IRUGO, \
                        show_psu_info, NULL, PSU##index##_TEMP3_INPUT_MIN); \
    static SENSOR_DEVICE_ATTR(psu##index##_vin_max, S_IRUGO, \
                        show_psu_info, NULL,  PSU##index##_VIN_MAX); \
    static SENSOR_DEVICE_ATTR(psu##index##_vin_min, S_IRUGO, \
                        show_psu_info, NULL,  PSU##index##_VIN_MIN); \
    static SENSOR_DEVICE_ATTR(psu##index##_vin_upper_crit, S_IRUGO, \
                        show_psu_info, NULL,  PSU##index##_VIN_UPPER_CRIT); \
    static SENSOR_DEVICE_ATTR(psu##index##_vin_lower_crit, S_IRUGO, \
                        show_psu_info, NULL,  PSU##index##_VIN_LOWER_CRIT); \
    static SENSOR_DEVICE_ATTR(psu##index##_vout_max, S_IRUGO, \
                        show_psu_info, NULL,  PSU##index##_VOUT_MAX); \
    static SENSOR_DEVICE_ATTR(psu##index##_vout_min, S_IRUGO, \
                        show_psu_info, NULL,  PSU##index##_VOUT_MIN); \
    static SENSOR_DEVICE_ATTR(psu##index##_iin_max, S_IRUGO, \
                        show_psu_info, NULL,  PSU##index##_IIN_MAX); \
    static SENSOR_DEVICE_ATTR(psu##index##_iout_max, S_IRUGO, \
                        show_psu_info, NULL,  PSU##index##_IOUT_MAX); \
    static SENSOR_DEVICE_ATTR(psu##index##_pin_max, S_IRUGO, \
                        show_psu_info, NULL,  PSU##index##_PIN_MAX); \
    static SENSOR_DEVICE_ATTR(psu##index##_pout_max, S_IRUGO, \
                        show_psu_info, NULL,  PSU##index##_POUT_MAX)

#define DECLARE_PSU_ATTR(index) \
    &sensor_dev_attr_psu##index##_present.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_power_good.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_vin.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_vout.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_iin.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_iout.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_pin.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_pout.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_model.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_serial.dev_attr.attr,\
    &sensor_dev_attr_psu##index##_temp1_input.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_temp2_input.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_temp3_input.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_fan1_input.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_fan_dir.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_temp1_input_max.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_temp1_input_min.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_temp2_input_max.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_temp2_input_min.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_temp3_input_max.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_temp3_input_min.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_vin_max.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_vin_min.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_vin_upper_crit.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_vin_lower_crit.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_vout_max.dev_attr.attr,\
    &sensor_dev_attr_psu##index##_vout_min.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_iin_max.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_iout_max.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_pin_max.dev_attr.attr, \
    &sensor_dev_attr_psu##index##_pout_max.dev_attr.attr

DECLARE_PSU_SENSOR_DEVICE_ATTR(1);
/*Duplicate nodes for lm-sensors.*/
static SENSOR_DEVICE_ATTR(in0_input, S_IRUGO, show_psu, NULL, PSU1_VOUT);
static SENSOR_DEVICE_ATTR(curr1_input, S_IRUGO, show_psu, NULL, PSU1_IOUT);
static SENSOR_DEVICE_ATTR(power1_input, S_IRUGO, show_psu, NULL, PSU1_POUT);
static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, show_psu, NULL, PSU1_TEMP1_INPUT);
static SENSOR_DEVICE_ATTR(fan1_input, S_IRUGO, show_psu, NULL, PSU1_FAN_INPUT);

static struct attribute *as9817_32_psu1_attrs[] = {
    /* psu attributes */
    DECLARE_PSU_ATTR(1),
    &sensor_dev_attr_curr1_input.dev_attr.attr,
    &sensor_dev_attr_in0_input.dev_attr.attr,
    &sensor_dev_attr_power1_input.dev_attr.attr,
    &sensor_dev_attr_temp1_input.dev_attr.attr,
    &sensor_dev_attr_fan1_input.dev_attr.attr,
    NULL
};
static struct attribute_group as9817_32_psu1_group = {
    .attrs = as9817_32_psu1_attrs,
};
/* ATTRIBUTE_GROUPS(as9817_32_psu1); */

DECLARE_PSU_SENSOR_DEVICE_ATTR(2);
/*Duplicate nodes for lm-sensors.*/
static SENSOR_DEVICE_ATTR(in1_input, S_IRUGO, show_psu, NULL, PSU2_VOUT);
static SENSOR_DEVICE_ATTR(curr2_input, S_IRUGO, show_psu, NULL, PSU2_IOUT);
static SENSOR_DEVICE_ATTR(power2_input, S_IRUGO, show_psu, NULL, PSU2_POUT);
static SENSOR_DEVICE_ATTR(temp2_input, S_IRUGO, show_psu, NULL, PSU2_TEMP1_INPUT);
static SENSOR_DEVICE_ATTR(fan2_input, S_IRUGO, show_psu, NULL, PSU2_FAN_INPUT);
static struct attribute *as9817_32_psu2_attrs[] = {
    /* psu attributes */
    DECLARE_PSU_ATTR(2),
    &sensor_dev_attr_curr2_input.dev_attr.attr,
    &sensor_dev_attr_in1_input.dev_attr.attr,
    &sensor_dev_attr_power2_input.dev_attr.attr,
    &sensor_dev_attr_temp2_input.dev_attr.attr,
    &sensor_dev_attr_fan2_input.dev_attr.attr,
    NULL
};
static struct attribute_group as9817_32_psu2_group = {
    .attrs = as9817_32_psu2_attrs,
};
/* ATTRIBUTE_GROUPS(as9817_32_psu2); */

const struct attribute_group *as9817_32_psu_groups[][2] = {
    {&as9817_32_psu1_group, NULL},
    {&as9817_32_psu2_group, NULL}
};

static struct as9817_32_psu_data *as9817_32_psu_update_device(struct device *dev)
{
    struct as9817_32_psu_data *data = dev_get_drvdata(dev);
    unsigned char pid = 0;
    int status = 0;

    mutex_lock(&data->update_lock);

    if (time_before(jiffies, data->last_updated + HZ * 5) && data->valid) {
        mutex_unlock(&data->update_lock);
        return data;
    }

    pid = data->pdev->id;
    data->valid = 0;
    /* To be compatible for older BMC firmware */
    data->ipmi_resp.status[PSU_VOUT_MODE] = 0xff;

    /* Get status from ipmi */
    data->ipmi_tx_data[0] = pid + 1; /* PSU ID base id for ipmi start from 1 */
    status = ipmi_send_message(&data->ipmi, IPMI_PSU_READ_CMD,
                                data->ipmi_tx_data, 1,
                                data->ipmi_resp.status,
                                sizeof(data->ipmi_resp.status));
    if (unlikely(status != 0)) {
        dev_err(dev, "Failed to get PSU status");
        goto exit;
    }

    if (unlikely(data->ipmi.rx_result != 0)) {
        status = -EIO;
        goto exit;
    }

    /* Get model name from ipmi */
    data->ipmi_tx_data[0] = pid + 1; /* PSU ID base id for ipmi start from 1 */
    data->ipmi_tx_data[1] = IPMI_PSU_MODEL_NAME_CMD;
    status = ipmi_send_message(&data->ipmi, IPMI_PSU_READ_CMD,
                                data->ipmi_tx_data, 2,
                                data->ipmi_resp.model,
                                sizeof(data->ipmi_resp.model) - 1);
    if (unlikely(status != 0)) {
        dev_err(dev, "Failed to get PSU model name.");
        goto exit;
    }

    if (unlikely(data->ipmi.rx_result != 0)) {
        status = -EIO;
        goto exit;
    }

    /* Get serial number from ipmi */
    data->ipmi_tx_data[0] = pid + 1; /* PSU ID base id for ipmi start from 1 */
    data->ipmi_tx_data[1] = IPMI_PSU_SERIAL_NUM_CMD;
    status = ipmi_send_message(&data->ipmi, IPMI_PSU_READ_CMD,
                                data->ipmi_tx_data, 2,
                                data->ipmi_resp.serial,
                                sizeof(data->ipmi_resp.serial) - 1);
    if (unlikely(status != 0)) {
        dev_err(dev, "Failed to get PSU serial number.");
        goto exit;
    }

    if (unlikely(data->ipmi.rx_result != 0)) {
        status = -EIO;
        goto exit;
    }

    /* Get fan direction from ipmi */
    data->ipmi_tx_data[0] = pid + 1; /* PSU ID base id for ipmi start from 1 */
    data->ipmi_tx_data[1] = IPMI_PSU_FAN_DIR_CMD;
    status = ipmi_send_message(&data->ipmi, IPMI_PSU_READ_CMD,
                                data->ipmi_tx_data, 2,
                                data->ipmi_resp.fandir,
                                sizeof(data->ipmi_resp.fandir) - 1);
    if (unlikely(status != 0)) {
        dev_err(dev, "Failed to get PSU fan direction.");
        goto exit;
    }

    if (unlikely(data->ipmi.rx_result != 0)) {
        status = -EIO;
        goto exit;
    }

    /* Get capability from ipmi */
    data->ipmi_tx_data[0] = pid + 1; /* PSU ID base id for ipmi start from 1 */
    data->ipmi_tx_data[1] = IPMI_PSU_INFO_CMD;
    status = ipmi_send_message(&data->ipmi, IPMI_PSU_READ_CMD,
                                data->ipmi_tx_data, 2,
                                data->ipmi_resp.info,
                                sizeof(data->ipmi_resp.info));
    if (unlikely(status != 0)) {
        dev_err(dev, "Failed to get PSU capability.");
        goto exit;
    }

    if (unlikely(data->ipmi.rx_result != 0)) {
        status = -EIO;
        goto exit;
    }

    data->last_updated = jiffies;
    data->valid = 1;

exit:
    mutex_unlock(&data->update_lock);
    return data;
}

#define VALIDATE_PRESENT_RETURN(id) \
do { \
    if (data->ipmi_resp.status[PSU_PRESENT] == 0) { \
        mutex_unlock(&data->update_lock);   \
        return -ENXIO; \
    } \
} while (0)

static ssize_t show_psu(struct device *dev, struct device_attribute *da,
                            char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    struct as9817_32_psu_data *data;
    unsigned char pid = 0;
    u32 value = 0;
    int present = 0;
    int error = 0;
    int multiplier = 1000;

    data = as9817_32_psu_update_device(dev);
    if (!data->valid) {
        return -EIO;
    }

    mutex_lock(&data->update_lock);

    pid = data->pdev->id;
    present = !!(data->ipmi_resp.status[PSU_PRESENT]);

    switch (attr->index) {
    case PSU1_PRESENT:
    case PSU2_PRESENT:
        value = present;
        break;
    case PSU1_POWER_GOOD:
    case PSU2_POWER_GOOD:
        VALIDATE_PRESENT_RETURN(pid);
        value = data->ipmi_resp.status[PSU_POWER_GOOD_PMBUS];
        break;
    case PSU1_IIN:
    case PSU2_IIN:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.status[PSU_IIN0] |
                    (u32)data->ipmi_resp.status[PSU_IIN1] << 8 |
                    (u32)data->ipmi_resp.status[PSU_IIN2] << 16);
        break;
    case PSU1_IOUT:
    case PSU2_IOUT:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.status[PSU_IOUT0] |
                    (u32)data->ipmi_resp.status[PSU_IOUT1] << 8 |
                    (u32)data->ipmi_resp.status[PSU_IOUT2] << 16);
        break;
    case PSU1_VIN:
    case PSU2_VIN:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.status[PSU_VIN0] |
                    (u32)data->ipmi_resp.status[PSU_VIN1] << 8 |
                    (u32)data->ipmi_resp.status[PSU_VIN2] << 16);
        break;
    case PSU1_VOUT:
    case PSU2_VOUT:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.status[PSU_VOUT0] |
                    (u32)data->ipmi_resp.status[PSU_VOUT1] << 8 |
                    (u32)data->ipmi_resp.status[PSU_VOUT2] << 16);
        break;
    case PSU1_PIN:
    case PSU2_PIN:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.status[PSU_PIN0] |
                    (u32)data->ipmi_resp.status[PSU_PIN1] << 8  |
                    (u32)data->ipmi_resp.status[PSU_PIN2] << 16 |
                    (u32)data->ipmi_resp.status[PSU_PIN3] << 24);
        value /= 1000; // Convert to milliwatt
        break;
    case PSU1_POUT:
    case PSU2_POUT:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.status[PSU_POUT0] |
                    (u32)data->ipmi_resp.status[PSU_POUT1] << 8  |
                    (u32)data->ipmi_resp.status[PSU_POUT2] << 16 |
                    (u32)data->ipmi_resp.status[PSU_POUT3] << 24);
        value /= 1000; // Convert to milliwatt
        break;
    case PSU1_TEMP1_INPUT:
    case PSU2_TEMP1_INPUT:
        VALIDATE_PRESENT_RETURN(pid);
        value = (s16)((u16)data->ipmi_resp.status[PSU_TEMP1_0] |
                      (u16)data->ipmi_resp.status[PSU_TEMP1_1] << 8);
        value *= 1000; // Convert to millidegree Celsius
        break;
    case PSU1_TEMP2_INPUT:
    case PSU2_TEMP2_INPUT:
        VALIDATE_PRESENT_RETURN(pid);
        value = (s16)((u16)data->ipmi_resp.status[PSU_TEMP2_0] |
                      (u16)data->ipmi_resp.status[PSU_TEMP2_1] << 8);
        value *= 1000; // Convert to millidegree Celsius
        break;
    case PSU1_TEMP3_INPUT:
    case PSU2_TEMP3_INPUT:
        VALIDATE_PRESENT_RETURN(pid);
        value = (s16)((u16)data->ipmi_resp.status[PSU_TEMP3_0] |
                      (u16)data->ipmi_resp.status[PSU_TEMP3_1] << 8);
        value *= 1000; // Convert to millidegree Celsius
        break;
    case PSU1_FAN_INPUT:
    case PSU2_FAN_INPUT:
        VALIDATE_PRESENT_RETURN(pid);
        multiplier = 1;
        value = ((u32)data->ipmi_resp.status[PSU_FAN0] |
                    (u32)data->ipmi_resp.status[PSU_FAN1] << 8);
        break;
    default:
        error = -EINVAL;
        goto exit;
    }

    mutex_unlock(&data->update_lock);

    return sprintf(buf, "%d\n", present ? value : 0);

exit:
    mutex_unlock(&data->update_lock);
    return error;
}

static ssize_t show_psu_info(struct device *dev, struct device_attribute *da,
                            char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    struct as9817_32_psu_data *data;
    unsigned char pid = 0;
    s32 value = 0;
    int present = 0;
    int error = 0;

    data = as9817_32_psu_update_device(dev);
    if (!data->valid) {
        return -EIO;
    }

    mutex_lock(&data->update_lock);

    pid = data->pdev->id;
    present = !!(data->ipmi_resp.status[PSU_PRESENT]);

    switch (attr->index) {
    case PSU1_TEMP1_INPUT_MAX:
    case PSU2_TEMP1_INPUT_MAX:
        VALIDATE_PRESENT_RETURN(pid);
        value = (s16)((u16)data->ipmi_resp.info[PSU_TEMP1_MAX0] |
                      (u16)data->ipmi_resp.info[PSU_TEMP1_MAX1] << 8);
        value *= 1000; // Convert to millidegree Celsius
        break;
    case PSU1_TEMP1_INPUT_MIN:
    case PSU2_TEMP1_INPUT_MIN:
        VALIDATE_PRESENT_RETURN(pid);
        value = (s16)((u16)data->ipmi_resp.info[PSU_TEMP1_MIN0] |
                      (u16)data->ipmi_resp.info[PSU_TEMP1_MIN1] << 8);
        value *= 1000; // Convert to millidegree Celsius
        break;
    case PSU1_TEMP2_INPUT_MAX:
    case PSU2_TEMP2_INPUT_MAX:
        VALIDATE_PRESENT_RETURN(pid);
        value = (s16)((u16)data->ipmi_resp.info[PSU_TEMP2_MAX0] |
                      (u16)data->ipmi_resp.info[PSU_TEMP2_MAX1] << 8);
        value *= 1000; // Convert to millidegree Celsius
        break;
    case PSU1_TEMP2_INPUT_MIN:
    case PSU2_TEMP2_INPUT_MIN:
        VALIDATE_PRESENT_RETURN(pid);
        value = (s16)((u16)data->ipmi_resp.info[PSU_TEMP2_MIN0] |
                      (u16)data->ipmi_resp.info[PSU_TEMP2_MIN1] << 8);
        value *= 1000; // Convert to millidegree Celsius
        break;
    case PSU1_TEMP3_INPUT_MAX:
    case PSU2_TEMP3_INPUT_MAX:
        VALIDATE_PRESENT_RETURN(pid);
        value = (s16)((u16)data->ipmi_resp.info[PSU_TEMP3_MAX0] |
                      (u16)data->ipmi_resp.info[PSU_TEMP3_MAX1] << 8);
        value *= 1000; // Convert to millidegree Celsius
        break;
    case PSU1_TEMP3_INPUT_MIN:
    case PSU2_TEMP3_INPUT_MIN:
        VALIDATE_PRESENT_RETURN(pid);
        value = (s16)((u16)data->ipmi_resp.info[PSU_TEMP3_MIN0] |
                      (u16)data->ipmi_resp.info[PSU_TEMP3_MIN1] << 8);
        value *= 1000; // Convert to millidegree Celsius
        break;
    case PSU1_VIN_UPPER_CRIT:
    case PSU2_VIN_UPPER_CRIT:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.info[PSU_VIN_UPPER_CRIT0] |
                (u32)data->ipmi_resp.info[PSU_VIN_UPPER_CRIT1] << 8 |
                (u32)data->ipmi_resp.info[PSU_VIN_UPPER_CRIT2] << 16);
        break;
    case PSU1_VIN_LOWER_CRIT:
    case PSU2_VIN_LOWER_CRIT:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.info[PSU_VIN_LOWER_CRIT0] |
                (u32)data->ipmi_resp.info[PSU_VIN_LOWER_CRIT1] << 8 |
                (u32)data->ipmi_resp.info[PSU_VIN_LOWER_CRIT2] << 16);
        break;
    case PSU1_VIN_MAX:
    case PSU2_VIN_MAX:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.info[PSU_VIN_MAX0] |
                (u32)data->ipmi_resp.info[PSU_VIN_MAX1] << 8 |
                (u32)data->ipmi_resp.info[PSU_VIN_MAX2] << 16);
        break;
    case PSU1_VIN_MIN:
    case PSU2_VIN_MIN:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.info[PSU_VIN_MIN0] |
                (u32)data->ipmi_resp.info[PSU_VIN_MIN1] << 8 |
                (u32)data->ipmi_resp.info[PSU_VIN_MIN2] << 16);
        break;
    case PSU1_VOUT_MAX:
    case PSU2_VOUT_MAX:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.info[PSU_VOUT_MAX0] |
                (u32)data->ipmi_resp.info[PSU_VOUT_MAX1] << 8 |
                (u32)data->ipmi_resp.info[PSU_VOUT_MAX2] << 16);
        break;
    case PSU1_VOUT_MIN:
    case PSU2_VOUT_MIN:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.info[PSU_VOUT_MIN0] |
                (u32)data->ipmi_resp.info[PSU_VOUT_MIN1] << 8 |
                (u32)data->ipmi_resp.info[PSU_VOUT_MIN2] << 16);
        break;
    case PSU1_IIN_MAX:
    case PSU2_IIN_MAX:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.info[PSU_IIN_MAX0] |
                (u32)data->ipmi_resp.info[PSU_IIN_MAX1] << 8 |
                (u32)data->ipmi_resp.info[PSU_IIN_MAX2] << 16);
        break;
    case PSU1_IOUT_MAX:
    case PSU2_IOUT_MAX:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.info[PSU_IOUT_MAX0] |
                (u32)data->ipmi_resp.info[PSU_IOUT_MAX1] << 8 |
                (u32)data->ipmi_resp.info[PSU_IOUT_MAX2] << 16);
        break;
    case PSU1_PIN_MAX:
    case PSU2_PIN_MAX:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.info[PSU_PIN_MAX0] |
                (u32)data->ipmi_resp.info[PSU_PIN_MAX1] << 8  |
                (u32)data->ipmi_resp.info[PSU_PIN_MAX2] << 16 |
                (u32)data->ipmi_resp.info[PSU_PIN_MAX3] << 24);
        value /= 1000; // Convert to milliwatt
        break;
    case PSU1_POUT_MAX:
    case PSU2_POUT_MAX:
        VALIDATE_PRESENT_RETURN(pid);
        value = ((u32)data->ipmi_resp.info[PSU_POUT_MAX0] |
                (u32)data->ipmi_resp.info[PSU_POUT_MAX1] << 8  |
                (u32)data->ipmi_resp.info[PSU_POUT_MAX2] << 16 |
                (u32)data->ipmi_resp.info[PSU_POUT_MAX3] << 24);
        value /= 1000; // Convert to milliwatt
        break;
    default:
        error = -EINVAL;
        goto exit;
    }

    mutex_unlock(&data->update_lock);

    return sprintf(buf, "%d\n", present ? value : 0);

exit:
    mutex_unlock(&data->update_lock);
    return error;
}

static ssize_t show_string(struct device *dev, struct device_attribute *da,
                                char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    struct as9817_32_psu_data *data;
    unsigned char pid = 0;
    char *str = NULL;
    int error = 0;

    data = as9817_32_psu_update_device(dev);
    if (!data->valid) {
        return -EIO;
    }

    mutex_lock(&data->update_lock);

    pid = data->pdev->id;

    switch (attr->index) {
    case PSU1_MODEL:
    case PSU2_MODEL:
        VALIDATE_PRESENT_RETURN(pid);
        str = data->ipmi_resp.model;
        break;
    case PSU1_SERIAL:
    case PSU2_SERIAL:
        VALIDATE_PRESENT_RETURN(pid);
        str = data->ipmi_resp.serial;
        break;
    case PSU1_FAN_DIR:
    case PSU2_FAN_DIR:
        VALIDATE_PRESENT_RETURN(pid);
        str = data->ipmi_resp.fandir;
        break;
    default:
        error = -EINVAL;
        goto exit;
    }

    mutex_unlock(&data->update_lock);
    return sprintf(buf, "%s\n", str);

exit:
    mutex_unlock(&data->update_lock);
    return error;
}

static int as9817_32_psu_probe(struct platform_device *pdev)
{
    int status = 0;
    struct device *hwmon_dev = NULL;
    struct as9817_32_psu_data *data = NULL;

    data = kzalloc(sizeof(struct as9817_32_psu_data), GFP_KERNEL);
    if (!data) {
        return -ENOMEM;
    }

    mutex_init(&data->update_lock);

    hwmon_dev = hwmon_device_register_with_groups(&pdev->dev, DRVNAME, 
                    data, as9817_32_psu_groups[pdev->id]);
    if (IS_ERR(hwmon_dev)) {
        status = PTR_ERR(hwmon_dev);
        return status;
    }

    data->hwmon_dev = hwmon_dev;
    data->pdev = pdev;

    /* Set up IPMI interface */
    status = init_ipmi_data(&data->ipmi, 0, &data->pdev->dev);
    if (status) {
        return status;
    }

    platform_set_drvdata(pdev, data);

    dev_info(&pdev->dev, "PSU%d device created\n", pdev->id + 1);

    return 0;
}

static int as9817_32_psu_remove(struct platform_device *pdev)
{
    struct as9817_32_psu_data *data = platform_get_drvdata(pdev); 

    mutex_lock(&data->update_lock);
    if (data->hwmon_dev) {
        hwmon_device_unregister(data->hwmon_dev);
        data->hwmon_dev = NULL;
    }
    ipmi_destroy_user(data->ipmi.user);
    mutex_unlock(&data->update_lock);

    kfree(data);

    return 0;
}

static int __init as9817_32_psu_init(void)
{
    int ret;
    int i;

    ret = platform_driver_register(&as9817_32_psu_driver);
    if (ret < 0)
        goto dri_reg_err;

    for (i = 0; i < NUM_OF_PSU; i++) {
        pdev[i] = platform_device_register_simple(DRVNAME, i, NULL, 0);
        if (IS_ERR(pdev[i])) {
            ret = PTR_ERR(pdev[i]);
            goto dev_reg_err;
        }
    }

    return 0;

dev_reg_err:
    platform_driver_unregister(&as9817_32_psu_driver);

dri_reg_err:
    return ret;
}

static void __exit as9817_32_psu_exit(void)
{
    int i;

    for (i = 0; i < NUM_OF_PSU; i++) {
        if (pdev[i] != NULL) {
            platform_device_unregister(pdev[i]);
        }
    }
    platform_driver_unregister(&as9817_32_psu_driver);
}

MODULE_AUTHOR("Roger Ho <roger530_ho@accton.com>");
MODULE_DESCRIPTION("as9817_32_psu driver");
MODULE_LICENSE("GPL");

module_init(as9817_32_psu_init);
module_exit(as9817_32_psu_exit);
