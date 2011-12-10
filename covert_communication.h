void handle_input(char *buf, int count);
void add_command(char* name, void* c, int needs_arg);
void clear_commands(void);


struct command{
    char* name;
    void* cmd;
    int needs_argument; // 0 == false; 1 == true
    struct list_head list;
};
