#include "scanner.h"
#include <stdlib.h>
#include <readline/readline.h>
#include <ctype.h>

struct scanner *scanner_new(void)
{
    return g_new(struct scanner, 1);
}

void scanner_free(struct scanner *s)
{
    g_free(s);
}

void scanner_set_text(struct scanner *s, char *t)
{
    s->text = t;
    s->cur_pos = t;
    s->type = TOK_INV;
}

static void scanner_set_token(struct scanner *s, char *p, enum token_type type, gpointer pp)
{
    s->cur_pos = p;
    s->type = type;
    s->data = pp;
}

static char *double_string(struct scanner *s, GString *str)
{
    char *p = s->cur_pos, *strn;
    ++p;
    gboolean ended = FALSE;
    while (!ended) {
        while (*p != '\0') {
            if (*p == '"') {
                ended = TRUE;
                ++p;
                break;
            }
            else if (*p == '\\') {
                ++p;
                if (*p == '\\' || *p == '"') {
                    g_string_append_c(str, *p);
                    ++p;
                }
                else {
                    g_string_append_c(str, *(p-1));
                    g_string_append_c(str, *p);
                    ++p;
                }
            }
            else {
                g_string_append_c(str, *p);
                ++p;
            }
        }
        if (!ended) {
            free(s->text);
            g_string_append_c(str, '\n');
            strn = readline("> ");
            scanner_set_text(s, strn);
            p = s->cur_pos;
        }
    }
    return p;
}

static char *single_string(struct scanner *s, GString *str)
{
    char *p = s->cur_pos, *strn;
    ++p;
    gboolean ended = FALSE;
    while (!ended) {
        while (*p != '\0') {
            if (*p == '\'') {
                ended = TRUE;
                ++p;
                break;
            }
            else if (*p == '\\') {
                ++p;
                if (*p == '\\' || *p == '\'') {
                    g_string_append_c(str, *p);
                    ++p;
                }
                else {
                    g_string_append_c(str, *(p-1));
                    g_string_append_c(str, *p);
                    ++p;
                }
            }
            else {
                g_string_append_c(str, *p);
                ++p;
            }
        }
        if (!ended) {
            free(s->text);
            g_string_append_c(str, '\n');
            strn = readline("> ");
            scanner_set_text(s, strn);
            p = s->cur_pos;
        }
    }
    return p;
}

enum token_type scanner_next_token(struct scanner *s)
{
    char *p = s->cur_pos;

    if (*p == '\0') {
        scanner_set_token(s, p, TOK_EOL, NULL);
        return TOK_EOL;
    }

    if (isspace(*p)) {
        GString *str = g_string_new(NULL);
        g_string_append_c(str, *p);
        ++p;
        while (isspace(*p)) {
            g_string_append_c(str, *p);
            ++p;
        }
        g_string_free(str, TRUE);
        scanner_set_token(s, p, TOK_BLANK, NULL);
        return TOK_BLANK;
    }

    if (*p == '>') {
        scanner_set_token(s, ++p, TOK_OP, GINT_TO_POINTER(OP_GREAT));
        return TOK_OP;
    }

    if (*p == '<') {
        scanner_set_token(s, ++p, TOK_OP, GINT_TO_POINTER(OP_LESS));
        return TOK_OP;
    }

    if (*p == '|') {
        scanner_set_token(s, ++p, TOK_PIPE, NULL);
        return TOK_PIPE;
    }

    GString *str = g_string_new(NULL);
    while (*p != '\0') {
        if (*p == '"') {
            p = double_string(s, str); 
        }
        else if (*p == '\'') {
            p = single_string(s, str);
        }
        else if (*p == '\\') {
            ++p;
            if (*p == '\\' || *p == '"' || *p == '\'') {
                g_string_append_c(str, *p);
                ++p;
            }
            else {
                g_string_append_c(str, *(p-1));
                g_string_append_c(str, *p);
                ++p;
            }
        }
        else if (isspace(*p) || *p == '|') {
            break;
        }
        else {
            g_string_append_c(str, *p);
            ++p;
        }
    }
    scanner_set_token(s, p, TOK_ID, g_string_free(str, FALSE));
    return TOK_ID;
}

