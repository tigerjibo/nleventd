/*
 * Copyright (C) 2013 Vadim Kochan <vadim4j@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <netinet/ether.h>

#include "defs.h"
#include "nl_parser.h"
#include "utils.h"

#ifndef NDA_RTA
    #define NDA_RTA(r)\
       ((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct ndmsg))))
#endif

#define ADDR_MAX 256
#define NUMB_MAX 10

char nl_qdisc[40] = {};
char nl_mtu[NUMB_MAX] = {};
char nl_broadcast[ADDR_MAX] = {};
char nl_address[ADDR_MAX] = {};
char nl_local[ADDR_MAX] = {};
char nl_anycast[ADDR_MAX] = {};
char nl_label[IFNAMSIZ] = {};
char nl_ifname[IFNAMSIZ] = {};
char nl_prefixlen[NUMB_MAX] = {};
char nl_lladdr[ADDR_MAX] = {};

static key_value_t *kv_addr = NULL;
static key_value_t *kv_link = NULL;
static key_value_t *kv_neigh = NULL;

static void rt_attrs_parse(struct rtattr *tb_attr[], int max,
        struct rtattr *rta, int len)
{
    memset(tb_attr, 0, sizeof(struct rtattr *) * (max + 1));
    
    while (RTA_OK(rta, len))
    {
        if (rta->rta_type <= max)
            tb_attr[rta->rta_type] = rta;

        rta = RTA_NEXT(rta, len);
    }
}

static char *event_name_get(int type)
{
    switch (type)
    {
        case RTM_NEWLINK:
            return "NEWLINK";
        case RTM_DELLINK:
            return "DELLINK"; 
        case RTM_SETLINK:
            return "SETLINK";
        case RTM_NEWADDR:
            return "NEWADDR";
        case RTM_DELADDR:
            return "DELADDR";
        case RTM_NEWROUTE:
            return "NEWROUTE";
	case RTM_DELROUTE:
            return "DELROUTE";
        case RTM_NEWNEIGH:
            return "NEWNEIGH";
        case RTM_DELNEIGH:
            return "DELNEIGH"; 
        case RTM_NEWRULE:
            return "NEWRULE";
        case RTM_DELRULE:
            return "DELRULE";
        case RTM_NEWQDISC:
            return "NEWQDISC";
        case RTM_DELQDISC:
            return "DELQDISC";
        case RTM_NEWTCLASS:
            return "NEWTCLASS";
        case RTM_DELTCLASS:
            return "DELTCLASS";
        case RTM_NEWTFILTER:
            return "NEWTFILTER";
        case RTM_DELTFILTER:
            return "DELTFILTER";
        case RTM_NEWACTION:
            return "NEWACTION";
        case RTM_DELACTION:
            return "DELACTION";
        case RTM_NEWPREFIX:
            return "NEWPREFIX";
        case RTM_NEWNEIGHTBL:
            return "NEWNEIGHTBL";
	case RTM_SETNEIGHTBL:
            return "SETNEIGHTBL";
	case RTM_NEWNDUSEROPT:
            return "NEWNDUSEROPT";
        case RTM_NEWADDRLABEL:
            return "NEWADDRLABEL";
	case RTM_DELADDRLABEL:
            return "DELADDRLABEL";
        case RTM_SETDCB:
            return "SETDCB";
        /* XXX Should be protected by config for older Linux */
        /* case RTM_NEWNETCONF: */
        /*     return "NEWNETCONF"; */
        /* case RTM_NEWMDB: */
        /*     return "NEWMDB"; */ 
        /* case RTM_DELMDB: */
        /*     return "DELMDB"; */
    }

    return NULL;
}

static char *ifa_family_name_get(int ifa_family)
{
    if (ifa_family == AF_INET)
        return "INET";
    else if (ifa_family == AF_INET6)
        return "INET6";

    return NULL;
}

static char *ifa_scope_name_get(int ifa_scope)
{
    switch (ifa_scope)
    {
        case RT_SCOPE_UNIVERSE:
            return "UNIVERSE";
        case RT_SCOPE_SITE:
            return "SITE";
        case RT_SCOPE_HOST:
            return "HOST";
        case RT_SCOPE_LINK:
            return "LINK";
        case RT_SCOPE_NOWHERE:
            return "NOWHERE";
    }

    return "UNKNOWN";
}

static char *hw_addr_parse(char *haddr, int htype)
{
    if (!(htype == ARPHRD_ETHER))
        return "UNKNOWN";

    return ether_ntoa((struct ether_addr *)haddr);
}

static key_value_t *rtnl_parse_link(struct nlmsghdr *msg)
{
    struct rtattr *tb_attrs[IFLA_MAX + 1];
    struct ifinfomsg *ifi = (struct ifinfomsg *)NLMSG_DATA(msg);

    if_indextoname(ifi->ifi_index, nl_ifname);

    nl_flag_set(kv_link, NL_IS_UP, ifi->ifi_flags, IFF_UP);
    nl_flag_set(kv_link, NL_IS_BROADCAST, ifi->ifi_flags,
            IFF_BROADCAST);
    nl_flag_set(kv_link, NL_IS_LOOPBACK, ifi->ifi_flags, IFF_LOOPBACK);
    nl_flag_set(kv_link, NL_IS_POINTOPOINT, ifi->ifi_flags,
            IFF_POINTOPOINT);
    nl_flag_set(kv_link, NL_IS_RUNNING, ifi->ifi_flags, IFF_RUNNING);
    nl_flag_set(kv_link, NL_IS_NOARP, ifi->ifi_flags, IFF_NOARP);
    nl_flag_set(kv_link, NL_IS_PROMISC, ifi->ifi_flags, IFF_PROMISC);
    nl_flag_set(kv_link, NL_IS_ALLMULTI, ifi->ifi_flags, IFF_ALLMULTI);
    nl_flag_set(kv_link, NL_IS_MASTER, ifi->ifi_flags, IFF_MASTER);
    nl_flag_set(kv_link, NL_IS_SLAVE, ifi->ifi_flags, IFF_SLAVE);
    nl_flag_set(kv_link, NL_IS_MULTICAST, ifi->ifi_flags,
            IFF_MULTICAST);

    rt_attrs_parse(tb_attrs, IFLA_MAX, IFLA_RTA(ifi),
            msg->nlmsg_len);

    if (tb_attrs[IFLA_ADDRESS])
    {
        nl_val_cpy(kv_link, NL_ADDRESS, hw_addr_parse(RTA_DATA(
            tb_attrs[IFLA_ADDRESS]), ifi->ifi_type));
    }

    if (tb_attrs[IFLA_BROADCAST])
    {
        nl_val_set(kv_link, NL_BROADCAST, hw_addr_parse(RTA_DATA(
            tb_attrs[IFLA_BROADCAST]), ifi->ifi_type));
    }

    if (tb_attrs[IFLA_MTU])
    {
        nl_val_set(kv_link, NL_MTU, itoa(*(unsigned int *)RTA_DATA(
            tb_attrs[IFLA_MTU])));
    }

    if (tb_attrs[IFLA_QDISC])
    {
        nl_val_set(kv_link, NL_QDISC, RTA_DATA(
            tb_attrs[IFLA_QDISC]));
    }

    return kv_link;
}

static key_value_t *rtnl_parse_addr(struct nlmsghdr *msg)
{
    struct rtattr *tb_attrs[IFA_MAX + 1];
    struct ifaddrmsg *addr_msg = (struct ifaddrmsg *)NLMSG_DATA(msg);
    char *ifa_family_name = ifa_family_name_get(addr_msg->ifa_family);
    char *ifa_scope_name = ifa_scope_name_get(addr_msg->ifa_scope);

    if (!ifa_family_name)
        return NULL;

    nl_val_set(kv_addr, NL_FAMILY, ifa_family_name);
    nl_val_cpy(kv_addr, NL_PREFIXLEN,
            itoa(addr_msg->ifa_prefixlen));

    nl_val_set(kv_addr, NL_SCOPE, ifa_scope_name);

    if_indextoname(addr_msg->ifa_index, nl_ifname);

    rt_attrs_parse(tb_attrs, IFA_MAX, IFA_RTA(addr_msg),
            msg->nlmsg_len);

    if (tb_attrs[IFA_ADDRESS])
    {
        inet_ntop(addr_msg->ifa_family, RTA_DATA(tb_attrs[IFA_ADDRESS]),
                nl_address, sizeof(nl_address));
    }

    if (tb_attrs[IFA_LOCAL])
    {
        inet_ntop(addr_msg->ifa_family, RTA_DATA(tb_attrs[IFA_LOCAL]),
                nl_local, sizeof(nl_local));
    }

    if (tb_attrs[IFA_LABEL])
    {
        nl_val_cpy(kv_addr, NL_LABEL,
                RTA_DATA(tb_attrs[IFA_LABEL]));
    }

    if (tb_attrs[IFA_BROADCAST])
    {
        inet_ntop(addr_msg->ifa_family,
                RTA_DATA(tb_attrs[IFA_BROADCAST]), nl_broadcast,
                sizeof(nl_broadcast));
    }

    if (tb_attrs[IFA_ANYCAST])
    {
        inet_ntop(addr_msg->ifa_family,
                RTA_DATA(tb_attrs[IFA_ANYCAST]), nl_anycast,
                sizeof(nl_anycast));
    }

    /* XXX add: IFA_CACHEINFO */

    return kv_addr;
}

static key_value_t *rtnl_parse_neigh(struct nlmsghdr *msg)
{
    struct ndmsg *nd_msg = (struct ndmsg *)NLMSG_DATA(msg);
    struct rtattr *tb_attrs[NDA_MAX + 1];

    nl_val_set(kv_neigh, NL_FAMILY, ifa_family_name_get(nd_msg->ndm_family));

    if_indextoname(nd_msg->ndm_ifindex, nl_ifname);

    nl_flag_set(kv_neigh, NL_IS_INCOMPLETE, nd_msg->ndm_state, NUD_INCOMPLETE);
    nl_flag_set(kv_neigh, NL_IS_REACHABLE, nd_msg->ndm_state, NUD_REACHABLE);
    nl_flag_set(kv_neigh, NL_IS_STALE, nd_msg->ndm_state, NUD_STALE);
    nl_flag_set(kv_neigh, NL_IS_DELAY, nd_msg->ndm_state, NUD_DELAY);
    nl_flag_set(kv_neigh, NL_IS_PROBE, nd_msg->ndm_state, NUD_PROBE);
    nl_flag_set(kv_neigh, NL_IS_FAILED, nd_msg->ndm_state, NUD_FAILED);

    nl_flag_set(kv_neigh, NL_IS_PROXY, nd_msg->ndm_flags, NTF_PROXY);
    nl_flag_set(kv_neigh, NL_IS_ROUTER, nd_msg->ndm_flags, NTF_ROUTER);

    rt_attrs_parse(tb_attrs, NDA_MAX, NDA_RTA(nd_msg), msg->nlmsg_len);

    if (tb_attrs[NDA_DST])
    {
        inet_ntop(nd_msg->ndm_family, RTA_DATA(tb_attrs[NDA_DST]),
            nl_address, sizeof(nl_address));
    }

    if (tb_attrs[NDA_LLADDR])
    {
        nl_val_cpy(kv_neigh, NL_LLADDR, hw_addr_parse(RTA_DATA(
            tb_attrs[NDA_LLADDR]), ARPHRD_ETHER));
    }

    return kv_neigh;
}

static key_value_t *rtnl_parse(struct nlmsghdr *msg)
{
    char *event_name = event_name_get(msg->nlmsg_type);
    key_value_t *kv = NULL;

    /* not supported RTNETLINK type */
    if (!event_name)
        return NULL;

    switch (msg->nlmsg_type)
    {
        case RTM_NEWADDR:
        case RTM_DELADDR:
            kv = rtnl_parse_addr(msg);
            break;
        case RTM_NEWLINK:
        case RTM_DELLINK:
            kv = rtnl_parse_link(msg);
            break;
        case RTM_NEWNEIGH:
        case RTM_DELNEIGH:
        {
            kv = rtnl_parse_neigh(msg);
            break;
        }
        case RTM_NEWROUTE:
	case RTM_DELROUTE:
        {
            break;
        }
    }

    nl_val_set(kv, NL_EVENT, event_name);
    return kv;
}

static void rtnl_parser_init(void)
{
    kv_link = key_value_add(kv_link, NL_QDISC, nl_qdisc);
    kv_link = key_value_add(kv_link, NL_MTU, nl_mtu);
    kv_link = key_value_add(kv_link, NL_BROADCAST, nl_broadcast);
    kv_link = key_value_add(kv_link, NL_ADDRESS, nl_address);
    kv_link = key_value_add(kv_link, NL_IS_MULTICAST, NULL);
    kv_link = key_value_add(kv_link, NL_IS_SLAVE, NULL);
    kv_link = key_value_add(kv_link, NL_IS_MASTER, NULL);
    kv_link = key_value_add(kv_link, NL_IS_ALLMULTI, NULL);
    kv_link = key_value_add(kv_link, NL_IS_PROMISC, NULL);
    kv_link = key_value_add(kv_link, NL_IS_NOARP, NULL);
    kv_link = key_value_add(kv_link, NL_IS_RUNNING, NULL);
    kv_link = key_value_add(kv_link, NL_IS_POINTOPOINT, NULL);
    kv_link = key_value_add(kv_link, NL_IS_LOOPBACK, NULL);
    kv_link = key_value_add(kv_link, NL_IS_BROADCAST, NULL);
    kv_link = key_value_add(kv_link, NL_IS_UP, NULL);
    kv_link = key_value_add(kv_link, NL_IFNAME, nl_ifname);
    kv_link = key_value_add(kv_link, NL_EVENT, NULL);
    kv_link = key_value_add(kv_link, NL_TYPE, "ROUTE");

    kv_addr = key_value_add(kv_addr, NL_FAMILY, NULL);
    kv_addr = key_value_add(kv_addr, NL_PREFIXLEN, nl_prefixlen);
    kv_addr = key_value_add(kv_addr, NL_SCOPE, NULL);
    kv_addr = key_value_add(kv_addr, NL_IFNAME, nl_ifname);
    kv_addr = key_value_add(kv_addr, NL_ADDRESS, nl_address);
    kv_addr = key_value_add(kv_addr, NL_LOCAL, NULL);
    kv_addr = key_value_add(kv_addr, NL_LABEL, nl_label);
    kv_addr = key_value_add(kv_addr, NL_BROADCAST, nl_broadcast);
    kv_addr = key_value_add(kv_addr, NL_ANYCAST, nl_anycast);
    kv_addr = key_value_add(kv_addr, NL_EVENT, NULL);
    kv_addr = key_value_add(kv_addr, NL_TYPE, "ROUTE");

    kv_neigh = key_value_add(kv_neigh, NL_FAMILY, NULL);
    kv_neigh = key_value_add(kv_neigh, NL_IFNAME, nl_ifname);
    kv_neigh = key_value_add(kv_neigh, NL_IS_INCOMPLETE, NULL);
    kv_neigh = key_value_add(kv_neigh, NL_IS_REACHABLE, NULL);
    kv_neigh = key_value_add(kv_neigh, NL_IS_STALE, NULL);
    kv_neigh = key_value_add(kv_neigh, NL_IS_DELAY, NULL);
    kv_neigh = key_value_add(kv_neigh, NL_IS_PROBE, NULL);
    kv_neigh = key_value_add(kv_neigh, NL_IS_FAILED, NULL);
    kv_neigh = key_value_add(kv_neigh, NL_IS_PROXY, NULL);
    kv_neigh = key_value_add(kv_neigh, NL_IS_ROUTER, NULL);
    kv_neigh = key_value_add(kv_neigh, NL_DST, nl_address);
    kv_neigh = key_value_add(kv_neigh, NL_LLADDR, nl_lladdr);
    kv_neigh = key_value_add(kv_neigh, NL_EVENT, NULL);
    kv_neigh = key_value_add(kv_neigh, NL_TYPE, "ROUTE");
}

static void rtnl_parser_cleanup(void)
{
    nl_kv_free_all(kv_link);
    nl_kv_free_all(kv_addr);
    nl_kv_free_all(kv_neigh);
}

nl_parser_t rtnl_parser_ops = {
    .nl_proto = NETLINK_ROUTE,
    .nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR |
        RTMGRP_NEIGH,
    .do_init = rtnl_parser_init,
    .do_cleanup = rtnl_parser_cleanup,
    .do_parse = rtnl_parse,
};