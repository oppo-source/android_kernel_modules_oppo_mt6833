#ifndef __AOLEXTRA_NETLINK_H__
#define __AOLEXTRA_NETLINK_H__

#include <linux/types.h>
#include <linux/compiler.h>

enum aolextra_cmd_type {
    AOL_CMD_DEFAULT = 0,
    AOL_CMD_START_DATA_TRANS,
    AOL_CMD_STOP_DATA_TRANS,
    AOL_CMD_ADD_WIFI_FENCE,
    AOL_CMD_REMOVE_WIFI_FENCE,
    AOL_CMD_PAUSE_WIFI_FENCE,
    AOL_CMD_RESUME_WIFI_FENCE,
    AOL_CMD_ADD_CELL_FENCE,
    AOL_CMD_REMOVE_CELL_FENCE,
    AOL_CMD_PAUSE_CELL_FENCE,
    AOL_CMD_RESUME_CELL_FENCE,
    AOL_CMD_SEND_COMMAND2SCP,
    AOL_CMD_INJECT_LOCATION2SCP,
    AOL_CMD_INJECT_WIFI_LIST2SCP,
    AOL_CMD_MAX
};

struct netlink_event_cb {
    int (*aol_bind)(void);
    int (*aol_unbind)(void);
    int (*aol_handler)(int cmd, void *data);
};

int aol_netlink_init(struct netlink_event_cb *cb);
void aol_netlink_deinit(void);
int aol_netlink_send_to_native(char *tag, unsigned int msg_id, char *buf, unsigned int length);

#endif /*__AOLEXTRA_NETLINK_H__ */