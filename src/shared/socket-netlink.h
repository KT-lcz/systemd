/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include "sd-netlink.h"

#include "in-addr-util.h"
#include "macro.h"
#include "socket-util.h"

int resolve_ifname(sd_netlink **rtnl, const char *name);
int resolve_interface(sd_netlink **rtnl, const char *name);
int resolve_interface_or_warn(sd_netlink **rtnl, const char *name);

int make_socket_fd(int log_level, const char* address, int type, int flags);

int socket_address_parse(SocketAddress *a, const char *s);
int socket_address_parse_and_warn(SocketAddress *a, const char *s);
int socket_address_parse_netlink(SocketAddress *a, const char *s);

bool socket_address_is(const SocketAddress *a, const char *s, int type);
bool socket_address_is_netlink(const SocketAddress *a, const char *s);

int in_addr_port_ifindex_name_from_string_auto(
                const char *s,
                int *ret_family,
                union in_addr_union *ret_address,
                uint16_t *ret_port,
                int *ret_ifindex,
                char **ret_server_name);
static inline int in_addr_ifindex_name_from_string_auto(const char *s, int *family, union in_addr_union *ret, int *ifindex, char **server_name) {
        return in_addr_port_ifindex_name_from_string_auto(s, family, ret, NULL, ifindex, server_name);
}
static inline int in_addr_ifindex_from_string_auto(const char *s, int *family, union in_addr_union *ret, int *ifindex) {
        return in_addr_ifindex_name_from_string_auto(s, family, ret, ifindex, NULL);
}
int in_addr_port_from_string_auto(const char *s, int *ret_family, union in_addr_union *ret_address, uint16_t *ret_port);

struct in_addr_full {
        int family;
        union in_addr_union address;
        uint16_t port;
        int ifindex;
        char *server_name;
        char *cached_server_string; /* Should not be handled directly, but through in_addr_full_to_string(). */
};

struct in_addr_full *in_addr_full_free(struct in_addr_full *a);
DEFINE_TRIVIAL_CLEANUP_FUNC(struct in_addr_full*, in_addr_full_free);
int in_addr_full_new(int family, const union in_addr_union *a, uint16_t port, int ifindex, const char *server_name, struct in_addr_full **ret);
int in_addr_full_new_from_string(const char *s, struct in_addr_full **ret);
const char *in_addr_full_to_string(struct in_addr_full *a);
