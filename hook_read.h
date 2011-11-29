typedef void (*fun_void_charp_int)(char *buf, int size);

/* Hooks the read system call. */
void hook_read(fun_void_charp_int cb);

/* Hooks the read system call. */
void unhook_read(void);
