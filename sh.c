#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

#define INPUT_BUF_SIZE 1024

ssize_t prompt(char input_buf[]);
int count_arguments(char input_buf[]);
int construct_argv(int arg_len, char input_buf[], char *argv[], char *files[]);
void set_first_arg(char *argv[]);
int exec_built_in_commands(char *argv[], char file_name[]);
void handle_child_process(char *argv[], char file_name[], char *files[], int is_append);

/**
 * This is a C implementation of a shell: see README for broad program structure and see helper method headers for detailed explanations
 * of their purpose, parameters, and return values
 */
int main() {

    char input_buf[INPUT_BUF_SIZE]; // This is the character array where we'll store the user input

    ssize_t input = prompt(input_buf); // We call the helper method prompt to print the prompt and read user input

    // This is the big while loop that represents our repl - only continues if given input by the user
    while (input) {

        // We call count_arguments to get the number of arguments that will be in the array of arguments passed
        // to execv and then store this number as arg_len
        int arg_len = count_arguments(input_buf);

        char *files[2]; // Array to keep track of any input and output redirection files
        char *argv[arg_len+1]; // Arguments array to be passed to execv (of size arg_len + 1 because null-terminated)

        // We call construct_argv to fill in argv with the arguments - it returns an int that tells us
        // whether the redirection character >> was encountered in the process. During this call, we also
        // store whether redirection is specified - with an input file stored in files[0] and an output
        // file stored in files[1].
        int is_append = construct_argv(arg_len, input_buf, argv, files);

        // Only if the input from the user is not just whitespace
        if(argv[0] != '\0') {

            // If is_append is less than 0, construct_argv encountered an error and so we continue
            if (is_append < 0) {
                continue;
            }

            // Below, we just copy the first argument to a new null-terminated character array file_name
            size_t name_len = strlen(argv[0]);
            char file_name[name_len+1];
            strncpy(file_name, argv[0], name_len);
            file_name[name_len] = '\0';

            // We call the helper method set_first_arg to ensure the first argument of the arguments array
            // is just the binary name of the program to be called in execv
            set_first_arg(argv);

            // We handle the built-in command exit outside exec_built_in_commands because we want to return
            // from the main function if exit is inputted
            if (strncmp(file_name, "exit", name_len) == 0) {
                return 0;
            } else if (exec_built_in_commands(argv, file_name) == -1) {
                // We only want to start a new process if none of the built-in commands were executed which
                // we know occurs if exec_built_in_commands returns -1
                handle_child_process(argv, file_name, files, is_append);
            }
            wait(0); // We have to wait for the executed command to finish before prompting and reading input again
        }
        // We reset all the space we used for the current iteration of the repl
        memset(input_buf, 0, INPUT_BUF_SIZE);
        memset(files, 0, 2);
        memset(argv, 0, INPUT_BUF_SIZE);
        // We have to prompt again
        input = prompt(input_buf);
    }

    return 0;
}

/**
 * Purpose: This is a helper method that prints out the prompt "33sh >" only if
 * we're running the prompt version of 33sh (as opposed to 33noprompt). Then it
 * reads the input from the user storing the input into input_buf and returning
 * the input size.
 *
 * Parameters: character array input_buf[]
 * Return value: ssize_t input (size of user input)
 */
ssize_t prompt(char input_buf[]) {
    #ifdef PROMPT
    // Only print prompt if compiled with the flag -DPROMPT
    int print = printf("33sh> ");
    fflush(stdout);
    if (print < 0) {
        // Here, we handle the case that an error occured in the sys call printf
        perror("print");
    }
    #endif
    ssize_t input = read(0, input_buf, INPUT_BUF_SIZE);
    if (input < 0) {
        // Here, we handle the case that an error occured in the sys call read
        perror("read");
    }
    return input;
}

/**
 * Purpose: This is a helper method that counts the number of arguments that will
 * be needed for the argv array that we later pass into the execv function. To do
 * this, it iterates through the user input and whenver a switch occurs between
 * whitespace and token characters it increments the count. Since we know that
 * if a redirection symbol occurs, we'll be taking out two to the "tokens", the
 * count must decrement by two if a redirection character is found. After iterating
 * through the input, the count is returned.
 *
 * Parameters: character array input_buf[]
 * Return value: the number of arguments needed in the argv array (int)
 */
int count_arguments(char input_buf[]) {
    // The beginning length is 1 to account for the first token that we don't check.
    int len = 1;
    size_t str_len = strlen(input_buf);
    // In this first condition, we check that the first character is either a space,
    // a tab, or a new line character, in which case we don't account for the beginning
    // token and the beginning length should be 0.
    if (strchr(" \n\t", input_buf[0]) != NULL) {
        len = 0;
    } else if ((input_buf[0] == '<') | (input_buf[0] == '>')) {
        len = -1;
    }
    for (size_t i = 1; i  < str_len; i++) {
        // This is the condition that checks for the a switch between whitespace and token characters
        if (strchr(" \n\t", input_buf[i]) == NULL && strchr(" \n\t", input_buf[i-1]) != NULL){
            len++;
        }
        // We decrement by 2 if we hit a redirection character
        if (input_buf[i] == '<') {
            len -= 2;
        } else if (input_buf[i] == '>') {
            if (input_buf[i-1] != '>') {
                len -= 2;
            }
        }
    }
    return len;
}

/**
 * Purpose: This is a helper method that constructs the char ** array of arguments
 * argv to be passed into execv. To do this, we need the instantiated array (declared
 * in main to be size of the arguments length counted in count_arguments + 1 - to
 * account for the null character at the end), we need the user input in input_buf,
 * we need arg_len that tells us the size of the array, and we need the char ** array
 * files where we will store any input or output redirection files. The method uses
 * strtok with the delimiter " \n\t" (since we want to tokenize on any of those chars)
 * to break up the input and store the tokens in argv as arguments. However, at each
 * iteration, we want to check for the redirection characters and for the files
 * afterwards, because these shouldn't be arguments in the argv array. It uses ints
 * is_input and is_output to catch those succeeding redirection files. After tokenizing
 * the n and null-terminating the array, it return the int append that just keeps track
 * of whether we observed a >> redirection character.
 *
 * Parameters: int arg_len, character array input_buf[], char *argv[] (the actual argument array), char *files[]
 * Return value: int append (which details whether the >> character appeared in the user input)
 */
int construct_argv(int arg_len, char input_buf[], char *argv[], char *files[]) {
    int count = 1; // Count keeps track of which index of argv we store each token
    int leave = 1; // Leave is our while loop conditional statement - set to 0 when we're done tokenizing
    int is_input = 0;
    int is_output = 0;
    int append = 0;
    int already_input = 0;
    int already_output = 0;

    char *arg = strtok(input_buf, " \n\t");
    // If we begin with a redirection character, we don't store it in argv, so we have to decrement the count
    if (*arg == '<') {
        is_input = 1; // By setting is_input to 1, we store the next token as the input file
        count--;
    } else if (*arg == '>') {
        is_output = 1; // By setting is_input to 1, we store the next token as the output file
        if (strcmp(arg, ">>") == 0) {
            append = 1;
        }
        count--;
    } else {
        argv[0] = arg;
    }

    while (leave) {
        arg = strtok(NULL, " \n\t");
        if (arg == NULL) {
            leave = 0;
        } else if (*arg == '<') {
            if (already_input) {
                // Here, we handle the case that the user input includes multiple input redirections
                fprintf(stderr, "syntax error: multiple input files\n");
                return -1;
            }
            is_input = 1;
        } else if (*arg == '>') {
            if (already_output) {
                // Here, we handle the case that the user input includes multiple output redirections
                fprintf(stderr, "syntax error: multiple output files\n");
                return -1;
            }
            if (strcmp(arg, ">>") == 0) {
                append = 1;
            }
            is_output = 1;
        } else {
            if (is_input) {
                files[0] = arg; // We store the input file as the first elemet of files
                is_input = 0;
            } else if (is_output) {
                files[1] = arg; // We store the output file as the second element of files
                is_output = 0;
            } else {
                argv[count] = arg;
                count++;
            }
        }
    }

    // If is_output or is_input is 1 that means we encountered a redirection character but no
    // argument afterwards so we handle this error below
    if (is_output == 1) {
        // Here, we handle the case that no output file was specified
        fprintf(stderr, "syntax error: no output file\n");
        return -1;
    }
    if (is_input == 1) {
        // Here, we handle the case that no input file was specified
        fprintf(stderr, "syntax error: no input file\n");
        return -1;
    }

    // We must manually null-terminate the argv array
    argv[arg_len] = '\0';

    return append;
}

/**
 * Purpose: This is a helper method that ensures the first argument of the argv array
 * is just the binary name instead of the full path to the program to b executed. In
 * order to do this, the method utilizes strtok with "/" as the delimiter to tokenize
 * and then save the last token (which will be the binary name).
 *
 * Parameters: char *argv[] (the argument array)
 * Return value: nothing
 */
void set_first_arg(char *argv[]) {
    int leave = 1; // Our while loop condition, set to 0 when we're done tokenizing
    char *first_arg = strtok(argv[0], "/");
    char *temp;
    while (leave) {
        temp = strtok(NULL, "/");
        // We only want to set first_arg to the token received if we haven't reached the end since NULL
        // will be returned by strtok in that case
        if (temp != NULL) {
            first_arg = temp;
        } else {
            leave = 0;
        }
    }
    argv[0] = first_arg;
}

/**
 * Purpose: This is a helper method that implements the built-in commands cd, ln, and rm.
 * The method utilizes the UNIX system calls, chdir, link, and unlink- corresponding to the
 * respective command- to accomplish this. All that needs to be done is to check whether
 * the user inputted command (stored in file_name) is one of the built-in commands and then
 * perform the call as well as error check if so. The method returns -1 if none of the
 * built-in commands were executed to tell main that we must proceed to execv.
 *
 * Parameters: char *argv[] (the argument array), char file_name (details the inputted command)
 * Return value: 0 if a built-in command is executed, -1 otherwise
 */
int exec_built_in_commands(char *argv[], char file_name[]) {
    if (strcmp(file_name, "cd") == 0) {
        if (argv[1] == NULL) {
            // Here, we handle the case that no argument was given after the command cd
            write(2, "cd: syntax error\n", 17);
        } else if (chdir(argv[1]) == -1) {
            // Here, we handle an error in the call to chdir
            perror("cd");
        }
        return 0;
    } else if (strcmp(file_name, "ln") == 0) {
        if (argv[1] == NULL) {
            // Here, we handle the case that no argument was given after the command ln
            write(2, "ln: missing file operand\n", 25);
        } else if (link(argv[1],argv[2]) == -1) {
            // Here, we handle an error in the call to link
            perror("ln");
        }
        return 0;
    } else if (strcmp(file_name, "rm") == 0) {
        if (argv[1] == NULL) {
            // Here, we handle the case that no argument was given after the command rm
            write(2, "rm: syntax error\n", 17);
        }
        if (unlink(argv[1]) == -1) {
            // Here, we handle an error in the call to unlink
            perror("rm");
        }
        return 0;
    }
    return -1;
}

/**
 * Purpose: This is a helper method to handle the child process when executing the program specified. The
 * method first forks, to create a new process. Then within the new process, we first check to see if we need
 * to handle redirection by checking files to see if we've stored any input or output files. If input was
 * specified, it first closes stdin and opens the input file stored in files[0] which is given the file
 * descriptor 0. If output was specified, it first closes stdout and opens the output file stored in files[1]
 * which is given the file descriptor 1 - depending on whether the > or >> character was used to specify
 * output, the O_TRUNC or the O_APPEND flag, respectively, must be used. After handling redirection, the
 * method just calls execv passing in the full path name to the program as the first argument and the
 * null-terminated argument vector as the second argument.
 *
 * Parameters: char *argv[] (the argument array), char file_name ((full path name to program), int is_append
 * Return value: nothing
 */
void handle_child_process(char *argv[], char file_name[], char *files[], int is_append) {
    if (!fork()) {
        if (files[0] != NULL) {
            close(0);
            // File opened to read only and if file doesn't exist it's to be created
            open(files[0], O_RDONLY | O_CREAT);
            // Now the input file has file descriptor 0
        }
        if (files[1] != NULL) {
            close(1);
            // File opened to write only and if file doesn't exist it's to be created
            if (is_append) {
                // O_APPEND flag included so that output is appended to the end of file
                open(files[1], O_WRONLY | O_CREAT| O_APPEND, 0600);
            } else {
                // O_TRUNC flag included so that the file is first truncated to zero length before output
                open(files[1], O_WRONLY| O_CREAT| O_TRUNC, 0600);
            }
            // Now the output file has file descriptor 1
        }
        execv(file_name, argv);
        // We only reach here if an error occurred in execv - so we handle with perror
        perror("execv");
        exit(1);
    }
}
