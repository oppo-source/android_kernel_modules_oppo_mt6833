#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <net/genetlink.h>
#include "aolextra_netlink.h"
#include "aolextra_ring_buffer.h"
/*******************************************************************************/
/*                             M A C R O S                                     */
/*******************************************************************************/
#define AOL_EXTRA_NETLINK_FAMILY_NAME "AOL_EXTRA"
#define AOL_PKT_SIZE NLMSG_DEFAULT_SIZE
#define MAX_BIND_PROCESS    4

#ifndef GENL_ID_GENERATE
#define GENL_ID_GENERATE    0
#endif

#define AOL_EXTRA_ATTR_MAX       (__AOL_ATTR_MAX - 1)

#define EPS 1e-6

/*******************************************************************************/
/*                             D A T A   T Y P E S                             */
/*******************************************************************************/
// Define netlink command id
enum aol_netlink_cmd_id {
    _AOL_NL_CMD_INVALID,
    AOL_NL_CMD_BIND,
    AOL_NL_CMD_SEND,
    AOL_NL_CMD_START_DATA_TRANS,
    AOL_NL_CMD_STOP_DATA_TRANS,
    AOL_NL_CMD_ADD_WIFI_FENCE,
    AOL_NL_CMD_REMOVE_WIFI_FENCE,
    AOL_NL_CMD_ADD_CELL_FENCE,
    AOL_NL_CMD_REMOVE_CELL_FENCE,
    AOL_NL_CMD_SEND_SCP_COMMAND,
    AOL_NL_CMD_INJECT_LOCATION2SCP,
    AOL_NL_CMD_INJECT_WIFI_LIST2SCP,
    _AOL_NL_CMD_MAX,
};

// Define netlink message formats
enum aol_attr {
    _AOL_ATTR_DEFAULT,
    AOL_ATTR_PORT,
    AOL_ATTR_HEADER,
    AOL_ATTR_MSG,
    AOL_ATTR_MSG_ID,
    AOL_ATTR_MSG_SIZE,
    AOL_ATTR_WIFI_FENCE_INFO,
    AOL_ATTR_WIFI_FENCE_ID,
    AOL_ATTR_CELL_FENCE_INFO,
    AOL_ATTR_CELL_FENCE_ID,
    AOL_ATTR_COMMAND_ID,
    AOL_ATTR_INJECT_LOCATION,
    AOL_ATTR_INJECT_WIFI_LIST,
    _AOL_ATTR_MAX,
};

enum wifiInfo_attr {
    _WIFI_INFO_ATTR_DEFAULT,
    WIFI_INFO_ATTR_BSSID,
    WIFI_INFO_ATTR_LEVEL,
    _WIFI_INFO_ATTR_MAX,
};

enum wifiFenceInfo_attr {
    _WIFI_FENCE_INFO_ATTR_DEFAULT,
    WIFI_FENCE_INFO_ATTR_ID,
    WIFI_FENCE_INFO_ATTR_TRANSITIONTYPES,
    WIFI_FENCE_INFO_ATTR_LOITERING_DELAY,
    WIFI_FENCE_INFO_ATTR_NOTIFYRESPONSIVENESS,
    WIFI_FENCE_INFO_ATTR_CLUSTER,
    _WIFI_FENCE_INFO_ATTR_MAX,
};

enum cellInfo_attr {
    _CELL_INFO_ATTR_DEFAULT,
    CELL_INFO_ATTR_RAT,
    CELL_INFO_ATTR_LEVEL,
    CELL_INFO_ATTR_CELL_ID,
    _CELL_INFO_ATTR_MAX,
};

enum cellFenceInfo_attr {
    _CELL_FENCE_INFO_ATTR_DEFAULT,
    CELL_FENCE_INFO_ATTR_ID,
    CELL_FENCE_INFO_ATTR_TRANSITIONTYPES,
    CELL_FENCE_INFO_ATTR_LOITERING_DELAY,
    CELL_FENCE_INFO_ATTR_NOTIFYRESPONSIVENESS,
    CELL_FENCE_INFO_ATTR_CLUSTER,
    _CELL_FENCE_INFO_ATTR_MAX,
};

enum sendCommand_attr {
    _SEND_COMMAND_ATTR_DEFAULT,
    SEND_COMMAND_ATTR_COMMAND,
    _SEND_COMMAND_ATTR_MAX,
};

enum injectLocation_attr {
    _INJECT_LOCATION_ATTR_DEFAULT,
    INJECT_LOCATION_ATTR_LOCATION,
    _INJECT_LOCATION_ATTR_MAX,
};

enum injectWifiList_attr {
    _INJECT_WIFI_LIST_ATTR_DEFAULT,
    INJECT_WIFI_LIST_ATTR_LIST,
    _INJECT_WIFI_LIST_ATTR_MAX,
};

enum link_status {
    LINK_STATUS_INIT,
    LINK_STATUS_INIT_DONE,
    LINK_STATUS_MAX,
};

struct aol_netlink_ctx {
    pid_t bind_pid;
    struct genl_family gnl_family;
    unsigned int seqnum;
    struct mutex nl_lock;
    enum link_status status;
    struct netlink_event_cb cb;
};
/*******************************************************************************/
/*                  F U N C T I O N   D E C L A R A T I O N S                  */
/*******************************************************************************/
static int aol_nl_bind(struct sk_buff *skb, struct genl_info *info);
static int aol_nl_add_wifi_fence(struct sk_buff *skb, struct genl_info *info);
static int aol_nl_remove_wifi_fence(struct sk_buff *skb, struct genl_info *info);
static int aol_nl_add_cell_fence(struct sk_buff *skb, struct genl_info *info);
static int aol_nl_remove_cell_fence(struct sk_buff *skb, struct genl_info *info);
static int aol_nl_send_scp_command(struct sk_buff *skb, struct genl_info *info);
static int aol_nl_inject_location(struct sk_buff *skb, struct genl_info *info);
static int aol_nl_inject_wifi_list(struct sk_buff *skb, struct genl_info *info);
static int aol_nl_start_data_transmission(struct sk_buff *skb, struct genl_info *info);
static int aol_nl_stop_data_transmission(struct sk_buff *skb, struct genl_info *info);
/*******************************************************************************/
/*                  G L O B A L  V A R I A B L E                               */
/*******************************************************************************/
/* Attribute policy */
static struct nla_policy aol_genl_policy[_AOL_ATTR_MAX + 1] = {
    [AOL_ATTR_PORT] = {.type = NLA_U32},
    [AOL_ATTR_HEADER] = {.type = NLA_NUL_STRING},
    [AOL_ATTR_MSG] = {.type = NLA_NUL_STRING},
    [AOL_ATTR_MSG_ID] = {.type = NLA_U32},
    [AOL_ATTR_MSG_SIZE] = {.type = NLA_U32},
    [AOL_ATTR_WIFI_FENCE_INFO] = {.type = NLA_NESTED},
    [AOL_ATTR_WIFI_FENCE_ID] = {.type = NLA_U32},
    [AOL_ATTR_CELL_FENCE_INFO] = {.type = NLA_NESTED},
    [AOL_ATTR_CELL_FENCE_ID] = {.type = NLA_U32},
    [AOL_ATTR_COMMAND_ID] = {.type = NLA_NESTED},
    [AOL_ATTR_INJECT_LOCATION] = {.type = NLA_NESTED},
    [AOL_ATTR_INJECT_WIFI_LIST] = {.type = NLA_NESTED},
};

static struct nla_policy wifi_info_policy[_WIFI_INFO_ATTR_MAX + 1] = {
    [WIFI_INFO_ATTR_BSSID] = {.type = NLA_NUL_STRING},
    [WIFI_INFO_ATTR_LEVEL] = {.type = NLA_U8},
};

static struct nla_policy wifi_fence_info_policy[_WIFI_FENCE_INFO_ATTR_MAX + 1] = {
    [WIFI_FENCE_INFO_ATTR_ID] = {.type = NLA_U8},
    [WIFI_FENCE_INFO_ATTR_TRANSITIONTYPES] = {.type = NLA_U8},
    [WIFI_FENCE_INFO_ATTR_LOITERING_DELAY] = {.type = NLA_U32},
    [WIFI_FENCE_INFO_ATTR_NOTIFYRESPONSIVENESS] = {.type = NLA_U32},
    [WIFI_FENCE_INFO_ATTR_CLUSTER] = {.type = NLA_NESTED_ARRAY, .nested_policy = wifi_info_policy, .len = MAX_WIFI_CLUSTER_SIZE},
};

static struct nla_policy cell_info_policy[_CELL_INFO_ATTR_MAX + 1] = {
    [CELL_INFO_ATTR_RAT] = {.type = NLA_U8},
    [CELL_INFO_ATTR_LEVEL] = {.type = NLA_U8},
    [CELL_INFO_ATTR_CELL_ID] = {.type = NLA_U64},
};

static struct nla_policy cell_fence_info_policy[_CELL_FENCE_INFO_ATTR_MAX + 1] = {
    [CELL_FENCE_INFO_ATTR_ID] = {.type = NLA_U8},
    [CELL_FENCE_INFO_ATTR_TRANSITIONTYPES] = {.type = NLA_U8},
    [CELL_FENCE_INFO_ATTR_LOITERING_DELAY] = {.type = NLA_U32},
    [CELL_FENCE_INFO_ATTR_NOTIFYRESPONSIVENESS] = {.type = NLA_U32},
    [CELL_FENCE_INFO_ATTR_CLUSTER] = {.type = NLA_NESTED_ARRAY, .nested_policy = cell_info_policy, .len = MAX_CELL_CLUSTER_SIZE},
};

static struct nla_policy send_command_policy[_SEND_COMMAND_ATTR_MAX + 1] = {
    [SEND_COMMAND_ATTR_COMMAND] = {.type = NLA_UNSPEC, .len = sizeof(struct cmd_params)},
};

static struct nla_policy inject_location_policy[_INJECT_LOCATION_ATTR_MAX + 1] = {
    [INJECT_LOCATION_ATTR_LOCATION] = {.type = NLA_UNSPEC, .len = sizeof(struct gnss_gfnc_location)},
};

static struct nla_policy inject_wifi_list_policy[_INJECT_WIFI_LIST_ATTR_MAX + 1] = {
    [INJECT_WIFI_LIST_ATTR_LIST] = {.type = NLA_NESTED_ARRAY, .nested_policy = wifi_info_policy, .len = MAX_WIFI_LIST_SIZE},
};

/* Operation definition */
static struct genl_ops aol_gnl_ops_array[] = {
    {
        .cmd = AOL_NL_CMD_BIND,
        .flags = 0,
        .policy = aol_genl_policy,
        .doit = aol_nl_bind,
        .dumpit = NULL,
    },
    {
        .cmd = AOL_NL_CMD_ADD_WIFI_FENCE,
        .flags = 0,
        .policy = aol_genl_policy,
        .doit = aol_nl_add_wifi_fence,
        .dumpit = NULL,
    },
    {
        .cmd = AOL_NL_CMD_REMOVE_WIFI_FENCE,
        .flags = 0,
        .policy = aol_genl_policy,
        .doit = aol_nl_remove_wifi_fence,
        .dumpit = NULL,
    },
    {
        .cmd = AOL_NL_CMD_ADD_CELL_FENCE,
        .flags = 0,
        .policy = aol_genl_policy,
        .doit = aol_nl_add_cell_fence,
        .dumpit = NULL,
    },
    {
        .cmd = AOL_NL_CMD_REMOVE_CELL_FENCE,
        .flags = 0,
        .policy = aol_genl_policy,
        .doit = aol_nl_remove_cell_fence,
        .dumpit = NULL,
    },
    {
        .cmd = AOL_NL_CMD_SEND_SCP_COMMAND,
        .flags = 0,
        .policy = aol_genl_policy,
        .doit = aol_nl_send_scp_command,
        .dumpit = NULL,
    },
    {
        .cmd = AOL_NL_CMD_INJECT_LOCATION2SCP,
        .flags = 0,
        .policy = aol_genl_policy,
        .doit = aol_nl_inject_location,
        .dumpit = NULL,
    },
    {
        .cmd = AOL_NL_CMD_INJECT_WIFI_LIST2SCP,
        .flags = 0,
        .policy = aol_genl_policy,
        .doit = aol_nl_inject_wifi_list,
        .dumpit = NULL,
    },
    {
        .cmd = AOL_NL_CMD_START_DATA_TRANS,
        .flags = 0,
        .policy = aol_genl_policy,
        .doit = aol_nl_start_data_transmission,
        .dumpit = NULL,
    },
    {
        .cmd = AOL_NL_CMD_STOP_DATA_TRANS,
        .flags = 0,
        .policy = aol_genl_policy,
        .doit = aol_nl_stop_data_transmission,
        .dumpit = NULL,
    },
};

const struct genl_multicast_group g_mcgrps2 = {
    .name = "AOL_EXTRA",
};

struct aol_netlink_ctx g_aol_netlink_ctx = {
    .gnl_family = {
        .id = GENL_ID_GENERATE,
        .hdrsize = 0,
        .name = AOL_EXTRA_NETLINK_FAMILY_NAME,
        .version = 1,
        .maxattr = _AOL_ATTR_MAX,
        .ops = aol_gnl_ops_array,
        .n_ops = ARRAY_SIZE(aol_gnl_ops_array),
    },
    .status = LINK_STATUS_INIT,
    .seqnum = 0,
};

struct aol_netlink_ctx *g_ctx2= &g_aol_netlink_ctx;
static bool g_already_bind;

/*******************************************************************************/
/*                              F U N C T I O N S                              */
/*******************************************************************************/
static int aol_nl_bind(struct sk_buff *skb, struct genl_info *info)
{
    struct nlattr *port_na;
    unsigned int port;

    pr_info("[%s]\n", __func__);

    if (info == NULL) {
        goto out;
    }

    if (mutex_lock_killable(&g_ctx2->nl_lock)) {
        return -1;
    }

    port_na = info->attrs[AOL_ATTR_PORT];
    if (port_na) {
        port = (unsigned int)nla_get_u32(port_na);
    } else {
        pr_info("[%s] No port_na found\n", __func__);
        mutex_unlock(&g_ctx2->nl_lock);
        return -1;
    }

    if (g_already_bind) {
        pr_info("[%s] Already bind before, only change port=[%d]", __func__, port);
        g_ctx2->bind_pid = port;
        mutex_unlock(&g_ctx2->nl_lock);
        goto out;
    }

    g_ctx2->bind_pid = port;
    g_already_bind = true;

    mutex_unlock(&g_ctx2->nl_lock);

    if (g_ctx2&& g_ctx2->cb.aol_bind) {
        g_ctx2->cb.aol_bind();
    }

out:
    return 0;
}

static int aol_nl_add_wifi_fence(struct sk_buff * skb, struct genl_info * info)
{
    int ret = 0;
    struct nlattr *attr_info = NULL;
    struct nlattr *test_attr[_WIFI_FENCE_INFO_ATTR_MAX + 1];
    struct wifi_fence_info params = {0};

    pr_info("[%s]", __func__);
    if (mutex_lock_killable(&g_ctx2->nl_lock))
        return -1;

    attr_info = info->attrs[AOL_ATTR_WIFI_FENCE_INFO];

    if (attr_info) {
        ret = nla_parse_nested_deprecated(test_attr, _WIFI_FENCE_INFO_ATTR_MAX,
                        attr_info, wifi_fence_info_policy, NULL);
        if (ret < 0) {
            pr_info("[%s] Fail to parse nested attributes, ret=[%d]\n", __func__, ret);
            mutex_unlock(&g_ctx2->nl_lock);
            return -1;
        }
        // GPS
        if (test_attr[WIFI_FENCE_INFO_ATTR_ID]) {
            params.id =
                nla_get_u8(test_attr[WIFI_FENCE_INFO_ATTR_ID]);
        }
        if (test_attr[WIFI_FENCE_INFO_ATTR_CLUSTER]) {
            int len = nla_memcpy(&(params.cluster), test_attr[WIFI_FENCE_INFO_ATTR_CLUSTER], sizeof(WifiInfo) * MAX_WIFI_CLUSTER_SIZE);
            pr_info("[%s] clu length:%d\n", __func__, len);
        }
        if (test_attr[WIFI_FENCE_INFO_ATTR_LOITERING_DELAY]) {
            params.loiteringDelay =
                nla_get_u32(test_attr[WIFI_FENCE_INFO_ATTR_LOITERING_DELAY]);
            pr_info("[%s] loit value:%d\n", __func__, params.loiteringDelay);
        }
        if (test_attr[WIFI_FENCE_INFO_ATTR_NOTIFYRESPONSIVENESS]) {
            params.notifyResponsiveness =
                nla_get_u32(test_attr[WIFI_FENCE_INFO_ATTR_NOTIFYRESPONSIVENESS]);
            pr_info("[%s] noti value:%d\n", __func__, params.notifyResponsiveness);
        }
        if (test_attr[WIFI_FENCE_INFO_ATTR_TRANSITIONTYPES]) {
            params.transitionTypes =
                nla_get_u8(test_attr[WIFI_FENCE_INFO_ATTR_TRANSITIONTYPES]);
            pr_info("[%s] tran value:%d\n", __func__, params.transitionTypes);
        }
    } else {
        pr_info("[%s] No wifi fence info found\n", __func__);
        mutex_unlock(&g_ctx2->nl_lock);
        return -1;
    }

    mutex_unlock(&g_ctx2->nl_lock);

    pr_info("[%s] add wifi fence param: [%d, %d, %d, %d, %d]\n", __func__,
        params.id, params.cluster[0].level, params.cluster[1].level, params.notifyResponsiveness,
            params.transitionTypes);

    if (g_ctx2&& g_ctx2->cb.aol_handler) {
        pr_info("[%s] call aol_handler: add wifi fence\n", __func__);
        g_ctx2->cb.aol_handler(AOL_CMD_ADD_WIFI_FENCE, (void *)&params);
    }

    return 0;
}

static int aol_nl_remove_wifi_fence(struct sk_buff * skb, struct genl_info * info)
{
    struct nlattr *attr_info = NULL;
    int id;

    pr_info("[%s]", __func__);
    if (mutex_lock_killable(&g_ctx2->nl_lock)) {
        return -1;
    }

    attr_info = info->attrs[AOL_ATTR_WIFI_FENCE_ID];

    if (attr_info) {
        id = nla_get_u32(attr_info);
    } else {
        pr_info("[%s] No remove info found\n", __func__);
        mutex_unlock(&g_ctx2->nl_lock);
        return -1;
    }

    mutex_unlock(&g_ctx2->nl_lock);

    pr_info("[%s] remove wifi fence param: [%d]\n", __func__, id);

    if (g_ctx2&& g_ctx2->cb.aol_handler) {
        pr_info("[%s] call aol_handler: remove wifi fence\n", __func__);
        g_ctx2->cb.aol_handler(AOL_CMD_REMOVE_WIFI_FENCE, (void *)&id);
    }

    return 0;
}

static int aol_nl_add_cell_fence(struct sk_buff * skb, struct genl_info * info)
{
    int ret = 0;
    struct nlattr *attr_info = NULL;
    struct nlattr *test_attr[_CELL_FENCE_INFO_ATTR_MAX + 1];
    struct cell_fence_info params = {0};

    pr_info("[%s]", __func__);
    if (mutex_lock_killable(&g_ctx2->nl_lock))
        return -1;

    attr_info = info->attrs[AOL_ATTR_CELL_FENCE_INFO];

    if (attr_info) {
        ret = nla_parse_nested_deprecated(test_attr, _CELL_FENCE_INFO_ATTR_MAX,
                        attr_info, cell_fence_info_policy, NULL);
        if (ret < 0) {
            pr_info("[%s] Fail to parse nested attributes, ret=[%d]\n", __func__, ret);
            mutex_unlock(&g_ctx2->nl_lock);
            return -1;
        }
        // GPS
        if (test_attr[CELL_FENCE_INFO_ATTR_ID]) {
            params.id =
                nla_get_u8(test_attr[CELL_FENCE_INFO_ATTR_ID]);
        }
        if (test_attr[CELL_FENCE_INFO_ATTR_TRANSITIONTYPES]) {
            params.transitionTypes =
                nla_get_u8(test_attr[CELL_FENCE_INFO_ATTR_TRANSITIONTYPES]);
            pr_info("[%s] tran value:%d\n", __func__, params.transitionTypes);
        }
        if (test_attr[CELL_FENCE_INFO_ATTR_CLUSTER]) {
            int len = nla_memcpy(&(params.cluster), test_attr[CELL_FENCE_INFO_ATTR_CLUSTER], sizeof(CellInfo) * MAX_CELL_CLUSTER_SIZE);
            pr_info("[%s] clu length:%d\n", __func__, len);
        }
        if (test_attr[CELL_FENCE_INFO_ATTR_LOITERING_DELAY]) {
            params.loiteringDelay =
                nla_get_u32(test_attr[CELL_FENCE_INFO_ATTR_LOITERING_DELAY]);
            pr_info("[%s] loit value:%d\n", __func__, params.loiteringDelay);
        }
        if (test_attr[CELL_FENCE_INFO_ATTR_NOTIFYRESPONSIVENESS]) {
            params.notifyResponsiveness =
                nla_get_u32(test_attr[CELL_FENCE_INFO_ATTR_NOTIFYRESPONSIVENESS]);
            pr_info("[%s] noti value:%d\n", __func__, params.notifyResponsiveness);
        }
    } else {
        pr_info("[%s] No cell fence info found\n", __func__);
        mutex_unlock(&g_ctx2->nl_lock);
        return -1;
    }

    mutex_unlock(&g_ctx2->nl_lock);

    pr_info("[%s] add cell fence param: [%d, %llu, %d, %d, %d]\n", __func__,
        params.id, params.cluster[0].cellId, params.cluster[0].level, params.notifyResponsiveness,
            params.transitionTypes);

    if (g_ctx2&& g_ctx2->cb.aol_handler) {
        pr_info("[%s] call aol_handler: add cell fence\n", __func__);
        g_ctx2->cb.aol_handler(AOL_CMD_ADD_CELL_FENCE, (void *)&params);
    }

    return 0;
}

static int aol_nl_remove_cell_fence(struct sk_buff * skb, struct genl_info * info)
{
    struct nlattr *attr_info = NULL;
    int id;

    pr_info("[%s]", __func__);
    if (mutex_lock_killable(&g_ctx2->nl_lock))
        return -1;

    attr_info = info->attrs[AOL_ATTR_CELL_FENCE_ID];

    if (attr_info) {
        id = nla_get_u32(attr_info);
    } else {
        pr_info("[%s] No remove info found\n", __func__);
        mutex_unlock(&g_ctx2->nl_lock);
        return -1;
    }

    mutex_unlock(&g_ctx2->nl_lock);

    pr_info("[%s] remove cell fence param: [%d]\n", __func__, id);

    if (g_ctx2&& g_ctx2->cb.aol_handler) {
        pr_info("[%s] call aol_handler: remove cell fence\n", __func__);
        g_ctx2->cb.aol_handler(AOL_CMD_REMOVE_CELL_FENCE, (void *)&id);
    }

    return 0;
}

static int aol_nl_send_scp_command(struct sk_buff * skb, struct genl_info * info)
{
    int ret = 0;
    struct nlattr *attr_info = NULL;
    struct nlattr *test_attr[_SEND_COMMAND_ATTR_MAX + 1];
    struct cmd_params params = {0};

    pr_info("[%s]", __func__);
    if (mutex_lock_killable(&g_ctx2->nl_lock))
        return -1;

    attr_info = info->attrs[AOL_ATTR_COMMAND_ID];

    if (attr_info) {
        ret = nla_parse_nested_deprecated(test_attr, _SEND_COMMAND_ATTR_MAX,
                        attr_info, send_command_policy, NULL);
        if (ret < 0) {
            pr_info("[%s] Fail to parse nested attributes, ret=[%d]\n", __func__, ret);
            mutex_unlock(&g_ctx2->nl_lock);
            return -1;
        }
        // GPS
        if (test_attr[SEND_COMMAND_ATTR_COMMAND]) {
            int len = nla_memcpy(&params, test_attr[SEND_COMMAND_ATTR_COMMAND], sizeof(struct cmd_params));
            pr_info("[%s] loc length:%d\n", __func__, len);
        }
    } else {
        pr_info("[%s] No cmd info found\n", __func__);
        mutex_unlock(&g_ctx2->nl_lock);
        return -1;
    }

    mutex_unlock(&g_ctx2->nl_lock);

    pr_info("[%s] send cmd param: [%d]\n", __func__, params.cmd);

    if (g_ctx2&& g_ctx2->cb.aol_handler) {
        pr_info("[%s] call aol_handler: send scp cmd\n", __func__);
        g_ctx2->cb.aol_handler(AOL_CMD_SEND_COMMAND2SCP, (void *)&params);
    }

    return 0;
}

static int aol_nl_inject_location(struct sk_buff * skb, struct genl_info * info)
{
    int ret = 0;
    struct nlattr *attr_info = NULL;
    struct nlattr *test_attr[_INJECT_LOCATION_ATTR_MAX + 1];
    struct gnss_gfnc_location params = {0};

    pr_info("[%s]", __func__);
    if (mutex_lock_killable(&g_ctx2->nl_lock)) {
        return -1;
    }

    attr_info = info->attrs[AOL_ATTR_INJECT_LOCATION];

    if (attr_info) {
        ret = nla_parse_nested_deprecated(test_attr, _INJECT_LOCATION_ATTR_MAX,
                        attr_info, inject_location_policy, NULL);
        if (ret < 0) {
            pr_info("[%s] Fail to parse nested attributes, ret=[%d]\n", __func__, ret);
            mutex_unlock(&g_ctx2->nl_lock);
            return -1;
        }
        // GPS
        if (test_attr[INJECT_LOCATION_ATTR_LOCATION]) {
            int len = nla_memcpy(&params, test_attr[INJECT_LOCATION_ATTR_LOCATION], sizeof(struct gnss_gfnc_location));
            pr_info("[%s] loc length:%d\n", __func__, len);
        }
    } else {
        pr_info("[%s] No inject location info found\n", __func__);
        mutex_unlock(&g_ctx2->nl_lock);
        return -1;
    }

    mutex_unlock(&g_ctx2->nl_lock);

    pr_info("[%s] inject location param: [%d, %lld]\n", __func__,
        params.flags, params.timestamp);

    if (g_ctx2&& g_ctx2->cb.aol_handler) {
        pr_info("[%s] call aol_handler: inject location\n", __func__);
        g_ctx2->cb.aol_handler(AOL_CMD_INJECT_LOCATION2SCP, (void *)&params);
    }

    return 0;
}

static int aol_nl_inject_wifi_list(struct sk_buff * skb, struct genl_info * info)
{
    int ret = 0;
    struct nlattr *attr_info = NULL;
    struct nlattr *test_attr[_INJECT_WIFI_LIST_ATTR_MAX + 1];
    WifiInfo params[MAX_WIFI_LIST_SIZE] = {0};

    pr_info("[%s]", __func__);
    if (mutex_lock_killable(&g_ctx2->nl_lock)) {
        return -1;
    }

    attr_info = info->attrs[AOL_ATTR_INJECT_WIFI_LIST];

    if (attr_info) {
        ret = nla_parse_nested_deprecated(test_attr, _INJECT_WIFI_LIST_ATTR_MAX,
                        attr_info, inject_wifi_list_policy, NULL);
        if (ret < 0) {
            pr_info("[%s] Fail to parse nested attributes, ret=[%d]\n", __func__, ret);
            mutex_unlock(&g_ctx2->nl_lock);
            return -1;
        }
        // WIFI List
        if (test_attr[INJECT_WIFI_LIST_ATTR_LIST]) {
            int len = nla_memcpy(&params, test_attr[INJECT_WIFI_LIST_ATTR_LIST], sizeof(WifiInfo) * MAX_WIFI_LIST_SIZE);
            pr_info("[%s] list length:%d\n", __func__, len);
        }
    } else {
        pr_info("[%s] No inject list info found\n", __func__);
        mutex_unlock(&g_ctx2->nl_lock);
        return -1;
    }

    mutex_unlock(&g_ctx2->nl_lock);

    pr_info("[%s] inject wifi list param: [%d, %d]\n", __func__,
        params[0].level, params[1].level);

    if (g_ctx2&& g_ctx2->cb.aol_handler) {
        pr_info("[%s] call aol_handler: inject wifi list\n", __func__);
        g_ctx2->cb.aol_handler(AOL_CMD_INJECT_WIFI_LIST2SCP, (void *)&params);
    }

    return 0;
}

static int aol_nl_start_data_transmission(struct sk_buff *skb, struct genl_info *info)
{
    if (g_ctx2 && g_ctx2->cb.aol_handler) {
        g_ctx2->cb.aol_handler(AOL_CMD_START_DATA_TRANS, NULL);
    }
    return 0;
}

static int aol_nl_stop_data_transmission(struct sk_buff *skb, struct genl_info *info)
{
    if (g_ctx2 && g_ctx2->cb.aol_handler) {
        g_ctx2->cb.aol_handler(AOL_CMD_STOP_DATA_TRANS, NULL);
    }
    return 0;
}

static int aol_netlink_msg_send(char *tag, unsigned int msg_id, char *buf, unsigned int length,
            pid_t pid, unsigned int seq)
{
    struct sk_buff *skb;
    void *msg_head = NULL;
    int ret = 0;

    // Allocate a generic netlink message buffer
    skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
    if (skb != NULL) {
        // Create message header
        msg_head = genlmsg_put(skb, 0, seq, &g_ctx2->gnl_family, 0, AOL_NL_CMD_SEND);
        if (msg_head == NULL) {
            pr_info("[%s] genlmsg_put fail\n", __func__);
            nlmsg_free(skb);
            return -EMSGSIZE;
        }

        // Add message attribute and content
        ret = nla_put_string(skb, AOL_ATTR_HEADER, tag);
        if (ret != 0) {
            pr_info("[%s] nla_put_string header fail, ret=[%d]\n", __func__, ret);
            genlmsg_cancel(skb, msg_head);
            nlmsg_free(skb);
            return ret;
        }

        if (length) {
            ret = nla_put(skb, AOL_ATTR_MSG, length, buf);
            if (ret != 0) {
                pr_info("[%s] nla_put fail, ret=[%d]\n", __func__, ret);
                genlmsg_cancel(skb, msg_head);
                nlmsg_free(skb);
                return ret;
            }

            ret = nla_put_u32(skb, AOL_ATTR_MSG_ID, msg_id);
            if (ret != 0) {
                pr_info("[%s] nal_put_u32 fail, ret=[%d]\n", __func__, ret);
                genlmsg_cancel(skb, msg_head);
                nlmsg_free(skb);
                return ret;
            }

            ret = nla_put_u32(skb, AOL_ATTR_MSG_SIZE, length);
            if (ret != 0) {
                pr_info("[%s] nal_put_u32 fail, ret=[%d]\n", __func__, ret);
                genlmsg_cancel(skb, msg_head);
                nlmsg_free(skb);
                return ret;
            }
        }

        // Finalize the message
        genlmsg_end(skb, msg_head);

        // Send message
        ret = genlmsg_unicast(&init_net, skb, pid);
        if (ret == 0)
            pr_info("[%s] Send msg succeed\n", __func__);
    } else {
        pr_info("[%s] Allocate message error\n", __func__);
        ret = -ENOMEM;
    }

    return ret;
}

static int aol_netlink_send_to_native_internal(char *tag,
                unsigned int msg_id, char *buf, unsigned int length)
{
    int ret = 0;
    unsigned int retry;

    ret = aol_netlink_msg_send(tag, msg_id, buf, length, g_ctx2->bind_pid, g_ctx2->seqnum);

    if (ret != 0) {
        pr_info("[%s] genlmsg_unicast fail, ret=[%d], pid=[%d], seq=[%d], tag=[%s]\n",
                __func__, ret, g_ctx2->bind_pid, g_ctx2->seqnum, tag);

        if (ret == -EAGAIN) {
            retry = 0;

            while (retry < 100 && ret == -EAGAIN) {
                msleep(20);
                ret = aol_netlink_msg_send(tag, msg_id, buf, length,
                                g_ctx2->bind_pid, g_ctx2->seqnum);
                retry++;
                pr_info("[%s] genlmsg_unicast retry(%d)...: ret=[%d] pid=[%d] seq=[%d] tag=[%s]\n",
                    __func__, retry, ret, g_ctx2->bind_pid, g_ctx2->seqnum, tag);
            }

            if (ret) {
                pr_info("[%s] genlmsg_unicast fail, ret=[%d] after retry %d times: pid=[%d], seq=[%d], tag=[%s]\n",
                    __func__, ret, retry, g_ctx2->bind_pid, g_ctx2->seqnum, tag);
            }
        }
    }

    g_ctx2->seqnum++;

    return ret;
}

int aol_netlink_send_to_native(char *tag, unsigned int msg_id, char *buf, unsigned int length)
{
    int ret = 0;
    int idx = 0;
    unsigned int send_len;
    unsigned int remain_len = length;

    if (g_ctx2->status != LINK_STATUS_INIT_DONE) {
        pr_info("[%s] Netlink should be init\n", __func__);
        return -2;
    }

    if (g_ctx2->bind_pid == 0) {
        pr_info("[%s] No bind service\n", __func__);
        return -3;
    }

    while (remain_len) {
        send_len = (remain_len > AOL_PKT_SIZE ? AOL_PKT_SIZE : remain_len);
        ret = aol_netlink_send_to_native_internal(tag, msg_id, &buf[idx], send_len);

        if (ret) {
            pr_info("[%s] From %d with len=[%d] fail, ret=[%d]\n"
                            , __func__, idx, send_len, ret);
            break;
        }

        remain_len -= send_len;
        idx += send_len;
    }

    return idx;
}

int aol_netlink_init(struct netlink_event_cb *cb)
{
    int ret = 0;

    mutex_init(&g_ctx2->nl_lock);
    ret = genl_register_family(&g_ctx2->gnl_family);

    if (ret != 0) {
        pr_info("[%s] GE_NELINK family registration fail, ret=[%d]\n", __func__, ret);
        return -2;
    }

    g_ctx2->status = LINK_STATUS_INIT_DONE;
    g_ctx2->bind_pid = 0;
    memcpy(&(g_ctx2->cb), cb, sizeof(struct netlink_event_cb));
    pr_info("[%s] aol netlink init succeed\n", __func__);

    return ret;
}

void aol_netlink_deinit(void)
{
    g_ctx2->status = LINK_STATUS_INIT;
    genl_unregister_family(&g_ctx2->gnl_family);
}