#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h> /* Needed for KERN_INFO */
#include <linux/init.h>   /* Custom named entry/exit function */
#include "sysmap.h"       /* Pointers to system functions */

// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
typedef int (*fun_int_void)(void);

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
    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    printk(KERN_INFO "Gruppe 6 says goodbye.\n");
}

/* Declare init and exit routines */
module_init(_init_module);
module_exit(_cleanup_module);
