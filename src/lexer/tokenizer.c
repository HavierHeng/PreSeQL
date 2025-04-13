#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <regex.h>

#include "../../include/tokenizer.h"


// Constructor for the Token struct
Token *create_token(TokenType type, const char *lexeme, int line_number) {
    Token *token = (Token *)malloc(sizeof(Token));
    token->type = type;
    token->lexeme = strdup(lexeme);
    token->line_number = line_number;
    return token;
}


// Destructor for the Token struct
void free_token(Token *token) {
    free(token->lexeme);
    free(token);
}


// Read source code
char* read_source_code(const char *filename) {
    
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error Opening file: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);

    long file_size = ftell(file);

    rewind(file);

    char *sql_commmands = (char *) malloc((file_size + 1) * sizeof(char));

    if (sql_commmands == NULL) {
        fprintf(stderr, "Error allocating memory from sql code file\n");
        exit(EXIT_FAILURE);
    }

    size_t read_size = fread(sql_commmands, sizeof(char), file_size, file);

    sql_commmands[read_size] = '\0';

    fclose(file);

    return sql_commmands;

}


// Recognizing keywords using strcmp
TokenType is_keyword_strcmp(const char *lexeme) {
    const struct {
        const char *lexeme;
        TokenType token_type;
    } keywords[] = {
        {"SELECT", TOKEN_KEYWORD_SELECT},
        {"FROM", TOKEN_KEYWORD_FROM},
        {"INSERT", TOKEN_KEYWORD_INSERT},
        {"INTO", TOKEN_KEYWORD_INTO},
        {"VALUES", TOKEN_KEYWORD_VALUES},
        {"CREATE", TOKEN_KEYWORD_CREATE},
        {"TABLE", TOKEN_KEYWORD_TABLE},
        {"BEGIN", TOKEN_KEYWORD_BEGIN},
        {"TRANSACTION", TOKEN_KEYWORD_TRANSACTION},
        {"COMMIT", TOKEN_KEYWORD_COMMIT},
        {NULL, TOKEN_UNKNOWN},
    };

    for (int i = 0; keywords[i].lexeme != NULL; i++) {
        if (strcmp(lexeme, keywords[i].lexeme) == 0) {
            return keywords[i].token_type;
        }
    }
    return TOKEN_UNKNOWN;
}

// Recognizing keywords using strcmp
TokenType is_type_strcmp(const char *lexeme) {
    const struct {
        const char *lexeme;
        TokenType token_type;
    } types[] = {
        {"VARCHAR", TOKEN_TYPE_VARCHAR},
        {"INT", TOKEN_TYPE_INT},
        {NULL, TOKEN_UNKNOWN},
    };

    for (int i = 0; types[i].lexeme != NULL; i++) {
        if (strcmp(lexeme, types[i].lexeme) == 0) {
            return types[i].token_type;
        }
    }
    return TOKEN_UNKNOWN;
}


// Recognizing keywords using strcmp
TokenType is_operator_strcmp(const char *lexeme) {
    const struct {
        const char *lexeme;
        TokenType token_type;
    } operators[] = {
        {"*", TOKEN_OP_STAR},
        {NULL, TOKEN_UNKNOWN}
    };

    for (int i = 0; operators[i].lexeme != NULL; i++) {
        if (strcmp(lexeme, operators[i].lexeme) == 0) {
            return operators[i].token_type;
        }
    }
    return TOKEN_UNKNOWN;
    
}

TokenType is_punctuation_strcmp(const char *lexeme) {
    const struct {
        const char *lexeme;
        TokenType token_type;
    } punctuations[] = {
        {"(", TOKEN_PUNC_OPEN_PAREN},
        {")", TOKEN_PUNC_CLOSE_PAREN},
        {"'", TOKEN_PUNC_QUOTE},
        {";", TOKEN_END_OF_LINE},
        {",", TOKEN_PUNC_COMMA},
        {NULL, TOKEN_UNKNOWN}
    };

    for (int i = 0; punctuations[i].lexeme != NULL; i++) {
        if (strcmp(lexeme, punctuations[i].lexeme) == 0) {
            return punctuations[i].token_type;
        }
    }
    return TOKEN_UNKNOWN;
}

bool is_varchar_literal_regex(const char *lexeme) {
    const char *pattern = "^'([^']|'')*'$";
    regex_t regex;
    int result = regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB);
    if (result != 0) {
        return false;
    }
    result = regexec(&regex, lexeme, 0, NULL, 0);
    regfree(&regex);
    if (result == 0) {
        return true;
    }
    return false;
}

bool is_integer_literal_regex(const char *lexeme) {
    const char *pattern = "^(0|[1-9][0-9]*)$";
    regex_t regex;
    int result = regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB);
    if (result != 0) {
        return false;
    }
    result = regexec(&regex, lexeme, 0, NULL, 0);
    regfree(&regex);
    if (result == 0) {
        return true;
    }
    return false;
}

bool is_identifier_regex(const char *lexeme) {
    const char *pattern = "^[a-zA-Z_]([a-zA-Z0-9])*$";
    regex_t regex;
    int result = regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB);
    if (result != 0) {
        return false;
    }
    result = regexec(&regex, lexeme, 0, NULL, 0);
    regfree(&regex);
    if (result == 0) {
        return true;
    }
    return false;
}

void free_tokens(Token **token_stream, size_t token_count) {
    for (size_t i = 0; i < token_count; i++) {
        free_token(token_stream[i]);
    }
    free(token_stream);
}

Token **tokenize(const char* sql_commands, size_t *token_count) {

    Token **token_stream = NULL;

    // Maximal Munch Tracking Variables
    size_t sql_commands_len = strlen(sql_commands);
    size_t current_position = 0;
    int current_line_number = 1;

    // Maximal Munch Implementation
    while (current_position < sql_commands_len) {
        
        TokenType token_type = TOKEN_UNKNOWN;
        size_t longest_match = 0;
        char matched_lexeme[1024] = {0};


        // Left to Right Scan producing candidates and testing them
        for (size_t i = current_position; i < sql_commands_len; ++i) {
            char candidate_lexeme[1024] = {0};
            memcpy(candidate_lexeme, sql_commands + current_position, i - current_position + 1);
            TokenType candidate_token_type = TOKEN_UNKNOWN;
            size_t candidate_match_len = i - current_position + 1;

            if ((candidate_token_type = is_keyword_strcmp(candidate_lexeme)) != TOKEN_UNKNOWN ||
            (candidate_token_type = is_type_strcmp(candidate_lexeme)) != TOKEN_UNKNOWN ||
            (candidate_token_type = is_operator_strcmp(candidate_lexeme)) != TOKEN_UNKNOWN ||
            (candidate_token_type = is_punctuation_strcmp(candidate_lexeme)) != TOKEN_UNKNOWN ||
            (candidate_token_type = is_identifier_regex(candidate_lexeme) ? TOKEN_IDENTIFIER : TOKEN_UNKNOWN) != TOKEN_UNKNOWN ||
            (candidate_token_type = is_varchar_literal_regex(candidate_lexeme) ? TOKEN_VARCHAR_LITERAL : TOKEN_UNKNOWN) != TOKEN_UNKNOWN ||
            (candidate_token_type = is_integer_literal_regex(candidate_lexeme) ? TOKEN_INT_LITERAL : TOKEN_UNKNOWN) != TOKEN_UNKNOWN) {
                
                token_type = candidate_token_type;
                longest_match = candidate_match_len;
                strcpy(matched_lexeme, candidate_lexeme);
            }
        }

        if (token_type != TOKEN_UNKNOWN) {
            Token *token = create_token(token_type, matched_lexeme, current_line_number);
            token_stream = (Token **)realloc(token_stream, (*token_count + 1) * sizeof(Token *));
            token_stream[*token_count] = token;
            (*token_count)++;
            printf("Token { type: %d, lexeme: '%s', line: %d }\n",
                token->type,
                token->lexeme,
                token->line_number);
            current_position += longest_match;
        } else {
            if (sql_commands[current_position] == '\n') {
                current_line_number++;
            }
            if (sql_commands[current_position] != ' ' && sql_commands[current_position] != '\n' &&
                sql_commands[current_position] != '\t' && sql_commands[current_position] != '\r') {
                printf("Error: Unrecognized character '%c' at line %d\n", sql_commands[current_position], current_line_number);
            }
            ++current_position;
        }
    }

    return token_stream;
}



// int main() {

//     const char *filename = "../../tests/sqlite_statements.txt";
    
//     char *sql_commands = read_source_code(filename);

//     // Tokens Streams
//     size_t token_count = 0;

//     Token **token_stream = tokenize(sql_commands, &token_count);

//     free_tokens(token_stream, token_count);

//     free(sql_commands);

//     return 0;
// }
