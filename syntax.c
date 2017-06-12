#include "syntax.h"

static gboolean redir(struct scanner *s, struct stmt_node *node, enum op_type ot)
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

static gboolean sp(struct scanner *s, struct stmt_node *node)
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
    p->input_fd = 0;
    p->output_fd = 1;
    p->error_fd = 2;
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

gboolean pstmt(struct scanner *s, struct pstmt_node **node)
{
    *node = g_new0(struct pstmt_node, 1);
    struct pstmt_node *p = *node;
    struct stmt_node *q;

    p->stmts = g_ptr_array_new();
    if (!sn(s, &q)) {
        g_ptr_array_free(p->stmts, TRUE);
        g_clear_pointer(node, g_free);
        return FALSE;
    }
    g_ptr_array_add(p->stmts, q);
    while (s->type == TOK_PIPE) {
        scanner_next_token(s);
        struct stmt_node * n1 = g_ptr_array_index(p->stmts, p->stmts->len-1);
        n1->output_redir = (gpointer) -1;
        sn(s, &q);
        q->input_redir = (gpointer) -1;
        g_ptr_array_add(p->stmts, q);
    }
    return TRUE;
}

