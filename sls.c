#include <pwd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifndef ENV_PATH
#define ENV_PATH "/bin:/sbin:/usr/bin:/usr/sbin"
#endif

static int exec_file(char **argv)
{
    pid_t pid;
    int ret;

    pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        execvp(argv[0], argv);
        perror("execvp");
        exit(errno == ENOENT ? 127 : 126);
    }
    else {
        waitpid(pid, &ret, 0);
        return WEXITSTATUS(ret);
    }
}

static int edit_file(char **argv)
{
    struct stat st;
    char *editor;
    int i;

    // TODO copy file to /tmp and edit as regular user

    editor = getenv("EDITOR");
    argv[0] = editor ? editor : "vi";

    for (i = 1; argv[i]; i++) {
        if (fstatat(AT_FDCWD, argv[i], &st, AT_SYMLINK_NOFOLLOW) != 0) {
            continue;
        }
        else if (!S_ISREG(st.st_mode)) {
            fprintf(stderr, "%s: File is not regular\n", argv[i]);
            return 1;
        }
    }

    return exec_file(argv);
}

static int run_shell(char *shell)
{
    char *argv[2];

    argv[0] = shell;
    argv[1] = NULL;

    return exec_file(argv);
}

static void print_usage(const char *name)
{
    // TODO need to do this gracefully (we already do ?)
    const char *help[] = {
        "-p do not reset environment",
        "-u execute command as user",
        "-s run interactive shell",
        "-c execute command",
        "-e edit file",
        NULL
    };

    int i;

    fprintf(stderr, "Usage: %s [-p] [-u user] -s\n", name);
    fprintf(stderr, "Usage: %s [-p] [-u user] -e file [file ...]\n", name);
    fprintf(stderr, "Usage: %s [-p] [-u user] -c command [args ...]\n", name);

    fprintf(stderr, "\nOptions:\n");

    for (i = 0; help[i]; i++) {
        fprintf(stderr, "  %s\n", help[i]);
    }
}

int main(int argc, char **argv)
{
    int pflag = 0, eflag = 0, cflag = 0, sflag = 0;
    char *shell, *term, *user = NULL;
    extern char **environ;
    struct passwd *pw;
    int opt;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    while ((opt = getopt(argc, argv, ":u:hpecs")) != -1) {
        switch (opt) {
        case 'p':
            pflag = 1;
            break;
        case 'e':
            eflag = 1;
            break;
        case 'c':
            cflag = 1;
            break;
        case 's':
            sflag = 1;
            break;
        case 'u':
            user = optarg;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        case '?':
            fprintf(stderr, "%s: Unrecognized option: %c\n\n", argv[0], optopt);
            print_usage(argv[0]);
            return 1;
        case ':':
            fprintf(stderr, "%s: Option requires an argument: %c\n\n", argv[0], optopt);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (getuid() != 0 && getgid() != getegid()) {
        fprintf(stderr, "%s: Permission denied\n", argv[0]);
        return 1;
    }

    pw = getpwnam(user ? user : "root");

    if (!pw) {
        perror("getpwnum");
        return 1;
    }

    if (setgid(pw->pw_gid) == -1) {
        perror("setgid");
        return 1;
    }

    if (setuid(pw->pw_uid) == -1) {
        perror("setuid");
        return 1;
    }

    if (!pflag && !eflag) {
        shell = pw->pw_shell ? pw->pw_shell : "/bin/sh";
        term = getenv("TERM");
        environ = NULL;

        if (term) {
            setenv("TERM", term, 0);
        }

        setenv("LOGNAME", pw->pw_name, 0);
        setenv("USER", pw->pw_name, 0);
        setenv("HOME", pw->pw_dir, 0);
        setenv("PATH", ENV_PATH, 0);
        setenv("SHELL", shell, 0);
    }

    if (eflag) {
        return edit_file(argv + optind - 1);
    }

    if (sflag) {
        return run_shell(getenv("SHELL"));
    }

    if (cflag) {
        return exec_file(argv + optind);
    }

    print_usage(argv[0]);
    return 1;
}
