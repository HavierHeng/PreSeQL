#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

/* What the Parser Should Do:
1. Go through the streams of tokens
2. For each token in tokens:
    2.1. See what is the first statement (Eg. select)
    2.2 if (token == select) {...} (Repeat this for each type of command we allow)
        2.2.1 see the next token (It must be a identifier_column)
        2.2.2 Iterate all the tokens until we see a token == FROM
        2.2.3 If token != FROM 
            2.2.3.1 return Syntex Error
        2.2.4 if token != table identifier
            2.2.4.1 return Syntax Error
        2.2.5 return SELECT OPCODE
*/

Token *peek(Parser *parser) {
    if (parser->pos < parser->count) {
        return parser->tokens[parser->pos];
    }
    return NULL;
}

Token *advance(Parser *parser) {
    if (parser->pos < parser->count) {
        return parser->tokens[parser->pos++];
    }
    return NULL;
}

bool match(Parser *parser, TokenType type) {
    Token *token = peek(parser);
    if (token && token->type == type) {
        parser->pos++;
        return true;
    }
    return false;
}

Token *expect(Parser *parser, TokenType type, const char *expected) {
    Token *token = advance(parser);
    if (!token || token->type != type) {
        fprintf(stderr, "Parse error: expected %s, got '%s'\n", expected, token ? token->lexeme : "EOF");
        return NULL;
    }

    return token;
}

SelectStatement *parse_select(Parser *parser) {

    if (!expect(parser, TOKEN_KEYWORD_SELECT, "SELECT")) goto error;

    char **columns = NULL;
    size_t column_count = 0;

    if (match(parser, TOKEN_OP_STAR)) {
        columns = (char **) malloc(sizeof(char *));
        columns[0] = strdup("*");
        column_count = 1;
    } else {
        do {
            Token *col = expect(parser, TOKEN_IDENTIFIER, "column name");
            if (!col) goto error;
            columns = realloc(columns, sizeof(char *) * (column_count + 1));
            columns[column_count++] = strdup(col->lexeme);
        } while (match(parser, TOKEN_PUNC_COMMA));
    }

    if (!expect(parser, TOKEN_KEYWORD_FROM, "FROM")) goto error;
    Token *table = expect(parser, TOKEN_IDENTIFIER, "table name");
    if (!table) goto error;
    expect(parser, TOKEN_END_OF_LINE, "';'");

    SelectStatement *stmt = malloc(sizeof(SelectStatement));
    stmt->columns = columns;
    stmt->column_count = column_count;
    stmt->table_name = strdup(table->lexeme);
    return stmt;


    error:
        fprintf(stderr, "SELECT statement parsing failed.\n");
        for (size_t i = 0; i < column_count; i++) free(columns[i]);
        free(columns);
        return NULL;
}

void parse_tokens(Token **token_streams, size_t token_count) {

    Parser parser = {.tokens = token_streams, .count = token_count, .pos=0};
    printf("Parsing Tokens\n");

    Token *token = peek(&parser);

    if (token->type == TOKEN_KEYWORD_SELECT) {
        SelectStatement *stmt = parse_select(&parser);
        if (!stmt) return;

        printf("Parsed SELECT statement on table %s with columns: ", stmt->table_name);
        for (size_t i = 0; i < stmt->column_count; i++) {
            printf("%s ", stmt->columns[i]);
        }
        printf("\n");

        // Free memory after printing
        for (size_t i = 0; i < stmt->column_count; i++) {
            free(stmt->columns[i]);
        }
        free(stmt->columns);
        free(stmt->table_name);
        free(stmt);
    } else {
        fprintf(stderr, "Unsupported Statement Starting with: %s\n", token->lexeme);
    }

    // TODO: Continue with other statements INSERT, CREATE, BEGIN, COMMIT (add into if as else if)


}
