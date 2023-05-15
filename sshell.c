#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CMDLINE_MAX 512
#define MAX_ARGS 16

// returns true if its a meta char or null term
int IsDelim(char c) {
    return c == '>' || c == '|' || c =='&' || c == '\0';
}

// struct for a job
struct Job {
    char cmd[CMDLINE_MAX];  // the input line
    char cmdTokens[CMDLINE_MAX];  // stores the tokens
    int numCmd;  // number of commands
    char ***commands;  // the commands array
    int outFd;  // output redirection file desciptor
    int background;  // background flag
    int *status;  // the status of the program
    pid_t *pids;  // the pids of the processes
    int *pipeFds;  // the pipe fds
};

// frees the memory allocated in a job struct
void DeleteJob(struct Job *toBeDeleted) {
    for (int i = 0; i < toBeDeleted->numCmd; i++) {
        free(toBeDeleted->commands[i]);
    }
    free(toBeDeleted->commands);
    free(toBeDeleted->status);
    free(toBeDeleted->pids);
    free(toBeDeleted->pipeFds);
    free(toBeDeleted);
}

// parse a job from the input cmd
struct Job* GenerateJob(char cmd[CMDLINE_MAX]) {
    // two loops
    // first loop is to count number of arguments and check for errors
    // Look forward into cmd, check for parsing errors and number of commands.
    int numCommands = 0;  // number of commands
    int validChar = 0;  // flag for if a valid char is seen
    int redirMetaChar = 0;  // if a > has been seen
    int cmdIterator = -1;  // iterator for cmd
    int tempLookahead;  // second iterator
    while (cmd[++cmdIterator]) {  // loop until the end of cmd
        switch (cmd[cmdIterator]) {
            case '|' :
                // if the character is a |
                // first make sure there is a valid char before
                if (!validChar) {
                    fprintf(stderr, "Error: missing command\n");
                    return NULL;
                }
                // loop forward until another delim char
                // if there is not a valid char after, then error
                validChar = 0;
                tempLookahead = cmdIterator;
                while (!IsDelim(cmd[++tempLookahead])) {
                    if (!isspace(cmd[tempLookahead])) {
                        validChar = 1;
                        break;
                    }
                }
                // if there was already a >, error
                if (redirMetaChar) {
                    fprintf(stderr, "Error: mislocated output redirection\n");
                    return NULL;
                }
                if (!validChar) {
                    fprintf(stderr, "Error: missing command\n");
                    return NULL;
                }
                // If we reach this point, the pipe has valid parsing.
                numCommands++;
                validChar = 0;
                break;
            case '>' :
                // if the character is a >
                // if there is a second one, go forward 1
                if (cmd[cmdIterator + 1] == '>') {
                    cmdIterator++;
                }
                // same as |, check previous char
                if (!validChar) {
                    fprintf(stderr, "Error: missing command\n");
                    return NULL;
                }
                // check for another valid char after it
                validChar = 0;
                tempLookahead = cmdIterator;
                while (!IsDelim(cmd[++tempLookahead])) {
                    if (!isspace(cmd[tempLookahead])) {
                        validChar = 1;
                    }
                }
                // if there isnt, no output file.
                if (!validChar) {
                    fprintf(stderr, "Error: no output file\n");
                    return NULL;
                }
                // if the next delimeter is a |, break
                if (cmd[tempLookahead] == '|') {
                    fprintf(stderr, "Error: mislocated output redirection\n");
                    return NULL;
                }
                redirMetaChar = 1;
                break;
            case '&' :
                // if the character is a &
                // make sure there is a valid char before it
                if (!validChar) {
                    fprintf(stderr, "Error: missing command\n");
                    return NULL;
                }
                // check for characters after it
                tempLookahead = cmdIterator + 1;
                while (isspace(cmd[tempLookahead])) {
                    tempLookahead++;
                }
                // if there are, we have an error
                if (cmd[tempLookahead] != '\0') {
                    fprintf(stderr, "Error: mislocated background sign\n");
                    return NULL;
                }
                break;
            default :
                if (!isspace(cmd[cmdIterator])) {
                    validChar = 1;
                }
        }  // switch end
    }  // while end
    // the final command
    if (validChar) {
        numCommands++;
    } else if (numCommands == 0) {  // this means line is empty
        return NULL;
    }

    // create the object we want to return
    struct Job *result = (struct Job*)malloc(sizeof(struct Job));
    if (!result) {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }

    // initialize parts of struct
    strcpy(result->cmd, cmd);
    strcpy(result->cmdTokens, cmd);
    char *token = strtok(result->cmdTokens, " \t|>&");

    result->outFd = STDOUT_FILENO;
    result->background = 0;

    result->numCmd = numCommands;
    result->commands = (char***)malloc(numCommands * sizeof(char**));
    if (!result->commands) {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }

    // append flag
    int append = 0;

    // reset iterator for second loop
    // this loop is to count the number of args in each cmd and load them
    cmdIterator = -1;
    for (int i = 0; i < numCommands; i++) {
        int numArgs = 0;  // number of args in this command
        int wasSpace = 1;  // using spaces to count number of args
        int skipNext = 0;  // skip next for file redirect
        int redirectFileIndex = -1;  // if there is a redirect, save index
        // loop until pipe
        while (cmd[++cmdIterator] && cmd[cmdIterator] != '|') {
            // if theres an &, we can set the & flag
            if (cmd[cmdIterator] == '&') {
                result->background = 1;
                break;
            }
            // if we come accross an >, then we dont count it as an arg
            // dont increment and skip the next token
            if (cmd[cmdIterator] == '>') {
                if (cmd[cmdIterator + 1] == '>') {
                    append = 1;
                    cmdIterator++;
                }
                skipNext = 1;
                redirectFileIndex = numArgs;
            } else if (!isspace(cmd[cmdIterator])) {
                if (skipNext) {
                    skipNext = 0;
                } else if (wasSpace) {
                    numArgs++;  // if its not a space, increase args count
                }
                wasSpace = 0;
            } else if (isspace(cmd[cmdIterator])) {
                wasSpace = 1;
            }
        }

        // check number of args
        if (numArgs > MAX_ARGS) {
            fprintf(stderr, "Error: too many process arguments\n");
            for (int j = 0; j < i; j++) {
                free(result->commands[j]);
            }
            free(result->commands);
            free(result);
            return NULL;
        }

        // create array for command
        result->commands[i] = (char**)malloc((numArgs + 1) * sizeof(char*));
        if (!result->commands[i]) {
            fprintf(stderr, "malloc error\n");
            exit(1);
        }
        result->commands[i][numArgs] = NULL;  // must end in NULL

        for (int j = 0; j <= numArgs; j++) {
            // if there this is the savd file name, open the file descriptor
            if (redirectFileIndex == j) {
                if (append) {
                    result->outFd = open(token,
                                         O_WRONLY | O_CREAT | O_APPEND, 0644);
                } else {
                    result->outFd = open(token,
                                         O_WRONLY | O_CREAT | O_TRUNC, 0644);
                }

                // if we have an error, we need to free it
                if (result->outFd == -1) {
                    fprintf(stderr, "Error: cannot open output file\n");
                    for (int k = 0; k <= i; k++) {
                        free(result->commands[k]);
                    }
                    free(result->commands);
                    free(result);
                    return NULL;
                }

                redirectFileIndex = -1;
                j--;
                token = strtok(NULL, " \t|>&");
            } else if (j < numArgs) {
                result->commands[i][j] = token;
                token = strtok(NULL, " \t|>&");
            }
        }
    }

    result->status = (int*)malloc(numCommands * sizeof(int));
    result->pids = (pid_t*)malloc(numCommands * sizeof(pid_t));
    result->pipeFds = (int*)malloc((numCommands - 1) * 2 * sizeof(int));
    if (!result->status || !result->pids || !result->pipeFds) {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }

    return result;
}

// this function prints the complete message for a job
void PrintCompleteMessage(struct Job *finishedJob) {
    fprintf(stderr, "+ completed '%s' ", finishedJob->cmd);
    for (int i = 0; i < finishedJob->numCmd; i++) {
        fprintf(stderr, "[%d]", finishedJob->status[i]);
    }
    fprintf(stderr, "\n");
}

// node for the background jobs list
struct BackgroundNode {
    struct Job *jobData;
    struct BackgroundNode *next;
};

// execute the job
void Execute(struct Job *currentJob, struct BackgroundNode **tail) {
    if (!currentJob || !tail) {
        return;
    }

    pid_t origin = getpid();

    int designation = -1;  // parent is -1, children will be 0, 1, 2...
    // create pipes for job (if there are 0 pipes, 0 are created)
    for (int i = 0; i < currentJob->numCmd - 1; i++) {
        if (pipe(currentJob->pipeFds + (i * 2))) {
            fprintf(stderr, "pipe error\n");
            exit(1);
        }
    }

    // loop to create child processes
    for (int i = 0; i < currentJob->numCmd; i++) {
        if (getpid() == origin) {  // make sure its the parent
            pid_t pid = fork();
            if (pid < 0) {  // fork error
                fprintf(stderr, "fork error\n");
                exit(1);
            } else if (!pid) {  // child
                if (i != 0) {  // assign the pipes
                    dup2(currentJob->pipeFds[(i - 1) * 2], STDIN_FILENO);
                }
                if (i != currentJob->numCmd - 1) {
                    dup2(currentJob->pipeFds[i * 2 + 1], STDOUT_FILENO);
                } else if (currentJob->outFd != STDOUT_FILENO) {
                    dup2(currentJob->outFd, STDOUT_FILENO);
                }
                designation = i;
                break;
            } else {  // if its the parent, save the pid
                currentJob->pids[i] = pid;
            }
        }
    }

    // close the file descriptors
    for (int i = 0; i < currentJob->numCmd - 1; i++) {
        close(currentJob->pipeFds[i * 2]);
        close(currentJob->pipeFds[i * 2 + 1]);
        if (currentJob->outFd != STDOUT_FILENO) {
            close(currentJob->outFd);
        }
    }

    // if its not the parent, execute the command
    if (designation != -1) {
        execvp(currentJob->commands[designation][0],
               currentJob->commands[designation]);
        // if we are here we have an error
        fprintf(stderr, "Error: command not found\n");
        exit(1);
    }

    // if its a background job
    if (currentJob->background) {
        struct BackgroundNode *bgNode;
        bgNode = (struct BackgroundNode*)malloc(sizeof(struct BackgroundNode));
        if (!bgNode) {
            fprintf(stderr, "malloc error\n");
            exit(1);
        }
        bgNode->jobData = currentJob;
        bgNode->next = NULL;
        (*tail)->next = bgNode;
        *tail = bgNode;
    } else {
        // if its not a background, wait for it to finish
        for (int i = 0; i < currentJob->numCmd; i++) {
            waitpid(currentJob->pids[i], currentJob->status + i, 0);
            currentJob->status[i] = WEXITSTATUS(currentJob->status[i]);
        }
    }
}

void CheckBgJobs(struct BackgroundNode *bgList, struct BackgroundNode **tail) {
    struct BackgroundNode *prev = bgList;
    struct BackgroundNode *iter = bgList->next;

    while (iter) {
        int complete = 1;
        struct Job *bgJob = iter->jobData;
        for (int i = 0; i < bgJob->numCmd; i++) {
            if (iter->jobData->pids[i] == getpid()) {
                continue;
            }
            if (!waitpid(bgJob->pids[i], bgJob->status + i, WNOHANG)) {
                complete = 0;
                break;
            } else {
                bgJob->pids[i] = getpid();
                bgJob->status[i] = WEXITSTATUS(bgJob->status[i]);
            }
        }
        if (complete) {
            PrintCompleteMessage(bgJob);
            DeleteJob(bgJob);
            if (iter == *tail) {
                *tail = prev;
            }
            prev->next = iter->next;
            free(iter);
            iter = prev->next;
        } else {
            iter = iter->next;
            prev = prev->next;
        }
    }
}


int main(void) {
    char cmd[CMDLINE_MAX];

    // create list for the background jobs
    struct BackgroundNode *bgList;
    bgList = (struct BackgroundNode*)malloc(sizeof(struct BackgroundNode));
    if (!bgList) {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }
    bgList->next = NULL;
    struct BackgroundNode *tail = bgList;


    int on = 1;
    while (on) {
        char *nl;

        /* Print prompt */
        printf("sshell@ucd$ ");
        fflush(stdout);

        /* Get command line */
        if (!fgets(cmd, CMDLINE_MAX, stdin)) {
            exit(1);
        }

        /* Print command line if stdin is not provided by terminal */
        if (!isatty(STDIN_FILENO)) {
            printf("%s", cmd);
            fflush(stdout);
        }

        /* Remove trailing newline from command line */
        nl = strchr(cmd, '\n');
        if (nl) {
            *nl = '\0';
        }

        int run = 1;
        struct Job *currentJob = GenerateJob(cmd);
        if (!currentJob) {
            run = 0;
        }

        if (run) {
            /* Builtin command */
            if (!strcmp(currentJob->commands[0][0], "exit")
                && currentJob->numCmd == 1) {
                if (!bgList->next) {  // if no active jobs
                    on = 0;
                    fprintf(stderr, "Bye...\n");
                    currentJob->status[0] = 0;
                } else {
                    fprintf(stderr, "Error: active jobs still running\n");
                    currentJob->status[0] = 1;
                }
            } else if (!strcmp(currentJob->commands[0][0], "pwd")
                       && currentJob->numCmd == 1) {
                // for pwd, keep making it larger if it doesnt fit
                int buffSize = CMDLINE_MAX;
                char *wd = (char*)malloc(buffSize * sizeof(char));
                if (!wd) {
                    fprintf(stderr, "malloc broke\n");
                    exit(1);
                }
                // try to get successful call
                while (!getcwd(wd, buffSize)) {
                    free(wd);
                    buffSize *= 2;
                    wd = (char*)malloc(buffSize * sizeof(char));
                    if (!wd) {
                        fprintf(stderr, "malloc broke\n");
                        exit(1);
                    }
                }
                printf("%s\n", wd);
                fflush(stdout);
                free(wd);
                currentJob->status[0] = 0;
            } else if (!strcmp(currentJob->commands[0][0], "cd")
                       && currentJob->numCmd == 1) {
                // change wd, just call the cmd
                if (chdir(currentJob->commands[0][1])) {
                    fprintf(stderr, "Error: cannot cd into directory\n");
                    currentJob->status[0] = 1;
                } else {
                    currentJob->status[0] = 0;
                }
            } else {
                /* Real Commands */
                Execute(currentJob, &tail);
            }
        }

        // check the background tasks
        CheckBgJobs(bgList, &tail);

        if (run && !currentJob->background) {
            PrintCompleteMessage(currentJob);
            DeleteJob(currentJob);
        }
    }

    free(bgList);

    return EXIT_SUCCESS;
}
