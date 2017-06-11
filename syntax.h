#ifndef _SYNTAX_H_
#define _SYNTAX_H_
#include <glib.h>
#include "scanner.h"

struct stmt_node {
    GPtrArray *argv;
    char *input_redir;
    char *output_redir;
    char *error_redir;
};

struct pstmt_node {
    GPtrArray *stmts;
};

gboolean sn(struct scanner *s, struct stmt_node **node);

#endif

