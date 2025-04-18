#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include "tokenizer.h"

typedef enum {
    VALUE_TEXT,
    VALUE_INTEGER
} ValueType;

typedef struct {
    ValueType type;
    union  {
        char *text_value;
        int int_value;
    };
} Value;

typedef enum {
    COLTYPE_INT,
    COLTYPE_VARCHAR,
} ColumnType;

typedef struct {
    char *name;
    ColumnType type;
    int type_length; // -1 for INT, others for varchar
} ColumnDef;

typedef enum {
    STMT_BEGIN,
    STMT_COMMIT,
} StatementType;

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

typedef struct {
    char **columns;
    size_t column_count;
    char *table_name;
    Value *values;
    size_t value_count;
} InsertStatement;

typedef struct {
    char *table_name;
    ColumnDef *columns;
    size_t column_count;
} CreateStatement;

typedef struct {
    StatementType type;
} ControlStatement;

Token *peek(Parser *parser);
Token *advance(Parser *parser);
bool match(Parser *parser, TokenType type);
Token *expect(Parser *parser, TokenType, const char *expected);
ColumnType parse_column_type(Token *type_token, int *out_length);
void free_select_statement(SelectStatement *stmt);
void free_insert_statement(InsertStatement *stmt);
void free_create_statement(CreateStatement *stmt);
void parse_tokens(Token **tokens, size_t count);

#endif
