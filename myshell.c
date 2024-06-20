#include <stdio.h>
#include <string.h>
#include <stdlib.h>			// exit()
#include <unistd.h>			// fork(), getpid(), exec()
#include <sys/wait.h>		        // wait()
#include <signal.h>			// signal()
#include <fcntl.h>			// close(), open()
#include <ctype.h>

#define MAX_COMMANDS 10  // Define the maximum number of commands in a pipeline

// Function prototypes
int parseInput(char *input_cmd);
void printShellPrompt();
char* readInput();
void executeCommand(char *input_command);
void executeParallelCommands(char *input_command);
void executeSequentialCommands(char *input_command);
void executeCommandRedirection(char *input_command);
void executePipelineCommands(char *input_command);

// Function to parse a single command and its arguments
char** parseSingleCmd(char *cmd) {
    // This function will parse the input string into multiple commands or a single command with arguments depending on the delimiter (&&, ##, >, or spaces).
    char *delimiter = " \n";
    
    // Remove leading and trailing white spaces and newline characters
    cmd = strtok(cmd, delimiter);
    if (cmd == NULL) {
        return NULL;  // No command to parse
    }

    // Check for built-in command 'cd' and handle it
    if (strcmp(cmd, "cd") == 0) {
        char *location = strtok(NULL, delimiter);
        if (location != NULL) {
            chdir(location);
        }
        return NULL;
    }

    size_t buff = 100;
    char **arguments = (char**)malloc((buff + 1) * sizeof(char*));
    if (arguments == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    size_t iterator = 0;
    while (cmd != NULL) {
        arguments[iterator] = strdup(cmd);
        if (arguments[iterator] == NULL) {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
        iterator++;

        // Resize the argument array if necessary
        if (iterator >= buff) {
            buff += 100;
            arguments = (char**)realloc(arguments, (buff + 1) * sizeof(char*));
            if (arguments == NULL) {
                perror("Memory allocation failed");
                exit(EXIT_FAILURE);
            }
        }

        cmd = strtok(NULL, delimiter);
    }

    arguments[iterator] = NULL;
    return arguments;
}

// Function to parse input and determine the type of command
int parseInput(char *input_cmd) {
    // Remove trailing newline character, if present
    size_t len = strlen(input_cmd);
    if (len > 0 && input_cmd[len - 1] == '\n') {
        input_cmd[len - 1] = '\0';
    }

    if (strcmp(input_cmd, "exit") == 0) {
        return 0; // Exit command
    }

    if (strstr(input_cmd, "&&") != NULL) {
        return 2; // Parallel execution of multiple commands
    } else if (strstr(input_cmd, "##") != NULL) {
        return 3; // Sequential execution of multiple commands
    } else if (strstr(input_cmd, ">") != NULL) {
        return 4; // Command redirection
    } else if (strstr(input_cmd, "|") != NULL) {
        return 5; // Pipeline command
    } else {
        return 1; // Single command execution
    }
}

// Function to execute a single command
void executeCommand(char *cmd) {
    // This function will fork a new process to execute a command
    char** cmd_args = parseSingleCmd(cmd);
    int rc = fork();

    if (rc < 0) {
        exit(0);
    } else if (rc == 0) {
        // Enable signals for child processes
        signal(SIGTSTP, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        
        int ret_val = execvp(cmd_args[0], cmd_args);

        if (ret_val < 0) {
            printf("Shell: Incorrect command\n");
            exit(1);
        }
    } else {
        int wait_rc = wait(NULL);
    }
}

// Function to execute multiple commands in parallel
void executeParallelCommands(char *cmd) {
    // This function will run multiple commands in parallel
    char *cmd_separator = "&&";
    char *saveptr;

    // Tokenize the input command using "&&" as the delimiter
    char *token = strtok_r(cmd, cmd_separator, &saveptr);

    while (token != NULL) {
        // Remove leading and trailing spaces
        while (*token == ' ') {
            token++;
        }
        size_t len = strlen(token);
        while (len > 0 && token[len - 1] == ' ') {
            token[len - 1] = '\0';
            len--;
        }

        // Skip empty tokens
        if (len > 0) {
            int rc = fork();

            if (rc < 0) {
                perror("Fork failed");
                exit(EXIT_FAILURE);
            } else if (rc == 0) {
                // Child process

                // Enable signals for child processes
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);

                char **cmd_args = parseSingleCmd(token);

                if (cmd_args == NULL) {
                    exit(0);
                }

                if (execvp(cmd_args[0], cmd_args) < 0) {
                    perror("Shell: Execution failed");
                    exit(EXIT_FAILURE);
                }
            }
        }

        // Get the next command token
        token = strtok_r(NULL, cmd_separator, &saveptr);
    }

    // Wait for all child processes to complete
    int status;
    while (wait(&status) > 0);
}

// Function to execute multiple commands sequentially
void executeSequentialCommands(char *cmd) {
    // This function will run multiple commands in parallel
    char *delimiter = "##";
    char *saveptr;

    // Tokenize the input command using "##" as the delimiter
    char *token = strtok_r(cmd, delimiter, &saveptr);

    while (token != NULL) {
        // Remove leading and trailing spaces
        while (*token == ' ') {
            token++;
        }
        size_t len = strlen(token);
        while (len > 0 && token[len - 1] == ' ') {
            token[len - 1] = '\0';
            len--;
        }

        // Skip empty tokens
        if (len > 0) {
            char *copy_cmd = strdup(token);
            executeCommand(copy_cmd);
            free(copy_cmd);
        }

        // Get the next command token
        token = strtok_r(NULL, delimiter, &saveptr);
    }
}

// Function to execute a single command with output redirection
void executeCommandRedirection(char *cmd) {
    // This function will run a single command with output redirected to an output file specified by the user
    char *command = strsep(&cmd, ">"); // Split the input into command and filename
    char *filename = cmd;

    // Remove leading and trailing white spaces from the filename
    while (*filename == ' ') {
        filename++;
    }
    size_t len = strlen(filename);
    while (len > 0 && filename[len - 1] == ' ') {
        filename[len - 1] = '\0';
        len--;
    }

    int rc = fork();

    if (rc < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    } else if (rc == 0) {
        // Child process

        // Close stdout and open the file for writing
        int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fd == -1) {
            perror("File open failed");
            exit(EXIT_FAILURE);
        }

        // Redirect stdout to the file descriptor
        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("Dup2 failed");
            exit(EXIT_FAILURE);
        }
        close(fd);

        char **arguments = parseSingleCmd(command);

        // Execute the command
        if (execvp(arguments[0], arguments) < 0) {
            perror("Shell: Execution failed");
            exit(EXIT_FAILURE);
        }
    } else {
        // Parent process
        int status;
        if (wait(&status) == -1) {
            perror("Wait failed");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status != 0) {
                fprintf(stderr, "Shell: Command exited with status %d\n", exit_status);
            }
        } else if (WIFSIGNALED(status)) {
            int term_signal = WTERMSIG(status);
            fprintf(stderr, "Shell: Command terminated by signal %d\n", term_signal);
        }
    }
}

// Function to execute commands in a pipeline
void executePipelineCommands(char *cmd) {
    char *token;
    char *commands[MAX_COMMANDS];  // Assuming a maximum number of commands
    int num_commands = 0;

    // Tokenize the pipeline into individual commands
    token = strtok(cmd, "|");
    while (token != NULL && num_commands < MAX_COMMANDS) {
        commands[num_commands] = token;
        num_commands++;
        token = strtok(NULL, "|");
    }

    int prev_pipe_read = -1; // File descriptor of the previous pipe's read end

    for (int i = 0; i < num_commands; i++) {
        int pipe_fds[2];

        // Create a pipe for communication (except for the last command)
        if (i < num_commands - 1) {
            if (pipe(pipe_fds) == -1) {
                perror("Pipe creation failed");
                exit(EXIT_FAILURE);
            }
        }

        int rc = fork();

        if (rc == -1) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        } else if (rc == 0) {
            // Child process

            // Close the read end of the previous pipe (not needed by the first command)
            if (prev_pipe_read != -1) {
                close(prev_pipe_read);
            }

            // Redirect stdin from the previous pipe's read end (if not the first command)
            if (prev_pipe_read != -1) {
                dup2(prev_pipe_read, STDIN_FILENO);
            }

            // Close the write end of the current pipe (not needed by the last command)
            if (i < num_commands - 1) {
                close(pipe_fds[1]);
            }

            // Redirect stdout to the current pipe's write end (if not the last command)
            if (i < num_commands - 1) {
                dup2(pipe_fds[1], STDOUT_FILENO);
            }

            char **cmd_args = parseSingleCmd(commands[i]);

            if (execvp(cmd_args[0], cmd_args) < 0) {
                perror("Command execution failed");
                exit(EXIT_FAILURE);
            }
        } else {
            // Parent process

            // Close the write end of the previous pipe's write end (not needed by the parent)
            if (prev_pipe_read != -1) {
                close(prev_pipe_read);
            }

            // Close the write end of the current pipe (not needed by the parent)
            if (i < num_commands - 1) {
                close(pipe_fds[1]);
            }

            prev_pipe_read = pipe_fds[0];  // Store the read end of the current pipe for the next iteration

            // Wait for the child process to complete
            int status;
            if (wait(&status) == -1) {
                perror("Wait failed");
                exit(EXIT_FAILURE);
            }
        }
    }
}

// Function to print the shell prompt
void printShellPrompt() {
    char working_directory[200];// Print the prompt in format - currentWorkingDirectory$
    if (getcwd(working_directory, 200) != NULL) {//To get working directory
        printf("%s$", working_directory);
    } else {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
}

// Function to read user input
char* readInput() {
    char* input_command = NULL;// accept input with 'getline()'
    size_t buffer_size = 0;
    ssize_t characters = getline(&input_command, &buffer_size, stdin);//Accept the input with getline

    if (characters == -1) {
        perror("getline");
        exit(EXIT_FAILURE);
    }

    return input_command;
}

int main() {
    signal(SIGINT, SIG_IGN);// Ignore SIGINT signal
    signal(SIGTSTP, SIG_IGN);//disable signals so shell will only terminate by exit command

    char working_directory[200];
    size_t buffer_size = 300;//size_t is an unsigned integral data type, used to declare sizes of host files in c
    char *input_command;

    while (1) {// This loop will keep your shell running until user exits.
        printShellPrompt();

        input_command = readInput();

        int ret_val = parseInput(input_command);// Parse input with 'strsep()' for different symbols (&&, ##, >) and for spaces.

        switch (ret_val) {
            case 0:
                printf("Exiting shell...\n");				// When user uses exit command.
                free(input_command);
                exit(EXIT_SUCCESS);
            case 2:
                executeParallelCommands(input_command);			// This function is invoked when user wants to run multiple commands in parallel (commands separated by &&)
                break;
            case 3:
                executeSequentialCommands(input_command);		// This function is invoked when user wants to run multiple commands sequentially (commands separated by ##)
                break;
            case 4:
                executeCommandRedirection(input_command);		// This function is invoked when user wants redirect output of a single command to and output file specificed by user
                break;
            case 5:
                executePipelineCommands(input_command);			// Handle pipeline command
                break;
            default:
                executeCommand(input_command);				// This function is invoked when user wants to run a single commands
                break;
        }

        free(input_command);
    }

    return 0;
}

