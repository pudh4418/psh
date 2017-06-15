#ifndef _SYNTAX_H_
#define _SYNTAX_H_
#include <glib.h>
#include "scanner.h"

struct stmt_node {
    GPtrArray *argv;
    char *input_redir;
    char *output_redir;
    char *error_redir;
    int input_fd, output_fd, error_fd;
};

struct pstmt_node {
    GPtrArray *stmts;
    gboolean back;
};

gboolean sn(struct scanner *s, struct stmt_node **node);
gboolean pstmt(struct scanner *s, struct pstmt_node **node);

#endif

