#ifndef BEBISH_EXECUTE_H
#define BEBISH_EXECUTE_H

enum { MAX_CMD_LEN = 256, MAX_ARGS = 64 };

void execute_child(char *args[]);
void execute_line(char *args[]);

#endif // BEBISH_EXECUTE_H
