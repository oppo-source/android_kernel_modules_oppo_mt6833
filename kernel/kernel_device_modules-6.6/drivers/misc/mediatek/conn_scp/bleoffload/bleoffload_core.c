/*******************************************************************************/
/*                     E X T E R N A L   R E F E R E N C E S                   */
/*******************************************************************************/
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/timer.h>
#include "conap_scp.h"
#include "conap_scp_ipi.h"
#include "bleoffload_core.h"

/*******************************************************************************/
/*                  F U N C T I O N   D E C L A R A T I O N S                  */
/*******************************************************************************/
static int _send_msg_to_scp(struct bleoffload_module_ctx *mctx, u32 msg_id, u32 msg_size,
					u8 *msg_data);
static void bleoffload_scp_state_change(int state);
static void bleoffload_scp_msg_notify(uint32_t msg_id, uint32_t *buf, uint32_t size);

static struct bleoffload_module_ctx g_ble_module_ctx = {
	.drv_type = DRV_TYPE_BLE,
	.enable = false,
	.client = {
		.port = 0,
		.seqnum = 0,
	},
	.scp_cb = {
		.conap_scp_msg_notify_cb = bleoffload_scp_msg_notify,
		.conap_scp_state_notify_cb = bleoffload_scp_state_change,
	},
	.scan_info = {
		.init_scan_id = 0,
		.init_scan = false,
		.start_scan_id = 0,
		.scan_data_sz = 0,
		.scan_data = NULL,
	},
};

static struct bleoffload_core_ctx g_bleoffload_ctx;

/*******************************************************************************/
/*                             Data buffer                                     */
/*******************************************************************************/
#define MAX_BUF_LEN     (3 * 1024)

struct bleoffload_data_buf {
	u8 buf[MAX_BUF_LEN];
	u32 size;
	struct list_head list;
};
struct bleoffload_buf_list {
	struct mutex lock;
	struct list_head list;
};
static struct bleoffload_buf_list g_bleoffload_data_buf_list;
static struct mutex g_bleoffload_msg_lock;

/*******************************************************************************/
/*                  Message Thread Definition                  */
/*******************************************************************************/
static int opfunc_scp_register(struct msg_op_data *op);
static int opfunc_scp_unregister(struct msg_op_data *op);
static int opfunc_send_msg(struct msg_op_data *op);
static int opfunc_recv_msg(struct msg_op_data *op);

static const msg_opid_func bleoffload_core_opfunc[] = {
	[BLEOFFLOAD_OPID_MODULE_BIND] = opfunc_scp_register,
	[BLEOFFLOAD_OPID_MODULE_UNBIND] = opfunc_scp_unregister,
	[BLEOFFLOAD_OPID_SEND_MSG] = opfunc_send_msg,
	[BLEOFFLOAD_OPID_RECV_MSG] = opfunc_recv_msg,
};

/*******************************************************************************/
/*                              F U N C T I O N S                              */
/*******************************************************************************/

static struct bleoffload_data_buf *bleoffload_data_buffer_alloc(void)
{
	struct bleoffload_data_buf *data_buf = NULL;

	if (list_empty(&g_bleoffload_data_buf_list.list)) {
		data_buf = kmalloc(sizeof(struct bleoffload_data_buf), GFP_KERNEL);
		if (data_buf == NULL) {
			pr_notice("[%s] kmalloc fail", __func__);
			return NULL;
		}
		INIT_LIST_HEAD(&data_buf->list);
	} else {
		mutex_lock(&g_bleoffload_data_buf_list.lock);
		data_buf = list_first_entry(&g_bleoffload_data_buf_list.list, struct bleoffload_data_buf, list);
		list_del(&data_buf->list);
		mutex_unlock(&g_bleoffload_data_buf_list.lock);
	}
	return data_buf;
}

static void bleoffload_data_buffer_free(struct bleoffload_data_buf *data_buf)
{
	if (!data_buf)
		return;
	mutex_lock(&g_bleoffload_data_buf_list.lock);
	list_add_tail(&data_buf->list, &g_bleoffload_data_buf_list.list);
	mutex_unlock(&g_bleoffload_data_buf_list.lock);
}

static int is_scp_ready(void)
{
	return conap_scp_is_ready();
}

/*******************************************************************************/
/*      O P          F U N C T I O N S                                         */
/*******************************************************************************/
static int opfunc_scp_register(struct msg_op_data *op)
{
	int ret = 0;
	struct bleoffload_core_ctx *ctx = &g_bleoffload_ctx;
	uint32_t module_id = op->op_data[0];
	uint32_t port = op->op_data[1];
	struct bleoffload_module_ctx *mctx = NULL;

	if (module_id >= BLEOFFLOAD_MODULE_SIZE) {
		pr_notice("[%s] incorrect module=[%d]", __func__, module_id);
		return -1;
	}

	mctx = ctx->module_ctx[module_id];

	if (mctx->enable && mctx->client.port == port) {
		pr_info("[%s] mod=[%d] already registered", __func__, module_id);
		return 0;
	}

	mctx->client.port = port;
	ret = conap_scp_register_drv(mctx->drv_type, &mctx->scp_cb);
	pr_info("[%s] SCP register drv_type=[%d], ret=[%d]", __func__, mctx->drv_type, ret);

	mctx->enable = true;

	return ret;
}

static int opfunc_scp_unregister(struct msg_op_data *op)
{
	int ret = 0;
	struct bleoffload_core_ctx *ctx = &g_bleoffload_ctx;
	uint32_t module_id = op->op_data[0];
	struct bleoffload_module_ctx *mctx = NULL;

	if (module_id >= BLEOFFLOAD_MODULE_SIZE) {
		pr_notice("[%s] incorrect module=[%d]", __func__, module_id);
		return -1;
	}
	mctx = ctx->module_ctx[module_id];

	ret = conap_scp_unregister_drv(mctx->drv_type);
	pr_info("[%s] SCP unregister drv_type=[%d], ret=[%d]", __func__, mctx->drv_type, ret);
	mctx->enable = false;

	return ret;
}

static int opfunc_send_msg(struct msg_op_data *op)
{
	struct bleoffload_core_ctx *ctx = &g_bleoffload_ctx;
	uint32_t msg_id, size, module_id;
	struct bleoffload_module_ctx *mctx = NULL;
	struct bleoffload_data_buf *data_buf;
	int ret = 0;

	module_id = (uint32_t)op->op_data[0];
	msg_id = (u32)op->op_data[1];
	data_buf = (struct bleoffload_data_buf *)op->op_data[2];
	size = (u32)op->op_data[3];

	if (module_id >= BLEOFFLOAD_MODULE_SIZE) {
		pr_notice("[%s] incorrect module id=[%d]", __func__, module_id);
		return -1;
	}

	mctx = ctx->module_ctx[module_id];

	ret = _send_msg_to_scp(mctx, msg_id, size, (data_buf == NULL ? NULL : data_buf->buf));

	bleoffload_data_buffer_free(data_buf);
	if (ret) {
		pr_notice("[%s] snd_msg fail [%d]", __func__, ret);
		return ret;
	}

	return ret;
}

static int opfunc_recv_msg(struct msg_op_data *op)
{
	int ret = 0;
	uint32_t module_id = (uint32_t)op->op_data[0];
	uint32_t msg_id = (uint32_t)op->op_data[1];
	struct bleoffload_data_buf *data_buf = (struct bleoffload_data_buf *)op->op_data[2];
	uint32_t size = (uint32_t)op->op_data[3];
	struct bleoffload_core_ctx *ctx = &g_bleoffload_ctx;
	struct bleoffload_module_ctx *mctx = NULL;

	if (module_id >= BLEOFFLOAD_MODULE_SIZE) {
		pr_notice("[%s] incorrect module [%d]", __func__, module_id);
		return -1;
	}
	mctx = ctx->module_ctx[module_id];

	if (mctx == NULL) {
		pr_notice("[%s] no module context", __func__);
		return -1;
	}

	pr_info("[%s] Send to netlink client, module=[%d]msg=[%d]size=[%d]", __func__, module_id, msg_id, size);
	bleoffload_netlink_send_to_native(&mctx->client, "[BLEOFFLOAD]", msg_id, data_buf->buf, size);
	bleoffload_data_buffer_free(data_buf);

	return ret;
}

/*******************************************************************************/
/*      handlers                                  */
/*******************************************************************************/
static void bleoffload_msg_notify(uint32_t module_id, uint32_t msg_id,
				uint32_t *buf, uint32_t size)
{
	int ret = 0;
	struct bleoffload_core_ctx *ctx = &g_bleoffload_ctx;
	struct bleoffload_module_ctx *mctx = NULL;
	struct bleoffload_data_buf *data_buf = NULL;

	pr_info("[%s] module[%d]msg[%d]size[%d]\n", __func__, module_id, msg_id, size);

	if (module_id >= BLEOFFLOAD_MODULE_SIZE) {
		pr_notice("[%s] incorrect module [%d]", __func__, module_id);
		return;
	}

	mctx = ctx->module_ctx[module_id];
	if (mctx == NULL || mctx->enable == false) {
		pr_notice("[%s] module=[%d] is not enable", __func__, module_id);
		return;
	}

	if (size > MAX_BUF_LEN) {
		pr_info("[%s] size [%d] exceed expected [%d]", __func__, size, MAX_BUF_LEN);
		return;
	}

	if (size > 0) {
		data_buf = bleoffload_data_buffer_alloc();
		if (!data_buf) {
			pr_notice("[%s] data buf is empty", __func__);
			return;
		}
		memcpy(&(data_buf->buf[0]), buf, size);
	}
	ret = msg_thread_send_4(&ctx->msg_ctx, BLEOFFLOAD_OPID_RECV_MSG,
							module_id, msg_id, (size_t)data_buf, size);

	if (ret)
		pr_warn("[%s] Notify recv msg fail, ret=[%d]\n", __func__, ret);

}

static int _send_msg_to_scp(struct bleoffload_module_ctx *mctx, uint32_t msg_id,
		uint32_t msg_size, uint8_t *msg_data)
{
	int ret;
	pr_info("[%s] drv_type[%d]msg_id[%d]msg_size[%d]\n", __func__, mctx->drv_type, msg_id, msg_size);

	ret = is_scp_ready();
	if (ret == 0) {
		pr_notice("[%s] scp not ready", __func__);
		return -1;
	}

	mutex_lock(&g_bleoffload_msg_lock);

	ret = conap_scp_send_message(mctx->drv_type, msg_id, msg_data, msg_size);
	if (ret)
		pr_warn("[%s] msg send fail [%d]", __func__, ret);

	mutex_unlock(&g_bleoffload_msg_lock);
	return ret;
}


static void bleoffload_common_state_change(uint32_t module_id, int state)
{
	struct bleoffload_core_ctx *ctx = &g_bleoffload_ctx;
	struct bleoffload_module_ctx *mctx = ctx->module_ctx[module_id];
	int ret;

	pr_info("[%s] module_id=[%d] state=[%d]", __func__, module_id, state);

	if (ctx->status == BLEOFFLOAD_INACTIVE) {
		pr_info("[%s] module ctx [%d] is inactive", __func__, module_id);
		return;
	}
	/* state = 1: scp ready */
	/* state = 0: scp stop */
	/* support test recover */
	if (state == 1) {
		struct bleoffload_scan_info *scan_info = NULL;
		if (mctx && mctx->scan_info.init_scan) {
			scan_info = &mctx->scan_info;
			ret = msg_thread_send_2(&ctx->msg_ctx, BLEOFFLOAD_OPID_SEND_MSG,
						module_id, scan_info->init_scan_id);
			pr_info("[%s] init scan ret=[%d]", __func__, ret);
		} else {
			pr_info("[%s] bleoffload not called", __func__);
			return;
		}
		if (ret == 0 && mctx->scan_info.scan_data) {
			struct bleoffload_data_buf *data_buf = NULL;
			data_buf = bleoffload_data_buffer_alloc();
			if (!data_buf) {
				pr_notice("[%s] data buf is empty", __func__);
				return;
			}
			memcpy(&(data_buf->buf[0]), scan_info->scan_data, scan_info->scan_data_sz);
			ret = msg_thread_send_4(&ctx->msg_ctx, BLEOFFLOAD_OPID_SEND_MSG,
						module_id, scan_info->start_scan_id,
						(size_t)data_buf, scan_info->scan_data_sz);
			pr_info("[%s] start scan ret=[%d]", __func__, ret);
		}
	}
}

static void bleoffload_scp_state_change(int state)
{
	bleoffload_common_state_change(BLEOFFLOAD_MODULE_BLE, state);
}

static void bleoffload_scp_msg_notify(unsigned int msg_id, unsigned int *buf, unsigned int size)
{
	bleoffload_msg_notify(BLEOFFLOAD_MODULE_BLE, msg_id, buf, size);
}

static int bleoffload_core_handler(uint32_t module_id, u32 msg_id, void *data, u32 sz)
{
	int ret = 0;
	struct bleoffload_core_ctx *ctx = &g_bleoffload_ctx;
	struct bleoffload_module_ctx *mctx = NULL;
	pr_info("[%s] module_id[%d]msg_id[%d]sz[%d]\n", __func__, module_id, msg_id, sz);

	if (module_id >= BLEOFFLOAD_MODULE_SIZE) {
		pr_notice("[%s] incorrect module [%d]", __func__, module_id);
		return -1;
	}

	mctx = ctx->module_ctx[module_id];

	if (mctx == NULL || mctx->enable == false) {
		pr_info("[%s] module ctx [%d] is inactive", __func__, module_id);
		return -1;
	}

	if (sz > MAX_BUF_LEN) {
		pr_notice("[%s] module[%d]msg_id[%d] data size [%d] exceed expected [%d]",
				__func__, module_id, msg_id, sz, MAX_BUF_LEN);
		return -2;
	}

	ret = _send_msg_to_scp(mctx, msg_id, sz, data);

	if (ret)
		pr_info("[%s] Send to msg thread fail, ret=[%d]\n", __func__, ret);

	return ret;
}

static int bleoffload_core_bind(uint32_t port, uint32_t module_id)
{
	int ret = 0;
	struct bleoffload_core_ctx *ctx = &g_bleoffload_ctx;
	pr_info("[%s] port[%d]module_id[%d]\n", __func__, port, module_id);

	if (module_id >= BLEOFFLOAD_MODULE_SIZE) {
		pr_notice("[%s] incorrect module=[%d]", __func__, module_id);
		return -1;
	}

	ret = msg_thread_send_2(&ctx->msg_ctx, BLEOFFLOAD_OPID_MODULE_BIND, module_id, port);

	if (ret)
		pr_info("[%s] Send to msg thread fail, ret=[%d]\n", __func__, ret);

	return ret;
}

static int bleoffload_core_unbind(uint32_t module_id)
{
	int ret = 0;
	struct bleoffload_core_ctx *ctx = &g_bleoffload_ctx;
	pr_info("[%s] module_id[%d]\n", __func__, module_id);

	if (module_id >= BLEOFFLOAD_MODULE_SIZE) {
		pr_notice("[%s] incorrect module=[%d]", __func__, module_id);
		return -1;
	}

	ret = msg_thread_send_1(&ctx->msg_ctx, BLEOFFLOAD_OPID_MODULE_UNBIND, module_id);

	if (ret)
		pr_info("[%s] Send to msg thread fail, ret=[%d]\n", __func__, ret);

	pr_info("[%s] already done", __func__);

	return 0;
}

static int bleoffload_core_init_scan(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz)
{
	struct bleoffload_core_ctx *ctx = &g_bleoffload_ctx;
	struct bleoffload_module_ctx *mctx = NULL;
	int ret;

	ret = bleoffload_core_handler(module_id, msg_id, data, sz);
	if (ret == 0) {
		mctx = ctx->module_ctx[module_id];
		mctx->scan_info.init_scan = true;
		mctx->scan_info.init_scan_id = msg_id;
	}

	return ret;
}

static int bleoffload_core_deinit_scan(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz)
{
	struct bleoffload_core_ctx *ctx = &g_bleoffload_ctx;
	struct bleoffload_module_ctx *mctx = NULL;
	int ret;

	ret = bleoffload_core_handler(module_id, msg_id, data, sz);
	if (ret == 0) {
		mctx = ctx->module_ctx[module_id];
		mctx->scan_info.init_scan = false;
		mctx->scan_info.init_scan_id = 0;
	}

	return ret;
}

static int bleoffload_core_start_scan(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz)
{
	int ret;
	uint8_t *data_buf;
	struct bleoffload_core_ctx *ctx = &g_bleoffload_ctx;
	struct bleoffload_module_ctx *mctx = NULL;

	ret = bleoffload_core_handler(module_id, msg_id, data, sz);
	if (ret == 0) {
		mctx = ctx->module_ctx[module_id];
		/* force free */
		kfree(mctx->scan_info.scan_data);
		mctx->scan_info.scan_data = NULL;

		data_buf = kmalloc(sz, GFP_KERNEL);
		if (data_buf == NULL)
			return -1;
		memcpy(data_buf, data, sz);
		mctx->scan_info.scan_data = data_buf;
		mctx->scan_info.scan_data_sz = sz;
		mctx->scan_info.start_scan_id = msg_id;
	}

	return ret;
}

static int bleoffload_core_stop_scan(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz)
{
	struct bleoffload_core_ctx *ctx = &g_bleoffload_ctx;
	struct bleoffload_module_ctx *mctx = NULL;
	int ret;

	ret = bleoffload_core_handler(module_id, msg_id, data, sz);
	if (ret == 0) {
		mctx = ctx->module_ctx[module_id];
		kfree(mctx->scan_info.scan_data);
		mctx->scan_info.scan_data = NULL;
		mctx->scan_info.scan_data_sz = 0;
		mctx->scan_info.start_scan_id = 0;
	}

	return ret;
}

int bleoffload_core_init(void)
{
	struct bleoffload_netlink_event_cb nl_cb;
	struct bleoffload_core_ctx *ctx = &g_bleoffload_ctx;
	int ret;
	pr_info("[%s] enter\n", __func__);

	/* data buffer init */
	mutex_init(&g_bleoffload_msg_lock);
	mutex_init(&g_bleoffload_data_buf_list.lock);
	INIT_LIST_HEAD(&g_bleoffload_data_buf_list.list);

	/* bleoffload ctx */
	memset(&g_bleoffload_ctx, 0, sizeof(struct bleoffload_core_ctx));

	/* Create bleoffload thread */
	ret = msg_thread_init(&ctx->msg_ctx, "oplus_bleoffload_thread",
					bleoffload_core_opfunc, BLEOFFLOAD_OPID_MAX);

	if (ret) {
		pr_info("bleoffload thread init fail, ret=[%d]\n", ret);
		return -1;
	}

	ctx->module_ctx[0] = &g_ble_module_ctx;

	/* Init netlink */
	nl_cb.bleoffload_bind = bleoffload_core_bind;
	nl_cb.bleoffload_unbind = bleoffload_core_unbind;
	nl_cb.bleoffload_init_scan = bleoffload_core_init_scan;
	nl_cb.bleoffload_deinit_scan = bleoffload_core_deinit_scan;
	nl_cb.bleoffload_start_scan = bleoffload_core_start_scan;
	nl_cb.bleoffload_stop_scan = bleoffload_core_stop_scan;
	nl_cb.bleoffload_handler = bleoffload_core_handler;

	ret = bleoffload_netlink_init(&nl_cb);

	if (ret)
		return ret;

	ctx->status = BLEOFFLOAD_ACTIVE;

	return 0;
}

void bleoffload_core_deinit(void)
{
	bleoffload_netlink_deinit();
	pr_info("[%s] deinit done", __func__);
}
