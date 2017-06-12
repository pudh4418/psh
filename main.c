#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <glib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/param.h>
#include <pwd.h>
#include "scanner.h"
#include "syntax.h"

char *get_homedir(void)
{
    static char *res = NULL;
    if (res)
        return res;
    res = getenv("HOME");
    if (res)
        return res;
    res = getpwuid(getuid())->pw_dir;
    return res;
}

gboolean ce_builtin(struct stmt_node *node, int *ret)
{
    char *s = node->argv->pdata[0];
    char *oldpath, *beforepath;
    char *h;
    if (strcmp(s, "cd") == 0) {
        beforepath = getcwd(NULL, 0);
        oldpath = getenv("OLDPWD");
        if (node->argv->len > 2) {
            h = node->argv->pdata[1];
            if (strcmp(h, "-") == 0)
                h = oldpath;
        }
        else
            h = get_homedir();
        if (chdir(h) == -1) {
            perror("cd");
        }
        setenv("OLDPWD", beforepath, 1);
        free(beforepath);
        h = getcwd(NULL, 0);
        setenv("PWD", h, 1);
        free(h);
        return TRUE;
    }
    else if (strcmp(s, "exit") == 0 || strcmp(s, "quit") == 0) {
        exit(0);
    }
    return FALSE;
}

int exec_pstmt(struct pstmt_node *node)
{
    struct stmt_node *sn, *snp;
    pid_t pid;
    int fd, fds[2], ret, fret;
    sn = g_ptr_array_index(node->stmts, 0);
    if (ce_builtin(sn, &ret)) {
        return ret;
    }
    for (int i = 0; i < node->stmts->len - 1; ++i) {
        sn = g_ptr_array_index(node->stmts, i);
        snp = g_ptr_array_index(node->stmts, i+1);
        pipe(fds);
        sn->output_fd = fds[1];
        snp->input_fd = fds[0];
    }
    for (int i = 0; i < node->stmts->len; ++i) {
        sn = g_ptr_array_index(node->stmts, i);
        if (sn->input_redir != NULL
                && sn->input_redir != (gpointer) -1
                && sn->input_fd == 0) {
            fd = open(sn->input_redir, O_RDONLY);
            sn->input_fd = fd;
        }
        if (sn->output_redir != NULL
                && sn->output_redir != (gpointer) -1
                && sn->output_fd == 1) {
            fd = open(sn->output_redir, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("open");
            }
            sn->output_fd = fd;
        }
        pid = fork();
        if (pid == 0) {
            /* child */
            if (sn->input_fd != 0) {
                dup2(sn->input_fd, 0);
                close(sn->input_fd);
            }
            if (sn->output_fd != 1) {
                dup2(sn->output_fd, 1);
                close(sn->output_fd);
            }
            if (execvp(sn->argv->pdata[0], (char *const *) sn->argv->pdata) == -1) {
                perror(sn->argv->pdata[0]);
            }
        }
        else {
            /* parent */
            if (sn->input_fd != 0) {
                close(sn->input_fd);
            }
            if (sn->output_fd != 1) {
                close(sn->output_fd);
            }
        }
    }
    for (int i = 0; i < node->stmts->len; ++i) {
        if (waitpid(-1, &ret, 0) == pid) {
            fret = ret;
        }
    }
    return fret;
}

int main(void)
{
    char *str = NULL;
    struct scanner *scanner;
    struct pstmt_node *n = NULL;
    using_history();
    scanner = scanner_new();
    while (TRUE) {
        str = readline("$ ");
        if (!str)
            break;
        if (strcmp(str, "") != 0) {
            add_history(str);
            scanner_set_text(scanner, str);
            scanner_next_token(scanner);
            if (pstmt(scanner, &n))
                exec_pstmt(n);
        }
        free(str);
    }
    scanner_free(scanner);
    return 0;
}
