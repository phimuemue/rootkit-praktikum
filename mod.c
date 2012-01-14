#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/init.h>      /* Custom named entry/exit function */
#include <net/tcp.h>
#include <net/udp.h>
#include <net/inet_sock.h>
#include <linux/socket.h>
#include <linux/unistd.h>    /* Original read-call */
#include <asm/cacheflush.h>  /* Needed for set_memory_ro, ...*/
#include <asm/pgtable_types.h>
#include <linux/types.h>
#include <linux/syscalls.h>
#include <linux/dirent.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/syscalls.h>

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

typedef asmlinkage long (*fun_long_int_int_int)(int, int, int);
fun_long_int_int_int our_sys_socket;
//extern asmlinkage long (*our_sys_socket)(int, int, int);

// we ourselves hook the (possibly already hooked) socketcall
typedef asmlinkage long (*fun_long_int_unsigned_long)(int, unsigned long __user*);
fun_long_int_unsigned_long orig_socketcall;

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

/* Print the number of running processes */
int print_nr_procs(void){
    fun_int_void npf; // function pointer to function counting processes
    int res;
    npf = (fun_int_void) ptr_nr_processes;
    res = npf();
    printk(KERN_INFO "%d processes running", res);
    return 0;
}

/* We saw that the gruppe1 rootkit sets the PIDs to 0
 * in order to hide the process.
 * We simply search for such a process to detect it.
 * Returns 1 if it finds such a task_struct (and thus
 * suspects the rootkit running), 0 otherwise */
int checkPIDzero(void){
    struct task_struct* task;
    int result = 0;
    task = &init_task;
    OUR_DEBUG("Simple task list:\n");
    do {
        if (task->pid == 0 && strcmp(task->comm, "swapper")){
            printk(KERN_INFO "Task with pid %d: %s\n", task->pid, task->comm);
            result++;
        }
    } while ((task = next_task(task)) != &init_task);
    return result;
}

/* We know that read is hooked whenever the rootkit is executed.
 * Moreover, we know that the "neighbouring" function are left
 * untouched.
 * Thus, we check, whether the address to read deviates heavily
 * from its neighbour addresses.
 */
int checkReadSysCall(void){
    int i;
    unsigned long tmp = 0;
    unsigned long diff;
    unsigned long sum=0;
    int neighbours = 1;
    void** sys_call_table = (void*) ptr_sys_call_table;
    for (i=-neighbours; i<=neighbours; ++i){
        printk(KERN_INFO "Address: %lu\n", tmp);
        if (i!=0){
            tmp = (unsigned long)(sys_call_table[__NR_read+i]);
            sum = sum + tmp/(2*neighbours);
        }
    }
    printk(KERN_INFO "Avg: %lu\n", sum);
    printk(KERN_INFO "Deviations:\n");
    for (i=-neighbours; i<=neighbours; ++i){
        if (i==0) continue; // don't compare the read-call with itself
        tmp = ((unsigned long)(sys_call_table[__NR_read + i]));
        if (tmp > sum){
            diff = tmp - sum;
            printk(KERN_INFO "%lu\n", tmp - sum);
        }
        else {
            diff = sum - tmp;
            printk(KERN_INFO "%lu\n", sum - tmp);
        }
        if (diff * 3 >= ((unsigned long)(sys_call_table[__NR_read])) - sum){
            return 0;
        }
    }
    return 1;
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

asmlinkage long hooked_socketcall(int call, unsigned long __user* args){
    // when a socket gets opened with exaclty the arguments which ss uses, return -1
    // returning -1 causes ss to try it via /proc/net, which we already hooked
    printk(KERN_INFO "[Rootkit-Detector] socketcall\n");
    if(call == SYS_SOCKET && args[0] == AF_NETLINK && args[1] == SOCK_RAW && args[2] == NETLINK_INET_DIAG)
        return our_sys_socket(args[0], args[1], args[2]);
    return orig_socketcall(call, args);
}

/* Initialization routine */
static int __init _init_module(void)
{
    int suspicious_processes;
    void** sys_call_table = (void*) ptr_sys_call_table;
    // "preprocessing" stuff
    printk(KERN_ALERT "This is rootkit detector of gruppe6 for gruppe1\n");
    printk(KERN_ALERT "Hooking socketcall.\n");
    orig_socketcall = (fun_long_int_unsigned_long)
                        sys_call_table[__NR_socketcall];
    make_page_writable((long unsigned int)sys_call_table);    
    sys_call_table[__NR_socketcall] = hooked_socketcall;
    make_page_readonly((long unsigned int)sys_call_table);    
    printk(KERN_ALERT "hooked socketcall.\n");
    print_nr_procs();
    our_sys_socket = ptr_sys_socket;
    // now the actual checks
    if ((suspicious_processes = checkPIDzero())){
        printk(KERN_ALERT "Warning: %d suspicious process with PID 0 found.\n", suspicious_processes);
    }
    if (checkReadSysCall()){
        printk(KERN_ALERT "Warning (heuristic): Read call may be hooked.\n");
    }
    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    void** sys_call_table = (void*) ptr_sys_call_table;
    printk(KERN_INFO "Gruppe 6 says goodbye.\n");
    make_page_writable((long unsigned int)sys_call_table);    
    sys_call_table[__NR_socketcall] = orig_socketcall;
    make_page_readonly((long unsigned int)sys_call_table);    
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
MODULE_DESCRIPTION("prevents your kernel from being hacked");/* What does it do? */

