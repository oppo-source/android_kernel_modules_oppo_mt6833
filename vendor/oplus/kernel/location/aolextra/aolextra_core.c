#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/timer.h>
#include "msg_thread.h"
#include "conap_scp.h"
#include "aolextra_netlink.h"
#include "aolextra_ring_buffer.h"

/*******************************************************************************/
/*                             D A T A   T Y P E S                             */
/*******************************************************************************/
enum aol_core_opid {
    AOL_OPID_DEFAULT = 0,
    AOL_OPID_SCP_REGISTER = 1,
    AOL_OPID_SCP_UNREGISTER = 2,
    AOL_OPID_SEND_MSG  = 3,
    AOL_OPID_RECV_MSG  = 4,
    AOL_OPID_MAX
};

enum aol_core_status {
    AOL_INACTIVE,
    AOL_ACTIVE,
};

enum aol_msg_id {
    AOL_MSG_ID_DEFAULT = 0,
    AOL_MSG_ID_WIFI_FENCE = 1,
    AOL_MSG_ID_CELL_FENCE = 2,
    AOL_MSG_ID_LOCATION = 3,
    AOL_MSG_ID_STATISTICS = 4,
    AOL_MSG_ID_CRITICAL_EVENT = 5,
    AOL_MSG_ID_MAX
};

struct aol_core_ctx {
    enum conap_scp_drv_type drv_type;
    struct msg_thread_ctx msg_ctx;
    struct conap_scp_drv_cb scp_test_cb;
    int status;
};

/*******************************************************************************/
/*                             M A C R O S                                     */
/*******************************************************************************/
#define MAX_BUF_LEN     (3 * 1024)
#define EVENT_ID_SCP_RESTORED     1

struct aol_core_ctx g_aol_ctx;
struct aol_core_rb g_rb2;
struct aol_core_rb g_rb1;

struct wifi_fence_info wf_info;
int wifi_remove_id;
struct cell_fence_info cf_info;
int cell_remove_id;
struct gnss_gfnc_location injecting_loc;

char g_buf1[MAX_BUF_LEN];
char g_buf2[MAX_BUF_LEN];
static int g_is_scp_ready = 0;
static int last_scp_ready = 0;

/*******************************************************************************/
/*                  F U N C T I O N   D E C L A R A T I O N S                  */
/*******************************************************************************/
static int opfunc_scp_register(struct msg_op_data *op);
static int opfunc_scp_unregister(struct msg_op_data *op);
static int opfunc_send_msg(struct msg_op_data *op);
static int opfunc_recv_msg(struct msg_op_data *op);

static void aol_core_state_change(int state);
static void aol_core_msg_notify(unsigned int msg_id, unsigned int *buf, unsigned int size);

static const msg_opid_func aol_core_opfunc[] = {
    [AOL_OPID_SCP_REGISTER] = opfunc_scp_register,
    [AOL_OPID_SCP_UNREGISTER] = opfunc_scp_unregister,
    [AOL_OPID_SEND_MSG] = opfunc_send_msg,
    [AOL_OPID_RECV_MSG] = opfunc_recv_msg,
};

/*******************************************************************************/
/*                              F U N C T I O N S                              */
/*******************************************************************************/
static void aol_push_message(struct aol_core_rb *rb, unsigned int type, unsigned int *buf)
{
    unsigned long flags;
    struct aol_rb_data *rb_data = NULL;

    // Get free space from ring buffer
    spin_lock_irqsave(&(rb->lock), flags);
    rb_data = aol_core_rb_pop_free(rb);

    if (rb_data) {
        if (type == AOL_MSG_ID_WIFI_FENCE) {
            memcpy(&(rb_data->raw_data.wifi_fence_raw), (struct wifi_fence_report_info *)buf,
                sizeof(struct wifi_fence_report_info));
        } else if (type == AOL_MSG_ID_CELL_FENCE) {
            memcpy(&(rb_data->raw_data.cell_fence_raw), (struct cell_fence_report_info *)buf,
                sizeof(struct cell_fence_report_info));
        } else if (type == AOL_MSG_ID_LOCATION) {
            memcpy(&(rb_data->raw_data.loc_raw), (struct gnss_gfnc_location *)buf,
                sizeof(struct gnss_gfnc_location));
        } else if (type == AOL_MSG_ID_STATISTICS) {
            memcpy(&(rb_data->raw_data.stat_raw), (struct aol_statistics_data *)buf,
                sizeof(struct aol_statistics_data));
        } else if (type == AOL_MSG_ID_CRITICAL_EVENT) {
            memcpy(&(rb_data->raw_data.critical_event_id), (int *)buf, sizeof(int));
        }
        rb_data->type = type;
        aol_core_rb_push_active(rb, rb_data);
    } else {
        pr_info("[%s] rb is NULL", __func__);
    }

    spin_unlock_irqrestore(&(rb->lock), flags);
}

static void aol_push_message2(struct aol_core_rb *rb, unsigned int type, unsigned int *buf)
{
    unsigned long flags;
    struct aol_rb_data *rb_data = NULL;

    // Get free space from ring buffer
    spin_lock_irqsave(&(rb->lock), flags);
    rb_data = aol_core_rb_pop_free(rb);

    if (rb_data) {
        if (type == AOL_CMD_ADD_WIFI_FENCE) {
            memcpy(&(rb_data->raw_data.wifiInfo2Scp), (struct wifi_fence_info *)buf,
                sizeof(struct wifi_fence_info));
        } else if (type == AOL_CMD_ADD_CELL_FENCE) {
            memcpy(&(rb_data->raw_data.cellInfo2Scp), (struct cell_fence_info *)buf,
                sizeof(struct cell_fence_info));
        } else if (type == AOL_CMD_SEND_COMMAND2SCP) {
            memcpy(&(rb_data->raw_data.para2Scp), (struct cmd_params *)buf,
                sizeof(struct cmd_params));
        } else if (type == AOL_CMD_INJECT_LOCATION2SCP) {
            memcpy(&(rb_data->raw_data.loc_raw), (struct gnss_gfnc_location *)buf,
                sizeof(struct gnss_gfnc_location));
        } else if (type == AOL_CMD_INJECT_WIFI_LIST2SCP) {
            memcpy(&(rb_data->raw_data.list2Scp), (struct WifiInfo *)buf,
                sizeof(WifiInfo)*MAX_WIFI_LIST_SIZE);
        } else if (type == AOL_CMD_REMOVE_WIFI_FENCE || type == AOL_CMD_REMOVE_CELL_FENCE) {
            memcpy(&(rb_data->raw_data.rm_fence_id), (int *)buf,
                sizeof(int));
        }
        rb_data->type = type;
        aol_core_rb_push_active(rb, rb_data);
    } else {
        pr_info("[%s] rb is NULL", __func__);
    }

    spin_unlock_irqrestore(&(rb->lock), flags);
}

static int is_scp_ready(void)
{
    int ret = 0;
    unsigned int retry = 10;
    struct aol_core_ctx *ctx = &g_aol_ctx;

    while (--retry > 0) {
        ret = conap_scp_is_drv_ready(ctx->drv_type);
        pr_info("[%s] ret:%d", __func__, ret);

        if (ret == 1) {
            g_is_scp_ready = 1;
            break;
        }

        msleep(20);
    }

    if (retry == 0) {
        g_is_scp_ready = 0;
        pr_info("SCP is not yet ready\n");
        return -1;
    }

    return ret;
}

/*******************************************************************************/
/*      O P          F U N C T I O N S                                         */
/*******************************************************************************/
static int opfunc_scp_register(struct msg_op_data *op)
{
    int ret = 0;
    struct aol_core_ctx *ctx = &g_aol_ctx;

    ret = conap_scp_register_drv(ctx->drv_type, &ctx->scp_test_cb);
    pr_info("SCP register drv_type=[%d], ret=[%d]", ctx->drv_type, ret);

    // Create ring buffer
    aol_core_rb_init(&g_rb1);
    aol_core_rb_init(&g_rb2);

    return ret;
}

static int opfunc_scp_unregister(struct msg_op_data *op)
{
    int ret = 0;
    struct aol_core_ctx *ctx = &g_aol_ctx;

    ret = conap_scp_unregister_drv(ctx->drv_type);
    pr_info("SCP unregister drv_type=[%d], ret=[%d]", ctx->drv_type, ret);

    // Destroy ring buffer
    aol_core_rb_deinit(&g_rb1);
    aol_core_rb_deinit(&g_rb2);

    return ret;
}

static int opfunc_send_msg(struct msg_op_data *op)
{
    int ret = 0;
    int type = -1;
    unsigned long flags;
    struct aol_rb_data *rb_data = NULL;
    int sz = 0;
    char *ptr = NULL;
    unsigned int cmd;
    struct aol_core_ctx *ctx = &g_aol_ctx;

    pr_info("[%s] is ready=[%d] ret=[%d]", __func__, g_is_scp_ready, ret);


    cmd = (unsigned int)op->op_data[0];
    memset(g_buf1, '\0', sizeof(g_buf1));

    spin_lock_irqsave(&(g_rb1.lock), flags);
    rb_data = aol_core_rb_pop_active(&g_rb1);

    if (rb_data == NULL) {
        spin_unlock_irqrestore(&(g_rb1.lock), flags);
        return -1;
    }

    type = rb_data->type;
    pr_info("[%s] cmd=[%d] type=[%d]", __func__, cmd, type);

    if (type == AOL_CMD_ADD_WIFI_FENCE) {
        pr_info("[%s] add wifi fence paras=[%d][%d][%d][%d]", __func__, rb_data->raw_data.wifiInfo2Scp.id, rb_data->raw_data.wifiInfo2Scp.transitionTypes,
            rb_data->raw_data.wifiInfo2Scp.cluster[0].level, rb_data->raw_data.wifiInfo2Scp.cluster[1].level);
        ptr = (char *)&(rb_data->raw_data.wifiInfo2Scp);
        sz = sizeof(struct wifi_fence_info);
    } else if (type == AOL_CMD_ADD_CELL_FENCE) {
        pr_info("[%s] add cell fence paras=[%d][%d]", __func__, rb_data->raw_data.cellInfo2Scp.id, rb_data->raw_data.cellInfo2Scp.transitionTypes);
        ptr = (char *)&(rb_data->raw_data.cellInfo2Scp);
        sz = sizeof(struct cell_fence_info);
    } else if (type == AOL_CMD_SEND_COMMAND2SCP) {
        //pr_info("[%s] report sz:%d", __func__, rb_data->raw_data.loc_raw);
        ptr = (char *)&(rb_data->raw_data.para2Scp);
        sz = sizeof(struct cmd_params);
    } else if (type == AOL_CMD_INJECT_LOCATION2SCP) {
        //pr_info("[%s] report sz:%d", __func__, rb_data->raw_data.loc_raw);
        ptr = (char *)&(rb_data->raw_data.loc_raw);
        sz = sizeof(struct gnss_gfnc_location);
    } else if (type == AOL_CMD_INJECT_WIFI_LIST2SCP) {
        ptr = (char *)&(rb_data->raw_data.list2Scp);
        sz = sizeof(WifiInfo) * MAX_WIFI_LIST_SIZE;
    } else if (type == AOL_CMD_REMOVE_WIFI_FENCE || type == AOL_CMD_REMOVE_CELL_FENCE) {
        pr_info("[%s] int para:%d\n", __func__, rb_data->raw_data.rm_fence_id);
        ptr = (char *)&(rb_data->raw_data.rm_fence_id);
        sz = sizeof(int);
    } else {
        aol_core_rb_push_free(&g_rb1, rb_data);
        spin_unlock_irqrestore(&(g_rb1.lock), flags);
        return -2;
    }

    memcpy(g_buf1, ptr, sz);
    // Free data
    aol_core_rb_push_free(&g_rb1, rb_data);
    spin_unlock_irqrestore(&(g_rb1.lock), flags);

    ret = conap_scp_send_message(ctx->drv_type, cmd,
                    (unsigned char *)&g_buf1, sz);

    pr_info("Send drv_type=[%d], cmd=[%d], ret=[%d]\n", ctx->drv_type, cmd, ret);
    return ret;
}

static int opfunc_recv_msg(struct msg_op_data *op)
{
    int ret = 0;
    int type = -1;
    unsigned long flags;
    struct aol_rb_data *rb_data = NULL;
    int sz = 0;
    char *ptr = NULL;
    unsigned int msg_id = (unsigned int)op->op_data[0];

    memset(g_buf2, '\0', sizeof(g_buf2));

    spin_lock_irqsave(&(g_rb2.lock), flags);
    rb_data = aol_core_rb_pop_active(&g_rb2);

    if (rb_data == NULL) {
        spin_unlock_irqrestore(&(g_rb2.lock), flags);
        return -1;
    }

    type = rb_data->type;
    pr_info("[%s] msg_id=[%d], type=[%d]\n", __func__, msg_id, type);

    if (type == AOL_MSG_ID_WIFI_FENCE) {
        pr_info("[%s] report id:%u trans:%u", __func__, rb_data->raw_data.wifi_fence_raw.id,
                rb_data->raw_data.wifi_fence_raw.transitionEvent);
        ptr = (char *)&(rb_data->raw_data.wifi_fence_raw);
        sz = sizeof(struct wifi_fence_report_info);
    } else if (type == AOL_MSG_ID_CELL_FENCE) {
        pr_info("[%s] report id:%u trans:%u", __func__, rb_data->raw_data.cell_fence_raw.id,
                rb_data->raw_data.cell_fence_raw.transitionEvent);
        ptr = (char *)&(rb_data->raw_data.cell_fence_raw);
        sz = sizeof(struct cell_fence_report_info);
    } else if (type == AOL_MSG_ID_LOCATION) {
        ptr = (char *)&(rb_data->raw_data.loc_raw);
        sz = sizeof(struct gnss_gfnc_location);
    } else if (type == AOL_MSG_ID_STATISTICS) {
        pr_info("[%s] loc cnt:%d %d\n", __func__, rb_data->raw_data.stat_raw.locCnt.total_req_cnt,
            rb_data->raw_data.stat_raw.locCnt.long_fence_cnt);
        ptr = (char *)&(rb_data->raw_data.stat_raw);
        sz = sizeof(struct aol_statistics_data);
    } else if (type == AOL_MSG_ID_CRITICAL_EVENT) {
        ptr = (char *)&(rb_data->raw_data.critical_event_id);
        sz = sizeof(int);
    } else {
        aol_core_rb_push_free(&g_rb2, rb_data);
        spin_unlock_irqrestore(&(g_rb2.lock), flags);
        return -2;
    }

    memcpy(g_buf2, ptr, sz);
    // Free data
    aol_core_rb_push_free(&g_rb2, rb_data);
    spin_unlock_irqrestore(&(g_rb2.lock), flags);

    pr_info("Send to netlink client, sz=[%d]\n", sz);
    aol_netlink_send_to_native("[AOL]", msg_id, g_buf2, sz);

    return ret;
}

/*******************************************************************************/
/*      C H R E T E S T     F U N C T I O N S                                  */
/*******************************************************************************/
void aol_core_msg_notify(unsigned int msg_id, unsigned int *buf, unsigned int size)
{
    int ret = 0;
    struct aol_core_ctx *ctx = &g_aol_ctx;
    unsigned int expect_size = 0;

    pr_info("[%s] dd msg_id=[%d]\n", __func__, msg_id);
    if (ctx->status == AOL_INACTIVE) {
        pr_info("aol extra ctx is inactive\n");
        return;
    }

    if (msg_id == AOL_MSG_ID_WIFI_FENCE) {
        expect_size = sizeof(struct wifi_fence_report_info);
        struct wifi_fence_report_info * ptr = (struct wifi_fence_report_info *) buf;
        pr_info("[%s] wf event %u %u\n", __func__, ptr->id, ptr->transitionEvent);
    } else if (msg_id == AOL_MSG_ID_CELL_FENCE) {
        expect_size = sizeof(struct cell_fence_report_info);
        struct cell_fence_report_info * ptr = (struct cell_fence_report_info *) buf;
        pr_info("[%s] cf event %u %u\n", __func__, ptr->id, ptr->transitionEvent);
    } else if (msg_id == AOL_MSG_ID_LOCATION) {
        expect_size = sizeof(struct gnss_gfnc_location);
    } else if (msg_id == AOL_MSG_ID_STATISTICS) {
        expect_size = sizeof(struct aol_statistics_data);
    } else if (msg_id == AOL_MSG_ID_CRITICAL_EVENT) {
        expect_size = sizeof(int);
    }

    if (expect_size != size) {
        pr_info("[%s] Buf size is unexpected, msg_id=[%u], expect size=[%u], recv size=[%u]\n",
            __func__, msg_id, expect_size, size);
        return;
    }

    aol_push_message(&g_rb2, msg_id, buf);

    ret = msg_thread_send_1(&ctx->msg_ctx, AOL_OPID_RECV_MSG, msg_id);

    if (ret)
        pr_info("[%s] Notify recv msg fail, ret=[%d]\n", __func__, ret);
}

void aol_core_state_change(int state)
{
    struct aol_core_ctx *ctx = &g_aol_ctx;
    int evt_id;
    pr_info("[%s] reason=[%d], now=[%d]", __func__, state, g_is_scp_ready);

    if (ctx->status == AOL_INACTIVE) {
        pr_info("aol extra ctx is inactive\n");
        return;
    }

    // state = 1: scp ready
    // state = 0: scp stop
    if (g_is_scp_ready != state) {
        last_scp_ready = g_is_scp_ready;
        g_is_scp_ready = state;
    }

    if (!last_scp_ready && g_is_scp_ready) {
        evt_id = EVENT_ID_SCP_RESTORED;
        aol_core_msg_notify(AOL_MSG_ID_CRITICAL_EVENT, &evt_id, sizeof(int));
    }
}

static int aol_core_handler(int cmd, void *data)
{
    int ret = 0;
    struct aol_core_ctx *ctx = &g_aol_ctx;

    pr_info("[%s] Get cmd: %d\n", __func__, cmd);

    if (ctx->status == AOL_INACTIVE) {
        pr_info("aol extra ctx is inactive\n");
        return -1;
    }

    if (!g_is_scp_ready) {
        ret = is_scp_ready();
        pr_info("[%s] is ready=[%d] ret=[%d]", __func__, g_is_scp_ready, ret);

        if (ret <= 0)
            return -1;
    }
    aol_push_message2(&g_rb1, cmd, data);
    ret = msg_thread_send_1(&ctx->msg_ctx, AOL_OPID_SEND_MSG, cmd);

    if (ret)
        pr_info("[%s] Send to msg thread fail, ret=[%d]\n", __func__, ret);

    return ret;
}

static int aol_core_bind(void)
{
    int ret = 0;
    struct aol_core_ctx *ctx = &g_aol_ctx;

    memset(&g_aol_ctx, 0, sizeof(struct aol_core_ctx));

    // Create thread
    ret = msg_thread_init(&ctx->msg_ctx, "k_geofence_thread",
            aol_core_opfunc, AOL_OPID_MAX);

    if (ret) {
        pr_info("aol extra thread init fail, ret=[%d]\n", ret);
        return -1;
    }

    ctx->drv_type = DRV_TYPE_EXTRA;
    ctx->status = AOL_ACTIVE;
    ctx->scp_test_cb.conap_scp_msg_notify_cb = aol_core_msg_notify;
    ctx->scp_test_cb.conap_scp_state_notify_cb = aol_core_state_change;

    ret = msg_thread_send(&ctx->msg_ctx, AOL_OPID_SCP_REGISTER);

    if (ret)
        pr_info("[%s] Send to msg thread fail, ret=[%d]\n", __func__, ret);

    return ret;
}

static int aol_core_unbind(void)
{
    return 0;
}

int aolextra_core_init(void)
{
    struct netlink_event_cb nl_cb;

    // Init netlink
    nl_cb.aol_bind = aol_core_bind;
    nl_cb.aol_unbind = aol_core_unbind;
    nl_cb.aol_handler = aol_core_handler;
    return aol_netlink_init(&nl_cb);
}

void aolextra_core_deinit(void)
{
    pr_info("[%s]\n", __func__);
    aol_netlink_deinit();
}