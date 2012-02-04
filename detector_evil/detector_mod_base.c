/**
  * Solution of gruppe5 for assignment 9
  */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/sysfs.h>
#include <linux/fs.h>

#include "sysmap.h"

MODULE_LICENSE ( "GPL" );

/**
 * Get unique id for the output
 */
int id;
module_param(id,int,0);

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

unsigned int *_sys_call_table;

static void detect_read(void);
static void count_modules(void);

MODULE_LICENSE("GPL");

/**
 * Init function of the module
 */
static int __init init_mod ( void )
{
        _sys_call_table = ROOT_sys_call_table;

	detect_read();
	count_modules();

	return 0;
}

/**
 * Cleanup function of the module
 */
static void __exit exit_mod ( void )
{
	
}

static void detect_read(void)
{
  unsigned int addr_table_read = _sys_call_table[__NR_read];
  unsigned int addr_sys_read = (unsigned int)ROOT_sys_read;
  
  // Compare the address of the original read system call with
  // the address stored in the syscall table
  
  if (addr_table_read != addr_sys_read)
    printk ( KERN_INFO "%i_detector_mod;read;read_detected\n",id);
  else
    printk ( KERN_INFO "%i_detector_mod;read;read_undetected\n",id);
}

static void count_modules(void)
{
  struct module *mod = THIS_MODULE;
  struct sysfs_dirent *entry, *parent;
  int count = 0;
  
  // Get internal file descriptor of the module directory of sysfs
  entry = mod->mkobj.kobj.sd;
  parent = entry->s_parent;
  
  // Loop over all modules and count
  for (entry = parent->s_dir.children; entry; entry = entry->s_sibling)
  {
    count++;
  }
  
 printk ( KERN_INFO "%i_detector_mod;modules;%i\n",id,count);
  
}


/**
 * Set init and cleanup functions
 */
module_init ( init_mod );
module_exit ( exit_mod );
