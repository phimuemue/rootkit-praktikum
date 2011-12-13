#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/init.h>      /* Custom named entry/exit function */
#include <linux/unistd.h>    /* Original read-call */
#include <linux/syscalls.h>
#include <asm/cacheflush.h>  /* Needed for set_memory_ro, ...*/
#include <asm/pgtable_types.h>
#include <linux/smp_lock.h>
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

// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
typedef int (*fun_int_void)(void);
typedef asmlinkage ssize_t (*fun_ssize_t_int_pvoid_size_t)(unsigned int, char __user *, size_t);

fun_ssize_t_int_pvoid_size_t original_read;

struct {
    unsigned short limit;
    unsigned int base;
} __attribute__ ((packed)) idtr; // interrupt descriptor table register

struct {
    unsigned short off1;
    unsigned short sel;
    unsigned char none, flags;
    unsigned short off2;
} __attribute__ ((packed)) idt; // interrupt descriptor table

unsigned sys_call_off;

#define CALLOFF 100

// this beast is currently not working
// void find_sys_call_table(void){
//     char *p;
//     char sc_asm[CALLOFF];
//     unsigned int counter = 0;
//     unsigned sys_call_table;
//     /* Find interrupt descriptor table */
//     asm ("sidt %0" : "=m" (idtr));
//     /* Find the 0x80 entry in the interrupt descriptor table */
//     memcpy (&idt, idtr.base + 8*0x80, sizeof (idt));
//     /* Reconstruct the location of the system_call() function */
//     sys_call_off = (idt.off2 << 16) | idt.off1;
//     //p = (char*)memmem(sc_asm, CALLOFF, "\xff\x14\x85", 3);
//     p = sc_asm;
//     while (p[0] != '\xff' ||
//            p[1] != '\x14' ||
//            p[2] != '\x85'){
//         counter++;
//         p++;
//     }
//     printk (KERN_INFO "counter is %d now\n", counter);
//     sys_call_table = *(unsigned*)(p+3);
//     printk(KERN_INFO "I think I found the system call table at %p (zum Vergleich der pointer aus sysmap.h: %p).\n", sys_call_table, ptr_sys_call_table);
// }

unsigned long **find_sys_call_table(void)  {
  unsigned long **sctable;
  unsigned long ptr;

  sctable = NULL;
  for (	ptr = (unsigned long)&lock_kernel;
	ptr < (unsigned long)&loops_per_jiffy;
	ptr += sizeof(void *)) {
    unsigned long *p;
    p = (unsigned long *)ptr;
    if (p[__NR_close] == (unsigned long) sys_close) {
      sctable = (unsigned long **)p;
      printk(KERN_INFO "I think I found the system call table at %p (zum Vergleich der pointer aus sysmap.h: %p).\n", sctable, ptr_sys_call_table);
      return &sctable[0];
    }
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
    OUR_TRY_MODULE_GET;
    retval = original_read(fd, buf, count);
    cur_buf = buf;
    if (retval > 0){
        // printk(KERN_INFO "%d = hooked_read(%d, %s, %d)\n", retval, fd, buf, count);
    }
    OUR_MODULE_PUT;
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
    find_sys_call_table();
    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    unhook_function();
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

