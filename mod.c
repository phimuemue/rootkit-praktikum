#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/init.h>      /* Custom named entry/exit function */
#include <linux/unistd.h>    /* Original read-call */
#include <asm/cacheflush.h>  /* Needed for set_memory_ro, ...*/

#include "sysmap.h"          /* Pointers to system functions */

// the following line gets updated by script 
void** sys_call_table;

// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
typedef int (*fun_int_void)(void);
typedef asmlinkage int (*fun_int_pchar_int_int)(const char*, int, int);

fun_int_pchar_int_int original_read;

/* Make a certain address writeable */
int make_page_writable(long unsigned int _addr){
  return set_memory_rw(_addr, 1);
}

/* Make a certain address readonly */
int make_page_readable(long unsigned int _addr){
  return set_memory_ro(_addr, 1);
}

int hooked_read(const char* c, int arg1, int arg2){
  printk(KERN_INFO "hooked_read\n");
  return original_read(c, arg1, arg2);
}

/* Hooks the read system call. */
int hook_function(){
  make_page_writable(sys_call_table);
  original_read = sys_call_table[__NR_read];
  sys_call_table[__NR_read] = (void*)hooked_read;
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
    sys_call_table = ptr_sys_call_table;
    hook_function();
    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    sys_call_table[__NR_read] = original_read;
    printk(KERN_INFO "Gruppe 6 says goodbye.\n");
}

/* Declare init and exit routines */
module_init(_init_module);
module_exit(_cleanup_module);
