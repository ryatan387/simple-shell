# SSHELL: Simple Shell

## Summary

This program,  `sshell`, functions as a simple shell providing the following
set of core features: execution of user-supplied commands with optional
arguments, builtin commands(exit, cd , and pwd), redirection of the standard
output of commands to files, and composition of commands via piping. In
addition, it also supports output redirection appending, as well as background
jobs. 

## Implementation

The implementation of this program follows three main steps:
1. Parsing user input to identify errors and what needs to be run
2. Executing the requested command
3. Managing background jobs

### Parsing 

After receiving user input, `sshell` parses the command line using the function
`GenerateJob()` The purpose of this function is to convert all the information
in the command line into a struct that contains all the necessary information
to run and print a command.

Some of the design choices are using a dynamically sized array (`commands`) of
a dynamically sized array to simulate a job of multiple commands. `numCmd` then
holds the number of commands. While a maximum number of commands and arguments
are specified, robustness does not hurt. Since the input of the function
`execvp` is a null terminated array, an array must be used for a command.

In order to do this, the command line is iterated over multiple times. The
first time will count the number of commands and make sure that the format of
the line is correct. The second loop will count the number of arguments in
each command, and generate the array used in `execvp()`. ‘cmdTokens’ will
contain each command. This is essentially where the tokens that ‘commands’
points to are stored in memory.

The other parts of the struct are ‘cmd’, the inputted line, `outFd` which holds
the output file descriptor (could be STDOUT_FILENO or otherwise parsed from
cmd), ‘background’ that is set to `1` if the task is background run, then
`status`, `pids`, and `pipeFds` which are dynamically allocated arrays that
holds the respective data for their command. These are initialized in the
function `GenerateJob()` to their default values.

### Execution

There are two different types of executions: built in commands and user
commands. Both are using the struct created from `GenerateJob()`, named
`currentJob`. This is done like this to simplify the completion message.

The type of command is extracted from the name of the first command from
`currentJob`. The built-in command `exit` will set the main loop condition to
false if there are no background jobs, while the other two call their
respective functions. 

For user commands, the function `Execute()` is called which takes in the struct
`currentJob`. This function first creates the pipes that will connect each
child to each other, then forks inside a for loop to create the child
processes. This is done so when the fork happens, each child will have a number
assigned to it and have the pipe file descriptors available to it. The number
assigned will let it know which pipes to connect to and which command to run.
Thus the children run concurrently and are all connected together.

The parent will remember the process id, and wait for all the child processes
to finish. The ‘PrintCompletedJob()’ function is then called on ‘currentJob’,
which prints the complete message.

### Background Jobs

The processes that are background jobs must have their data remembered for a
further iteration. The `Job` struct stored the data already, so a linked list
is used that will contain a `Job` struct. A pointer `bgList` will point to the
head of the list, and a `tail` pointer points to the end. A linked list is
chosen as we must iterate through the entire thing every time, and the
existence of a tail pointer means adding a new element can be done in constant
time. 

Background jobs will depend on the `background` flag in the `currentJob`
struct, and if it is `1`, then `currentJob` will be inserted into the linked
list. 

After the execution of each command, the linked list will be iterated through
starting from `bgList` and check if all the child processes have exited, and if
so call `PrintCompletedJob()`.

### Memory management

There is a function `DeleteJob` that frees all the dynamically allocated arrays
in a `Job` struct. This is called at the end of every main loop, to ensure no
memory leaks.

## Testing

Testing of this program included running the provided test script, and our own
tests. Basic commands from the assignment page were run, and then edge cases
surrounding parsing were tested.

## References

The man7.org page was used to learn about the specifics of new commands, like
`pipe()`, `dup2`, `fork()`, `execvp()`, `strtok()`, and `open()`
