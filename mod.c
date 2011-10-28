#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/init.h>      /* Custom named entry/exit function */
#include <linux/unistd.h>    /* Original read-call */
#include <asm/cacheflush.h>  /* Needed for set_memory_ro, ...*/
#include <asm/pgtable_types.h>

#include "sysmap.h"          /* Pointers to system functions */

// the following line gets updated by script 


// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
typedef int (*fun_int_void)(void);
typedef asmlinkage ssize_t (*fun_ssize_t_int_pvoid_size_t)(unsigned int, char __user *, size_t);

fun_ssize_t_int_pvoid_size_t original_read;

/* Make a certain address writeable */
void make_page_writable(long unsigned int _addr){
    unsigned int dummy;
    pte_t *pageTableEntry = lookup_address(_addr, &dummy);

    pageTableEntry->pte |=  _PAGE_RW;
  //return set_memory_rw(_addr, 1);
}

/* Make a certain address readonly */
void make_page_readonly(long unsigned int _addr){
    unsigned int dummy;
    pte_t *pageTableEntry = lookup_address(_addr, &dummy);
    pageTableEntry->pte = pageTableEntry->pte & ~_PAGE_RW;

  //return set_memory_ro(_addr, 1);
}

//ssize_t hooked_read(int fd, void *buf, size_t count){
asmlinkage ssize_t hooked_read(unsigned int fd, char __user *buf, size_t count){
    printk(KERN_INFO "This is a hooked function!!!");
    ssize_t retval = original_read(fd, buf, count);
  //printk(KERN_ALERT "hooked_read: params: fd: %d, buf: %d, count: %d~~~~~~~~~~ return value: %d \n", fd, (int)buf, count, retval);
  return retval;
}

/* Hooks the read system call. */
void hook_function(void){
  void** sys_call_table = (void *) ptr_sys_call_table;
  original_read = sys_call_table[__NR_read];
  printk(KERN_ALERT "mod.ko: 2");

  make_page_writable((long unsigned int) ptr_sys_call_table);
  printk(KERN_ALERT "mod.ko: 3");

  sys_call_table[__NR_read] = (void*) hooked_read;
  printk(KERN_ALERT "mod.ko: 4");

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
    printk(KERN_INFO "This is the kernel module of gruppe 6.\n");
    print_nr_procs();
    hook_function();
    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    //sys_call_table[__NR_read] = original_read;
    make_page_readonly((long unsigned int) ptr_sys_call_table);
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
MODULE_AUTHOR("Philipp Müller, Roman Karlstetter");	/* Who wrote this module? */
MODULE_DESCRIPTION("hacks your kernel");                /* What does it do? */

