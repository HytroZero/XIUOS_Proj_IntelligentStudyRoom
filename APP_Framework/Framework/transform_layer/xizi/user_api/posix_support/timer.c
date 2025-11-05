/*
 * Copyright (c) 2020 AIIT XUOS Lab
 * XiUOS  is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *        http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/**
* @file:    timer.c
* @brief:   posix api of soft timer
* @version: 3.0
* @author:  AIIT XUOS Lab
* @date:    2023/3/8
*
*/

#include <string.h>
#include <stdio.h>
#include "include/timer.h"
#include "include/semaphore.h"
#include "include/pthread.h"

#define TIMER_NUM 20

static sem_t timer_sem[TIMER_NUM];
static pthread_t timer_task[TIMER_NUM];
static char timer_idx[TIMER_NUM];

struct timer_func {
    union sigval value;
    int timer_flags;
    void (* user_timer_function)(union sigval val);
};

struct timer_func g_timer_func[TIMER_NUM];

static void *timer_callback(void *args) 
{
    clockid_t clockid = *((clockid_t *)args);

    while (1) {
        if (g_timer_func[clockid].user_timer_function != NULL) {
            if (0 == sem_timedwait(&(timer_sem[clockid]), NULL)) {
                g_timer_func[clockid].value.sival_ptr = &clockid;
                g_timer_func[clockid].user_timer_function(g_timer_func[clockid].value);
            }
        }
    }
}

int timer_create(clockid_t clockid, struct sigevent * evp, timer_t * timerid)
{
    int timer_id;
    char timer_name[16];

    if ((NULL == evp) || (NULL == timerid)) {
        errno = EINVAL;
        return -1;
    }

    /* Only support SIGEV_THREAD. */
    if (evp->sigev_notify != SIGEV_THREAD) {
        errno = ENOTSUP;
        return -1;
    }

    memset(timer_name, 0, sizeof(timer_name));
    snprintf(timer_name, sizeof(timer_name), "timer_%ld", clockid);

    sem_init(&(timer_sem[clockid]), 0, 0);

    g_timer_func[clockid].value = evp->sigev_value;
    g_timer_func[clockid].user_timer_function = evp->sigev_notify_function;
    g_timer_func[clockid].timer_flags = *(int *)(evp->sigev_notify_attributes);

    pthread_attr_t attr;
    attr.schedparam.sched_priority = 22;
    attr.stacksize = 2048;

    pthread_args_t args;
    args.pthread_name = timer_name;
    args.arg = &clockid;

    pthread_create(&(timer_task[clockid]), &attr, &timer_callback, (void *)&args);

    timer_id = UserTimerCreate(timer_name, NULL, (void *)&(timer_sem[clockid]), clockid, g_timer_func[clockid].timer_flags);
    *timerid = timer_id;
    return timer_id;
}

int timer_delete(timer_t timerid)
{
    sem_t sem;
    int timer_id = timerid;
    pthread_kill(timer_task[timer_id], 0);
    
    UserTimerQuitRun(timerid);

    sem = timer_sem[timer_id];
    sem_destroy(&sem);

    return 0;
}

int timer_settime(timer_t timerid, int flags, const struct itimerspec *restrict value,
                  struct itimerspec *restrict ovalue)
{
    if (NULL == value) {
        errno = EFAULT;
        return -1;
    }

    //reference from RTT
    /* calculate timer period(tick);  To avoid lost of accuracy, because "TICK_PER_SECOND" maybe 100, 1000, 1024 and so on.
        *
        *          tick                        millisecond                          millisecond * TICK_PER_SECOND
        *  ------------------------- = --------------------------  --->  tick = -------------------------------------
        *    TICK_PER_SECOND           MILLISECOND_PER_SECOND                         MILLISECOND_PER_SECOND
        *
        */
    uint32_t ms_value = value->it_interval.tv_nsec / 1000000;
    uint32_t ticks = (value->it_interval.tv_sec * TICK_PER_SECOND) + (ms_value * TICK_PER_SECOND) / 1000;

    UserTimerModify(timerid, ticks);

    if (flags) {
        UserTimerStartRun(timerid);  
    }
}
