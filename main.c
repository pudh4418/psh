#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <glib.h>
#include <unistd.h>
#include "scanner.h"
#include "syntax.h"


int main(void)
{
    char *str = NULL;
    struct scanner *scanner;
    str = readline("$ ");

    scanner = scanner_new();
    scanner_set_text(scanner, str);
    struct stmt_node *n = NULL;
    scanner_next_token(scanner);
    sn(scanner, &n);
    for (int i = 0; i < n->argv->len; ++i)
        g_print("%s\n", n->argv->pdata[i]);
    g_print("Input redir: %s\n", n->input_redir);
    g_print("Output redir: %s\n", n->output_redir);
/*    enum token_type tt;
    while ((tt=scanner_next_token(scanner))!=TOK_EOL) {
        g_print("%s\n", token_name[tt]);
    }*/
    free(str);
    scanner_free(scanner);
    return 0;
}
