/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_cam.h"

#define OTP_SIZE 2340
extern void custom_eeprom_read(bool *is_valid, void *d_data);
static bool do_set_otp_once;
static unsigned char ois_otp[OTP_SIZE];


/*
 * mtk's part: start
 */
#define UTL(a)      (((a) > 0   ) ? (a) : -(a))
static unsigned int enable_oiscomp_log;
module_param(enable_oiscomp_log, uint, 0644);
MODULE_PARM_DESC(enable_oiscomp_log, "enable_oiscomp_log");

static unsigned int enforce_oiscomp;
module_param(enforce_oiscomp, uint, 0644);
MODULE_PARM_DESC(enforce_oiscomp, "enforce_oiscomp");

static unsigned int enforce_oiscode = 2048;
module_param(enforce_oiscode, uint, 0644);
MODULE_PARM_DESC(enforce_oiscode, "enforce_oiscode");

static int gReserved = 1;
static unsigned int gBUF0[1156];
static int gBUF1[128];
static int gBUF2[128];
static int gBUF3[16384];
static int gBUF4[289];
static int gBUF5[289];
static int gBUF6[289];
static int gBUF7[289];
static int gBUF8[289];
static int gBUF9[289];
static int gBUF10[27744];
static int gBUF11[13872];
static int gBUF12[585];

struct MTK_TSF_OIS_COMP_STRUCT1 {
	int para1;
	int para2;
	int para3;
	int para4;
	int para5;
	int para6;
	int para5_1;
	int para6_1;
	int para5_2;
	int para6_2;
};

struct MTK_TSF_OIS_COMP_STRUCT2 {
	/*Only Update Once*/
	struct MTK_TSF_OIS_COMP_STRUCT1 tPARA;
	int *pOIS_TBL;
	int OIS_READOUT_TIME;

	/*Perframe Update*/
	int OIS_VECT_SIZE;
	int *pOIS_POSITION_X;
	int *pOIS_POSITION_Y;

	int OIS_EXPOSURE_TIME;
	int OIS_SOF_TIME;
	int OIS_NDD_ID;
	unsigned int *SHADING_TABLE;

};

struct MTK_TSF_OIS_COMP_STRUCT3 {
	int                array1[12];
	int                array2[12];
	int                array3[12];
	int                array4[12];
};

struct MTK_TSF_OIS_COMP_STRUCT4 {
	int *p_para1;
	int *p_para2;
	int *p_para3;
	int *p_para4;
	int *p_para5;
	int *p_para6;
	int *p_para7;
	int *p_para8;
	int *p_para9;
};

struct MTK_TSF_OIS_COMP_STRUCT5 {
	int para1;
	int para2;

	int *p_para1;
	int *p_para2;
	unsigned int array1[17];

};

int OISComp_Fun1_1(int a, int wrap)
{
	int tmp;

	tmp = (a >= wrap) ? (wrap - a) : (a);
	return tmp;
}

void OISComp_Fun1(struct MTK_TSF_OIS_COMP_STRUCT1 *ptInStruct, int *pInBuf1, int *pInBuf2)
{
	if(enable_oiscomp_log)
		pr_info("%s\n", __func__);
	int                var1_1, var1_2;
	int                var2_1, var2_2, var2_3, var2_4, var2_5, var2_6, var2_7, var2_8,
		var2_9, var2_10, var2_11, var2_12;
	int                var3_1, var3_2, var3_3;

	struct MTK_TSF_OIS_COMP_STRUCT3    *ptStruct, *p_ptStruct;

	ptStruct = (struct MTK_TSF_OIS_COMP_STRUCT3 *)gBUF11;

	int *p_array[4];
	const int *p_pInBuf1 = pInBuf1;

	p_ptStruct = ptStruct;

	int para3 = ptInStruct->para3 - 1;
	int para4 = ptInStruct->para4 - 1;
	int product = ptInStruct->para3 * ptInStruct->para4;

	for (int idx1 = para4; idx1 != 0; idx1--) {
		for (int idx2 = para3; idx2 != 0; idx2--) {
			p_array[0] = p_ptStruct->array4;
			p_array[1] = p_ptStruct->array3;
			p_array[2] = p_ptStruct->array2;
			p_array[3] = p_ptStruct->array1;
			p_ptStruct++;

			for (int m = 0; m < 4; m++) {
				for (int i = 0; i < 4; i++) {
					long long int temp = 0;
					long long int temp1 = *(p_pInBuf1++);
					long long int temp2 = *(p_pInBuf1++);

					temp = (temp2 << 32) | (temp1 & 0xFFFFFFFF);
					*(p_array[m]++) = temp & 0xFFFFF;
					*(p_array[m]++) = (temp >> 20) & 0xFFFFF;
					*(p_array[m]++) = (temp >> 40) & 0xFFFFF;
				}
			}
		}
	}

	int *p_pInBuf2 = pInBuf2;

	p_ptStruct = ptStruct;
	for (int idx1 = 0; idx1 <= para4; idx1++) {

		var3_2 = 0;
		if (idx1 == para4)
			var3_2 = 1;

		var1_2 = idx1;
		if (idx1 == para4)
			var1_2 = para4 - 1;

		for (int idx2 = 0; idx2 <= para3; idx2++) {

			var1_1 = idx2;
			if (idx2 == para3)
				var1_1 = para3 - 1;

			var3_1 = 0;
			if (idx2 == para3)
				var3_1 = 1;

			var3_3 = var3_1 * var3_2;
			p_ptStruct = ptStruct + var1_2 * para3 + var1_1;
			p_array[0] = p_ptStruct->array4;
			p_array[1] = p_ptStruct->array3;
			p_array[2] = p_ptStruct->array2;
			p_array[3] = p_ptStruct->array1;

			for (int k = 0; k < 4; k++) {
				var2_1 = OISComp_Fun1_1(p_array[k][0], 524288);
				var2_2 = OISComp_Fun1_1(p_array[k][1], 524288);
				var2_3 = OISComp_Fun1_1(p_array[k][2], 524288);
				var2_4 = OISComp_Fun1_1(p_array[k][3], 524288);
				var2_5 = OISComp_Fun1_1(p_array[k][4], 524288);
				var2_6 = OISComp_Fun1_1(p_array[k][5], 524288);
				var2_7 = OISComp_Fun1_1(p_array[k][6], 524288);
				var2_8 = OISComp_Fun1_1(p_array[k][7], 524288);
				var2_9 = OISComp_Fun1_1(p_array[k][8], 524288);
				var2_10 = OISComp_Fun1_1(p_array[k][9], 524288);
				var2_11 = OISComp_Fun1_1(p_array[k][10], 524288);
				var2_12 = OISComp_Fun1_1(p_array[k][11], 524288);

				int var2 = var2_12 +
					(var2_10 + var2_7 + var2_3) * var3_1 +
					(var2_11 + var2_9 + var2_6) * var3_2 +
					(var2_8 + var2_4 + var2_5 + var2_1 + var2_2) * var3_3;

				*(p_pInBuf2 + k * product) = ((var2 + 2) >> 2);
			}
			p_pInBuf2++;
		}
	}
}

void OISComp_Fun2_3(struct MTK_TSF_OIS_COMP_STRUCT1 *ptInStruct, int *pInBuf, unsigned int *pOtBuf)
{
	if(enable_oiscomp_log)
		pr_info("%s\n", __func__);
	int idx1, idx2;
	int value1;
	int para3 = ptInStruct->para3 - 1;
	int para4 = ptInStruct->para4 - 1;
	int product = para3 * para4;
	long long int val1, val2, val3;
	int *p_i;
	int ch[4] = { 0,1,2,3 };


	for (idx2 = 0; idx2 < 4; idx2++) {
		value1 = ch[idx2];
		p_i = pInBuf + value1 * product * 12;
		for (idx1 = 0; idx1 < product; idx1++) {
			int var = (idx1 * 32) + (value1 * 8);

			for (int k = 0; k < 4; k++) {
				val1 = *(p_i++);
				val2 = *(p_i++);
				val3 = *(p_i++);

				long long int temp = (val3 << 40) | (val2 << 20) | val1;

				pOtBuf[var++] = temp & 0xFFFFFFFF;
				pOtBuf[var++] = temp >> 32;
			}
		}
	}
}

int OISComp_Fun2_2_3_1(int InVar1, unsigned int InPara1)
{
	int para1 = 1 << (InPara1 - 1);
	int y;

	y = (InVar1 >= 0 ? InVar1 : -InVar1);

	y += para1;

	y >>= InPara1;

	if (InVar1 < 0)
		y = -y;

	return y;
}

void OISComp_Fun2_2_3(int *pInBuf0, int *pInBuf1, int *pInBuf2, int *pInBuf3, int *pInBuf4,
	int *pInBuf5, int InPara1, int InPara2, int InPara3, int InPara4)
{
	int        var1, var2, var3, var4, var6, var7, var8, var9, var10,
		var11, var12, var13, var14, var15, var16, var17;
	int        a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16;
	int		*p_pInBuf = pInBuf5;

	var14 = InPara1 * InPara4 + InPara2;
	var16 = var14 + 1;
	var15 = var14 + InPara4;
	var17 = var14 + InPara4 + 1;

	var1 = pInBuf0[var14] << 8;
	var2 = pInBuf0[var16] << 8;
	var3 = pInBuf0[var15] << 8;
	var4 = pInBuf0[var17] << 8;

	var6 = pInBuf1[var14];
	var7 = pInBuf2[var14];
	var8 = pInBuf1[var15];
	var9 = pInBuf2[var15];
	var10 = pInBuf3[var14];
	var11 = pInBuf3[var16];
	var12 = pInBuf4[var14];
	var13 = pInBuf4[var16];

	a3 = (var7 - var6);
	a7 = var6 + var6 + var6;
	a10 = var2 - var1 - var6 - var6 - var7;

	a6 = (var12 - var10);
	a9 = var10 + var10 + var10;
	a11 = var3 - var1 - var10 - var10 - var12;

	a12 = (var9 - var8);
	a13 = var8 + var8 + var8;
	a14 = var4 - var3 - var8 - var8 - var9;

	a15 = (var13 - var11);
	a16 = var11 + var11 + var11;

	a1 = a12 - a3;
	a2 = a15 - a6;
	a4 = a13 - a7;
	a5 = a16 - a9;
	a8 = a14 - a10 - a2 - a5;

	*p_pInBuf = OISComp_Fun2_2_3_1(a1, 6);    p_pInBuf++;
	*p_pInBuf = OISComp_Fun2_2_3_1(a2, 6);    p_pInBuf++;
	*p_pInBuf = OISComp_Fun2_2_3_1(a3, 6);    p_pInBuf++;
	*p_pInBuf = OISComp_Fun2_2_3_1(a4, 6);    p_pInBuf++;
	*p_pInBuf = OISComp_Fun2_2_3_1(a5, 6);    p_pInBuf++;
	*p_pInBuf = OISComp_Fun2_2_3_1(a6, 6);    p_pInBuf++;
	*p_pInBuf = OISComp_Fun2_2_3_1(a7, 6);    p_pInBuf++;
	*p_pInBuf = OISComp_Fun2_2_3_1(a8, 6);    p_pInBuf++;
	*p_pInBuf = OISComp_Fun2_2_3_1(a9, 6);    p_pInBuf++;
	*p_pInBuf = OISComp_Fun2_2_3_1(a10, 6);    p_pInBuf++;
	*p_pInBuf = OISComp_Fun2_2_3_1(a11, 6);    p_pInBuf++;
	*p_pInBuf = var1 >> (6 - 1);
	*p_pInBuf += 1;
	*p_pInBuf >>= 1;
}

void OISComp_Fun2_2_2_1(int *pInBuf1, int *pInBuf2, int *pInBuf3, int *pInBuf4,
	int InPara1, int InPara2)
{

	int idx1, idx2, idx3;
	int var1, var2, var3, var4, var5, var6;
	int para1 = 96;

	var1 = pInBuf2[2] - pInBuf2[1];
	var4 = var1 * var1;

	var3 = pInBuf2[1] - pInBuf2[0];
	var6 = var3 * var3;


	var2 = pInBuf2[InPara1 - 1] - pInBuf2[InPara1 - 2];
	var5 = var2 * var2;

	for (idx2 = 0; idx2 < InPara2; idx2++) {
		idx3 = idx2 * InPara1;
		pInBuf3[idx3] = (int)(var6*pInBuf1[idx3] / para1);
		idx3++;

		for (idx1 = 1; idx1 < InPara1 - 2; idx1++) {
			pInBuf3[idx3] = (int)(var4*pInBuf1[idx3] / para1);
			pInBuf4[idx3 - 1] = pInBuf3[idx3];
			idx3++;
		}
		pInBuf3[idx3] = (int)(var5*pInBuf1[idx3] / para1);
		pInBuf4[idx3 - 1] = (int)(var4*pInBuf1[idx3] / para1);
		pInBuf4[idx3] = 0;

	}
}

void OISComp_Fun2_2_2_2(int *pInBuf1, int *pInBuf2, int *pInBuf3, int *pInBuf4,
	int InPara1, int InPara2)
{
	int idx1, idx2, idx3;
	int var1, var2, var3, var4;
	int para1 = 96;

	var1 = pInBuf2[1] - pInBuf2[0];

	var3 = var1 * var1;

	var2 = pInBuf2[InPara2 - 1] - pInBuf2[InPara2 - 2];

	var4 = var2 * var2;

	for (idx1 = 0; idx1 < InPara1; idx1++)
		pInBuf3[idx1] = (int)(var3*pInBuf1[idx1] / para1);

	for (idx2 = 1; idx2 < InPara2 - 2; idx2++) {
		idx3 = idx2 * InPara1;
		for (idx1 = 0; idx1 < InPara1; idx1++) {
			pInBuf3[idx3] = (int)(var3*pInBuf1[idx3] / para1);
			pInBuf4[idx3 - InPara1] = pInBuf3[idx3];
			idx3++;
		}

	}

	idx3 = (InPara2 - 2)*InPara1;
	for (idx1 = 0; idx1 < InPara1; idx1++) {
		pInBuf3[idx3] = (int)(var4*pInBuf1[idx3] / para1);
		pInBuf4[idx3 - InPara1] = (int)(var3*pInBuf1[idx3] / para1);
		pInBuf4[idx3] = 0;
		idx3++;
	}
}

void OISComp_Fun2_2_1(int *pInBuf1, int *pInBuf2, int InPara1, int InPara2, int InPara3, int *pOtBuf)
{
	int idx1, idx2;
	int *p_array;
	int array1[129];

	p_array = array1;
	pOtBuf[0] = p_array[0] = 0;

	long long int var1 = 1 << 12;

	for (idx1 = 1; idx1 < InPara1 - 1; idx1++) {
		long long int tmp = var1 / (pInBuf1[idx1 + 1] - pInBuf1[idx1 - 1]);
		long long int tmp1 = (pInBuf1[idx1] - pInBuf1[idx1 - 1])*tmp;
		long long int tmp2 = var1 * var1 / (tmp1*pOtBuf[idx1 - 1] / var1 + 2 * var1);

		pOtBuf[idx1] = ((tmp1 - var1)*tmp2) / var1;
		p_array[idx1] = ((pInBuf2[idx1 + 1] - pInBuf2[idx1])*var1 / (pInBuf1[idx1 + 1] - pInBuf1[idx1]))
			- ((pInBuf2[idx1] - pInBuf2[idx1 - 1])*var1 / (pInBuf1[idx1] - pInBuf1[idx1 - 1]));

		p_array[idx1] = (((6 * p_array[idx1] * (tmp)-tmp1 * p_array[idx1 - 1])*tmp2) / var1) / var1;
	}

	pOtBuf[InPara1 - 1] = 0;

	for (idx2 = InPara1 - 2; idx2 >= 0; idx2--)
		pOtBuf[idx2] = pOtBuf[idx2] * pOtBuf[idx2 + 1] / var1 + p_array[idx2];
}

void OISComp_Fun2_2_1_1(int *pInBuf1, int *pInBuf2, int InPara1, int InPara2, int *pOtBuf)
{
	int j;
	int *p_pInBuf;
	int *p_pOtBuf;

	for (j = 0; j < InPara1; j++) {
		p_pInBuf = pInBuf2 + j * InPara2;
		p_pOtBuf = pOtBuf + j * InPara2;
		OISComp_Fun2_2_1(pInBuf1, p_pInBuf, InPara2, 1000, 1000, p_pOtBuf);
	}
}

void OISComp_Fun2_2_1_2(int *pInBuf1, int *pInBuf2, int InPara1, int InPara2, int *pOtBuf)
{
	int i, j;
	int *p_array1;
	int *p_array2;
	int    array1[129];
	int    array2[129];

	for (i = 0; i < InPara2; i++) {
		p_array1 = array1;
		p_array2 = array2;

		for (j = 0; j < InPara1; j++)
			*p_array1++ = pInBuf2[j*InPara2 + i];

		OISComp_Fun2_2_1(pInBuf1, array1, InPara1, 1000, 1000, array2);

		for (j = 0; j < InPara1; j++)
			pOtBuf[j*InPara2 + i] = *p_array2++;
	}
}

int OISComp_Fun2_2(struct MTK_TSF_OIS_COMP_STRUCT1 *ptInStruct1, struct MTK_TSF_OIS_COMP_STRUCT4 *ptInStruct2,
	int *ptInBuf)
{
	if(enable_oiscomp_log)
		pr_info("%s\n", __func__);
	bool flag = false;
	int idx1, idx2, idx3;
	int value1, value2, value3, value4;
	int c[4] = { 0,1,2,3 };

	int para4, para3, para3_, para4_;
	int var1, var2;
	int *p_Buf1;
	int *p_array;
	int *p_ptInBuf;

	para3 = ptInStruct1->para3;
	para4 = ptInStruct1->para4;
	var1 = para3 * para4;

	para3_ = ptInStruct1->para3 - 1;
	para4_ = ptInStruct1->para4 - 1;
	var2 = para3_ * para4_;

	int *p_para4 = ptInStruct2->p_para4;
	int *p_para5 = ptInStruct2->p_para5;

	int array12[12];

	int        *p_para1 = ptInStruct2->p_para1;
	int        *p_para2 = ptInStruct2->p_para2;
	int        *p_para6 = ptInStruct2->p_para6;
	int        *p_para7 = ptInStruct2->p_para7;
	int        *p_para8 = ptInStruct2->p_para8;
	int        *p_para9 = ptInStruct2->p_para9;

	for (idx3 = 0; idx3 < 4; idx3++) {
		value1 = c[idx3];

		p_Buf1 = ptInStruct2->p_para3 + value1 * var1;
		OISComp_Fun2_2_1_1(p_para2, p_Buf1, para4, para3, p_para4);
		OISComp_Fun2_2_1_2(p_para1, p_Buf1, para4, para3, p_para5);

		OISComp_Fun2_2_2_1(p_para4, p_para2, p_para6, p_para7, para3, para4);
		OISComp_Fun2_2_2_2(p_para5, p_para1, p_para8, p_para9, para3, para4);

		for (idx2 = 0; idx2 < var2; idx2++) {
			value3 = idx2 % para3_;
			value4 = idx2 / para3_;

			p_array = array12;
			OISComp_Fun2_2_3(p_Buf1, p_para6, p_para7, p_para8, p_para9,
				p_array, value4, value3, para4, para3);

			p_ptInBuf = ptInBuf + (value1*var2 + idx2) * 12;

			for (idx1 = 0; idx1 < 12; idx1++) {
				value2 = *p_array;
				p_array++;
				if (UTL(value2) >= 524288) {
					flag = true;
					if (value2 < 0)
						value2 = -(524288 - 1);
					if (value2 > 0)
						value2 = 524288 - 1;
				}
				if (value2 < 0)
					value2 = UTL(value2) + 524288;
				*p_ptInBuf++ = value2;
			}

		}
	}

	if (true == flag) {
		if(enable_oiscomp_log)
			pr_info("[%s] OVERFLOW\n", __func__);
		return false;
	} else {
		return true;
	}
}

void OISComp_Fun2_1(struct MTK_TSF_OIS_COMP_STRUCT1 *ptInStruct, int *pInBuf1, int *pInBuf2)
{
	if(enable_oiscomp_log)
		pr_info("%s\n", __func__);
	pInBuf1[0] = 0;
	pInBuf1[1] = ptInStruct->para5_1;
	pInBuf1[ptInStruct->para3 - 1] = (ptInStruct->para1 >> 1) - 1;

	for (int i = 2; i < ptInStruct->para3 - 1; i++)
		pInBuf1[i] = pInBuf1[1] + ptInStruct->para5 * (i - 1);

	pInBuf2[0] = 0;
	pInBuf2[1] = ptInStruct->para6_1;
	pInBuf2[ptInStruct->para4 - 1] = (ptInStruct->para2 >> 1) - 1;

	for (int j = 2; j < ptInStruct->para4 - 1; j++)
		pInBuf2[j] = pInBuf2[1] + ptInStruct->para6 * (j - 1);
}

int OISComp_Fun2(struct MTK_TSF_OIS_COMP_STRUCT1 *ptInStruct1, struct MTK_TSF_OIS_COMP_STRUCT4 *ptInStruct2,
	int *pInBuf1, unsigned int *pInBuf2, int *pInBuf3)
{
	if(enable_oiscomp_log)
		pr_info("%s\n", __func__);
	int ret = true;
	int *p_var1, *p_var2;

	p_var1 = ptInStruct2->p_para1;
	p_var2 = ptInStruct2->p_para2;

	OISComp_Fun2_1(ptInStruct1, p_var2, p_var1);
	memcpy(ptInStruct2->p_para3, pInBuf1, 1156 * sizeof(int));
	if(OISComp_Fun2_2(ptInStruct1, ptInStruct2, pInBuf3)!=true)
		return false;

	OISComp_Fun2_3(ptInStruct1, pInBuf3, pInBuf2);

	return ret;
}

int OISComp_Core_Func1(struct MTK_TSF_OIS_COMP_STRUCT1 *ptPARA,
	struct MTK_TSF_OIS_COMP_STRUCT5 *ptStruct1,
	struct MTK_TSF_OIS_COMP_STRUCT2 *ptStruct2, unsigned int InPara1)
{
	if(enable_oiscomp_log)
		pr_info("%s\n", __func__);
	int Ret = true;

	ptStruct1->p_para1 = ptStruct2->pOIS_POSITION_X;
	ptStruct1->p_para2 = ptStruct2->pOIS_POSITION_Y;
	ptStruct1->para2 = ptStruct2->OIS_VECT_SIZE;
	ptStruct1->para1 = InPara1;

	if (ptStruct2->pOIS_POSITION_X == NULL || ptStruct2->OIS_VECT_SIZE == 0) {
		if (ptStruct2->pOIS_POSITION_X == NULL){
			if(enable_oiscomp_log)
				pr_info("[%s] OIS position is Null\n", __func__);
		}
		if (ptStruct2->OIS_VECT_SIZE == 0){
			if(enable_oiscomp_log)
				pr_info("[%s] Vector size is Null\n", __func__);
		}
		return false;
	}

	int var1 = ptPARA->para3 - 1;
	int var2 = ptPARA->para4 - 1;

	ptPARA->para5 = ptPARA->para1 / (2 * var1);
	ptPARA->para5_1 = (ptPARA->para1 / 2 - ptPARA->para5 * (var1 - 2)) / 2;
	ptPARA->para5_2 = ptPARA->para1 / 2 - ptPARA->para5_1 - ptPARA->para5 * (var1 - 2);
	ptPARA->para6 = ptPARA->para2 / (2 * var2);
	ptPARA->para6_1 = (ptPARA->para2 / 2 - ptPARA->para6 * (var2 - 2)) / 2;
	ptPARA->para6_2 = ptPARA->para2 / 2 - ptPARA->para6_1 - ptPARA->para6 * (var2 - 2);

	OISComp_Fun1(ptPARA, (int *)(ptStruct2->SHADING_TABLE), (int *)gBUF0);

	return Ret;
}

int OISComp_Core_Func2_1(int InPara1, int *pInBuf, struct MTK_TSF_OIS_COMP_STRUCT1 *ptPARA, unsigned int *pOtBuf)
{
	if(enable_oiscomp_log)
		pr_info("%s\n", __func__);
	int Ret = true;

	int type = 0;
	int var1 = (65 - 1) / (16), var2 = 0, var3 = 0;

	if (ptPARA->para1 == 0 || ptPARA->para2 == 0) {
		if(enable_oiscomp_log)
			pr_info("[%s] Raw width or height is 0\n", __func__);
		return false;
	}

	if (ptPARA->para1 * 9 == ptPARA->para2 * 16) {
		type = 1;
		var1 = (65 - 1) / (16) * 3 / 4;
		var2 = ((65 - 1) - var1 * (16)) / 2;
	} else {
		type = 0;
		var1 = (65 - 1) / (17 - 1);
		var2 = 0;
	}

	int index = 9 - 1, index1 = 0, index2 = 0;

	for (int i = 0; i < 9; i++) {
		if (InPara1 <= (pInBuf[i * 65] & 0x0000FFFF)) {
			if (i > 0)
				index = i - 1;
			else
				index = i;
			break;
		}
	}
	index2 = index;
	index1 = index + 1;
	if (index1 >= 9) {
		index2--;
		index1--;
	}
	if ((pInBuf[index2 * 65] & 0x0000FFFF) <
		(pInBuf[index1 * 65] & 0x0000FFFF)) {
		int value2 = (pInBuf[index2 * 65] & 0x0000FFFF);
		int value1 = (pInBuf[index1 * 65] & 0x0000FFFF);
		int diff = value1 - value2;
		int diff_half = (diff) >> 1;

		var3 = (((InPara1 - value2) << 10) + diff_half) / (diff);

		if (var3 < 0)
			var3 = 0;
		if (var3 > (1 << 10))
			var3 = (1 << 10);
	}
	for (int i = 0, j = var2; i < 17; i++, j += var1) {
		int value2 = ((pInBuf[index2 * 65 + j] & 0xFFFF0000) >> 16);
		int value1 = ((pInBuf[index1 * 65 + j] & 0xFFFF0000) >> 16);

		if (value1 > value2)
			pOtBuf[i] = (((value1 - value2) * var3 + (var3 >> 1)) >> 10) + (value2);
		else
			pOtBuf[i] = -(((value2 - value1) * var3 + (var3 >> 1)) >> 10) + (value2);
	}
	return Ret;
}

int OISComp_Core_Func2(struct MTK_TSF_OIS_COMP_STRUCT5 *ptStruct1, struct MTK_TSF_OIS_COMP_STRUCT1 *ptPARA,
	int *pOtBuf)
{
	if(enable_oiscomp_log)
		pr_info("%s\n", __func__);
	int Ret = true;

	if (ptStruct1->p_para1 == NULL) {
		if(enable_oiscomp_log)
			pr_info("[%s] OIS position is Null\n", __func__);
		Ret = false;
		return Ret;
	}

	if (ptStruct1->para1) {
		if (ptStruct1->p_para1 == NULL || ptStruct1->p_para2 == NULL ||
			pOtBuf == NULL) {
			ptStruct1->para1 = 0;
			for (int i = 0; i < 17; i++)
				ptStruct1->array1[i] = 8192;
		} else {
			unsigned int array0[17];
			unsigned int array1_1[17];

			OISComp_Core_Func2_1(2048, pOtBuf, ptPARA, &array0[0]);
			OISComp_Core_Func2_1(ptStruct1->p_para1[0], pOtBuf, ptPARA,
				&array1_1[0]);

			if (ptStruct1->para1 == 2 && ptStruct1->para2 >= 2) {
				unsigned int array1_2[17];

				OISComp_Core_Func2_1(ptStruct1->p_para1[1], pOtBuf, ptPARA, &array1_2[0]);
				int val1 = 1024;
				int val2 = 0;

				for (int i = 5; i < 17 - 5; i++) {
					val2 = (i - 4) * 1024 / 8;
					val1 = 1024 - val2;
					array1_1[i] = (array1_1[i] * val1 +
						array1_2[i] * val2 + (1024 >> 1)) / 1024;
				}

				for (int i = 12; i < 17; i++)
					array1_1[i] = array1_2[i];
			}
			for (int i = 0; i < 17; i++) {
				ptStruct1->array1[i] = (array1_1[i] * 8192 +
					(array0[i] >> 1)) / array0[i];
			}
		}
	}
	return Ret;
}


int OISComp_Core_Func3(struct MTK_TSF_OIS_COMP_STRUCT1 *ptStruct, unsigned int *pInBuf1,
	unsigned int *pInBuf2, unsigned int *pOtBuf)
{
	if(enable_oiscomp_log)
		pr_info("%s\n", __func__);
	int Ret = true;

	if (pInBuf1 == NULL || pInBuf2 == NULL) {
		if (pInBuf1 == NULL){
			if(enable_oiscomp_log)
				pr_info("[%s] pInBuf1 is NULL\n", __func__);
		}
		if (pInBuf2 == NULL) {
			if(enable_oiscomp_log)
				pr_info("[%s] pInBuf2 is NULL\n", __func__);
		}
		Ret = false;
	} else {
		int para3 = ptStruct->para3;
		int para4 = ptStruct->para4;
		unsigned int *p_pInBuf = pInBuf1;
		unsigned int *p_pOtBuf = pInBuf1;
		int product = para3 * para4;
		unsigned int var1 = 8192;

		for (int idx1 = 0; idx1 < para4; idx1++) {
			if (pInBuf2 != NULL)
				var1 = pInBuf2[idx1];
			for (int idx2 = 0; idx2 < para3; idx2++) {
				int idx = idx1 * para3 + idx2;
				int val1 = p_pInBuf[idx + product * 0];
				int val2 = p_pInBuf[idx + product * 1];
				int val3 = p_pInBuf[idx + product * 2];
				int val4 = p_pInBuf[idx + product * 3];

				int var2 = var1 + (8192 >> 1) / 8192;

				p_pOtBuf[idx + product * 0] = val1 * var2 / 8192;
				p_pOtBuf[idx + product * 1] = val2 * var2 / 8192;
				p_pOtBuf[idx + product * 2] = val3 * var2 / 8192;
				p_pOtBuf[idx + product * 3] = val4 * var2 / 8192;

			}
		}

		struct MTK_TSF_OIS_COMP_STRUCT4 tParam;

		tParam.p_para1 = gBUF1;
		tParam.p_para2 = gBUF2;
		tParam.p_para3 = gBUF3;
		tParam.p_para4 = gBUF4;
		tParam.p_para5 = gBUF5;
		tParam.p_para6 = gBUF6;
		tParam.p_para7 = gBUF7;
		tParam.p_para8 = gBUF8;
		tParam.p_para9 = gBUF9;

		if (OISComp_Fun2(ptStruct, &tParam, p_pOtBuf, (unsigned int *)pOtBuf,
			gBUF10) != true) {
			if(enable_oiscomp_log)
				pr_info("[%s] OISComp_Fun2 Fail\n", __func__);
			Ret = false;
		}

	}
	return Ret;
}

void OISComp_Core(void *pParaIn)
{
	if(enable_oiscomp_log)
		pr_info("%s\n", __func__);
	struct MTK_TSF_OIS_COMP_STRUCT2 *p_ptStruct2 = (struct MTK_TSF_OIS_COMP_STRUCT2 *)pParaIn;
	struct MTK_TSF_OIS_COMP_STRUCT5 tStruct5;
	struct MTK_TSF_OIS_COMP_STRUCT1 *p_ptStruct1 = &(p_ptStruct2->tPARA);

	if (gReserved == 0) {
		if(enable_oiscomp_log)
			pr_info("[%s] Function Bypass\n", __func__);
		return;
	}

	if (OISComp_Core_Func1(p_ptStruct1, &tStruct5, p_ptStruct2, gReserved) != true) {
		if(enable_oiscomp_log)
			pr_info("[%s][Error] OISComp_Core_Func1 Fail\n", __func__);
		return;
	}

	if(enable_oiscomp_log){
		pr_info("[%s] b1: %d, %d, %d, %d, %d\n",
			__func__, p_ptStruct2->SHADING_TABLE[263], p_ptStruct2->SHADING_TABLE[4359],
			p_ptStruct2->SHADING_TABLE[7943], p_ptStruct2->SHADING_TABLE[7],
			p_ptStruct2->SHADING_TABLE[8167]);
		pr_info("[%s] b2: %d, %d, %d, %d, %d\n",
			__func__, p_ptStruct2->SHADING_TABLE[271], p_ptStruct2->SHADING_TABLE[4367],
			p_ptStruct2->SHADING_TABLE[7951], p_ptStruct2->SHADING_TABLE[15],
			p_ptStruct2->SHADING_TABLE[8175]);
		pr_info("[%s] b3: %d, %d, %d, %d, %d\n",
			__func__, p_ptStruct2->SHADING_TABLE[279], p_ptStruct2->SHADING_TABLE[4375],
			p_ptStruct2->SHADING_TABLE[7959], p_ptStruct2->SHADING_TABLE[23],
			p_ptStruct2->SHADING_TABLE[8183]);
		pr_info("[%s] b4: %d, %d, %d, %d, %d\n",
			__func__, p_ptStruct2->SHADING_TABLE[287], p_ptStruct2->SHADING_TABLE[4383],
			p_ptStruct2->SHADING_TABLE[7967], p_ptStruct2->SHADING_TABLE[31],
			p_ptStruct2->SHADING_TABLE[8191]);
	}

	if (OISComp_Core_Func2(&tStruct5, p_ptStruct1, p_ptStruct2->pOIS_TBL) != true) {
		if(enable_oiscomp_log)
			pr_info("[%s][Error] OISComp_Core_Func2 Fail\n", __func__);
		return;
	}

	if (OISComp_Core_Func3(p_ptStruct1, gBUF0, tStruct5.array1, p_ptStruct2->SHADING_TABLE) != true) {
		if(enable_oiscomp_log)
			pr_info("[%s][Error] OISComp_Core_Func3 Fail\n", __func__);
		return;
	}


	if(enable_oiscomp_log){
		pr_info("[%s] a0: %d, %d, %d, %d\n",
			__func__, tStruct5.array1[0], tStruct5.array1[8], tStruct5.array1[16],
			tStruct5.array1[15]);
		pr_info("[%s] a1: %d, %d, %d, %d, %d\n",
			__func__, p_ptStruct2->SHADING_TABLE[263], p_ptStruct2->SHADING_TABLE[4359],
			p_ptStruct2->SHADING_TABLE[7943], p_ptStruct2->SHADING_TABLE[7],
			p_ptStruct2->SHADING_TABLE[8167]);
		pr_info("[%s] a2: %d, %d, %d, %d, %d\n",
			__func__, p_ptStruct2->SHADING_TABLE[271], p_ptStruct2->SHADING_TABLE[4367],
			p_ptStruct2->SHADING_TABLE[7951], p_ptStruct2->SHADING_TABLE[15],
			p_ptStruct2->SHADING_TABLE[8175]);
		pr_info("[%s] a3: %d, %d, %d, %d, %d\n",
			__func__, p_ptStruct2->SHADING_TABLE[279], p_ptStruct2->SHADING_TABLE[4375],
			p_ptStruct2->SHADING_TABLE[7959], p_ptStruct2->SHADING_TABLE[23],
			p_ptStruct2->SHADING_TABLE[8183]);
		pr_info("[%s] a4: %d, %d, %d, %d, %d\n",
			__func__, p_ptStruct2->SHADING_TABLE[287], p_ptStruct2->SHADING_TABLE[4383],
			p_ptStruct2->SHADING_TABLE[7967], p_ptStruct2->SHADING_TABLE[31],
			p_ptStruct2->SHADING_TABLE[8191]);
	}

	if(enable_oiscomp_log)
		pr_info("[%s] Function Done\n", __func__);
}

void mtk_ois_comp_core(struct mtk_cam_tuning *param, int *ois_x, int *ois_y, int *ois_vector_size)
{
	if(enable_oiscomp_log)
		pr_info("%s\n", __func__);
	struct MTK_TSF_OIS_COMP_STRUCT2 tParaIn;

	tParaIn.pOIS_TBL = gBUF12;
	tParaIn.OIS_VECT_SIZE = *ois_vector_size;
	tParaIn.pOIS_POSITION_X = ois_x;
	tParaIn.pOIS_POSITION_Y = ois_y;

	tParaIn.tPARA.para1 = param->width;
	tParaIn.tPARA.para2 = param->height;
	tParaIn.tPARA.para3 = param->x_num;
	tParaIn.tPARA.para4 = param->y_num;
	tParaIn.SHADING_TABLE = param->shading_tbl;

	if(((gBUF12[260]&0x0000FFFF) != 2048) ||
		((gBUF12[0]&0x0000FFFF) > (gBUF12[520]&0x0000FFFF))){
		if(enable_oiscomp_log)
			pr_info("[%s] error! otp table is incorrect\n", __func__);
		return;
	}
	if(param->shading_tbl == NULL) {
		if(enable_oiscomp_log)
			pr_info("[%s] error! shading table is NULL\n", __func__);
		return;
	}
	if(param->width == 0 || param->height == 0) {
		if(enable_oiscomp_log)
			pr_info("[%s] error param! width: %d, height: %d\n", __func__,
		param->width, param->height);
		return;
	} else if (!((param->width * 3 == param->height * 4) ||
		(param->width * 9 == param->height * 16))){
		pr_info("[%s] error param! width: %d, height: %d\n", __func__,
			param->width, param->height);
		return;
	}
	if(*ois_vector_size == 0 || *ois_vector_size > 2) {
		if(enable_oiscomp_log)
			pr_info("[%s] error param! ois_vector_size: %d\n", __func__,
		*ois_vector_size);
		return;
	}
	if(ois_x == NULL || *ois_x < 0 || *ois_x > 4095) {
		if (ois_x == NULL){
			if(enable_oiscomp_log)
				pr_info("[%s] error param! ois_x is NULL\n", __func__);
		} else{
			if(enable_oiscomp_log)
				pr_info("[%s] error param! ois_x: %d\n", __func__, *ois_x);
		}
		return;
	}
	if(ois_y == NULL){
		if(enable_oiscomp_log)
			pr_info("[%s] error param! ois_y is NULL\n", __func__);
	}

	if(param->x_num == 0 || param->y_num == 0) {
		if(enable_oiscomp_log)
			pr_info("[%s] warning param! x_num: %d, y_num: %d\n", __func__,
		param->x_num, param->y_num);
		tParaIn.tPARA.para3 = 17;
		tParaIn.tPARA.para4 = 17;
	}
	if(enforce_oiscomp)
		tParaIn.pOIS_POSITION_X[0] = enforce_oiscode;
	if(enable_oiscomp_log){
		pr_info("[%s] input1: %d, %d, %d, %d\n",
			__func__, tParaIn.tPARA.para1, tParaIn.tPARA.para2, tParaIn.tPARA.para3, tParaIn.tPARA.para4);
		pr_info("[%s] input2: %d, %d\n",
			__func__, tParaIn.pOIS_POSITION_X[0], tParaIn.OIS_VECT_SIZE);
		pr_info("[%s] input3: %d, %d, %d, %d, %d\n",
			__func__, (tParaIn.pOIS_TBL[0]&0x0000FFFF), (tParaIn.pOIS_TBL[130]&0x0000FFFF),
			(tParaIn.pOIS_TBL[260]&0x0000FFFF), (tParaIn.pOIS_TBL[390]&0x0000FFFF),
			(tParaIn.pOIS_TBL[520]&0x0000FFFF));
		pr_info("[%s] input4: %d, %d\n", __func__, enforce_oiscomp, enforce_oiscode);
	}

	OISComp_Core(&tParaIn);
}

void mtk_set_ois_table(const bool *is_valid,
	const void *ois_calibration_data, unsigned int data_size)
{
	if (*is_valid && data_size == 2340) {
		if(enable_oiscomp_log)
			pr_info("%s: set ois table\n", __func__);
		memcpy(gBUF12, ois_calibration_data, 2340);
	} else{
		if(enable_oiscomp_log)
			pr_info("%s: ois table has already been set\n", __func__);
	}
}
/*
 * mtk's part: end
 */

/*
 * oplus's part: start
 */
#define LOG_TAG "mtk_cam_tuning"

#define LOG_INF(format, args...) do { \
	if (enable_oiscomp_log) \
		pr_info(LOG_TAG " [%s] " format, __func__, ##args); \
} while (0)

#define EVENT_BUF_SIZE (256)
#define MAX_EVENT_NUM (128)
#define NS_IN_MS (1000000)
#define OIS_INTERVAL_MS (2)
#define SEARCH_THRESHOLD (150 * NS_IN_MS)
#define POLL_INTERVAL_MS (110 * OIS_INTERVAL_MS)
static struct hf_manager_task hf_task;
static struct hf_manager_event G_event_data[EVENT_BUF_SIZE];
static struct hf_manager_event Once_event_data[MAX_EVENT_NUM];
static unsigned int writeBufIndex = 0;
static struct out_ois_param last_ois_res;

void oplus_cam_enable_ois(void) {
	int ret = 0;
	int sensor_type = SENSOR_TYPE_OIS2_POWER;
	int val1 = HF_MANAGER_SENSOR_ENABLE;
	int val2 = OIS_INTERVAL_MS * NS_IN_MS;
	struct hf_manager_cmd cmd;
	struct hf_manager_batch *batch = NULL;
	if (hf_task.client)
		return;
	hf_task.client = hf_client_create();
	if (NULL == hf_task.client) {
		LOG_INF("hf_client_create fail\n");
		return;
	}
	// poweron ois
	ret = hf_client_find_sensor(hf_task.client, sensor_type);
	if (ret < 0) {
		LOG_INF("hf_client_find_sensor %u fail\n", sensor_type);
		return;
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.sensor_type = sensor_type;
	cmd.action = val1;
	batch = (struct hf_manager_batch *)cmd.data;
	batch->delay = val2;
	batch->latency = 0;
	ret = hf_client_control_sensor(hf_task.client, &cmd);
	if (ret < 0) {
		LOG_INF("hf_client_control_sensor %u fail\n", sensor_type);
		return;
	}
	// enable ois
	sensor_type = SENSOR_TYPE_OIS1;
	ret = hf_client_find_sensor(hf_task.client, sensor_type);
	if (ret < 0) {
		LOG_INF("hf_client_find_sensor %u fail\n", sensor_type);
		return;
	}
	cmd.sensor_type = sensor_type;
	cmd.action = val1;
	batch = (struct hf_manager_batch *)cmd.data;
	batch->delay = val2;
	batch->latency = 0;
	ret = hf_client_control_sensor(hf_task.client, &cmd);
	if (ret < 0) {
		LOG_INF("hf_client_control_sensor %u fail\n", sensor_type);
		return;
	}
	LOG_INF("enable ois\n");
	writeBufIndex = 0;
	memset(&last_ois_res, 0, sizeof(struct out_ois_param));
	memset(G_event_data, 0, EVENT_BUF_SIZE * sizeof(struct hf_manager_event));
	return;
}

void oplus_cam_disable_ois(void) {
	int ret = 0;
	int sensor_type = SENSOR_TYPE_OIS1;
	int val1 = HF_MANAGER_SENSOR_DISABLE;
	int val2 = 0;
	struct hf_manager_cmd cmd;
	struct hf_manager_batch *batch = NULL;
	if (NULL == hf_task.client) {
		LOG_INF("hf_client not exit\n");
		return;
	}
	// disable ois
	ret = hf_client_find_sensor(hf_task.client, sensor_type);
	if (ret < 0) {
		LOG_INF("hf_client_find_sensor %u fail\n", sensor_type);
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.sensor_type = sensor_type;
	cmd.action = val1;
	batch = (struct hf_manager_batch *)cmd.data;
	batch->delay = val2;
	batch->latency = 0;
	ret = hf_client_control_sensor(hf_task.client, &cmd);
	if (ret < 0) {
		LOG_INF("hf_client_control_sensor %u fail\n", sensor_type);
	}
	// poweroff ois
	sensor_type = SENSOR_TYPE_OIS2_POWER;
	ret = hf_client_find_sensor(hf_task.client, sensor_type);
	if (ret < 0) {
		LOG_INF("hf_client_find_sensor %u fail\n", sensor_type);
	}
	cmd.sensor_type = sensor_type;
	cmd.action = val1;
	batch = (struct hf_manager_batch *)cmd.data;
	batch->delay = val2;
	batch->latency = 0;
	ret = hf_client_control_sensor(hf_task.client, &cmd);
	if (ret < 0) {
		LOG_INF("hf_client_control_sensor %u fail\n", sensor_type);
	}
	if (hf_task.client) {
		hf_client_destroy(hf_task.client);
		hf_task.client = NULL;
	}
	LOG_INF("disable ois");
	return;
}

static int fetch_next(struct hf_client_fifo *hf_fifo,
				  struct hf_manager_event *event)
{
	unsigned long flags;
	int have_event;

	spin_lock_irqsave(&hf_fifo->buffer_lock, flags);
	have_event = hf_fifo->head != hf_fifo->tail;
	if (have_event) {
		*event = hf_fifo->buffer[hf_fifo->tail++];
		hf_fifo->tail &= hf_fifo->bufsize - 1;
		hf_fifo->buffull = false;
		hf_fifo->client_active = ktime_get_boottime_ns();
	}
	spin_unlock_irqrestore(&hf_fifo->buffer_lock, flags);
	return have_event;
}

int oplus_hf_client_poll_sensor(struct hf_client *client,
		struct hf_manager_event *data, int count)
{
	int read = 0;
	struct hf_client_fifo *hf_fifo = &client->hf_fifo;
	for (;;) {
		if (READ_ONCE(hf_fifo->head) == READ_ONCE(hf_fifo->tail))
			return 0;
		if (count == 0)
			break;
		while (read < count &&
			fetch_next(hf_fifo, &data[read])) {
			read++;
		}
		if (read)
			break;
	}
	return read;
}

void oplus_poll_timer_callback(struct timer_list *timer) {
	oplus_cam_get_ois_data();
	mod_timer(timer, jiffies + msecs_to_jiffies(POLL_INTERVAL_MS));
}

void oplus_cam_get_ois_data(void) {
	struct hf_client *client = hf_task.client;
	int size = 0;
	if (NULL == client)
		return;
	size = oplus_hf_client_poll_sensor(client, Once_event_data, MAX_EVENT_NUM);
	if (size <= 0)
		return;
	for (int i = 0; i < size; ++i) {
		G_event_data[(writeBufIndex + i) % EVENT_BUF_SIZE] = Once_event_data[i];
	}
	writeBufIndex += size;
	// handle case when writeIndex exceeds buffer size
	if (writeBufIndex >= EVENT_BUF_SIZE)
		writeBufIndex -= EVENT_BUF_SIZE;
	LOG_INF("poll_sensor: size %d  %d  %lld  %lld \n", size,
		(writeBufIndex -1 + EVENT_BUF_SIZE) % EVENT_BUF_SIZE,
		Once_event_data[0].timestamp,
		Once_event_data[size -1].timestamp);
}

static inline u64 abs_time_diff(u64 a, u64 b) {
	return (a > b) ? (a - b) : (b - a);
}

void dump_ois_data(int start_index, int count) {
	int i ;
	LOG_INF("dump ois data start");
	for (i = start_index; i < start_index + count; i++) {
		LOG_INF("data index = %d [%lld,%d,%d]\n", i % EVENT_BUF_SIZE,
			G_event_data[i % EVENT_BUF_SIZE].timestamp,
			G_event_data[i % EVENT_BUF_SIZE].word[2],
			G_event_data[i % EVENT_BUF_SIZE].word[3]);
	}
	LOG_INF("dump ois data end");
}

int find_matching_event_index(s64 time_stamp, int startIndex) {
	int i = -1, j = -1;
	i = startIndex;
	if ((time_stamp - G_event_data[startIndex].timestamp) >= (OIS_INTERVAL_MS * NS_IN_MS)) {
		return startIndex;
	}
	do {
		if (abs_time_diff(G_event_data[i].timestamp, time_stamp) < (OIS_INTERVAL_MS * NS_IN_MS)) {
			j = (i + EVENT_BUF_SIZE - 1) % EVENT_BUF_SIZE;
			if (abs_time_diff(G_event_data[i].timestamp, time_stamp) <= abs_time_diff(G_event_data[j].timestamp, time_stamp)) {
				return i;
			} else {
				return j;
			}
		}
		if (i == 0) {
			i = EVENT_BUF_SIZE - 1;
		} else {
			i--;
		}
	} while (i != startIndex);
	return -1;
}

void oplus_cam_calc_ois_data(struct mtk_cam_tuning *param, struct out_ois_param *ois) {
	s64 time_diff = -1 * NS_IN_MS;
	s64 start_time, end_time;
	s64 search_start_timestamp, last_timestamp;
	int start_index = -1, end_index = -1;
	int count = 0,  search_start_index = 0, gap = 0;
	if (NULL == param || NULL == ois)
		return;
	if (0 == param->exp_time_ns) {
		*ois = last_ois_res;
		return;
	}
	search_start_index = (writeBufIndex -1 + EVENT_BUF_SIZE) % EVENT_BUF_SIZE;
	last_timestamp = G_event_data[search_start_index].timestamp;
	LOG_INF("param->sof_boottime_ns %llu param->readout_ns %llu param->exp_time_ns %llu", param->sof_boottime_ns, param->readout_ns, param->exp_time_ns);
	if (param->exp_time_ns > SEARCH_THRESHOLD) {
		end_index = search_start_index;
		start_index = search_start_index;
		goto MEAN_CALCULATE;
	}
	start_time = param->sof_boottime_ns - param->exp_time_ns + param->readout_ns / 2;
	end_time = param->sof_boottime_ns + param->readout_ns / 2;
	start_time += time_diff;
	end_time += time_diff;
	LOG_INF("start_time %lld  end_time %lld last_time_stamp %lld", start_time, end_time, last_timestamp);
	end_index = find_matching_event_index(end_time, search_start_index);
	if (-1 != end_index) {
		search_start_timestamp = G_event_data[end_index].timestamp;
		search_start_index = end_index;
		if (search_start_timestamp > start_time) {
			gap = (search_start_timestamp - start_time) / (OIS_INTERVAL_MS * NS_IN_MS) - 2;
			search_start_index -= (gap >= 0) ? gap : 0;
			search_start_index = (search_start_index + EVENT_BUF_SIZE) % EVENT_BUF_SIZE;
			start_index = find_matching_event_index(start_time, search_start_index);
		}
		if (-1 == start_index) {
			start_index = end_index;
		}
	} else {
		LOG_INF("not match");
		return;
	}
MEAN_CALCULATE:
	if (-1 != start_index && -1 != end_index) {
		if (start_index <= end_index) {
			count = end_index - start_index + 1;
		} else {
			count = EVENT_BUF_SIZE - start_index + end_index + 1;
		}
		for (int i = start_index; i < start_index + count; i++) {
			ois->ois_x += G_event_data[i % EVENT_BUF_SIZE].word[2] / 100000;
			ois->ois_y += G_event_data[i % EVENT_BUF_SIZE].word[3] / 100000;
		}
		ois->ois_x = ois->ois_x / count;
		ois->ois_y = ois->ois_y / count;
		ois->ois_vector_size = 1;
		if (enable_oiscomp_log) {
			dump_ois_data(start_index, count);
		}
	}
	last_ois_res = *ois;
}

/*
 * oplus's part: end
 */

void mtk_cam_tuning_probe(void)
{
}

void mtk_cam_tuning_init(struct mtk_cam_tuning *param)
{
	pr_info("%s: \n", __func__);

	if (!do_set_otp_once) {
		do_set_otp_once = true;

		bool is_otp_valid = false;

		custom_eeprom_read(&is_otp_valid, (void *)ois_otp);

		mtk_set_ois_table(&is_otp_valid, (void *)ois_otp, OTP_SIZE);
	}

	oplus_cam_enable_ois();

	return;
}

void mtk_cam_tuning_update(struct mtk_cam_tuning *param)
{
	struct out_ois_param ois;
	pr_info("%s: \n", __func__);
	memset(&ois, 0, sizeof(struct out_ois_param));
	oplus_cam_get_ois_data();
	oplus_cam_calc_ois_data(param, &ois);
	/* note: get ois info. before entering mtk_ois_comp_core() */
	LOG_INF("input ois.ois_x %d ois.ois_y %d  ois.ois_vector_size %d", ois.ois_x, ois.ois_y,ois.ois_vector_size);
	mtk_ois_comp_core(param, &ois.ois_x, &ois.ois_y, &ois.ois_vector_size);
	return;
}
