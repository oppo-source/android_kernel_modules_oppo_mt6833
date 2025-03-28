#ifndef _BLEOFFLOAD_CORE_H_
#define _BLEOFFLOAD_CORE_H_

#include <linux/types.h>
#include <linux/compiler.h>
#include "msg_thread.h"
#include "bleoffload_netlink.h"

enum bleoffload_module {
	BLEOFFLOAD_MODULE_BLE,
	BLEOFFLOAD_MODULE_SIZE
};

enum bleoffload_core_opid {
	BLEOFFLOAD_OPID_DEFAULT = 0,
	BLEOFFLOAD_OPID_MODULE_BIND = 1,
	BLEOFFLOAD_OPID_MODULE_UNBIND = 2,
	BLEOFFLOAD_OPID_SEND_MSG  = 3,
	BLEOFFLOAD_OPID_RECV_MSG  = 4,
	BLEOFFLOAD_OPID_MAX
};

enum bleoffload_core_status {
	BLEOFFLOAD_INACTIVE,
	BLEOFFLOAD_ACTIVE,
};


struct bleoffload_scan_info {
	uint32_t init_scan_id;
	bool init_scan;
	uint32_t start_scan_id;
	uint32_t scan_data_sz;
	uint8_t *scan_data;
};

struct bleoffload_module_ctx {
	enum conap_scp_drv_type drv_type;
	bool enable;
	uint32_t port;
	struct netlink_client_info client;
	struct conap_scp_drv_cb scp_cb;
	struct bleoffload_scan_info scan_info;
};

struct bleoffload_core_ctx {
	struct msg_thread_ctx msg_ctx;
	int status;
	struct bleoffload_module_ctx *module_ctx[BLEOFFLOAD_MODULE_SIZE];
};


int bleoffload_core_init(void);
void bleoffload_core_deinit(void);

#endif // _BLEOFFLOAD_CORE_H_
