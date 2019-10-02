#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <sys/mount.h>

#define DEFAULT_STACK_SIZE 8000000

typedef struct args {
    char **binds;
    char **envs;
    char *root;
    char *command;
    char **args;
    uint32_t bind_count;
    uint32_t env_count;
    uint32_t arg_count;
} ARGS_t;

void free_all(char **binds, uint32_t bind_count) {
    uint32_t i;

    for(i = 0; i < bind_count; i++) {
        free(binds[i]);
    }

    free(binds);
}

int child(void *args) {
    ARGS_t *v;
    char *envs[2], *cargs;
    
    v = (ARGS_t*)args;
    cargs = *v->args;
    envs[0] = (v->envs) ? *v->envs : NULL;
    envs[1] = NULL;

    chdir(v->root);
    if (chroot(v->root) != 0) {
		return -1;
	}

    if(execle(v->command, cargs, NULL, envs) == -1) {
        fprintf(stderr, "%s: %s\n", "execle() error", strerror(errno));
        return -1;
    }
    
    return 0;
}

int main(int argc, char **argv) {
    ARGS_t args;
    void *child_stack;

    pid_t pid;

    args.binds = NULL;
    args.envs = NULL;
    args.bind_count = 0;
    args.env_count = 0;
    child_stack = NULL;

    {
        int32_t c;
        while((c = getopt(argc, argv, "b:e:s:")) != -1) {
            switch(c) {
                case 'b':
                    {
                        uint32_t size;

                        size = strlen(optarg);
                        args.bind_count++;
                        args.binds = realloc(args.binds, sizeof(char*) * args.bind_count);
                        args.binds[args.bind_count-1] = malloc(sizeof(char) * size);
                        memcpy(args.binds[args.bind_count-1], optarg, size);
                    }
                    break;
                case 'e':
                    {
                        uint32_t size;

                        size = strlen(optarg);
                        args.env_count++;
                        args.envs = realloc(args.envs, sizeof(char*) * args.env_count);    
                        args.envs[args.env_count-1] = malloc(sizeof(char) * size);
                        memcpy(args.envs[args.env_count-1], optarg, size);
                    }
                    break;
                case 's':
                    child_stack = malloc(optopt);
                    memset(child_stack, 0, optopt);
                    break;
                default:
                    fprintf(stderr, "%s\n", "opt hit default [??]");
                    goto err;
            }
        }
    }

    if(!child_stack) {
        child_stack = malloc(DEFAULT_STACK_SIZE);
    }

    args.root = argv[optind];
    args.command = argv[optind+1];
    args.args = &argv[optind+2];

    if((pid = clone(child, (uint8_t*)child_stack + DEFAULT_STACK_SIZE, SIGCHLD|CLONE_NEWUTS|CLONE_NEWPID|CLONE_NEWIPC|CLONE_NEWNS, &args)) == -1) {
        fprintf(stderr, "%s: %s\n", "Clone error", strerror(errno));
        goto err;
    } 

    waitpid(pid, NULL, 0);

    free_all(args.binds, args.bind_count);
    free_all(args.envs, args.env_count);
    free(child_stack);

    return EXIT_SUCCESS;

err:
    free_all(args.binds, args.bind_count);
    free_all(args.envs, args.env_count);
    free(child_stack);

    return EXIT_FAILURE;
}