#ifndef __LUNAIX_IOPOLL_H
#define __LUNAIX_IOPOLL_H

#include <lunaix/device.h>
#include <lunaix/ds/llist.h>
#include <lunaix/fs.h>

#include <usr/lunaix/poll.h>

typedef struct llist_header poll_evt_q;

struct poll_opts
{
    struct pollfd** upoll;
    int upoll_num;
    int timeout;
};

struct iopoller
{
    poll_evt_q evt_listener;
    struct v_file* file_ref;
    pid_t pid;
};

struct iopoll
{
    struct iopoller** pollers;
    int n_poller;
};

static inline void
iopoll_listen_on(struct iopoller* listener, poll_evt_q* source)
{
    llist_append(source, &listener->evt_listener);
}

void
iopoll_wake_pollers(poll_evt_q*);

void
iopoll_init(struct iopoll*);

void
iopoll_free(pid_t, struct iopoll*);

int
iopoll_install(pid_t, struct iopoll*, struct v_fd*);

int
iopoll_remove(pid_t, struct iopoll*, int);

static inline void
poll_setrevt(struct poll_info* pinfo, int evt)
{
    pinfo->revents = (pinfo->revents & ~evt) | evt;
}

static inline int
poll_checkevt(struct poll_info* pinfo, int evt)
{
    return pinfo->events & evt;
}

#endif /* __LUNAIX_POLL_H */