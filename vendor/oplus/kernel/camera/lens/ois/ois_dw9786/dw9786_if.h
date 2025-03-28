#ifndef DW9786_IF_H
#define DW9786_IF_H

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include "hf_manager.h"
#include "hf_sensor_io.h"
#include "adaptor-i2c.h"

#define DW9786_OIS_SLAVE_ADDR 0x19    /* OIS slave address(7bit) */
#define DW9786_FLASH_SLAVE_ADDR 0x3A    /* flash memory slave address(7bit) */

#define DW9786_CHIP_EN_ADDR 0xE000    /* standby mode enable, 0: sleep mode, 1: standby mode */
#define DW9786_MCU_ACTIVE_ADDR 0xE004    /* idle mode, 0: standby mode, 1: idle mode(MCU on) */
#define DW9786_DEVICE_RESET_ADDR 0xE008    /* device reset enable, 1: reset */
#define DW9786_SPI_WORK_MODE_ADDR 0xB040    /* 0: master, 1: intercept */
#define DW9786_IMU_TYPE_ADDR 0xB836    /* b'0110: bmi260 */
#define DW9786_USER_PROTECTION_ADDR 0x97E6    /* 0xEA: user protect off, 0x00: user protect on */
#define DW9786_USER_PROTECTION_A_ADDR 0xEDB0    /* 0xA23F: user protect a off, 0x00: user protect a on */
#define DW9786_DUMMY_DATA_ADDR 0x3FFF
#define DW9786_MODE_CONTROL_ADDR 0xB026
#define DW9786_STATUS_ADDR 0xB020
#define DW9786_MEMORY_AREA_SELECT_ADDR 0xB03E    /* 0x5959: set area, 0xA6A6: module area */
#define DW9786_ACTIVE_CONTROL_ADDR 0xB022
#define DW9786_AF_SERVO_ON_CONTROL 0xB024
#define DW9786_DUMMY_DATA_ADDR 0x3FFF
#define MCS_START_ADDRESS 0x6000
#define LUT_START_ADDRESS 0x4000
#define MCS_CHECKSUM_SIZE 10240
#define GYRO_OFFSET_X_CALI_PASS 0x1
#define GYRO_OFFSET_X_CALI_ERR 0x10
#define GYRO_OFFSET_Y_CALI_PASS 0x2
#define GYRO_OFFSET_Y_CALI_ERR 0x20
#define DW9786_RELEASE_VERSION_H_ADDR 0x9800
#define DW9786_RELEASE_VERSION_L_ADDR 0x9802
#define DW9786_RELEASE_DATE_ADDR 0x9804
#define DW9786_FMC_PAGE_NUM 20
#define DW9786_FW_PACKET_SIZE 128
#define DW9786_GYRO_GAIN_POL_X 0xB800
#define DW9786_GYRO_GAIN_POL_Y 0xB802
#define DW9786_GYRO_ROT_MAT_X 0xB812
#define DW9786_GYRO_ROT_MAT_Y 0xB814
#define DW9786_GYRO_GAIN_X 0xB806
#define DW9786_GYRO_GAIN_Y 0xB808
#define DW9786_GYRO_RAW_X_ADDR 0xB1A0
#define DW9786_GYRO_RAW_Y_ADDR 0xB2A0
#define DW9786_OIS_TARGET_X_ADDR 0xB118
#define DW9786_OIS_TARGET_Y_ADDR 0xB218
#define DW9786_OIS_HALL_X_ADDR 0xB104
#define DW9786_OIS_HALL_Y_ADDR 0xB204
#define DW9786_LEN_POS_X_ADDR 0xB102
#define DW9786_LEN_POS_Y_ADDR 0xB202
#define DW9786_TARGET_HALL_X_ADDR 0xB100
#define DW9786_TARGET_HALL_Y_ADDR 0xB200
#define DW9786_GYRO_GAIN_X_MAX 0x8000
#define DW9786_GYRO_GAIN_Y_MAX 0x8000
#define DW9786_TRIPOD_EN_ADDR 0xB028
#define DW9786_OIS_OP_MODE_ADDR 0xB0B2

#define DW9786_CHIP_ID_ADDR 0xB000
#define DW9786_CHIP_ID_VAL 0x9786
#define DW9786_PRODUCT_ID_ADDR 0xE018
#define DW9786_FW_VERSION_ADDR 0xB002
#define DW9786_FW_DATE_ADDR 0xB006

#define DW9786_CHECK_LOOP_TIMES 200
#define DW9786_CHECK_WAIT_TIME 100
#define MEMORY_AREA_CODE 1
#define MEMORY_AREA_LUT 2
#define MEMORY_AREA_USER 3

#define DW9786_TX_SIZE_32_BYTE  32
#define DW9786_TX_SIZE_64_BYTE  64
#define DW9786_TX_SIZE_128_BYTE  128
#define DW9786_TX_SIZE_256_BYTE  256
#define DW9786_TX_BUFFER_SIZE  DW9786_TX_SIZE_256_BYTE
#define DW9786_RX_BUFFER_SIZE  4


#define DW9786_REG_OIS_CTRL  0x0000
#define DW9786_REG_OIS_STS  0x0001
#define DW9786_REG_AF_CTRL  0x0200
#define DW9786_REG_AF_STS   0x0201
#define DW9786_REG_FWUP_CTRL  0x1000
#define DW9786_REG_FWUP_ERR  0x1001
#define DW9786_REG_FWUP_CHKSUM  0x1002
#define DW9786_REG_APP_VER  0x1008
#define DW9786_REG_DATA_BUF  0x1100

#define DW9786_OIS_OFF   0x00
#define DW9786_AF_OFF   0x00
#define DW9786_STATE_READY  0x01
#define DW9786_STATE_INIT  0x00
#define DW9786_RESET_REQ   0x80
#define DW9786_FWUP_CTRL_32_SET  0x01
#define DW9786_FWUP_CTRL_64_SET  0x03
#define DW9786_FWUP_CTRL_128_SET  0x05
#define DW9786_FWUP_CTRL_256_SET  0x07

#define DW9786_REG_GCAL_CTRL 0x0600
#define DW9786_REG_GX_OFFSET 0x0604
#define DW9786_REG_GY_OFFSET 0x0606
#define DW9786_REG_OIS_ERR 0x0004
#define DW9786_G_OFFSET_EN 0x01
#define DW9786_ERR_GCALX 0x0100
#define DW9786_ERR_GCALY 0x0200
#define DW9786_NO_ERROR 0x0000

#define DW9786_REGINFO_BLK_UP_CTRL 0x0300
#define DW9786_OIS_INFO_EN 0x01
#define DW9786_ERR_ODI 0x0040

#define DW9786_HALL_DATA_START  0x1100

#define DW9786_REG_OIS_CTRL  0x0000
#define DW9786_REG_OIS_MODE  0x0002
#define DW9786_REG_GX_GAIN 0x0608
#define DW9786_REG_GY_GAIN 0x060C
#define DW9786_OIS_ON   0x01
#define DW9786_STILL_MODE   0x00
#define DW9786_GX_GAIN_EN 0x02
#define DW9786_GY_GAIN_EN 0x04
#define DW9786_REG_GYRO_X 0x0B04
#define DW9786_REG_GYRO_Y 0x0B06
#define DW9786_REG_HALL_X 0x0700
#define DW9786_REG_HALL_Y 0x0702
#define DW9786_REG_GYRO_COMPUTED_X 0x0B08
#define DW9786_REG_GYRO_COMPUTED_Y 0x0B0A
#define DW9786_REG_SX_OUT 0x0B10
#define DW9786_REG_SY_OUT 0x0B12
#define DW9786_REG_STATMON_CTRL 0x0B00
#define DW9786_REG_SLEEP_MODE 0x0080

/* dw9786 device structure */
struct dw9786_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev subdev;
	struct regulator *vin;
	struct regulator *iovdd;
	struct regulator *afvdd;
	struct regulator *oisvdd;
	struct hf_device hf_dev;
	bool need_ois_on;
};

struct DW9786_GYRO_OFFSET {
	uint16_t offset_x;
	uint16_t offset_y;
};

enum IMU_TYPE {
	IMU_TYPE_BMI260 = 0b0110,
	IMU_TYPE_ICM45621 = 0b1110,
};

extern struct dw9786_device *dw9786;

/* Log interfaces */
#define LOG_INF(fmt, args...) pr_info("ois_dw9786 I [%s] " fmt, __func__, ##args)
#define LOG_ERR(fmt, args...) pr_err("ois_dw9786 E [%s] " fmt, __func__, ##args)
#define LOG_DBG(cond, ...)  do { if ( (cond) ) { LOG_INF(__VA_ARGS__); } }while(0)

/* i2c communication interfaces */
int i2c_read_8bit(uint16_t slave_addr, uint16_t reg_addr, uint8_t *data);
int i2c_read_16bit(uint16_t slave_addr, uint16_t reg_addr, uint16_t *data);
int i2c_read_32bit(uint16_t slave_addr, uint16_t reg_addr, uint32_t *data);
int i2c_block_read(uint16_t slave_addr, uint16_t reg_addr, void *data, uint32_t size);
int i2c_write_8bit(uint16_t slave_addr, uint16_t reg_addr, uint8_t data);
int i2c_write_16bit(uint16_t slave_addr, uint16_t reg_addr, uint16_t data);
int i2c_write_32bit(uint16_t slave_addr, uint16_t reg_addr, uint32_t data);
int i2c_block_write(uint16_t slave_addr, uint16_t reg_addr, void *data, uint32_t size);

/* i2c communation interfaces for OIS control*/
#define I2C_READ_8BIT_OIS(reg_addr, data) i2c_read_8bit((DW9786_OIS_SLAVE_ADDR), (reg_addr), (data))
#define I2C_READ_16BIT_OIS(reg_addr, data) i2c_read_16bit((DW9786_OIS_SLAVE_ADDR), (reg_addr), (data))
#define I2C_READ_32BIT_OIS(reg_addr, data) i2c_read_32bit((DW9786_OIS_SLAVE_ADDR), (reg_addr), (data))
#define I2C_BLOCK_READ_OIS(reg_addr, data, size) i2c_block_read((DW9786_OIS_SLAVE_ADDR), (reg_addr), (data), (size))
#define I2C_WRITE_8BIT_OIS(reg_addr, data) i2c_write_8bit((DW9786_OIS_SLAVE_ADDR), (reg_addr), (data))
#define I2C_WRITE_16BIT_OIS(reg_addr, data) i2c_write_16bit((DW9786_OIS_SLAVE_ADDR), (reg_addr), (data))
#define I2C_WRITE_32BIT_OIS(reg_addr, data) i2c_write_32bit((DW9786_OIS_SLAVE_ADDR), (reg_addr), (data))
#define I2C_BLOCK_WRITE_OIS(reg_addr, data, size) i2c_block_write((DW9786_OIS_SLAVE_ADDR), (reg_addr), (data), (size))

/* i2c communation interfaces for firmware update*/
#define I2C_READ_8BIT_FLASH(reg_addr, data) i2c_read_8bit((DW9786_FLASH_SLAVE_ADDR), (reg_addr), (data))
#define I2C_READ_16BIT_FLASH(reg_addr, data) i2c_read_16bit((DW9786_FLASH_SLAVE_ADDR), (reg_addr), (data))
#define I2C_READ_32BIT_FLASH(reg_addr, data) i2c_read_32bit((DW9786_FLASH_SLAVE_ADDR), (reg_addr), (data))
#define I2C_BLOCK_READ_FLASH(reg_addr, data, size) i2c_block_write((DW9786_FLASH_SLAVE_ADDR), (reg_addr), (data), (size))
#define I2C_WRITE_8BIT_FLASH(reg_addr, data) i2c_write_8bit((DW9786_FLASH_SLAVE_ADDR), (reg_addr), (data))
#define I2C_WRITE_16BIT_FLASH(reg_addr, data) i2c_write_16bit((DW9786_FLASH_SLAVE_ADDR), (reg_addr), (data))
#define I2C_BLOCK_WRITE_FLASH(reg_addr, data, size) i2c_block_write((DW9786_FLASH_SLAVE_ADDR), (reg_addr), (data), (size))

/* delay interfaces */
void dw9786_udelay(int us);
void dw9786_mdelay(int ms);

/* dw9786 functional interfaces */
void dw9786_set_gyro_gain(uint32_t gain_x, uint32_t gain_y);
int dw9786_set_hall(uint32_t hall_x, uint32_t hall_y);
int dw9786_get_position(uint16_t* position_x, uint16_t* position_y);

int dw9786_chip_enable(bool en);
int dw9786_mcu_active(bool en);
int dw9786_device_reset(void);
void dw9786_chip_info(void);
int dw9786_set_spi_work_mode(uint8_t mode);
int dw9786_set_imu_type(uint8_t type);
int dw9786_wait_check_register(uint16_t reg, uint16_t ref);
int dw9786_check_register(uint16_t reg, uint16_t ref);
int dw9786_check_and_write(uint16_t reg, uint16_t data, bool* rewrite_flag);
uint32_t dw9786_calc_checksum(uint8_t *data, uint32_t length);
int dw9786_module_store(void);
int dw9786_set_store(void);
int dw9786_module_erase(void);
int dw9786_set_erase(void);
uint32_t dw9786_read_flash_checksum(void);
uint32_t dw9786_checksum(int type);
void dw9786_memory_area_select(int type);
int dw9786_auto_read_check(void);
int dw9786_gyro_offset_calibration(uint16_t *offset_x, uint16_t *offset_y);
int dw9786_update_fw(void);
int dw9786_servo_on(void);
int dw9786_servo_off(void);
int dw9786_ois_on(void);
int dw9786_movie_mode_on(void);
int dw9786_sleep_mode(void);
int dw9786_idle_mode(void);
int dw9786_read_firmware(void);
int dw9786_hall_calibration(void);
int dw9786_servo_gain_calibration(void);
int dw9786_lens_offset_calibration(void);

#endif    /* DW9786_IF_H */