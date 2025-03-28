#ifndef __OIS_DEF_H__
#define __OIS_DEF_H__

typedef struct {
	//for sensor and instance share device index
	int32_t gyro_id; //update while probe success
	int32_t mois_id; //update while probe success

	//the follow paramters for calibration
	int32_t mois_gain_x;
	int32_t mois_gain_y;
	int32_t mois_delay_x;
	int32_t mois_delay_y;
	int32_t mois_offset_x;
	int32_t mois_offset_y;
	int32_t work_frequency; //MOIS control frequency
	int32_t report_frequency; //MOIS hall offset data and gyro data report to client frequency less than work frequency

	//for work controls
	int16_t focus_distance; //af feedback to mois algorithm
	uint8_t ids;
	uint8_t mode; //mois work mode
	uint8_t orientation; //cell phone posture

	uint8_t pantilt;
	uint8_t anglelimit;
	//for alignment
	//coord_t gyro;
	//coord_t camera;
} mois_config_data;

enum {
	AK_EnableMOIS, // driver ON  (unimplemented)
	AK_DisableMOIS, // driver OFF (unimplemented)
	AK_Movie, // Movie mode
	AK_Still, // Still mode
	AK_EnterDownLoadMode, // FW DL      (unimplemented)
	AK_CenteringOn, // SOIS OFF (lens centering)
	AK_CenteringOff, //            (unimplemented)
	AK_Pantilt, //            (unimplemented)
	AK_Scene, //            (unimplemented)
	AK_SceneFilterOn, //            (unimplemented)
	AK_SceneFIlterOff, //            (unimplemented)
	AK_SceneRangeOn, //            (unimplemented)
	AK_SceneRangeOff, //            (unimplemented)
	AK_ManualMovieLens, // scene manual mode (unimplemented)
	AK_TestMode, // scene test mode   (unimplemented)
	AK_WorkingMode,
	AK_StandbyMode,
	AK_SleepMode, // sleep mode, Low power consumption
	AK_ModeEnd // enum end
};

enum {
	MOIS_Private = AK_ModeEnd,
	MOIS_Gyro_Gain_Cal,
	AK_Centering,
};

struct i2c_ops_info {
	char name[30];
	unsigned short RegAddr;
	unsigned int RegData;
};

enum DBG_ARG_IDX {
	DBG_ARG_IDX_I2C_ADDR,
	DBG_ARG_IDX_I2C_DATA,
	DBG_ARG_IDX_MAX_NUM,
};

#endif // __OIS_BU24721_DEF_H__