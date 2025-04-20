/* Example client application that uses the PreSeQL database 
 * PreSeQL is an embedded database - this means that it needs a client application to make sense
 * This program can read from stdin or run in CLI mode (default).
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preseql.h"

#define INPUT_BUFFER_SIZE 1024

void run_client() {
    char preseql_input[INPUT_BUFFER_SIZE];
    char *statement = NULL;
    size_t statement_len = 0;

    printf("Welcome to PreSeQL\n");
    printf("Type Statments or 'exit' to quit\n");

    while (1) {
        
        // Running Client
        printf(statement_len == 0 ? "preseql> " : "   --> ");
        if (!fgets(preseql_input, INPUT_BUFFER_SIZE, stdin)) {
            printf("\nExiting.\n");
            break;
        }

        // Remove all trailing \n and replace with null terminator
        size_t len = strlen(preseql_input);
        if (preseql_input[len - 1] == '\n') preseql_input[len - 1] = '\0';

        // Exit the Program
        if (statement_len == 0 && (strcmp(preseql_input, "exit") == 0 || strcmp(preseql_input, ".quit") == 0) ){
            printf("Goodbye. \n");
            break;
        }

        // Clear screen
        if (statement_len == 0 && strcmp(preseql_input, "clear") == 0) {
            printf("\033[2J\033[H");  
            fflush(stdout);
            continue;
        }

        // Append input line to full statement buffer
        size_t line_len = strlen(preseql_input);
        char *new_statement = realloc(statement, statement_len + line_len + 1);
        if (!new_statement) {
            fprintf(stderr, "Memory allocation failed.\n");
            free(statement);
            break;
        }

        statement = new_statement;
        memcpy(statement + statement_len, preseql_input, line_len);
        statement_len += line_len;
        statement[statement_len] = '\0';

        if (strchr(preseql_input, ';')) {
            // Tokenize
            size_t token_count = 0;
            Token **token_stream = tokenize(statement, &token_count);

            for (size_t i = 0; i < token_count; i++) {
                printf("Token { type: %d, lexeme: '%s', line: %d }\n",
                    token_stream[i]->type,
                    token_stream[i]->lexeme,
                    token_stream[i]->line_number);
            }

            // TODO: Parsing
            parse_tokens(token_stream, token_count);

            // Clean Up
            free_tokens(token_stream, token_count);
            free(statement);
            statement = NULL;
            statement_len = 0;
        }
    }

    free(statement);
}

int main(int argc, char** argv) {
    run_client();
    return EXIT_SUCCESS;
}
