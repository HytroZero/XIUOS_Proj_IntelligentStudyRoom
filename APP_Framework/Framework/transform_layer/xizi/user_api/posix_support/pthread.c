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
* @file:    pthread.c
* @brief:   posix api of pthread (改进版：修复未对齐读取、trampoline 包装、字符串安全复制)
* @version: 1.1
* @author:  AIIT XUOS Lab / 修订: ChatGPT
* @date:    2025/11/05
*
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "include/pthread.h"

/* 你原来代码的默认值 */
#define DEFAULT_STACK_SIZE  2048
#define DEFAULT_PRIORITY    (KTASK_PRIORITY_MAX/2 + KTASK_PRIORITY_MAX/4)

/* --------------------------
   注意：下面的实现假设：
   - UtaskType 结构包含至少这些成员：
       char name[...];
       int prio;
       size_t stack_size;
       void (*func_entry)(void *);   // 内核通常期望此原型
       void *func_param;
     如果 UtaskType.func_entry 的类型不是 `void (*)(void *)`（例如是返回 void* 的函数指针），
     请根据注释把 wrapper/trampoline 部分调整或直接把 start_routine 赋值过去。
   - UserTaskCreate/Startup/Delete/Quit/GetTaskID 等函数在其它模块实现。
   -------------------------- */

/* wrapper 用来包装用户传入的 start_routine 和其参数，放在堆上以保证对齐 */
struct thread_startup_info {
    void *(*start_routine)(void *);
    void *arg;
};

/* trampoline：实际作为任务入口传给内核（必须匹配 UtaskType.func_entry 的类型）
   它从 heap 上取回启动信息，调用用户的 start_routine，然后释放 info 并退出任务。
   注意：如果用户的 start_routine 需要返回值并供其他线程 join 使用，
   还需实现返回值收集机制（目前未实现）。 */
static void thread_entry(void *vp)
{
    struct thread_startup_info info;
    /* 使用 memcpy 从可能非对齐地址安全复制数据（不过我们这里是分配在 heap，通常已对齐） */
    memcpy(&info, vp, sizeof(info));

    /* 在调用用户函数前释放 heap 上的 info（防止泄露）
       若你的内核要求 info 在任务运行期间仍然存在，请改为不释放或把 info 放到静态/任务专用结构 */
    free(vp);

    /* 调用用户的 start_routine（按 POSIX，返回值可通过 pthread_exit / join 传递，这里简化） */
    if (info.start_routine) {
        (void)info.start_routine(info.arg);
    }

    /* 线程结束调用内核退出函数（你的平台可能为 UserTaskQuit()） */
    UserTaskQuit();

    /* 一般不会返回到这里 */
}

/* pthread_create：更稳健处理 arg、名字、stack 大小与 wrapper */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg)
{
    int ret;
    int pid;
    char task_name[32] = {0};
    static int utask_id = 0;
    UtaskType task;

    /* 填充优先级/栈大小（确保对齐）*/
    if (NULL == attr) {
        task.prio = KTASK_PRIORITY_MAX / 2;
        task.stack_size = 4096;
    } else {
        task.prio = attr->schedparam.sched_priority;

        /* 向上对齐到 16 字节，避免栈对齐问题 */
        size_t ss = attr->stacksize;
        if (ss == 0) ss = DEFAULT_STACK_SIZE;
        task.stack_size = (ss + 15) & ~((size_t)15);
    }

    /* 我们使用 trampoline (thread_entry) 作为内核入口，来确保函数签名兼容性
       注意：如果 UtaskType.func_entry 的类型本来就与 start_routine 完全兼容
       （例如它也是 void *(*)(void* )），则可以直接赋值 start_routine 而不使用 wrapper。 */
    task.func_entry = thread_entry;

    /* 默认把 arg 直接传递给用户函数，但我们通过 heap 上的 startup_info 包装 */
    struct thread_startup_info *info = malloc(sizeof(*info));
    if (info == NULL) {
        return -1; /* 内存不足 */
    }
    info->start_routine = start_routine;
    info->arg = arg;

    /* task.func_param 应指向包装结构，以便 trampoline 能读取 */
    task.func_param = (void *)info;

    /* 处理任务名：如果调用方约定传入一个包含名字的结构，需要调用方保证此约定。
       为保持通用性：如果 arg 被识别为我们自己的约定类型（pthread_args_t），
       下面这一段可以按需启用。因为我们不知道 pthread_args_t 的定义，
       这里仅在 arg 非 NULL 且通过简单检测可识别时替换名字：
       （注：此处去掉对 arg 的结构体字段直接访问，避免未对齐访问）
    */
    /* 默认生成名字 */
    snprintf(task_name, sizeof(task_name), "utask%02d", utask_id++);

    /* 如果你有单独的约定（例如 arg 是指向 pthread_args_t），并且想要支持从中读取名字，
       请在这里做类型检查并安全复制（谨慎——不要直接把任意指针按 struct 解引用）。 */

    /* 安全地复制到 task.name，确保终止符 */
    /* 假设 task.name 的长度至少和 task_name 相同；如不是请调整 */
    snprintf(task.name, sizeof(task.name), "%s", task_name);

    pid = UserTaskCreate(task);
    if (pid < 0) {
        free(info);
        return -1;
    }

    ret = UserTaskStartup(pid);
    if (ret < 0) {
        /* 启动失败，释放 info 并删除任务（若需要） */
        free(info);
        /* 有些平台需要删除 task 资源，这里尝试删除 */
        UserTaskDelete(pid);
        return -1;
    }

    /* 返回 pthread_t（我们把 pid 转换为 pthread_t） */
    *thread = (pthread_t)(long)pid;

    return 0;
}

int pthread_attr_init(pthread_attr_t *attr)
{
    /* 初始化为默认值（如需更多字段请补充） */
    if (attr == NULL) return -1;
    attr->schedparam.sched_priority = KTASK_PRIORITY_MAX / 2;
    attr->stacksize = DEFAULT_STACK_SIZE;
    return 0;
}

int pthread_attr_setschedparam(pthread_attr_t           *attr,
                               struct sched_param const *param)
{
    if (attr == NULL || param == NULL) return -1;
    attr->schedparam.sched_priority = param->sched_priority;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stack_size)
{
    if (attr == NULL) return -1;
    attr->stacksize = stack_size;
    return 0;
}

void pthread_exit(void *value_ptr){
    /* TODO: 支持退出值传递给 pthread_join */
    (void)value_ptr;
    UserTaskQuit();
}

pthread_t pthread_self(void){
    pthread_t pthread;
    pthread = (pthread_t)(long)UserGetTaskID();
    return pthread;
}

int pthread_setschedparam(pthread_t thread, int policy,
                          const struct sched_param *pParam)
{
    (void)thread; (void)policy; (void)pParam;
    return 0;
}

int pthread_setschedprio(pthread_t thread, int prio)
{
    (void)thread; (void)prio;
    /* 若内核支持设置优先级，应在此实现 syscall 或内核调用 */
    return 0;
}

int pthread_equal(pthread_t t1, pthread_t t2)
{
    return (int)(t1 == t2);
}

int pthread_cancel(pthread_t thread)
{
    (void)thread;
    /* 若内核支持取消，需要实现具体逻辑 */
    return -1;
}

void pthread_testcancel(void)
{
    return;
}

int pthread_setcancelstate(int state, int *oldstate)
{
    (void)state; (void)oldstate;
    return -1;
}

int pthread_setcanceltype(int type, int *oldtype)
{
    (void)type; (void)oldtype;
    return -1;
}

int pthread_join(pthread_t thread, void **retval)
{
    (void)thread; (void)retval;
    /* 若需要支持 join，需要在创建线程时保存返回值并允许阻塞等待 */
    return -1;
}

/* pthread_kill：修复了对栈上地址取址再解引用导致的未对齐访问问题
   之前错误：
     int32_t *thread_id_tmp = (void *)&thread;
     UserTaskDelete(*thread_id_tmp);
   这一行为会在 RISC-V 上引发 Load address misaligned
*/
int pthread_kill(pthread_t thread, int sig)
{
    (void)sig;
    int pid = (int)(long)thread;  /* 根据 pthread_t 的实际定义调整转换 */
    UserTaskDelete(pid);
    return 0;
}

/* 可选的对齐检测帮助函数（用于调试）
   如果你希望在运行时检测未对齐问题，可以开启并在可疑位置调用 */
#if 0
#include <inttypes.h>
static inline int is_aligned(const void *p, size_t align) {
    return (((uintptr_t)p) & (align - 1)) == 0;
}
#endif


