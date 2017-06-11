#include "syntax.h"

gboolean redir(struct scanner *s, struct stmt_node *node, enum op_type ot)
{
    enum token_type tt = s->type;
    if (tt == TOK_BLANK)
        tt = scanner_next_token(s);
    if (tt != TOK_ID) {
        g_printerr("Syntax Error!\n");
        return FALSE;
    }
    if (ot == OP_LESS)
        node->input_redir = s->data;
    else if (ot == OP_GREAT)
        node->output_redir = s->data;
    tt = scanner_next_token(s);
    if (tt == TOK_BLANK)
        tt = scanner_next_token(s);
    if (tt == TOK_OP) {
        ot = GPOINTER_TO_INT(s->data);
        scanner_next_token(s);
        redir(s, node, ot);
    }
    return TRUE;
}

gboolean sp(struct scanner *s, struct stmt_node *node)
{
    enum token_type tt;

    tt = s->type;
    if (tt == TOK_BLANK) {
        tt = scanner_next_token(s);
        if (tt == TOK_ID) {
            g_ptr_array_add(node->argv, s->data);
            scanner_next_token(s);
            sp(s, node);
        }
        else if (tt == TOK_OP) {
            enum op_type ot = GPOINTER_TO_INT(s->data);
            scanner_next_token(s);
            redir(s, node, ot);
        }
    }
    return TRUE;
}

gboolean sn(struct scanner *s, struct stmt_node **node)
{
    *node = g_new0(struct stmt_node, 1);
    struct stmt_node *p = *node;
    p->argv = g_ptr_array_new();

    if (s->type == TOK_BLANK)
        scanner_next_token(s);
    if (s->type != TOK_ID) {
        g_printerr("Syntax error");
        g_clear_pointer(node, g_free);
        return FALSE;
    }
    g_ptr_array_add(p->argv, s->data);
    scanner_next_token(s);
    sp(s, p);
    g_ptr_array_add(p->argv, NULL);
    return TRUE;
}

