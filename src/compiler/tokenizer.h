#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    TOKEN_KEYWORD_SELECT,           // 0
    TOKEN_KEYWORD_FROM,             // 1
    TOKEN_KEYWORD_INSERT,           // 2
    TOKEN_KEYWORD_INTO,             // 3
    TOKEN_KEYWORD_VALUES,           // 4
    TOKEN_KEYWORD_CREATE,           // 5
    TOKEN_KEYWORD_TABLE,            // 6
    TOKEN_KEYWORD_BEGIN,            // 7
    TOKEN_KEYWORD_COMMIT,           // 8
    TOKEN_KEYWORD_TRANSACTION,      // 9
    TOKEN_TYPE_VARCHAR,             // 10
    TOKEN_TYPE_INT,                 // 11
    TOKEN_END_OF_LINE,              // 12
    TOKEN_PUNC_COMMA,               // 13
    TOKEN_PUNC_OPEN_PAREN,          // 14
    TOKEN_PUNC_CLOSE_PAREN,         // 15
    TOKEN_PUNC_QUOTE,               // 16
    TOKEN_OP_STAR,                  // 17
    TOKEN_IDENTIFIER,               // 18
    TOKEN_VARCHAR_LITERAL,          // 19
    TOKEN_INT_LITERAL,              // 20
    TOKEN_UNKNOWN                   // 21
} TokenType;


// Our Token object
typedef struct {
    TokenType type;
    char *lexeme;
    int line_number;
} Token;

Token *create_token(TokenType type, const char *lexeme, int line_number);
void free_token(Token *token);
char* read_source_code(const char *filename);

TokenType is_keyword_strcmp(const char *lexeme);
TokenType is_type_strcmp(const char *lexeme);
TokenType is_operator_strcmp(const char *lexeme);
TokenType is_punctuation_strcmp(const char *lexeme);
bool is_varchar_literal_regex(const char *lexeme);
bool is_integer_literal_regex(const char *lexeme);
bool is_identifier_regex(const char *lexeme);

int tokenize(const char *input, Token ***token_stream, size_t *token_count);
void free_tokens(Token **tokens, size_t count);

#endif