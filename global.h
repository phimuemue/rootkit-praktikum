// here go some global definitions and functions
#ifndef GRUPPE6_GLOBAL
#define GRUPPE6_GLOBAL

#include <linux/module.h>


// since we had no problems without get/put, we commented it out
//#define USE_MODULE_GET_PUT
#ifdef USE_MODULE_GET_PUT
#define OUR_TRY_MODULE_GET if (!try_module_get(THIS_MODULE)){return -EAGAIN;}
#define OUR_MODULE_PUT module_put(THIS_MODULE)
#else
#define OUR_TRY_MODULE_GET
#define OUR_MODULE_PUT
#endif

#define DEBUG_ON
#ifdef DEBUG_ON
#define OUR_DEBUG(...) printk(KERN_INFO __VA_ARGS__)
#else
#define OUR_DEBUG(...)
#endif


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


/* Make a certain address writeable */
void make_page_writable(long unsigned int _addr);

/* Make a certain address readonly */
void make_page_readonly(long unsigned int _addr);

#endif
