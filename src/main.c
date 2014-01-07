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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <memory.h>
#include <getopt.h>

#include "defs.h"
#include "err.h"
#include "nl_parser.h"
#include "rules.h"
#include "utils.h"

#define SECS 1000
#define NL_MSG_MAX 8192

static struct pollfd *poll_list = NULL;
static int poll_count;
static struct sigaction sig_act = {};
static int do_exit = 0;
static rules_t *rules = NULL;
static int dump_msgs = 0;

static char *rules_dir = CONF_DIR "/" RULES_DIR;

nl_parser_t *parsers[] = 
{
    &nl_route_ops,
    NULL,
};

static void sig_int(int num)
{
    do_exit = 1;
}

static void nl_msg_dump(key_value_t *nl_msg)
{
    for (; nl_msg; nl_msg = nl_msg->next)
    {
        printf("%s=%s\n", (char *)nl_msg->key, (char *)nl_msg->value);
    }

    printf("----------------------------------------\n");
}

static nl_msg_free(key_value_t *kv)
{
    key_value_t *kv_next;

    while (kv)
    {
        kv_next = kv->next;

        if (kv->value)
            free(kv->value);

        free(kv);

        kv = kv_next;
    }
}

int do_poll_netlink()
{
    int i, rcv_len, msg_len;
    struct iovec *iov;
    struct nlmsghdr *h;
    key_value_t *kv;

    while (!do_exit)
    {
        if (poll(poll_list, poll_count, 10 * SECS) < 0)
            continue;

        if (errno == EINTR)
            return 0;

        for (i = 0; i < poll_count; i++)
        {
            if (!(poll_list[i].revents & POLLIN))
                continue;

            /* alloc buffer only before first recvmsg call */
            if (!parsers[i]->msg_hdr->msg_iov)
            {
                iov = (struct iovec *)malloc( sizeof(struct iovec));
                iov->iov_base = malloc(NL_MSG_MAX);
                iov->iov_len = NL_MSG_MAX;

                parsers[i]->msg_hdr->msg_iov = iov;
                parsers[i]->msg_hdr->msg_iovlen = 1;
            }

            iov = parsers[i]->msg_hdr->msg_iov;

            rcv_len = recvmsg(poll_list[i].fd, parsers[i]->msg_hdr, 0);

            if (rcv_len <= 0)
                continue;

            if (parsers[i]->msg_hdr->msg_namelen !=
                    sizeof(*parsers[i]->addr))
            {
                continue;
            }

            for (h = (struct nlmsghdr *)iov->iov_base;
                    NLMSG_OK(h, (unsigned int)rcv_len);
                    h = NLMSG_NEXT(h, rcv_len))
            {
                if (h->nlmsg_type == NLMSG_DONE)
                   continue;

                if (h->nlmsg_type == NLMSG_ERROR)
                    continue;

                msg_len = h->nlmsg_len - sizeof(*h);

                if (msg_len < 0 || msg_len > rcv_len)
                    continue;

                kv = parsers[i]->do_parse(h, msg_len);

                if (dump_msgs)
                    nl_msg_dump(kv);

                rules_exec_by_match(rules, kv);
                nl_msg_free(kv);
            }
        }
    }
}

static int usage(char *progname)
{
    printf("\n%s [OPTIONS]\n\n", progname);
    printf("-c, --conf-dir  PATH        specify rules directory\n");    
    printf("-D, --dump-msgs             prints each Netlink msg in key=value format\n");

    return -1;
}

static int parse_opts(int argc, char **argv)
{
    int c;
    struct option opts_long[] =
    {
        {"dump-msgs", 0, NULL, 'D'},        
        {"conf-dir", 1, NULL, 'c'},
        {NULL, 0, NULL, 0},
    };

    while ((c = getopt_long(argc, argv, "c:D", opts_long, NULL)) != -1)
    {
        switch (c)
        {
        case 'c':
            rules_dir = optarg;
            break;
        case 'D':
            dump_msgs = 1;
            break;
        default:
            return -1;
        }
    }

    return 0;
}

static void poll_init()
{
    int i;

    poll_count = ARRAY_SIZE(parsers) - 1;
    poll_list = (struct pollfd *)malloc(poll_count * sizeof(struct pollfd));

    for (i = 0; parsers[i]; i++)
    {
        poll_list[i].fd = parsers[i]->sock;
        poll_list[i].events = POLLIN;
    }
}

static void poll_cleanup()
{
    free(poll_list);
}

int main(int argc, char **argv)
{
    sig_act.sa_handler = sig_int;
    sigaction(SIGINT, &sig_act, 0);
    sigaction(SIGKILL, &sig_act, 0);

    if (parse_opts(argc, argv))
        return usage(argv[0]);

    if (rules_read(rules_dir, &rules))
        RET_ERROR_SYS("Cant parse rules\n");

    if (parsers_init(parsers))
        RET_ERROR_SYS("Cant initialize netlink parsers\n");

    poll_init();

    do_poll_netlink();

    poll_cleanup();
    parsers_cleanup(parsers);
    rules_free_all(rules);

    return 0;
}
