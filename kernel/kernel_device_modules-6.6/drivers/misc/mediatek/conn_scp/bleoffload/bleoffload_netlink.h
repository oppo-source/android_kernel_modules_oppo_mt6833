#ifndef _BLEOFFLOAD_NETLINK_H_
#define _BLEOFFLOAD_NETLINK_H_

#include <linux/types.h>
#include <linux/compiler.h>
#include <net/genetlink.h>

// Define netlink command id
enum bleoffload_netlink_cmd_id {
	_BLEOFFLOAD_NL_CMD_INVALID,
	BLEOFFLOAD_NL_CMD_BIND,
	BLEOFFLOAD_NL_CMD_SEND,
	BLEOFFLOAD_NL_CMD_MSG_TO_SCP,
	BLEOFFLOAD_NL_CMD_INIT_SCAN,
	BLEOFFLOAD_NL_CMD_DEINIT_SCAN,
	BLEOFFLOAD_NL_CMD_START_SCAN,
	BLEOFFLOAD_NL_CMD_STOP_SCAN,
	_BLEOFFLOAD_NL_CMD_MAX,
};

// Define netlink message formats
enum bleoffload_attr {
	_BLEOFFLOAD_ATTR_DEFAULT,
	BLEOFFLOAD_ATTR_PORT,
	BLEOFFLOAD_ATTR_MODULE,
	BLEOFFLOAD_ATTR_HEADER,
	BLEOFFLOAD_ATTR_MSG,
	BLEOFFLOAD_ATTR_MSG_ID,
	BLEOFFLOAD_ATTR_MSG_SIZE,
	BLEOFFLOAD_ATTR_MSG_DATA,
	BLEOFFLOAD_ATTR_TEST_INFO,
	_BLEOFFLOAD_ATTR_MAX,
};

enum link_status {
	LINK_STATUS_INIT,
	LINK_STATUS_INIT_DONE,
	LINK_STATUS_MAX,
};

struct netlink_client_info {
	uint32_t port;
	uint32_t seqnum;
};

struct bleoffload_netlink_event_cb {
	int (*bleoffload_bind)(uint32_t port, uint32_t module_id);
	int (*bleoffload_unbind)(uint32_t module_id);
	int (*bleoffload_init_scan)(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz);
	int (*bleoffload_deinit_scan)(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz);
	int (*bleoffload_start_scan)(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz);
	int (*bleoffload_stop_scan)(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz);
	int (*bleoffload_handler)(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz);
};

struct bleoffload_netlink_ctx {
	pid_t bind_pid;
	struct genl_family gnl_family;
	unsigned int seqnum;
	struct mutex nl_lock;
	enum link_status status;
	struct bleoffload_netlink_event_cb cb;
};

int bleoffload_netlink_init(struct bleoffload_netlink_event_cb *cb);
void bleoffload_netlink_deinit(void);
int bleoffload_netlink_send_to_native(struct netlink_client_info *client,
			char *tag, unsigned int msg_id, char *buf, unsigned int length);

#endif /*_BLEOFFLOAD_NETLINK_H_ */
