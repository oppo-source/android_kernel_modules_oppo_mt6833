#include "dw9786_if.h"
#include "dw9786_fw_data.h"

#include "adaptor-i2c.h"

struct mutex dw9786_idle_mutex;

int i2c_read_8bit(uint16_t slave_addr, uint16_t reg_addr, uint8_t *data)
{
	int ret;
	struct i2c_client *i2c_client;
	i2c_client = dw9786->hf_dev.private_data;
	ret = adaptor_i2c_rd_u8(i2c_client, slave_addr, reg_addr, data);
	if (ret < 0) {
		LOG_ERR("failed, slave addr: 0x%02X, reg addr: 0x%02X, ret: %d", slave_addr, reg_addr, ret);
	}
	return ret;
}

int i2c_read_16bit(uint16_t slave_addr, uint16_t reg_addr, uint16_t *data)
{
	int ret;
	struct i2c_client *i2c_client;
	i2c_client = dw9786->hf_dev.private_data;
	ret = adaptor_i2c_rd_u16(i2c_client, slave_addr, reg_addr, data);
	if (ret < 0) {
		LOG_ERR("failed, slave addr: 0x%02X, reg addr: 0x%02X, ret: %d", slave_addr, reg_addr, ret);
	}
	return ret;
}

int i2c_read_32bit(uint16_t slave_addr, uint16_t reg_addr, uint32_t *data)
{
	int ret;
	struct i2c_client *i2c_client;
	i2c_client = dw9786->hf_dev.private_data;
	ret = adaptor_i2c_rd_p8(i2c_client, slave_addr, reg_addr, (uint8_t*)data, 4);
	if (ret < 0) {
		LOG_ERR("failed, slave addr: 0x%02X, reg addr: 0x%02X, ret: %d", slave_addr, reg_addr, ret);
	}
	return ret;
}

int i2c_block_read(uint16_t slave_addr, uint16_t reg_addr, void *data, uint32_t size)
{
	int ret;
	struct i2c_client *i2c_client;
	i2c_client = dw9786->hf_dev.private_data;
	ret = adaptor_i2c_rd_p8(i2c_client, slave_addr, reg_addr, (uint8_t*)data, size);
	if (ret < 0) {
		LOG_ERR("failed, slave addr: 0x%02X, reg addr: 0x%02X, data: %p, size: %d, ret: %d", slave_addr, reg_addr, data, size, ret);
	}
	return ret;
}

int i2c_write_8bit(uint16_t slave_addr, uint16_t reg_addr, uint8_t data)
{
	int ret;
	struct i2c_client *i2c_client;
	i2c_client = dw9786->hf_dev.private_data;
	ret = adaptor_i2c_wr_u8(i2c_client, slave_addr, reg_addr, data);
	if (ret < 0) {
		LOG_ERR("failed, slave addr: 0x%02X, reg addr: 0x%02X, data: 0x%02X, ret: %d", slave_addr, reg_addr, data, ret);
	}
	return ret;
}

int i2c_write_16bit(uint16_t slave_addr, uint16_t reg_addr, uint16_t data)
{
	int ret;
	struct i2c_client *i2c_client;
	i2c_client = dw9786->hf_dev.private_data;
	ret = adaptor_i2c_wr_u16(i2c_client, slave_addr, reg_addr, data);
	if (ret < 0) {
		LOG_ERR("failed, slave addr: 0x%02X, reg addr: 0x%02X, data: 0x%02X, ret: %d", slave_addr, reg_addr, data, ret);
	}
	return ret;
}

int i2c_write_32bit(uint16_t slave_addr, uint16_t reg_addr, uint32_t data)
{
	int ret;
	struct i2c_client *i2c_client;
	i2c_client = dw9786->hf_dev.private_data;
	ret = adaptor_i2c_wr_p8(i2c_client, slave_addr, reg_addr, (uint8_t*)&data, 4);
	if (ret < 0) {
		LOG_ERR("failed, slave addr: 0x%02X, reg addr: 0x%02X, data: 0x%02X, ret: %d", slave_addr, reg_addr, data, ret);
	}
	return ret;
}

int i2c_block_write(uint16_t slave_addr, uint16_t reg_addr, void *data, uint32_t size)
{
	int ret;
	struct i2c_client *i2c_client;
	i2c_client = dw9786->hf_dev.private_data;
	ret = adaptor_i2c_wr_p8(i2c_client, slave_addr, reg_addr, (uint8_t*)data, size);
	if (ret < 0) {
		LOG_ERR("failed, slave addr: 0x%02X, reg addr: 0x%02X, data: %p, ret: %d", slave_addr, reg_addr, data, ret);
	}
	return ret;
}

void dw9786_udelay(int us)
{
	mdelay((us + 999) / 1000);
	return;
}

void dw9786_mdelay(int ms)
{
	mdelay(ms + 1);
	return;
}

int DW9786_Store_OIS_Cal_Data (void)
{
	uint8_t txdata[DW9786_TX_BUFFER_SIZE];
	uint8_t rxdata[DW9786_RX_BUFFER_SIZE];
	uint16_t repeatedCnt = 1000;
	int32_t data_error = 0xFFFF;

	I2C_READ_8BIT_OIS(DW9786_REG_OIS_STS, rxdata);
	LOG_ERR("DW9786_REG_OIS_STS: %u", rxdata[0]);

	if (rxdata[0] != DW9786_STATE_READY)
	{
		txdata[0] = DW9786_OIS_OFF;
		I2C_WRITE_8BIT_OIS(DW9786_REG_OIS_CTRL, txdata[0]);
	}

	txdata[0] = DW9786_OIS_INFO_EN; /* Set OIS_INFO_EN */
	/* Write 1 Byte to REG_INFO_BLK_UP_CTRL */
	I2C_WRITE_8BIT_OIS(DW9786_REGINFO_BLK_UP_CTRL, txdata[0]);
	LOG_ERR("write DW9786_REGINFO_BLK_UP_CTRL: %u", txdata[0]);
	/* I2C_Write_Data(REG_INFO_BLK_UP_CTRL, 1, txdata); */
	dw9786_udelay(100); /* Delay 100 ms */

	do
	{
		if (repeatedCnt == 0)
		{
			/* Abnormal Termination Error. */
			LOG_ERR("REG_INFO_BLK_UP_CTRL failed,: %u", rxdata[0]);
			return 0;
		}
		dw9786_udelay(50); /* Delay 50 ms */
		I2C_READ_8BIT_OIS(DW9786_REGINFO_BLK_UP_CTRL, rxdata);
		repeatedCnt--;
	} while ((rxdata[0] & DW9786_OIS_INFO_EN) == DW9786_OIS_INFO_EN);

	I2C_READ_8BIT_OIS(DW9786_REG_OIS_ERR, rxdata);
	I2C_READ_8BIT_OIS(DW9786_REG_OIS_ERR + 1, rxdata + 1);
	data_error = (uint16_t)((rxdata[0]) | (rxdata[1] << 8 ));

	if ((data_error & DW9786_ERR_ODI) != DW9786_NO_ERROR)
	{
		/* Different INFORWRITE data on flash */
		LOG_ERR("DW9786_ERR_ODI error %d", data_error);
		return 0;
	}

	return 1;
	/* INFORWRITE data on flash Success Process */
}

void GyroRead(uint32_t addr)
{
	unsigned char rxdata[4];
	I2C_READ_8BIT_OIS(addr, rxdata);
	I2C_READ_8BIT_OIS(addr+1, rxdata + 1);
	I2C_READ_8BIT_OIS(addr+2, rxdata + 2);
	I2C_READ_8BIT_OIS(addr+3, rxdata + 3);
	LOG_ERR("[GyroRead] addr= 0x%x, read = 0x%x %x %x %x", addr, rxdata[0], rxdata[1], rxdata[2], rxdata[3]);
}

void GyroWrite(uint32_t addr, uint32_t gain)
{
	unsigned char txdata[4];
	txdata[0] = gain & 0x00FF;
	txdata[1] = (gain & 0xFF00) >> 8;
	txdata[2] = (gain & 0xFF0000) >> 16;
	txdata[3] = (gain & 0xFF000000) >> 24;
	I2C_WRITE_8BIT_OIS(addr, txdata[0]); /* write REG_GGX Little endian*/
	I2C_WRITE_8BIT_OIS(addr+1, txdata[1]);
	I2C_WRITE_8BIT_OIS(addr+2, txdata[2]);
	I2C_WRITE_8BIT_OIS(addr+3, txdata[3]);
	LOG_ERR("[GyroRead] gain = %u, addr= 0x%x, write = 0x%x %x %x %x", gain, addr, txdata[0], txdata[1], txdata[2], txdata[3]);
}

void dw9786_set_gyro_gain(uint32_t gain_x, uint32_t gain_y)
{
	uint16_t gain_x_origin, gain_y_origin;
	uint16_t gain_x_current, gain_y_current;
	int ret;
	I2C_READ_16BIT_OIS(DW9786_GYRO_GAIN_X, &gain_x_origin);
	I2C_READ_16BIT_OIS(DW9786_GYRO_GAIN_Y, &gain_y_origin);
	LOG_INF("original gyro gain: [%04X, %04X]", gain_x_origin, gain_y_origin);
	ret = I2C_WRITE_16BIT_OIS(DW9786_GYRO_GAIN_X, gain_x);
	if (ret < 0) {
		LOG_ERR("write gyro gain x failed");
	}
	ret = I2C_WRITE_16BIT_OIS(DW9786_GYRO_GAIN_Y, gain_y);
	if (ret < 0) {
		LOG_ERR("write gyro gain y failed");
	}
	I2C_READ_16BIT_OIS(DW9786_GYRO_GAIN_X, &gain_x_current);
	I2C_READ_16BIT_OIS(DW9786_GYRO_GAIN_Y, &gain_y_current);
	LOG_INF("after write gyro gain: [%04X, %04X]", gain_x_current, gain_y_current);

	return;
}

int dw9786_set_hall(uint32_t x, uint32_t y)
{
	int ret = -1;
	int ret1 = -1;
	ret = I2C_WRITE_16BIT_OIS(DW9786_TARGET_HALL_X_ADDR, x);
	ret1 = I2C_WRITE_16BIT_OIS(DW9786_TARGET_HALL_Y_ADDR, y);
	if (ret < 0 || ret1 < 0) {
		return -1;
	}

	return 0;
}

int dw9786_get_position(uint16_t* position_x, uint16_t* position_y)
{
	int ret;
	int ret1;
	ret = I2C_READ_16BIT_OIS(0xB102, position_x);
	ret1 = I2C_READ_16BIT_OIS(0xB202, position_y);
	if (ret < 0 || ret1 < 0) {
		return -1;
	}

	return 0;
}

int dw9786_fw_download(void)
{
	uint8_t txdata[DW9786_TX_BUFFER_SIZE + 2];
	uint8_t rxdata[DW9786_RX_BUFFER_SIZE];
	uint8_t* chkBuff = NULL;
	uint16_t txBuffSize;
	uint32_t i, chkIdx;
	uint16_t subaddr_FLASH_DATA_BIN_1;

	uint16_t idx = 0;
	uint16_t check_sum;
	uint32_t updated_ver;
	uint32_t new_fw_ver;
	uint32_t current_fw_ver;
	uint16_t reg_addr;
	int rc = 0;
	chkBuff = (uint8_t*)kzalloc(DW9786_FW_SIZE, GFP_KERNEL);

	I2C_READ_8BIT_OIS(DW9786_REG_APP_VER, rxdata);
	I2C_READ_8BIT_OIS(DW9786_REG_APP_VER + 1, rxdata + 1);
	I2C_READ_8BIT_OIS(DW9786_REG_APP_VER + 2, rxdata + 2);
	I2C_READ_8BIT_OIS(DW9786_REG_APP_VER + 3, rxdata + 3);
	new_fw_ver = *(uint32_t *)&dw9786_fw_data[DW9786_FW_SIZE - 12];
	current_fw_ver = ((uint32_t *)rxdata)[0];

	LOG_ERR("current_fw_ver: 0x%x, new_fw_ver: 0x%x", current_fw_ver, new_fw_ver);
	if (current_fw_ver == new_fw_ver)
	{
		LOG_ERR("version is the same, no need to update");
		return 0;
	}

	if (current_fw_ver != 0)
	{
		I2C_READ_8BIT_OIS(DW9786_REG_OIS_STS, rxdata);
		if (rxdata[0] != DW9786_STATE_READY)
		{
			txdata[0] = DW9786_OIS_OFF;
			rc = I2C_WRITE_8BIT_OIS(DW9786_REG_OIS_CTRL, txdata[0]);
			if (rc < 0)
			{
				LOG_ERR("error 1");
				goto error_hand;
			}
		}
		I2C_READ_8BIT_OIS(DW9786_REG_AF_STS, rxdata);
		if (rxdata[0] != DW9786_STATE_READY)
		{
			txdata[0] = DW9786_AF_OFF;
			rc = I2C_WRITE_8BIT_OIS(DW9786_REG_AF_CTRL, txdata[0]);
			if (rc < 0)
			{
				LOG_ERR("error 2");
				goto error_hand;
			}
		}
	}

	txBuffSize = DW9786_TX_SIZE_256_BYTE;
	switch (txBuffSize)
	{
		case DW9786_TX_SIZE_32_BYTE:
			txdata[0] =  DW9786_FWUP_CTRL_32_SET;
			break;
		case DW9786_TX_SIZE_64_BYTE:
			txdata[0] = DW9786_FWUP_CTRL_64_SET;
			break;
		case DW9786_TX_SIZE_128_BYTE:
			txdata[0] = DW9786_FWUP_CTRL_128_SET;
			break;
		case DW9786_TX_SIZE_256_BYTE:
			txdata[0] = DW9786_FWUP_CTRL_256_SET;
			break;
		default:
			break;
	}
	rc = I2C_WRITE_8BIT_OIS(DW9786_REG_FWUP_CTRL, txdata[0]);
	if (rc < 0)
	{
		LOG_ERR("error 3");
		goto error_hand;
	}
	dw9786_udelay(60);
	check_sum = 0;

	subaddr_FLASH_DATA_BIN_1 = DW9786_REG_DATA_BUF;
	for (i = 0; i < (DW9786_FW_SIZE / txBuffSize); i++)
	{
		memcpy(&chkBuff[txBuffSize * i], &dw9786_fw_data[idx], txBuffSize);
		for (chkIdx = 0; chkIdx < txBuffSize; chkIdx += 2)
		{
			check_sum += ((chkBuff[chkIdx + 1 + (txBuffSize * i)] << 8) |  chkBuff[chkIdx + (txBuffSize * i)]);
		}
		memcpy(txdata, &dw9786_fw_data[idx], txBuffSize);
		reg_addr = subaddr_FLASH_DATA_BIN_1;
		rc = I2C_BLOCK_WRITE_OIS(reg_addr, txdata, DW9786_TX_BUFFER_SIZE);
		if (rc < 0)
		{
			LOG_ERR("error 4");
			goto error_hand;
		}
		LOG_ERR("update ois fw blk_num: %d 0x%x%x rc %d", i + 1, txdata[0], txdata[1], rc);
		idx += txBuffSize;
		dw9786_udelay(20);
	}

	((uint16_t*)txdata)[0] = check_sum;
	LOG_ERR("test %d",((uint16_t*)txdata)[1]);
	reg_addr = DW9786_REG_FWUP_CHKSUM;
	rc = I2C_BLOCK_WRITE_OIS(reg_addr, txdata, 2);
	if (rc < 0)
	{
		LOG_ERR("error 5");
		goto error_hand;
	}
	dw9786_udelay(200);

	I2C_READ_8BIT_OIS(DW9786_REG_FWUP_ERR, rxdata);
	if (rxdata[0] != DW9786_NO_ERROR)
	{
		LOG_ERR("update fw error 0x%x", rxdata[0]);
		return -1;
	}

	txdata[0] = DW9786_RESET_REQ;
	rc = I2C_WRITE_8BIT_OIS(DW9786_REG_FWUP_CTRL,txdata[0]);
	if (rc < 0)
	{
		goto error_hand;
	}
	dw9786_udelay(200);

	I2C_READ_8BIT_OIS(DW9786_REG_APP_VER, rxdata);
	I2C_READ_8BIT_OIS(DW9786_REG_APP_VER + 1, rxdata + 1);
	I2C_READ_8BIT_OIS(DW9786_REG_APP_VER + 2, rxdata + 2);
	I2C_READ_8BIT_OIS(DW9786_REG_APP_VER + 3, rxdata + 3);

	updated_ver = *(uint32_t *)rxdata;

	LOG_ERR("updated_ver: 0x%x, new_fw_ver: 0x%x", updated_ver, new_fw_ver);
	if (updated_ver != new_fw_ver)
	{
		LOG_ERR("update fw failed,, update version is not equal with read");
		return -1;
	}
error_hand:

	LOG_ERR("update fw end, rc: %d", rc);

	return rc;
}

int dw9786_chip_enable(bool en)
{
	int ret = -1;
	unsigned short stdby[17] = {0x0100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
								0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
								0x0000, 0x0000, 0x0000};
	if (en) {
		LOG_INF("enter standby mode");
		ret = I2C_BLOCK_WRITE_OIS(DW9786_CHIP_EN_ADDR, (unsigned char *)stdby, 34);
	} else {
		LOG_INF("enter sleep mode");
		ret = I2C_WRITE_16BIT_OIS(DW9786_CHIP_EN_ADDR, 0);
	}

	return ret;
}

int dw9786_mcu_active(bool en)
{
	int ret = -1;

	if (en) {
		LOG_INF("enter idle mode");
		ret = I2C_WRITE_16BIT_OIS(DW9786_MCU_ACTIVE_ADDR, 1);
	} else {
		LOG_INF("enter standby mode");
		ret = I2C_WRITE_16BIT_OIS(DW9786_MCU_ACTIVE_ADDR, 0);
	}

	if (!(ret < 0)) {
		mdelay(1);
		LOG_ERR("mcu active(%d) success", en);
	} else {
		LOG_ERR("mcu active(%d) failed,", en);
	}

	return ret;
}

int dw9786_device_reset(void)
{
	int ret = -1;
	uint16_t ois_status;
	uint16_t chip_en;
	uint16_t mcu_active;
	unsigned short stdby[17] = {0x0100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
								0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
								0x0000, 0x0000, 0x0000};

	mutex_lock(&dw9786_idle_mutex);
	LOG_INF("dw9786_idle_mutex: %p", &dw9786_idle_mutex);
	I2C_READ_16BIT_OIS(DW9786_STATUS_ADDR, &ois_status);
	LOG_INF("ois status: 0x%04X", ois_status);

	if (ois_status == 0x1021) {
		LOG_INF("ois has been enabled, return 0");
		mutex_unlock(&dw9786_idle_mutex);
		return 0;
	}

	I2C_READ_16BIT_OIS(DW9786_STATUS_ADDR, &chip_en);
	LOG_INF("chip_en: 0x%04X", chip_en);
	if (chip_en != 0x0001) {
		ret = I2C_WRITE_16BIT_OIS(DW9786_CHIP_EN_ADDR, 0x0000);    /* sleep mode */
		if (ret < 0) {
			LOG_ERR("enter sleep mode failed,");
			mutex_unlock(&dw9786_idle_mutex);
			return ret;
		}
		dw9786_mdelay(2);
		ret = I2C_BLOCK_WRITE_OIS(DW9786_CHIP_EN_ADDR, (unsigned char *)stdby, 34);    /* standby mode */
		if (ret < 0) {
			LOG_ERR("enter standby mode failed,");
			mutex_unlock(&dw9786_idle_mutex);
			return ret;
		}
		dw9786_mdelay(5);
	}

	I2C_READ_16BIT_OIS(DW9786_STATUS_ADDR, &mcu_active);
	LOG_INF("mcu_active: 0x%04X", mcu_active);
	if (mcu_active != 0x0001) {
		ret = I2C_WRITE_16BIT_OIS(DW9786_MCU_ACTIVE_ADDR, 0x0001);    /* idle mode */
		if (ret < 0) {
			LOG_ERR("enter Idle mode failed,");
			mutex_unlock(&dw9786_idle_mutex);
			return ret;
		}
		dw9786_mdelay(20);
	}

	mutex_unlock(&dw9786_idle_mutex);
	LOG_INF("reset complete");
	return 0;
}

void dw9786_chip_info(void)
{
	uint16_t data;
	uint32_t fw_version;

	I2C_READ_16BIT_OIS(DW9786_PRODUCT_ID_ADDR, &data);
	LOG_INF("product_id: 0x%04X", data);
	I2C_READ_16BIT_OIS(DW9786_CHIP_ID_ADDR, &data);
	LOG_INF("chip_id: 0x%04X", data);
	I2C_READ_32BIT_OIS(DW9786_FW_VERSION_ADDR, &fw_version);
	LOG_INF("firmware_version: 0x%08X", fw_version);
	I2C_READ_16BIT_OIS(DW9786_FW_DATE_ADDR, &data);
	LOG_INF("firmware_date: 0x%04X", data);

	return;
}

int dw9786_set_spi_work_mode(uint8_t mode)
{
	int ret = -1;

	if (mode == 0) {
		LOG_INF("set spi work mode to master");
		ret = I2C_WRITE_16BIT_OIS(DW9786_SPI_WORK_MODE_ADDR, 0);
	} else if (mode == 1) {
		LOG_INF("set spi work mode to intercept");
		ret = I2C_WRITE_16BIT_OIS(DW9786_SPI_WORK_MODE_ADDR, 1);
	}

	return ret;
}

int dw9786_set_imu_type(uint8_t type)
{
	return I2C_WRITE_16BIT_OIS(DW9786_IMU_TYPE_ADDR, type);
}

/***************************************************
* @brief wait and compare register value and reference value
* @param [i] reg register add to read
* @param [o] ref reference value
* @return 0: equal, others: unequal
****************************************************/
int dw9786_wait_check_register(uint16_t reg, uint16_t ref)
{
	int ret = -1;
	uint16_t data;

	for (int i = 0; i < DW9786_CHECK_LOOP_TIMES; i++) {
		I2C_READ_16BIT_OIS(reg, &data);
		if (data == ref) {
			ret = 0;
			break;
		} else {
			if (i > DW9786_CHECK_LOOP_TIMES - 2) {
				LOG_INF("check_register failed,: 0x%04X", data);
			}
		}
		mdelay(DW9786_CHECK_WAIT_TIME);
	}

	return ret;
}

/***************************************************
* @brief compare register value and reference value, retry 1 time
* @param [i] reg register add to read
* @param [o] ref reference value
* @return 0: equal, others: unequal
****************************************************/
int dw9786_check_register(uint16_t reg, uint16_t ref)
{
	int ret = -1;
	uint16_t data;

	for (int i = 0; i < 2; i++) {
		I2C_READ_16BIT_OIS(reg, &data);
		LOG_INF("dw9786_check_register 0x%x 0x%x", data, reg);
		if (data == ref) {
			ret = 0;
			break;
		}
		mdelay(10);
	}

	return ret;
}

/***************************************************
* @brief check register value, if not equal to ref then rewrite
* @param [i] reg register add to read
* @param [o] ref reference value
* @return 0: success, others: failed
****************************************************/
int dw9786_check_and_write(uint16_t reg, uint16_t ref, bool* rewrite_flag)
{
	int ret = -1;
	uint16_t data;

	ret = I2C_READ_16BIT_OIS(reg, &data);
	if (ret < 0) {
		LOG_ERR("read reg: %04X failed, ret: %d", reg, ret);
		return ret;
	}

	if (ref == data) {
		return 0;
	}

	LOG_INF("reg %04X value not equal to ref, rewrite, reg value: %04X, ref value: %04X", reg, data, ref);
	ret = I2C_WRITE_16BIT_OIS(reg, ref);
	if (ret < 0) {
		LOG_ERR("rewrite failed, ret: %d", ret);
		return ret;
	}
	*rewrite_flag = true;

	return 0;
}

uint32_t dw9786_calc_checksum(uint8_t *data, uint32_t length)
{
	uint32_t check_sum = 0xFFFFFFFF;

	for (int i = 0; i < length; i += 4) {
		check_sum += (unsigned int)(*(data + i + 3)) +
					 (unsigned int)(*(data + i + 2) << 8) +
					 (unsigned int)(*(data + i + 1) << 16) +
					 (unsigned int)(*(data + i + 0) << 24);
	}

	return ~check_sum;
}

int dw9786_module_store(void)
{
	LOG_INF("start module store....");
	I2C_WRITE_8BIT_FLASH(DW9786_USER_PROTECTION_ADDR, 0xEA);    /* user protection off */
	I2C_WRITE_8BIT_FLASH(DW9786_DUMMY_DATA_ADDR, 0x00);    /* user memory write dummy data */
	mdelay(10);    /* absolutely necessary */

	I2C_WRITE_16BIT_OIS(DW9786_MODE_CONTROL_ADDR, 0x000A);    /* store&erase mode */

	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0xA000) == 0) {
		I2C_WRITE_16BIT_OIS(DW9786_USER_PROTECTION_A_ADDR, 0xA23F);    /* user protection a off */
		I2C_WRITE_16BIT_OIS(DW9786_MEMORY_AREA_SELECT_ADDR, 0xA6A6);    /* seleect module memory */
		mdelay(1);
		I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0001);    /* execute store */
		mdelay(100);
	} else {
		LOG_ERR("module store execute failed,");
		return -1;
	}

	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0xA001) == 0) {
		LOG_INF("module store success");
	} else {
		LOG_ERR("module store failed,");
		return -1;
	}

	return 0;
}

int dw9786_set_store(void)
{
	LOG_INF("start set store....");
	I2C_WRITE_8BIT_FLASH(DW9786_USER_PROTECTION_ADDR, 0xEA);    /* user protection off */
	I2C_WRITE_8BIT_FLASH(DW9786_DUMMY_DATA_ADDR, 0x00);    /* user memory write dummy data */
	mdelay(10);    /* absolutely necessary */

	I2C_WRITE_16BIT_OIS(DW9786_MODE_CONTROL_ADDR, 0x000A);    /* store&erase mode */

	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0xA000) == 0) {
		I2C_WRITE_16BIT_OIS(DW9786_USER_PROTECTION_A_ADDR, 0xA23F);    /* user protection a off */
		I2C_WRITE_16BIT_OIS(DW9786_MEMORY_AREA_SELECT_ADDR, 0x5959);    /* seleect set memory */
		mdelay(1);
		I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0001);    /* execute store */
		mdelay(100);
	} else {
		LOG_ERR("set store execute failed,");
		return -1;
	}

	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0xA001) == 0) {
		LOG_INF("set store success");
	} else {
		LOG_ERR("set store failed,");
		return -1;
	}

	return 0;
}

int dw9786_module_erase(void)
{
	LOG_INF("start module erase....");

	I2C_WRITE_16BIT_OIS(DW9786_MODE_CONTROL_ADDR, 0x000B);    /* store&erase mode */

	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0xB000) == 0) {    /* read status */
		I2C_WRITE_16BIT_OIS(DW9786_USER_PROTECTION_A_ADDR, 0xA23F);    /* user protection a off */
		I2C_WRITE_16BIT_OIS(DW9786_MEMORY_AREA_SELECT_ADDR, 0xA6A6);    /* seleect module memory */
		mdelay(1);
		I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0001);    /* execute erase */
		mdelay(100);
	} else {
		LOG_ERR("module erase failed,");
		return -1;
	}

	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0xB001) == 0) {
		LOG_INF("module erase success");
	} else {
		LOG_ERR("module erase failed,");
		I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0000);    /* user protection on */
		return -1;
	}

	return 0;
}

int dw9786_set_erase(void)
{
	LOG_INF("module erase start....");

	I2C_WRITE_16BIT_OIS(DW9786_MODE_CONTROL_ADDR, 0x000B);    /* store&erase mode */

	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0xB000) == 0) {    /* read Status */
		I2C_WRITE_16BIT_OIS(DW9786_USER_PROTECTION_A_ADDR, 0xA23F);    /* user protection a off */
		I2C_WRITE_16BIT_OIS(DW9786_MEMORY_AREA_SELECT_ADDR, 0x5959);    /* seleect set memory */
		mdelay(1);
		I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0001);    /* execute erase */
		mdelay(100);
	} else {
		LOG_ERR("module erase failed,");
		return -1;
	}

	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0xB001) == 0) {
		LOG_INF("module erase success");
	} else {
		LOG_ERR("module erase failed,");
		I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0000);    /* user protection on */
		return -1;
	}

	return 0;
}

uint32_t dw9786_read_flash_checksum(void)
{
	uint16_t csh, csl;
	uint32_t checksum;

	I2C_READ_16BIT_OIS(0xED60, &csh);
	I2C_READ_16BIT_OIS(0xED64, &csl);
	checksum = ((uint32_t)(csh << 16)) | csl;
	LOG_INF("flash memory checksum: 0x%08X", checksum);

	return checksum;
}

uint32_t dw9786_checksum(int type)
{
	unsigned short csh, csl;
	unsigned int checksum;

	if (type == MEMORY_AREA_LUT) {
	    /* not used */
	} else if (type == MEMORY_AREA_CODE) {
		dw9786_memory_area_select(MEMORY_AREA_CODE);
		I2C_WRITE_16BIT_OIS(0xED48, MCS_START_ADDRESS);
		I2C_WRITE_16BIT_OIS(0xED4C, MCS_CHECKSUM_SIZE);
		I2C_WRITE_16BIT_OIS(0xED50, 0x0001);
	}

	dw9786_wait_check_register(0xED04, 0x00);
	I2C_READ_16BIT_OIS(0xED54, &csh);
	I2C_READ_16BIT_OIS(0xED58, &csl);
	checksum = ((unsigned int)(csh << 16)) | csl;
	LOG_INF("flash memory checksum: 0x%08X", checksum);

	return checksum;
}

void dw9786_memory_area_select(int type)
{
	I2C_WRITE_16BIT_OIS(DW9786_CHIP_EN_ADDR, 1);
	I2C_WRITE_16BIT_OIS(DW9786_MCU_ACTIVE_ADDR, 0);

	if (type == MEMORY_AREA_LUT) {
		I2C_WRITE_16BIT_OIS(0xEDBC, 0xDB01);
	}
	else if (type == MEMORY_AREA_CODE) {
		I2C_WRITE_16BIT_OIS(0xE2F8, 0xC0DE);
	}
	else if (type == MEMORY_AREA_USER) {
		I2C_WRITE_16BIT_OIS(0xEDB0, 0xA23F);
	}

	I2C_WRITE_16BIT_OIS(0xED00, 0x0000);
	mdelay(1);

	return;
}

int dw9786_gyro_offset_calibration(uint16_t *offset_x, uint16_t *offset_y)
{
	int ret = 0;
	uint16_t gyro_status;

	LOG_INF("start gyro offset cali....");

	I2C_WRITE_16BIT_OIS(DW9786_MODE_CONTROL_ADDR, 0x0006);    /* gyro offset calibration mode */

	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0x6000) == 0) {
		I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0001);    /* Lens ofs calibration execute command */
		mdelay(100);
	} else {
		LOG_INF("switch to calibration mode failed,");
		return -1;
	}

	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0x6001) == 0) {    /* when calibration is done, Status changes to 0x6001 */
		LOG_INF("calibration finish");
	} else {
		LOG_INF("calibration failed,");
		return -2;
	}

	I2C_READ_16BIT_OIS(0xB80C, offset_x);
	I2C_READ_16BIT_OIS(0xB80E, offset_y);
	I2C_READ_16BIT_OIS(0xB838, &gyro_status);
	LOG_INF("gyro offset x: 0x%04X(%d)", *offset_x, (int16_t)*offset_x);
	LOG_INF("gyro offset y: 0x%04X(%d)", *offset_y, (int16_t)*offset_y);
	LOG_INF("gyro offset calibration result status: 0x%04X", gyro_status);

	if((gyro_status & 0x8000) == 0x8000) {
		if (gyro_status & GYRO_OFFSET_X_CALI_PASS) {
			LOG_INF("gyro offset x calibration pass");
		}
		if (gyro_status & GYRO_OFFSET_X_CALI_ERR) {
			ret += GYRO_OFFSET_X_CALI_ERR;
			LOG_INF("gyro offset x calibration failed,");
		}
		if (gyro_status & GYRO_OFFSET_Y_CALI_PASS) {
			LOG_INF("gyro offset y calibration pass");
		}
		if (gyro_status & GYRO_OFFSET_Y_CALI_ERR) {
			ret += GYRO_OFFSET_Y_CALI_ERR;
			LOG_INF("gyro offset y calibration failed,");
		}
	} else {
		ret = -3;
		LOG_INF("calibration failed,");
		return ret;
	}

	if(ret == 0) {
		ret = dw9786_set_store();
		dw9786_device_reset();
	}

	return ret;
}

int dw9786_hall_calibration(void)
{
	int ret = 0;
	uint16_t cali_status;
	LOG_INF("start hall cali....");
	dw9786_device_reset();

	I2C_WRITE_16BIT_OIS(0xB026, 0x0003);    /* hall calibration mode */
	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0x3000) == 0) {
		I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0001);    /* x axis hall calibration execute command */
	} else {
		LOG_INF("switch to calibration mode failed,");
		return -1;
	}
	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0x3001) == 0) {    /* when calibration is done, Status changes to 3x6001 */
		LOG_INF("x axis hall calibration finish");
	} else {
		LOG_INF("x axis hall calibration failed,");
		return -2;
	}
	I2C_READ_16BIT_OIS(0xB700, &cali_status);
	LOG_INF("x axis hall calibration status:0x%04X", cali_status);

	I2C_WRITE_16BIT_OIS(0xB026, 0x0003);    /* hall calibration mode */
	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0x3000) == 0) {
		I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0003);    /* y axis hall calibration execute command */
	} else {
		LOG_INF("switch to calibration mode failed,");
		return -1;
	}
	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0x3003) == 0) {    /* when calibration is done, Status changes to 3x6001 */
		LOG_INF("y axis hall calibration finish");
	} else {
		LOG_INF("y axis hall calibration failed,");
		return -2;
	}
	I2C_READ_16BIT_OIS(0xB700, &cali_status);
	LOG_INF("x and y axis hall calibration status: 0x%04X", cali_status);

	I2C_WRITE_16BIT_OIS(0xB026, 0x0003);    /* hall calibration mode */
	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0x3000) == 0) {
		I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0005);    /* z axis hall calibration execute command */
	} else {
		LOG_INF("switch to calibration mode failed,");
		return -1;
	}
	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0x3005) == 0) {    /* when calibration is done, Status changes to 3x6001 */
		LOG_INF("z axis hall calibration finish");
	} else {
		LOG_INF("z axis hall calibration failed,");
		return -2;
	}
	I2C_READ_16BIT_OIS(0xB700, &cali_status);
	LOG_INF("z axis hall calibration status: 0x%04X", cali_status);

	dw9786_set_store();
	dw9786_module_store();
	dw9786_device_reset();

	return ret;
}

int dw9786_servo_gain_calibration(void)
{
	int ret = 0;
	uint16_t cali_status;
	LOG_INF("start servo gain cali....");
	dw9786_device_reset();

	I2C_WRITE_16BIT_OIS(0xB026, 0x0004);    /* servo gain calibration mode */
	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0x4000) == 0) {
		I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0001);    /* servo gain calibration execute command */
	} else {
		LOG_INF("switch to calibration mode failed,");
		return -1;
	}
	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0x4001) == 0) {    /* when calibration is done, Status changes to 0x4001 */
		LOG_INF("servo gain calibration finish");
	} else {
		LOG_INF("servo gain calibration failed,");
		return -2;
	}
	I2C_READ_16BIT_OIS(0xB702, &cali_status);
	LOG_INF("servo gain calibration status:0x%04X", cali_status);

	dw9786_set_store();
	dw9786_module_store();
	dw9786_device_reset();

	return ret;
}

int dw9786_lens_offset_calibration(void)
{
	int ret = 0;
	uint16_t cali_status;
	LOG_INF("start lens offset cali....");
	dw9786_device_reset();

	I2C_WRITE_16BIT_OIS(0xB026, 0x0005);    /* lens offset calibration mode */
	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0x5000) == 0) {
		I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0001);    /* lens offset calibration execute command */
	} else {
		LOG_INF("switch to calibration mode failed,");
		return -1;
	}
	if (dw9786_wait_check_register(DW9786_STATUS_ADDR, 0x5001) == 0) {    /* when calibration is done, Status changes to 0x4001 */
		LOG_INF("lens offset calibration finish");
	} else {
		LOG_INF("lens offset calibration failed,");
		return -2;
	}

	I2C_READ_16BIT_OIS(0xB706, &cali_status);
	LOG_INF("lens offset calibration status:0x%04X", cali_status);

	dw9786_set_store();
	dw9786_module_store();
	dw9786_device_reset();

	return ret;
}

int dw9786_update_fw(void)
{
	uint16_t project_info = 0, fw_version = 0, release_date = 0;
	uint32_t checksum;

	I2C_READ_16BIT_OIS(DW9786_RELEASE_VERSION_H_ADDR, &project_info);
	I2C_READ_16BIT_OIS(DW9786_RELEASE_VERSION_L_ADDR, &fw_version);
	I2C_READ_16BIT_OIS(DW9786_RELEASE_DATE_ADDR, &release_date);
	checksum = dw9786_read_flash_checksum();
	LOG_INF("module project info: 0x%04X, fw_version: 0x%04X, release date: 0x%04X, checksum: %04X",
	        project_info, fw_version, release_date, checksum);
	LOG_INF("firmware project info: 0x%04X, fw_version: 0x%04X, release date: 0x%04X, checksum: %04X",
	        DW9786_PROJECT_VERSION, DW9786_FW_VERSION, DW9786_RELEASE_DATE, DW9786_FW_CHECKSUM);

	if ((project_info == DW9786_PROJECT_VERSION) && (fw_version == DW9786_FW_VERSION) &&
	    (release_date == DW9786_RELEASE_DATE) && (checksum == DW9786_FW_CHECKSUM)) {
		LOG_INF("firmware is the latest version, no firmware update required");
		return 0;
	}

	LOG_INF("firmware update required, start download firmware...");
	int addr = MCS_START_ADDRESS;
	dw9786_memory_area_select(MEMORY_AREA_CODE);
	/* firmware memory erase */
	for (uint32_t i = 0; i < DW9786_FMC_PAGE_NUM; ++i) {
		I2C_WRITE_16BIT_OIS(0xED08, addr);    /* set erase address */
		I2C_WRITE_16BIT_OIS(0xED0C, 0x0002);    /* sector erase(2KB) */
		addr += 0x800;
		mdelay(5);
	}
	/* flash firmware */
	I2C_WRITE_16BIT_OIS(0xED28, MCS_START_ADDRESS);
	for (uint32_t i = 0; i < DW9786_FW_SIZE; i += DW9786_FW_PACKET_SIZE) {
		I2C_BLOCK_WRITE_OIS(0xED2C, (unsigned char*)(dw9786_fw_data + i), DW9786_FW_PACKET_SIZE);
	}

	uint32_t checksum_flash = dw9786_checksum(MEMORY_AREA_CODE);
	if (checksum_flash != DW9786_FW_CHECKSUM) {
		LOG_INF("flash checksum failed,!, firmware checksum: 0x%08X, flash checksum: 0x%08X", DW9786_FW_CHECKSUM, checksum_flash);
		LOG_INF("firmware download failed,");
		dw9786_chip_enable(false);
		return -1;
	}
	LOG_INF("flash checksum passed!, checksum: 0x%08X", checksum_flash);
	LOG_INF("firmware download success");

	dw9786_device_reset();

	return 0;
}

int dw9786_servo_on(void)
{
	I2C_WRITE_16BIT_OIS(DW9786_MODE_CONTROL_ADDR, 0x0001);
	mdelay(1);
	I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0002);
	mdelay(1);
	I2C_WRITE_16BIT_OIS(DW9786_AF_SERVO_ON_CONTROL, 0x0002);
	mdelay(1);

	if(dw9786_wait_check_register(DW9786_STATUS_ADDR, 0x1022) == 0) {
		LOG_INF("servo on success");
		return 0;
	}
	LOG_INF("servo on failed,");

	return -1;
}

int dw9786_servo_off(void)
{
	I2C_WRITE_16BIT_OIS(DW9786_MODE_CONTROL_ADDR, 0x0001);
	mdelay(1);
	I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0000);
	mdelay(1);
	I2C_WRITE_16BIT_OIS(DW9786_AF_SERVO_ON_CONTROL, 0x0000);
	mdelay(1);

	if(dw9786_wait_check_register(DW9786_STATUS_ADDR, 0x1000) == 0) {
		LOG_INF("servo off success");
		return 0;
	}
	LOG_INF("servo off failed,");

	return -1;
}

int dw9786_ois_on_thread_func(void* data)
{
	uint16_t reg_value;
	uint16_t ois_mode = 0;
	mutex_lock(&dw9786_idle_mutex);
	I2C_READ_16BIT_OIS(DW9786_MCU_ACTIVE_ADDR, &reg_value);
	LOG_INF("mcu active: 0x%04X", reg_value);
	I2C_READ_16BIT_OIS(DW9786_STATUS_ADDR, &reg_value);
	LOG_INF("ois status: 0x%04X", reg_value);
	LOG_INF("dw9786_idle_mutex: %p", &dw9786_idle_mutex);
	I2C_READ_16BIT_OIS(DW9786_OIS_OP_MODE_ADDR, &ois_mode);
	LOG_INF("ois mode: 0x%04X", ois_mode);

	if (reg_value == 0x1021 && ois_mode == 0x8000) {
		LOG_INF("ois has been enabled, return 0");
		mutex_unlock(&dw9786_idle_mutex);
		return 0;
	}

	I2C_WRITE_16BIT_OIS(DW9786_MODE_CONTROL_ADDR, 0x0001);
	mdelay(1);
	I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0002);
	mdelay(1);
	I2C_WRITE_16BIT_OIS(DW9786_AF_SERVO_ON_CONTROL, 0x0002);
	mdelay(1);
	I2C_WRITE_16BIT_OIS(DW9786_OIS_OP_MODE_ADDR, 0x8000);
	mdelay(1);
	I2C_WRITE_16BIT_OIS(0xB96E, 0x1902);    /* set eis data sample rate */
	mdelay(1);
	I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0001);
	mdelay(1);
	mutex_unlock(&dw9786_idle_mutex);

	if(dw9786_check_register(DW9786_STATUS_ADDR, 0x1021) == 0) {
		LOG_INF("ois on success");
		return 0;
	}
	LOG_INF("ois on failed,");

	return -1;
}

int dw9786_ois_on(void)
{
	int err;

	LOG_ERR("dw9786_ois_on thread initalizing...\n");
	struct task_struct *ois_on_task = kthread_create(dw9786_ois_on_thread_func, NULL, "ois_on_kthread");
	if (IS_ERR(ois_on_task)) {
		LOG_ERR("unable to start dw9786_ois_on thread/n");
		err = PTR_ERR(ois_on_task);
		ois_on_task = NULL;
		dw9786_ois_on_thread_func(NULL);
		return 0;
	}
	wake_up_process(ois_on_task);
	return 0;
}

int dw9786_movie_on_thread_func(void* data)
{
	uint16_t ois_status = 0;
	uint16_t ois_mode = 0;
	mutex_lock(&dw9786_idle_mutex);
	I2C_READ_16BIT_OIS(DW9786_STATUS_ADDR, &ois_status);
	LOG_INF("ois status: 0x%04X", ois_status);
	I2C_READ_16BIT_OIS(DW9786_OIS_OP_MODE_ADDR, &ois_mode);
	LOG_INF("ois mode: 0x%04X", ois_mode);

	if (ois_status == 0x1021 && ois_mode == 0x8001) {
		LOG_INF("ois has been enabled, return 0");
		mutex_unlock(&dw9786_idle_mutex);
		return 0;
	}

	I2C_WRITE_16BIT_OIS(DW9786_MODE_CONTROL_ADDR, 0x0001);
	mdelay(1);
	I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0002);
	mdelay(1);
	I2C_WRITE_16BIT_OIS(DW9786_AF_SERVO_ON_CONTROL, 0x0002);
	mdelay(1);
	I2C_WRITE_16BIT_OIS(DW9786_OIS_OP_MODE_ADDR, 0x8001);
	mdelay(1);
	I2C_WRITE_16BIT_OIS(0xB96E, 0x1901);    /* set eis data sample rate */
	mdelay(1);
	I2C_WRITE_16BIT_OIS(DW9786_ACTIVE_CONTROL_ADDR, 0x0001);
	mdelay(1);

	if(dw9786_check_register(DW9786_STATUS_ADDR, 0x1021) == 0) {
		LOG_INF("movie mode on success");
		mutex_unlock(&dw9786_idle_mutex);
		return 0;
	}
	LOG_ERR("movie mode on failed");
	mutex_unlock(&dw9786_idle_mutex);

	return -1;
}

int dw9786_movie_mode_on(void)
{
	int err;

	struct task_struct *movie_on_task = kthread_create(dw9786_movie_on_thread_func, NULL, "movie_on_kthread");
	if (IS_ERR(movie_on_task)) {
		LOG_ERR("unable to start dw9786_movie_on thread/n");
		err = PTR_ERR(movie_on_task);
		movie_on_task = NULL;
		dw9786_movie_on_thread_func(NULL);
		return 0;
	}
	wake_up_process(movie_on_task);
	return 0;
}

int dw9786_sleep_mode(void)
{
	int ret = -1;
	int ret1 = -1;

	mutex_lock(&dw9786_idle_mutex);
	ret = dw9786_mcu_active(false);
	ret1 = dw9786_chip_enable(false);
	mutex_unlock(&dw9786_idle_mutex);
	if (ret < 0 || ret1 < 0) {
		LOG_INF("dw9786_sleep_mode failed %d ,%d", ret, ret1);
		return -1;
	}

	return 0;
}

int dw9786_idle_mode_thread_func(void *arg)
{
	int ret = -1;
	int ret1 = -1;

	mutex_lock(&dw9786_idle_mutex);
	LOG_INF("dw9786_idle_mutex: %p", &dw9786_idle_mutex);
	ret = dw9786_chip_enable(true);
	mdelay(5);
	ret1 = dw9786_mcu_active(true);
	mdelay(20);
	mutex_unlock(&dw9786_idle_mutex);
	if (ret < 0 || ret1 < 0) {
		return -1;
	}

	LOG_ERR("idle_mode_thread return...");

	return 0;
}

int dw9786_idle_mode(void)
{
	int err;
	int ret = -1;
	uint16_t mcu_active = 0;

	ret = I2C_READ_16BIT_OIS(DW9786_MCU_ACTIVE_ADDR, &mcu_active);
	LOG_INF("mcu_active: 0x%04X", mcu_active);
	if (mcu_active == 0x0001) {
		LOG_INF("have enter idle mode success");
		return 0;
	}

	LOG_ERR("dw9786_idle_mode thread initalizing...\n");
	struct task_struct *idle_mode_task = kthread_create(dw9786_idle_mode_thread_func, NULL, "idle_mode_kthread");
	if (IS_ERR(idle_mode_task)) {
		LOG_ERR("unable to start dw9786_idle_mode thread/n");
		err = PTR_ERR(idle_mode_task);
		idle_mode_task = NULL;
		dw9786_idle_mode_thread_func(NULL);
		return err;
	}
	wake_up_process(idle_mode_task);
	return 0;
}

int dw9786_read_firmware(void)
{
	uint16_t *buffer = kmalloc(40960, GFP_KERNEL);
	if (buffer == NULL) {
		LOG_ERR("malloc buffer failed");
		return -1;
	}

	I2C_WRITE_16BIT_OIS(DW9786_CHIP_EN_ADDR, 0x0000); /* shutdown mode */
	mdelay(2);
	I2C_WRITE_16BIT_OIS(DW9786_CHIP_EN_ADDR, 0x0001); /* standby mode */
	mdelay(5);
	I2C_WRITE_16BIT_OIS(0xE2F8, 0xC0DE);   /* code protection off */
	mdelay(1);
	I2C_WRITE_16BIT_OIS(0xED00, 0x0000);   /* select mcs */
	mdelay(1);

	for (uint16_t i = 0; i < DW9786_FW_SIZE; i += DW9786_FW_PACKET_SIZE) {
		I2C_WRITE_16BIT_OIS(0xED28, MCS_START_ADDRESS + i);
		I2C_BLOCK_READ_OIS(MCS_START_ADDRESS + i, (uint8_t*)buffer + i, DW9786_FW_PACKET_SIZE);
	}

	for (int i = 0; i < 40960; ++i) {
		LOG_INF("addr: %d< 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, ", i,
				buffer[i + 0], buffer[i + 1], buffer[i + 2], buffer[i + 3], buffer[i + 4], buffer[i + 5], buffer[i + 6], buffer[i + 7],
				buffer[i + 8], buffer[i + 9], buffer[i + 10], buffer[i + 11], buffer[i + 12], buffer[i + 13], buffer[i + 14], buffer[i + 15]);
	}

	if (buffer) {
		kfree(buffer);
	}

	return 0;
}

