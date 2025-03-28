/*******************************************************************************/
/*                    E X T E R N A L   R E F E R E N C E S                    */
/*******************************************************************************/
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include "bleoffload_netlink.h"

/*******************************************************************************/
/*                             M A C R O S                                     */
/*******************************************************************************/
#define BLEOFFLOAD_NETLINK_FAMILY_NAME "BLEOFFLOAD"
#define BLEOFFLOAD_PKT_SIZE NLMSG_DEFAULT_SIZE
#define MAX_BIND_PROCESS    4

#ifndef GENL_ID_GENERATE
#define GENL_ID_GENERATE    0
#endif

#define BLEOFFLOAD_ATTR_MAX       (__AOL_ATTR_MAX - 1)

/*******************************************************************************/
/*                             D A T A   T Y P E S                             */
/*******************************************************************************/


/*******************************************************************************/
/*                  F U N C T I O N   D E C L A R A T I O N S                  */
/*******************************************************************************/
static int bleoffload_nl_bind(struct sk_buff *skb, struct genl_info *info);
static int bleoffload_nl_msg_to_scp(struct sk_buff *skb, struct genl_info *info);
static int bleoffload_nl_init_scan(struct sk_buff *skb, struct genl_info *info);
static int bleoffload_nl_deinit_scan(struct sk_buff *skb, struct genl_info *info);
static int bleoffload_nl_start_scan(struct sk_buff *skb, struct genl_info *info);
static int bleoffload_nl_stop_scan(struct sk_buff *skb, struct genl_info *info);

/*******************************************************************************/
/*                  G L O B A L  V A R I A B L E                               */
/*******************************************************************************/
/* Attribute policy */
static struct nla_policy bleoffload_genl_policy[_BLEOFFLOAD_ATTR_MAX + 1] = {
	[BLEOFFLOAD_ATTR_PORT] = {.type = NLA_U32},
	[BLEOFFLOAD_ATTR_MODULE] = {.type = NLA_U32},
	[BLEOFFLOAD_ATTR_HEADER] = {.type = NLA_NUL_STRING},
	[BLEOFFLOAD_ATTR_MSG_ID] = {.type = NLA_U32},
	[BLEOFFLOAD_ATTR_MSG_SIZE] = {.type = NLA_U32},
	[BLEOFFLOAD_ATTR_MSG_DATA] = {.type = NLA_BINARY},
};

/* Operation definition */
static struct genl_ops bleoffload_gnl_ops_array[] = {
	{
		.cmd = BLEOFFLOAD_NL_CMD_BIND,
		.flags = 0,
		.policy = bleoffload_genl_policy,
		.doit = bleoffload_nl_bind,
		.dumpit = NULL,
	},
	{
		.cmd = BLEOFFLOAD_NL_CMD_MSG_TO_SCP,
		.flags = 0,
		.policy = bleoffload_genl_policy,
		.doit = bleoffload_nl_msg_to_scp,
		.dumpit = NULL,
	},
	{
		.cmd = BLEOFFLOAD_NL_CMD_INIT_SCAN,
		.flags = 0,
		.policy = bleoffload_genl_policy,
		.doit = bleoffload_nl_init_scan,
		.dumpit = NULL,
	},
	{
		.cmd = BLEOFFLOAD_NL_CMD_DEINIT_SCAN,
		.flags = 0,
		.policy = bleoffload_genl_policy,
		.doit = bleoffload_nl_deinit_scan,
		.dumpit = NULL,
	},
	{
		.cmd = BLEOFFLOAD_NL_CMD_START_SCAN,
		.flags = 0,
		.policy = bleoffload_genl_policy,
		.doit = bleoffload_nl_start_scan,
		.dumpit = NULL,
	},
	{
		.cmd = BLEOFFLOAD_NL_CMD_STOP_SCAN,
		.flags = 0,
		.policy = bleoffload_genl_policy,
		.doit = bleoffload_nl_stop_scan,
		.dumpit = NULL,
	},
};

static struct bleoffload_netlink_ctx g_bleoffload_netlink_ctx = {
	.gnl_family = {
		.id = GENL_ID_GENERATE,
		.hdrsize = 0,
		.name = BLEOFFLOAD_NETLINK_FAMILY_NAME,
		.version = 1,
		.maxattr = _BLEOFFLOAD_ATTR_MAX,
		.ops = bleoffload_gnl_ops_array,
		.n_ops = ARRAY_SIZE(bleoffload_gnl_ops_array),
	},
	.status = LINK_STATUS_INIT,
	.seqnum = 0,
};

static struct bleoffload_netlink_ctx *g_ctx = &g_bleoffload_netlink_ctx;

/*******************************************************************************/
/*                              F U N C T I O N S                              */
/*******************************************************************************/
static int bleoffload_nl_bind(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attr_na;
	unsigned int port, module;
	int ret = -1;

	if (info == NULL) {
		pr_info("[%s] info is NULL\n", __func__);
		return -1;
	}

	if (mutex_lock_killable(&g_ctx->nl_lock)) {
		pr_info("[%s] killable\n", __func__);
		return -1;
	}

	/* port id */
	attr_na = info->attrs[BLEOFFLOAD_ATTR_PORT];
	if (attr_na == NULL) {
		pr_info("[%s] No attr_na found\n", __func__);
		mutex_unlock(&g_ctx->nl_lock);
		return -1;
	}
	port = (unsigned int)nla_get_u32(attr_na);

	/* module id */
	attr_na = info->attrs[BLEOFFLOAD_ATTR_MODULE];
	if (attr_na == NULL) {
		pr_info("[%s] No module found\n", __func__);
		mutex_unlock(&g_ctx->nl_lock);
		return -1;
	}

	module = (unsigned int)nla_get_u32(attr_na);

	pr_info("[%s] port=[%u] module=[%u]", __func__, port, module);

	mutex_unlock(&g_ctx->nl_lock);

	if (g_ctx && g_ctx->cb.bleoffload_bind)
		ret = g_ctx->cb.bleoffload_bind(port, module);

	return ret;
}

static int bleoffload_nl_msg_to_scp(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attr_info = NULL;
	u32 msg_id, msg_sz, module_id = 0;
	u8 *buf = NULL;
	struct nlattr **attrs;
	int ret = -1;

	if (info == NULL) {
		pr_info("[%s] info is NULL\n", __func__);
		return -1;
	}

	attrs = info->attrs;
	if (attrs[BLEOFFLOAD_ATTR_MODULE] == NULL ||
		attrs[BLEOFFLOAD_ATTR_MSG_ID] == NULL ||
		attrs[BLEOFFLOAD_ATTR_MSG_SIZE] == NULL) {
		pr_info("[%s] attrs is NULL\n", __func__);
		return -1;
	}

	if (mutex_lock_killable(&g_ctx->nl_lock)) {
		pr_info("[%s] killable\n", __func__);
		return -1;
	}

	module_id = nla_get_u32(attrs[BLEOFFLOAD_ATTR_MODULE]);
	msg_id = nla_get_u32(attrs[BLEOFFLOAD_ATTR_MSG_ID]);
	msg_sz = nla_get_u32(attrs[BLEOFFLOAD_ATTR_MSG_SIZE]);
	pr_info("[%s] module_id[%d]msg_id[%d]msg_sz[%d]", __func__, module_id, msg_id, msg_sz);

	attr_info = attrs[BLEOFFLOAD_ATTR_MSG_DATA];
	if (attr_info != NULL)
		buf = nla_data(attr_info);

	mutex_unlock(&g_ctx->nl_lock);

	if (g_ctx && g_ctx->cb.bleoffload_handler) {
		pr_info("[%s] call bleoffload_handler", __func__);
		ret = g_ctx->cb.bleoffload_handler(module_id, msg_id, buf, msg_sz);
	}

	return ret;
}

static int bleoffload_netlink_msg_send(char *tag, unsigned int msg_id, char *buf, unsigned int length,
								pid_t pid, unsigned int seq)
{
	struct sk_buff *skb;
	void *msg_head = NULL;
	int ret = 0;
	pr_info("[%s] msg_id[%d]length[%d]seq[%d]\n", __func__, msg_id, length, seq);

	/* Allocate a generic netlink message buffer */
	skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (skb != NULL) {
		/* Create message header */
		msg_head = genlmsg_put(skb, 0, seq, &g_ctx->gnl_family, 0, BLEOFFLOAD_NL_CMD_SEND);
		if (msg_head == NULL) {
			pr_info("[%s] genlmsg_put fail\n", __func__);
			nlmsg_free(skb);
			return -EMSGSIZE;
		}

		/* Add message attribute and content */
		ret = nla_put_string(skb, BLEOFFLOAD_ATTR_HEADER, tag);
		if (ret != 0) {
			pr_info("[%s] nla_put_string header fail, ret=[%d]\n", __func__, ret);
			genlmsg_cancel(skb, msg_head);
			nlmsg_free(skb);
			return ret;
		}

		if (length) {
			ret = nla_put(skb, BLEOFFLOAD_ATTR_MSG, length, buf);
			if (ret != 0) {
				pr_info("[%s] nla_put fail, ret=[%d]\n", __func__, ret);
				genlmsg_cancel(skb, msg_head);
				nlmsg_free(skb);
				return ret;
			}

			ret = nla_put_u32(skb, BLEOFFLOAD_ATTR_MSG_ID, msg_id);
			if (ret != 0) {
				pr_info("[%s] nal_put_u32 fail, ret=[%d]\n", __func__, ret);
				genlmsg_cancel(skb, msg_head);
				nlmsg_free(skb);
				return ret;
			}

			ret = nla_put_u32(skb, BLEOFFLOAD_ATTR_MSG_SIZE, length);
			if (ret != 0) {
				pr_info("[%s] nal_put_u32 fail, ret=[%d]\n", __func__, ret);
				genlmsg_cancel(skb, msg_head);
				nlmsg_free(skb);
				return ret;
			}
		}

		/* Finalize the message */
		genlmsg_end(skb, msg_head);

		/* Send message */
		ret = genlmsg_unicast(&init_net, skb, pid);
		if (ret == 0)
			pr_info("[%s] Send msg succeed\n", __func__);
	} else {
		pr_info("[%s] Allocate message error\n", __func__);
		ret = -ENOMEM;
	}

	return ret;
}

static int bleoffload_nl_init_scan(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attr_info = NULL;
	u32 msg_id, msg_sz, module_id = 0;
	u8 *buf = NULL;
	struct nlattr **attrs;
	int ret = -1;

	if (info == NULL)
		return -1;

	attrs = info->attrs;
	if (attrs[BLEOFFLOAD_ATTR_MODULE] == NULL ||
		attrs[BLEOFFLOAD_ATTR_MSG_ID] == NULL ||
		attrs[BLEOFFLOAD_ATTR_MSG_SIZE] == NULL)
		return -1;

	if (mutex_lock_killable(&g_ctx->nl_lock))
		return -1;

	module_id = nla_get_u32(attrs[BLEOFFLOAD_ATTR_MODULE]);
	msg_id = nla_get_u32(attrs[BLEOFFLOAD_ATTR_MSG_ID]);
	msg_sz = nla_get_u32(attrs[BLEOFFLOAD_ATTR_MSG_SIZE]);

	attr_info = attrs[BLEOFFLOAD_ATTR_MSG_DATA];
	if (attr_info != NULL)
		buf = nla_data(attr_info);

	mutex_unlock(&g_ctx->nl_lock);

	if (g_ctx && g_ctx->cb.bleoffload_init_scan) {
		pr_info("[%s] call bleoffload_init_scan", __func__);
		ret = g_ctx->cb.bleoffload_init_scan(module_id, msg_id, buf, msg_sz);
	}

	return ret;
}

static int bleoffload_nl_deinit_scan(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attr_info = NULL;
	u32 msg_id, msg_sz, module_id = 0;
	u8 *buf = NULL;
	struct nlattr **attrs;
	int ret = -1;

	if (info == NULL)
		return -1;

	attrs = info->attrs;
	if (attrs[BLEOFFLOAD_ATTR_MODULE] == NULL ||
		attrs[BLEOFFLOAD_ATTR_MSG_ID] == NULL ||
		attrs[BLEOFFLOAD_ATTR_MSG_SIZE] == NULL)
		return -1;

	if (mutex_lock_killable(&g_ctx->nl_lock))
		return -1;

	module_id = nla_get_u32(attrs[BLEOFFLOAD_ATTR_MODULE]);
	msg_id = nla_get_u32(attrs[BLEOFFLOAD_ATTR_MSG_ID]);
	msg_sz = nla_get_u32(attrs[BLEOFFLOAD_ATTR_MSG_SIZE]);

	attr_info = attrs[BLEOFFLOAD_ATTR_MSG_DATA];
	if (attr_info != NULL)
		buf = nla_data(attr_info);

	mutex_unlock(&g_ctx->nl_lock);

	if (g_ctx && g_ctx->cb.bleoffload_deinit_scan) {
		pr_info("[%s] call bleoffload_deinit_scan", __func__);
		ret = g_ctx->cb.bleoffload_deinit_scan(module_id, msg_id, buf, msg_sz);
	}
	return ret;
}

static int bleoffload_nl_start_scan(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attr_info = NULL;
	u32 msg_id, msg_sz, module_id = 0;
	u8 *buf = NULL;
	struct nlattr **attrs;
	int ret = -1;

	if (info == NULL)
		return -1;

	attrs = info->attrs;
	if (attrs[BLEOFFLOAD_ATTR_MODULE] == NULL ||
		attrs[BLEOFFLOAD_ATTR_MSG_ID] == NULL ||
		attrs[BLEOFFLOAD_ATTR_MSG_SIZE] == NULL)
		return -1;

	if (mutex_lock_killable(&g_ctx->nl_lock))
		return -1;

	module_id = nla_get_u32(attrs[BLEOFFLOAD_ATTR_MODULE]);
	msg_id = nla_get_u32(attrs[BLEOFFLOAD_ATTR_MSG_ID]);
	msg_sz = nla_get_u32(attrs[BLEOFFLOAD_ATTR_MSG_SIZE]);

	attr_info = attrs[BLEOFFLOAD_ATTR_MSG_DATA];
	if (attr_info != NULL)
		buf = nla_data(attr_info);

	mutex_unlock(&g_ctx->nl_lock);

	if (g_ctx && g_ctx->cb.bleoffload_start_scan) {
		pr_info("[%s] call bleoffload_start_scan", __func__);
		ret = g_ctx->cb.bleoffload_start_scan(module_id, msg_id, buf, msg_sz);
	}

	return ret;
}

static int bleoffload_nl_stop_scan(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attr_info = NULL;
	u32 msg_id, msg_sz, module_id = 0;
	u8 *buf = NULL;
	struct nlattr **attrs;
	int ret = -1;

	if (info == NULL)
		return -1;

	attrs = info->attrs;
	if (attrs[BLEOFFLOAD_ATTR_MODULE] == NULL ||
		attrs[BLEOFFLOAD_ATTR_MSG_ID] == NULL ||
		attrs[BLEOFFLOAD_ATTR_MSG_SIZE] == NULL)
		return -1;

	if (mutex_lock_killable(&g_ctx->nl_lock))
		return -1;

	module_id = nla_get_u32(attrs[BLEOFFLOAD_ATTR_MODULE]);
	msg_id = nla_get_u32(attrs[BLEOFFLOAD_ATTR_MSG_ID]);
	msg_sz = nla_get_u32(attrs[BLEOFFLOAD_ATTR_MSG_SIZE]);

	attr_info = attrs[BLEOFFLOAD_ATTR_MSG_DATA];
	if (attr_info != NULL)
		buf = nla_data(attr_info);

	mutex_unlock(&g_ctx->nl_lock);

	if (g_ctx && g_ctx->cb.bleoffload_stop_scan) {
		pr_info("[%s] call bleoffload_stop_scan", __func__);
		ret = g_ctx->cb.bleoffload_stop_scan(module_id, msg_id, buf, msg_sz);
	}
	return ret;
}

static int bleoffload_netlink_send_to_native_internal(struct netlink_client_info *client, char *tag,
				unsigned int msg_id, char *buf, unsigned int length)
{
	int ret = -EAGAIN;
	unsigned int retry = 0;
	pr_info("[%s] msg_id[%d]length[%d]", __func__, msg_id, length);

	while (retry < 100 && ret == -EAGAIN) {
		ret = bleoffload_netlink_msg_send(tag, msg_id, buf, length,
								client->port, client->seqnum);
		pr_info("[%s] genlmsg_unicast retry(%d) ret=[%d] pid=[%d] seq=[%d] tag=[%s]",
					__func__, retry, ret, client->port, client->seqnum, tag);

		retry++;
	}

	client->seqnum++;

	return ret;
}

int bleoffload_netlink_send_to_native(struct netlink_client_info *client,
			char *tag, unsigned int msg_id, char *buf, unsigned int length)
{
	int ret = 0;
	int idx = 0;
	unsigned int send_len;
	unsigned int remain_len = length;

	if (g_ctx->status != LINK_STATUS_INIT_DONE) {
		pr_notice("[%s] Netlink should be init\n", __func__);
		return -2;
	}

	if (client->port == 0) {
		pr_notice("[%s] No bind service", __func__);
		return -3;
	}

	while (remain_len) {
		send_len = (remain_len > BLEOFFLOAD_PKT_SIZE ? BLEOFFLOAD_PKT_SIZE : remain_len);
		ret = bleoffload_netlink_send_to_native_internal(client, tag, msg_id,
							&buf[idx], send_len);

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

int bleoffload_netlink_init(struct bleoffload_netlink_event_cb *cb)
{
	int ret = 0;

	mutex_init(&g_ctx->nl_lock);
	ret = genl_register_family(&g_ctx->gnl_family);

	if (ret != 0) {
		pr_info("[%s] GE_NELINK family registration fail, ret=[%d]\n", __func__, ret);
		return -2;
	}

	g_ctx->status = LINK_STATUS_INIT_DONE;
	g_ctx->bind_pid = 0;
	memcpy(&(g_ctx->cb), cb, sizeof(struct bleoffload_netlink_event_cb));
	pr_info("[%s] bleoffload netlink init succeed\n", __func__);

	return ret;
}

void bleoffload_netlink_deinit(void)
{
	g_ctx->status = LINK_STATUS_INIT;
	genl_unregister_family(&g_ctx->gnl_family);
	pr_info("[%s] bleoffload netlink deinit done\n", __func__);
}
