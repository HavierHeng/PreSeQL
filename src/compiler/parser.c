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

Token *peek(Parser *parser)
{
    if (parser->pos < parser->count)
    {
        return parser->tokens[parser->pos];
    }
    return NULL;
}

Token *advance(Parser *parser)
{
    if (parser->pos < parser->count)
    {
        return parser->tokens[parser->pos++];
    }
    return NULL;
}

bool match(Parser *parser, TokenType type)
{
    Token *token = peek(parser);
    if (token && token->type == type)
    {
        parser->pos++;
        return true;
    }
    return false;
}

Token *expect(Parser *parser, TokenType type, const char *expected)
{
    Token *token = advance(parser);
    if (!token || token->type != type)
    {
        fprintf(stderr, "Parse error: expected %s, got '%s'\n", expected, token ? token->lexeme : "EOF");
        return NULL;
    }

    return token;
}

ColumnType parse_column_type(Token *type_token, int *out_length)
{
    if (strcmp(type_token->lexeme, "INT") == 0)
    {
        *out_length = -1;
        return COLTYPE_INT;
    }
    else if (strncmp(type_token->lexeme, "VARCHAR", 7) == 0)
    {
        *out_length = 100; // default, unless parsing extra

        // Check for VARCHAR(n)
        const char *paren = strchr(type_token->lexeme, '(');
        if (paren)
        {
            int len = atoi(paren + 1); // naive extract
            *out_length = len;
        }
        return COLTYPE_VARCHAR;
    }
    else
    {
        fprintf(stderr, "Unknown column type: %s\n", type_token->lexeme);
        *out_length = -1;
        return COLTYPE_VARCHAR; // fallback
    }
}

SelectStatement *parse_select(Parser *parser)
{

    char **columns = NULL;
    size_t column_count = 0;

    if (!expect(parser, TOKEN_KEYWORD_SELECT, "SELECT"))
        goto error;
    if (match(parser, TOKEN_OP_STAR))
    {
        columns = (char **)malloc(sizeof(char *));
        columns[0] = strdup("*");
        column_count = 1;
    }
    else
    {
        do
        {
            Token *col = expect(parser, TOKEN_IDENTIFIER, "column name");
            if (!col)
                goto error;
            columns = realloc(columns, sizeof(char *) * (column_count + 1));
            columns[column_count++] = strdup(col->lexeme);
        } while (match(parser, TOKEN_PUNC_COMMA));
    }

    if (!expect(parser, TOKEN_KEYWORD_FROM, "FROM"))
        goto error;
    Token *table = expect(parser, TOKEN_IDENTIFIER, "table name");
    if (!table)
        goto error;
    expect(parser, TOKEN_END_OF_LINE, "';'");

    SelectStatement *stmt = malloc(sizeof(SelectStatement));
    stmt->columns = columns;
    stmt->column_count = column_count;
    stmt->table_name = strdup(table->lexeme);
    return stmt;

error:
    fprintf(stderr, "Select statement parsing failed.\n");
    for (size_t i = 0; i < column_count; i++)
        free(columns[i]);
    free(columns);
    return NULL;
}

InsertStatement *parse_insert(Parser *parser)
{

    char **columns = NULL;
    size_t column_count = 0;
    Value *values = NULL;
    size_t value_count = 0;
    Value val;

    if (!expect(parser, TOKEN_KEYWORD_INSERT, "INSERT"))
        goto error;
    if (!expect(parser, TOKEN_KEYWORD_INTO, "INTO"))
        goto error;
    Token *table = expect(parser, TOKEN_IDENTIFIER, "table name");
    if (!table)
        goto error;
    if (match(parser, TOKEN_PUNC_OPEN_PAREN))
    {
        do
        {
            Token *col = expect(parser, TOKEN_IDENTIFIER, "column name");
            if (!col)
                goto error;
            columns = realloc(columns, sizeof(char *) * (column_count + 1));
            columns[column_count++] = strdup(col->lexeme);
        } while (match(parser, TOKEN_PUNC_COMMA));

        if (!expect(parser, TOKEN_PUNC_CLOSE_PAREN, "')'"))
            goto error;
    }
    if (!expect(parser, TOKEN_KEYWORD_VALUES, "VALUES"))
        goto error;
    if (!expect(parser, TOKEN_PUNC_OPEN_PAREN, "("))
        goto error;

    do
    {
        Token *val_token = advance(parser);
        if (!val_token)
            goto error;

        if (val_token->type == TOKEN_VARCHAR_LITERAL)
        {
            val.type = VALUE_TEXT;
            val.text_value = strndup(val_token->lexeme + 1, strlen(val_token->lexeme) - 2);
        }
        else if (val_token->type == TOKEN_INT_LITERAL)
        {
            val.type = VALUE_INTEGER;
            val.int_value = atoi(val_token->lexeme);
        }
        else
        {
            fprintf(stderr, "Unexpected token '%s' in value list.\n", val_token->lexeme);
            goto error;
        }

        values = realloc(values, sizeof(Value) * (value_count + 1));
        values[value_count++] = val;

    } while (match(parser, TOKEN_PUNC_COMMA));

    if (!expect(parser, TOKEN_PUNC_CLOSE_PAREN, "')'"))
        goto error;
    if (!expect(parser, TOKEN_END_OF_LINE, "';'"))
        goto error;

    InsertStatement *stmt = malloc(sizeof(InsertStatement));
    stmt->table_name = strdup(table->lexeme);
    stmt->columns = columns;
    stmt->column_count = column_count;
    stmt->values = values;
    stmt->value_count = value_count;

    return stmt;

error:
    fprintf(stderr, "INSERT statement parsing failed.\n");
    for (size_t i = 0; i < column_count; i++)
        free(columns[i]);
    free(columns);
    return NULL;
}

CreateStatement *parse_create(Parser *parser)
{

    ColumnDef *columns = NULL;
    size_t column_count = 0;

    if (!expect(parser, TOKEN_KEYWORD_CREATE, "CREATE"))
        goto error;
    if (!expect(parser, TOKEN_KEYWORD_TABLE, "TABLE"))
        goto error;

    Token *table = expect(parser, TOKEN_IDENTIFIER, "table name");
    if (!table)
        goto error;

    if (!expect(parser, TOKEN_PUNC_OPEN_PAREN, "'('"))
        goto error;

    do
    {
        Token *col_name = expect(parser, TOKEN_IDENTIFIER, "column name");
        if (!col_name)
            goto error;

        Token *type_token = peek(parser);
        if (!type_token)
            goto error;

        ColumnType col_type;
        int type_length = -1;

        if (type_token->type == TOKEN_TYPE_INT)
        {
            advance(parser);
            col_type = COLTYPE_INT;
        }
        else if (type_token->type == TOKEN_TYPE_VARCHAR)
        {
            advance(parser);
            col_type = COLTYPE_VARCHAR;

            if (match(parser, TOKEN_PUNC_OPEN_PAREN))
            {
                Token *length_token = expect(parser, TOKEN_INT_LITERAL, "VARCHAR length");
                if (!length_token)
                    goto error;

                type_length = atoi(length_token->lexeme);

                if (!expect(parser, TOKEN_PUNC_CLOSE_PAREN, "')'"))
                    goto error;
            }
            else
            {
                type_length = 255; // default VARCHAR length
            }
        }
        else
        {
            fprintf(stderr, "Unsupported column type: %s\n", type_token->lexeme);
            goto error;
        }

        ColumnDef def = {
            .name = strdup(col_name->lexeme),
            .type = col_type,
            .type_length = type_length};

        columns = realloc(columns, sizeof(ColumnDef) * (column_count + 1));
        columns[column_count++] = def;

    } while (match(parser, TOKEN_PUNC_COMMA));

    if (!expect(parser, TOKEN_PUNC_CLOSE_PAREN, "')'"))
        goto error;
    if (!expect(parser, TOKEN_END_OF_LINE, "';'"))
        goto error;

    CreateStatement *stmt = malloc(sizeof(CreateStatement));
    stmt->table_name = strdup(table->lexeme);
    stmt->columns = columns;
    stmt->column_count = column_count;
    return stmt;

error:
    fprintf(stderr, "CREATE TABLE parsing failed.\n");
    for (size_t i = 0; i < column_count; i++)
        free(columns[i].name);
    free(columns);
    return NULL;
}

ControlStatement *parse_begin(Parser *parser)
{
    if (!expect(parser, TOKEN_KEYWORD_BEGIN, "BEGIN"))
        return NULL;
    if (!expect(parser, TOKEN_KEYWORD_TRANSACTION, "TRANSACTION"))
        return NULL;
    if (!expect(parser, TOKEN_END_OF_LINE, "';'"))
        return NULL;

    ControlStatement *stmt = malloc(sizeof(ControlStatement));
    stmt->type = STMT_BEGIN;
    return stmt;
}

ControlStatement *parse_commit(Parser *parser)
{
    if (!expect(parser, TOKEN_KEYWORD_COMMIT, "COMMIT"))
        return NULL;
    if (!expect(parser, TOKEN_END_OF_LINE, "';'"))
        return NULL;

    ControlStatement *stmt = malloc(sizeof(ControlStatement));
    stmt->type = STMT_COMMIT;
    return stmt;
}

void free_select_statement(SelectStatement *stmt)
{
    for (size_t i = 0; i < stmt->column_count; i++)
    {
        free(stmt->columns[i]);
    }
    free(stmt->columns);
    free(stmt->table_name);
    free(stmt);
}

void free_insert_statement(InsertStatement *stmt)
{
    if (!stmt)
        return;

    free(stmt->table_name);

    if (stmt->columns)
    {
        for (size_t i = 0; i < stmt->column_count; i++)
        {
            free(stmt->columns[i]);
        }
        free(stmt->columns);
    }

    if (stmt->values)
    {
        for (size_t i = 0; i < stmt->value_count; i++)
        {
            if (stmt->values[i].type == VALUE_TEXT)
            {
                free(stmt->values[i].text_value);
            }
        }
        free(stmt->values);
    }

    free(stmt);
}

void free_create_statement(CreateStatement *stmt)
{
    for (size_t i = 0; i < stmt->column_count; i++)
    {
        free(stmt->columns[i].name);
    }
    free(stmt->columns);
    free(stmt->table_name);
    free(stmt);
}

void parse_tokens(Token **token_streams, size_t token_count)
{

    Parser parser = {.tokens = token_streams, .count = token_count, .pos = 0};
    printf("Parsing Tokens\n");

    Token *token = peek(&parser);

    if (token->type == TOKEN_KEYWORD_SELECT)
    {
        SelectStatement *stmt = parse_select(&parser);
        if (!stmt)
            return;

        // Debugging
        printf("Parsed SELECT statement on table %s with columns: ", stmt->table_name);
        for (size_t i = 0; i < stmt->column_count; i++)
        {
            printf("%s ", stmt->columns[i]);
        }
        printf("\n");

        free_select_statement(stmt);
    }
    else if (token->type == TOKEN_KEYWORD_INSERT)
    {
        InsertStatement *stmt = parse_insert(&parser);
        if (!stmt)
            return;

        if (stmt->column_count != stmt->value_count)
        {
            fprintf(stderr, "Number of coloumns and values does not match\n");
            return;
        }

        // Debugging
        printf("Parsed INSERT statement on table %s with columns and values: ", stmt->table_name);
        for (size_t i = 0; i < stmt->column_count; i++)
        {
            printf("%s ", stmt->columns[i]);
            if (stmt->values[i].type == VALUE_TEXT)
            {
                printf("'%s' ", stmt->values[i].text_value);
            }
            else if (stmt->values[i].type == VALUE_INTEGER)
            {
                printf("%d ", stmt->values[i].int_value);
            }
        }
        printf("\n");

        free_insert_statement(stmt);
    }
    else if (token->type == TOKEN_KEYWORD_CREATE)
    {
        CreateStatement *stmt = parse_create(&parser);
        if (!stmt)
            return;

        printf("Parsed Create statement on table %s with columns and types: ", stmt->table_name);
        for (size_t i = 0; i < stmt->column_count; i++)
        {
            printf("%s ", stmt->columns[i].name);
            if (stmt->columns[i].type == COLTYPE_INT)
            {
                printf("INT ");
            }
            else if (stmt->columns[i].type == COLTYPE_VARCHAR)
            {
                printf("VARCHAR(%d) ", stmt->columns[i].type_length);
            }
        }
        printf("\n");

        free_create_statement(stmt);
    }
    else if (token->type == TOKEN_KEYWORD_BEGIN)
    {
        ControlStatement *stmt = parse_begin(&parser);
        if (!stmt)
            return;

        printf("Parsed BEGIN TRANSACTION statement\n");
        free(stmt);
    }
    else if (token->type == TOKEN_KEYWORD_COMMIT)
    {
        ControlStatement *stmt = parse_commit(&parser);
        if (!stmt)
            return;

        printf("Parsed COMMIT statement\n");
        free(stmt);
    }
    else
    {
        fprintf(stderr, "Unsupported Statement Starting with: %s\n", token->lexeme);
    }
}

void *parse_tokens_with_type(Token **token_streams, size_t token_count, int *out_stmt_type)
{
    Parser parser = {.tokens = token_streams, .count = token_count, .pos = 0};
    printf("Parsing Tokens\n");

    Token *token = peek(&parser);
    if (!token)
        return NULL;

    if (token->type == TOKEN_KEYWORD_SELECT)
    {
        SelectStatement *stmt = parse_select(&parser);
        if (stmt && out_stmt_type)
            *out_stmt_type = STMT_SELECT;
        return stmt;
    }
    else if (token->type == TOKEN_KEYWORD_BEGIN)
    {
        ControlStatement *stmt = parse_begin(&parser);
        if (stmt && out_stmt_type)
            *out_stmt_type = STMT_BEGIN;
        return stmt;
    }
    else if (token->type == TOKEN_KEYWORD_COMMIT)
    {
        ControlStatement *stmt = parse_commit(&parser);
        if (stmt && out_stmt_type)
            *out_stmt_type = STMT_COMMIT;
        return stmt;
    }
    else
    {
        fprintf(stderr, "Unsupported Statement Starting with: %s\n", token->lexeme);
        return NULL;
    }
}