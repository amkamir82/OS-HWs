#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define FALSE 0
#define TRUE 1
#define INPUT_STRING_SIZE 80

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"

int cmd_quit(tok_t arg[]) {
    printf("Bye\n");
    exit(0);
    return 1;
}

int cmd_help(tok_t arg[]);

int cmd_pwd();

int cmd_cd();

/* Command Lookup table */
typedef int cmd_fun_t(tok_t args[]); /* cmd functions take token array and return int */
typedef struct fun_desc {
    cmd_fun_t *fun;
    char *cmd;
    char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
        {cmd_help, "?",    "show this help menu"},
        {cmd_quit, "quit", "quit the command shell"},
        {cmd_pwd,  "pwd",  "show current directory"},
        {cmd_cd,   "cd",   "change directory"},
};


int cmd_pwd() {
    char current_path[INPUT_STRING_SIZE + 1];

    if (getcwd(current_path, sizeof(current_path)) != NULL) {
        getcwd(current_path, sizeof(current_path));

        printf("%s\n", current_path);
        return 1;
    }
    return 0;
}

int cmd_cd(tok_t arg[]) {
    if (arg[0] != NULL) {
        chdir(arg[0]);
        return 1;
    }
    return 0;
}

int cmd_help(tok_t arg[]) {
    int i;
    for (i = 0; i < (sizeof(cmd_table) / sizeof(fun_desc_t)); i++) {
        printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
    }
    return 1;
}

int lookup(char cmd[]) {
    int i;
    for (i = 0; i < (sizeof(cmd_table) / sizeof(fun_desc_t)); i++) {
        if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
    }
    return -1;
}


void signal_handler(__sighandler_t __handler) {
    signal(SIGINT, __handler);
    signal(SIGQUIT, __handler);
    signal(SIGTSTP, __handler);
    signal(SIGTTIN, __handler);
    signal(SIGTTOU, __handler);
}

void init_shell() {
    /* Check if we are running interactively */
    shell_terminal = STDIN_FILENO;

    /** Note that we cannot take control of the terminal if the shell
        is not interactive */
    shell_is_interactive = isatty(shell_terminal);

    if (shell_is_interactive) {

        /* force into foreground */
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        shell_pgid = getpid();
        /* Put shell in its own process group */
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        /* Take control of the terminal */
        tcsetpgrp(shell_terminal, shell_pgid);
        tcgetattr(shell_terminal, &shell_tmodes);
        signal_handler(SIG_IGN);
    }
    /** YOUR CODE HERE */
}

/**
 * Add a process to our process list
 */
void add_process(process *p) {
    /** YOUR CODE HERE */
}

/**
 * Creates a process given the inputString from stdin
 */
process *create_process(char *inputString) {
    /** YOUR CODE HERE */
    return NULL;
}


int shell(int argc, char *argv[]) {
    char *s = malloc(INPUT_STRING_SIZE + 1);            /* user input string */
    tok_t *t;            /* tokens parsed from input */
    int lineNum = 0;
    int fundex = -1;
    pid_t pid = getpid();        /* get current processes PID */
    pid_t ppid = getppid();    /* get parents PID */
    pid_t cpid, tcpid, cpgid;
    char *PATH = getenv("PATH");
    tok_t *sub_paths = getToks(PATH);

    init_shell();

    // printf("%s running as PID %d under %d\n",argv[0],pid,ppid);

    lineNum = 0;
    // fprintf(stdout, "%d: ", lineNum);
    while ((s = freadln(stdin))) {
        t = getToks(s); /* break the line into tokens */
        fundex = lookup(t[0]); /* Is first token a shell literal */
        if (fundex >= 0) cmd_table[fundex].fun(&t[1]);
        else {
            int i = 0;
            int check_ampersand = 0;
            while (t[i] != NULL) {
                if (strcmp(t[i], "&") == 0) {
                    check_ampersand = 1;
                    t[i] = NULL;
                }
                i++;
            }
            int p_id = fork();
            if (p_id > 0) {
                int a;
                setpgid(p_id, p_id);
                if (!check_ampersand) {
                    tcsetpgrp(shell_terminal, p_id);
                    waitpid(p_id, &a, WUNTRACED);
                }
                tcsetpgrp(shell_terminal, getpid());
            } else if (p_id == 0) {
                signal_handler(SIG_DFL);
                int index_of_direct_to_output = isDirectTok(t, ">");
                if (index_of_direct_to_output > 0) {
                    int output_file = open(t[index_of_direct_to_output + 1], O_WRONLY | O_CREAT | O_TRUNC);
                    dup2(output_file, STDOUT_FILENO);
                    t[index_of_direct_to_output] = t[index_of_direct_to_output + 1] = NULL;
                }

                int index_of_direct_from_input = isDirectTok(t, "<");
                if (index_of_direct_from_input > 0) {
                    int input_file = open(t[index_of_direct_from_input + 1], O_RDONLY);
                    dup2(input_file, STDIN_FILENO);
                    t[index_of_direct_from_input] = t[index_of_direct_from_input + 1] = NULL;
                }
                int exec_code;
                exec_code = execv(t[0], t);
                char *sadegh = malloc(sizeof(char) * INPUT_STRING_SIZE);
                int sub_path_index = 0;
                while (sub_paths[sub_path_index] != NULL) {
                    strcpy(sadegh, sub_paths[sub_path_index]);
//                    printf("=====%s\n", sadegh);
                    strcat(sadegh, "/");
                    strcat(sadegh, t[0]);
                    if (access(sadegh, F_OK) != -1) {
                        t[0] = sadegh;
                        exec_code = execv(t[0], t);
                    }
                    sub_path_index += 1;
                }
//                fprintf(stdout, "This shell only supports built-ins. Replace this to run programs as commands.\n");
                exit(exec_code);
            }
//            else {
//                printf("sadeghhhhh");
//            }
//            fprintf(stdout, "This shell only supports built-ins. Replace this to run programs as commands.\n");
        }
        // fprintf(stdout, "%d: ", lineNum);
    }
    return 0;

}


