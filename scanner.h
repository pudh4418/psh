#ifndef _SCANNER_H_
#define _SCANNER_H_
#include <glib.h>

enum token_type {
    TOK_INV,
    TOK_BLANK,
    TOK_ID,
    TOK_OP,
    TOK_PIPE,
    TOK_EOL
};

static char *token_name[] = {"TOK_INV", "TOK_BLANK", "TOK_ID", "TOK_OP", "TOK_PIPE", "TOK_EOL"};

enum op_type {
    OP_LESS,
    OP_GREAT
};

struct scanner {
    char *text;
    char *cur_pos;
    enum token_type type;
    gpointer data;
};

struct scanner *scanner_new(void);
void scanner_free(struct scanner *s);
void scanner_set_text(struct scanner *s, char *t);
enum token_type scanner_next_token(struct scanner *s);

#endif

