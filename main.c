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
#include <errno.h>
#include <signal.h>
#include "scanner.h"
#include "syntax.h"

struct prs {
    GPtrArray *argv;
    pid_t pid;
    gboolean completed;
    gboolean stopped;
    int stat;
};

struct job {
    pid_t pgrp;
    int n_ps;
    int fin_ps;
    struct prs *ps;
};

GPtrArray *jobs;
int tty_fd;
pid_t tty_pgrp;

gboolean job_stopped(struct job *j)
{
    struct prs *p;
    for (int i = 0; i < j->n_ps; ++i) {
        p = j->ps + i;
        if (!p->stopped)
            return FALSE;
    }
    return TRUE;
}

gboolean job_completed(struct job *j)
{
    struct prs *p;
    for (int i = 0; i < j->n_ps; ++i) {
        p = j->ps + i;
        if (!p->completed)
            return FALSE;
    }
    return TRUE;
}

void print_jobs(void)
{
    for (int i = 0; i < jobs->len; ++i) {
        struct job *job = g_ptr_array_index(jobs, i);
        printf("[%d]   ", i+1);
        if (job_completed(job))
            printf("Done ");
        else if (job_stopped(job))
            printf("Stopped ");
        else
            printf("Running ");
        GPtrArray *a = (job->ps[0]).argv;
        for (int k = 0; k < a->len - 1; ++k)
            printf("%s ", a->pdata[k]);
        for (int kk = 1; kk < job->n_ps; ++kk) {
            printf("| ");
            for (int k = 0; k < a->len - 1; ++k)
                printf("%s ", a->pdata[k]);
        }
        printf("&");

        printf("\n");
    }
}

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
    else if (strcmp(s, "jobs") == 0) {
        print_jobs();
        return TRUE;
    }
    return FALSE;
}


int update_job_stat(pid_t pid, int r)
{
    if (pid == 0 || errno == ECHILD) {
        return -1;
    }
    else if (pid < 0)
        return -1;
    for (int i = 0; i < jobs->len; ++i) {
        struct job *job = g_ptr_array_index(jobs, i);
        for (int j = 0; j < job->n_ps; ++j) {
            struct prs *p = job->ps + j;
            if (p->pid == pid) {
                p->stat = r;
                if (WIFSTOPPED (r)) {
                    p->stopped = TRUE;
                }
                else
                    p->completed = TRUE;
                return 0;
            }
        }
    }
    return -1;
}

void jobs_fore(struct job *j, int c)
{
    int ret;
    pid_t pid;
    tcsetpgrp(tty_fd, j->pgrp);
    while (TRUE) {
        pid = waitpid(-1, &ret, WUNTRACED);
        if (!update_job_stat(pid, ret)) {
            break;
        }
        if (!job_stopped(j)) {
            break;
        }
        if (!job_completed(j)) {
            break;
        }
    }
    tcsetpgrp(tty_fd, tty_pgrp);
}

void jobs_back(struct job *j, int c)
{
    if (c)
        kill(-j->pgrp, SIGCONT);
}

void chld_handler(int sig)
{
    pid_t pid;
    int ret;
    sigset_t sset;
    sigprocmask(SIG_BLOCK, &sset, NULL);
    while (TRUE) {
        pid = waitpid(-1, &ret, WUNTRACED);
        if (pid == -1)
            break;
        if (!update_job_stat(pid, ret)) {
            break;
        }
    }
    sigprocmask(SIG_UNBLOCK, &sset, NULL);
}

int exec_pstmt(struct pstmt_node *node)
{
    struct stmt_node *sn, *snp;
    pid_t pid, pgid;
    int fd, fds[2], ret, fret;
    struct job *job;
    sigset_t sset;

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
    job = g_new(struct job, 1);
    job->n_ps = node->stmts->len;
    job->ps = g_new(struct prs, job->n_ps);
    pgid = 0;
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
        sigemptyset(&sset);
        sigaddset(&sset, SIGCHLD);
        sigprocmask(SIG_BLOCK, &sset, NULL);
        pid = fork();
        if (pid == 0) {
            /* child */
            if (pgid == 0)
                pgid = getpid();
            setpgid(0, pgid);
            if (!node->back)
                tcsetpgrp(tty_fd, pgid);
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);
            sigprocmask(SIG_UNBLOCK, &sset, NULL);
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
                exit(EXIT_FAILURE);
            }
        }
        else {
            /* parent */
            if (pgid == 0)
                pgid = pid;
            setpgid(pid, pgid);
            job->ps[i].pid = pid;
            job->ps[i].completed = FALSE;
            job->ps[i].stopped = FALSE;
            job->ps[i].argv = sn->argv;
            if (!node->back)
                tcsetpgrp(tty_fd, pgid);
            if (sn->input_fd != 0) {
                close(sn->input_fd);
            }
            if (sn->output_fd != 1) {
                close(sn->output_fd);
            }
        }
    }
    job->pgrp = pgid;
    job->fin_ps = 0;
    g_ptr_array_add(jobs, job);
    sigprocmask(SIG_UNBLOCK, &sset, NULL);
    if (node->back) {
        jobs_back(job, 0);
        return 0;
    }
    else {
        jobs_fore(job, 0);
        return 0;
    }
}

void check_back(void)
{
    int t, ret;
    pid_t pid;

    if (jobs->len == 0)
        return;

    for (int i = 0; i < jobs->len; ++i) {
        struct job *job = g_ptr_array_index(jobs, i);
        if (job->fin_ps != job->n_ps) {
            t = 0;
            for (int j = 0; j < job->n_ps - job->fin_ps; ++j) {

                if (waitpid(-job->pgrp, NULL, WNOHANG) != 0) {
                    ++t;
                }
            }
            job->fin_ps += t;
            if (job->fin_ps == job->n_ps) {
                g_print("[%d]   Done\n", i+1);
            }
        }
    }
/*    do
        pid = waitpid(-1, &ret, WNOHANG | WUNTRACED);
    while (!update_job_stat(pid, ret));*/


    gboolean f = TRUE;
    for (int i = 0; i < jobs->len; ++i) {
        struct job *job = g_ptr_array_index(jobs, i);
        f = f && (job->fin_ps == job->n_ps);
    }
    if (f)
        g_ptr_array_remove_range(jobs, 0, jobs->len);
}

int main(void)
{
    char *str = NULL;
    struct scanner *scanner;
    struct pstmt_node *n = NULL;
    using_history();

    jobs = g_ptr_array_new();
    tty_fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
    tty_pgrp = tcgetpgrp(tty_fd);
    setpgid (getpid(), getpid());
    if (tty_fd == -1)
        g_printerr("Could not get controlling terminal\n");
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGCHLD, chld_handler);

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
        check_back();
        free(str);
    }
    scanner_free(scanner);
    g_ptr_array_free(jobs, TRUE);
    return 0;
}
