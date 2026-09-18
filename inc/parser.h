#ifndef BEBISH_PARSER_H
#define BEBISH_PARSER_H

#include <stddef.h>

typedef enum {
  TOKEN_WORD,
  TOKEN_PIPE,
  TOKEN_AND,
  TOKEN_OR,
} TokenType;

typedef struct {
  char *value;
  TokenType type;
} Token;

int read_line(char **line, size_t *len);
int parse_line(char *args[], char *line);

#endif // BEBISH_PARSER_H
