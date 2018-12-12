#ifndef _AC_MANAGER_H
#define _AC_MANAGER_H

#define WLAN_IFACE_ID	0

int ac_init_dev_attr(struct capwap_wtp *wtp);
int ac_update_dev_attr(struct capwap_wtp *wtp);

int ac_add_wlan(struct capwap_wtp *wtp);

#endif // _AC_MANAGER_H
