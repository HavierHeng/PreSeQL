#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include "./tokenizer.h"

typedef struct {
    Token **tokens;
    size_t count;
    size_t pos;
} Parser;

typedef struct {
    char **columns;
    size_t column_count;
    char *table_name;
} SelectStatement;

Token *peek(Parser *parser);
Token *advance(Parser *parser);
bool match(Parser *parser, TokenType type);
Token *expect(Parser *parser, TokenType, const char *expected);
void parse_tokens(Token **tokens, size_t count);

#endif