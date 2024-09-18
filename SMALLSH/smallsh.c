// Previously attempted course in Fall 2023.

#define _POSIX_C_SOURCE 200809L     // Enables Portable Operating System Interface (POSIX) features, ensuring code can use POSIX system interfaces
#define _GNU_SOURCE 700             // POSIX extension. 

/* HEADER FILES: These provide function prototypes, macros, and type definitions that the program will use.*/
#include <err.h>              // Provides functions for reporting errors.
#include <errno.h>            // Defines the integer variable errno, which is set by system calls and some library functions in the event of an error to indicate what went wrong. 
#include <stdio.h>            // Provides functionalities for input/output stream operations
#include <stdlib.h>           // Provides general-purpose functions, including dynamic memory management, random number generation, and process control.
#include <unistd.h>           // Provides access to the POSIX operating system API.
#include <ctype.h>
#include <stdint.h>           // Defines exact-width integer types, like int8_t, int16_t, 
#include <fcntl.h>            // Provides functions and macros to handle files, such as opening, closing, and manipulating file descriptors.
#include <sys/wait.h>         // For waitpid
#include <signal.h>           // For kill and SIGCONT
#include <string.h>           // For strchr, strncpy, strcpy, strlen, strdup
#include <stdbool.h>          // Boolean type and values
#include <limits.h>

#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif

/* GLOBAL VARIABLES (AKA file-scoped objects) */
int last_foreground_exit_status = 0;
pid_t last_background_pid = 0;                      // To store the PID of the last background process
volatile sig_atomic_t sigint_received = 0;
                 
char *words[MAX_WORDS];

/* MAIN FUNCTIONS/FORWARD DECLARATIONS: inform the compiler about the function signature before use */
void manage_background_processes();                                                             // 1A:    Input - Managing background processes
void display_prompt();                                                                          // 1B:    Input - Display prompt
size_t wordsplit(char const *line);                                                             // 2:     Word Splitting
char param_scan(char const *word, char const **start, char const **end);                        // 3A:    Expansion - Scan word for next parameter   
char * expand(char const *word);                                                                // 3B I:  Expansion - Expand
char * build_str(char const *start, char const *end);                                           // 3B II: Expansion - Build string
void parse_input(size_t nwords, char* line);                                                    //
void execute_command(size_t nwords, char *command, char *args[], int background, 
                    char *input_redirection, char *output_redirection);
void handle_sigint(int sig);
void sigchld_handler(int sig);
void setup_signal_handlers();
void segfault_sigaction(int signal, siginfo_t *si, void *arg);

int main(int argc, char *argv[]) {
    // Setup signal handling for segmentation faults
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = segfault_sigaction;
    
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("Error setting up signal handler for SIGSEGV");
        exit(EXIT_FAILURE);
    }
    
    FILE *input = stdin;
    char *input_fn = "(stdin)";
    char *line = NULL;
    size_t n = 0;
    ssize_t line_len;               

    // File input handling
    if (argc == 2) {
        input_fn = argv[1];
        input = fopen(input_fn, "re");
        if (!input) err(1, "%s", input_fn);
    } else if (argc > 2) {
        errx(1, "too many arguments");
    }

    // Setup signal handling
    setup_signal_handlers();

    for (;;) {
        // Check and handle background processes
        manage_background_processes();

        // Display prompt in interactive mode
        if (input == stdin) {
            display_prompt();
        }

        // Read input line
        line_len = getline(&line, &n, input);
        if (line_len < 0) {
            if (feof(input)) break; // End of file or stream, exit loop
            err(1, "%s", input_fn); // Error handling for getline
        }

        // Word splitting and expansion
        size_t nwords = wordsplit(line);
        for (size_t i = 0; i < nwords; ++i) {
            // fprintf(stderr, "Word %zu: %s\n", i, words[i]);
            char* exp_word = expand(words[i]);
            free(words[i]);                                             // Free the original word
            words[i] = exp_word;                                        // Assign expanded word
            // fprintf(stderr, "Expanded Word %zu: %s\n", i, words[i]);
        }

        // Parse & Execute
        parse_input(nwords, line);
    }

    // Cleanup before exiting
    if (input != stdin) {
        fclose(input);
    }
    free(line);

    return last_foreground_exit_status;
}

char *words[MAX_WORDS] = {0};

/* PART 1A: Input - Managing background processes                                                                     MODERATELY CONFIDENT
*/
void manage_background_processes() { 
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        if (WIFEXITED(status)) {
            fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t)pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "Child process %jd terminated by signal %d.\n", (intmax_t)pid, WTERMSIG(status));
        } else if (WIFSTOPPED(status)) {
            kill(pid, SIGCONT);
            // fprintf(stderr, "Child process %jd stopped by signal %d.\n", (intmax_t)pid, WSTOPSIG(status));
        } else if (WIFCONTINUED(status)) {
            fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t)pid);
        }
    }
}

/* PART 1B: Input - Display prompt                                                                                    MODERATELY CONFIDENT
*/
void display_prompt() {
    char *ps1 = getenv("PS1");      // Try to get the PS1 environment variable

    // Check if the effective user ID is 0 (root user)
        if (!ps1) {
            if (geteuid() == 0) {
                ps1 = "#";          // Default prompt for root if PS1 is not set
            } else {
                ps1 = "$";          // Default prompt for non-root users
            }
        }

    fprintf(stderr, "%s", ps1);     // Print the prompt to stderr
}

/* PART 2: Word Splitting                                                                                             PROVIDED IN SKELETON CODE
 * Input:    takes a string (line)
 * Function: splits string into words based on whitespace
 *           recognizes comments as '#' at the beginning of a word
 *           backslash (\) escapes
 *           updates the words[] array with pointers to the words, each as an allocated string.
 * Returns:  number of words parsed
 */
size_t wordsplit(char const *line) {
    size_t wlen = 0;
    size_t wind = 0;

    char const *c = line;
    for (;*c && isspace(*c); ++c);                              /* discard leading space */

    for (; *c;) {
        if (wind == MAX_WORDS) break;
        /* read a word */
        if (*c == '#') break;
        for (;*c && ! isspace(*c); ++c) {
            if (*c == '\\') ++c;
            void *tmp = realloc(words[wind], sizeof **words * (wlen +2));
            if (!tmp) err(1, "realloc");
            words[wind] = tmp;
            words[wind][wlen++] = *c;
            words[wind][wlen] = '\0';
        }
        ++wind;
        wlen = 0;
        for (;*c && isspace(*c); ++c);
    }
    return wind;
}

/* PART 3A: Expansion - Scan word for next parameter                                                                  PROVIDED IN SKELETON CODE

 * Input:    char const *word - The input string to be scanned for parameter patterns. This string 
                                is expected to potentially contain shell parameters that need expansion, 
                                such as $$, $!, $?, or ${parameter}.
             char const **start - A pointer to a pointer to char, used to return the start position of 
                                  the first parameter pattern found in the input string.
             char const **end -  A pointer to a pointer to char, used to return the end position of the 
                                 first parameter pattern found in the input string.

 * Function: this function is a utility for parsing and expanding shell parameters within a string. By 
             identifying the start and end of parameter patterns, it facilitates the extraction and subsequent 
             expansion of these parameters. The use of prev allows for efficient iterative scanning through a string 
             to find multiple parameters without overlapping or re-scanning the same segments.

 * Returns:  char ret - The function returns a character that indicates the type of parameter found 
                        ($, !, ?, or {). If no parameter pattern is found, it returns 0.
 */
char param_scan(char const *word, char const **start, char const **end) {
    static char const *prev;
    if (!word) word = prev;
    
    char ret = 0;
    *start = 0;
    *end = 0;
    for (char const *s = word; *s && !ret; ++s) {
        s = strchr(s, '$');
        if (!s) break;
        switch (s[1]) {
        case '$':
        case '!':
        case '?':
        ret = s[1];
        *start = s;
        *end = s + 2;
        break;
        case '{':;
        char *e = strchr(s + 2, '}');
        if (e) {
            ret = s[1];
            *start = s;
            *end = e + 1;
        }
        break;
        }
    }
    prev = *end;
    return ret;
}

/* PART 3B I: Expansion - Expand                                                                                      PARTIALLY PROVIDED IN SKELETON CODE

 * Input:    char const *word - The input string that might contain shell parameter patterns to be expanded.

 * Function: Expands all instances of $! $$ $? and ${param} in a string 
   
 * Placeholders:       <BGPID> the background process ID
                         <PID> the current process ID
                      <STATUS> the exit status of the last command
                 <Parameter: > the value of a named environment variable, respectively

 * Returns:  a newly allocated string that the caller must free
 */
char *expand(char const *word){
    char const *pos = word;
    char const *start, *end;
    char c = param_scan(pos, &start, &end);
    build_str(NULL, NULL);
    build_str(pos, start);

    while (c) {
        if (c == '!' && strncmp(start, "$!", 2) == 0) {
            if (last_background_pid > 0) {
                char bgpid[20];
                sprintf(bgpid, "%d", last_background_pid);
                build_str(bgpid, NULL);
            } else {
                build_str("", NULL); // Do not add anything if no background process has been run.
            }
        } else if (c == '$') {
            char pid[20];
            sprintf(pid, "%d", getpid());
            build_str(pid, NULL);
        } else if (c == '?') {
            char status[20];
            sprintf(status, "%d", last_foreground_exit_status);
            build_str(status, NULL);
        } else if (c == '{') {
            char *varname = strndup(start + 2, end - start - 3);
            char *varval = getenv(varname);
            free(varname);
            build_str(varval ? varval : "", NULL);
        }

        pos = end;
        c = param_scan(pos, &start, &end);
        build_str(pos, start);
    }

    return build_str(start, NULL);
}

/* PART 3B II: Expansion - Build String                                                                               PROVIDED IN SKELETON CODE

 * Input:    char const *start: A pointer to the start of the segment to be appended.
             char const *end:   A pointer to the end of the segment. If NULL, the function appends from start to the end of the string.

 * Function: Builds up a base string by appending supplied strings/character ranges to it.

 * Returns:  the current state of the base string after appending the new segment. Subsequent calls to build_str for appending more data 
             or resetting will modify this same string.
 */
char *build_str(char const *start, char const *end){
    static size_t base_len = 0;
    static char *base = 0;

    if (!start) {
        /* Reset; new base string, return old one */
        char *ret = base;
        base = NULL;
        base_len = 0;
        return ret;
    }
    /* Append [start, end) to base string 
    * If end is NULL, append whole start string to base string.
    * Returns a newly allocated string that the caller must free.
    */
    size_t n = end ? end - start : strlen(start);
    size_t newsize = sizeof *base *(base_len + n + 1);
    void *tmp = realloc(base, newsize);
    if (!tmp) err(1, "realloc");
    base = tmp;
    memcpy(base + base_len, start, n);
    base_len += n;
    base[base_len] = '\0';

    return base;
}

// PART 4: Parsing
void parse_input(size_t nwords, char* line) {
    // Initialize parsing variables
    char *command = NULL;
    char *args[MAX_WORDS] = {0};
    int background = 0;
    char *input_redirection = NULL, *output_redirection = NULL;

    // Parse each word
    for (size_t i = 0; i < nwords; ++i) {
        char *word = words[i];
        // Check for background operator
        if (strcmp(word, "&") == 0 && i == nwords - 1) {
            background = 1;
        } else if (strcmp(word, "<") == 0) {    // Input redirection    (READ)
            input_redirection = words[++i];
        } else if (strcmp(word, ">") == 0) {    // Output redirection   (WRITE)
            output_redirection = words[++i];
        } else if (strcmp(word, ">>") == 0) {   // Output redirection   (APPEND)
            output_redirection = words[++i];
        } else if (command == NULL) {
            command = word;                     // The first non-operator word is the command
        } else {
            args[i] = word;                     // Subsequent words are arguments
        }
    }

    execute_command(nwords, command, args, background, input_redirection, output_redirection);
}

// PART 5: Execute
void execute_command(size_t nwords, char *command, char *args[], int background, char *input_redirection, char *output_redirection) {
    if (strcmp(command, "exit") == 0) {
        if (nwords == 2) {
            char *endptr;
            errno = 0; 
            long int parsed_exit_status = strtol(words[1], &endptr, 10);

            // Error. Second argument is not a valid integer. Do not exit
            if (*endptr != '\0' || errno == ERANGE) {
                fprintf(stderr, "Exit error: invalid argument. Argument must be an integer.\n");
                return;

            // Exit - 2 arguments
            } else {
                exit((int)parsed_exit_status); 
            }

        // Error. Too many arguments. Do not exit
        } else if (nwords > 2) {
            fprintf(stderr, "Exit error: Too many arguments.\n");
            return;

        // Exit - 1 argument     
        } else {
            exit(last_foreground_exit_status); 
        }

    } else if (strcmp(command, "cd") == 0) {
        // Default to HOME directory
        const char *dir = getenv("HOME"); 

        // If there is an argument provided, use it as the directory
        if (nwords == 2) {
            dir = words[1];

        // If more than one argument is provided, report an error    
        } else if (nwords > 2) {
            fprintf(stderr, "cd error: Too many arguments.\n");
            return;
        }

        // Attempt to change the directory
        if (chdir(dir) != 0) {
            // If changing the directory fails, report an error
            perror("cd");
        }

    } else {
        // Fork and execute non-built-in commands
        pid_t pid = fork();

        // Child process
        if (pid == 0) { 
            // Reset signal handlers to default behavior
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            // Initialize a new array to hold the arguments for execvp
            char *exec_args[MAX_WORDS] = {0};
            int exec_argc = 0;

            for (size_t i = 0; i < nwords; ++i) {
                if (strcmp(words[i], "<") == 0 && (i + 1 < nwords)) {
                    // Handle input redirection
                    int in_fd = open(words[i + 1], O_RDONLY);
                    if (in_fd < 0) {
                        perror("open input");
                        exit(EXIT_FAILURE);
                    }
                    if (dup2(in_fd, STDIN_FILENO) < 0) {
                        perror("dup2 input");
                        exit(EXIT_FAILURE);
                    }
                    close(in_fd);
                    i++; // Skip the filename in the next iteration
                } else if ((strcmp(words[i], ">") == 0 || strcmp(words[i], ">>") == 0) && (i + 1 < nwords)) {
                    // Handle output redirection
                    int flags = strcmp(words[i], ">>") == 0 ? (O_WRONLY | O_CREAT | O_APPEND) : (O_WRONLY | O_CREAT | O_TRUNC);
                    int out_fd = open(words[i + 1], flags, 0666);
                    if (out_fd < 0) {
                        perror("open output");
                        exit(EXIT_FAILURE);
                    }
                    if (dup2(out_fd, STDOUT_FILENO) < 0) {
                        perror("dup2 output");
                        exit(EXIT_FAILURE);
                    }
                    close(out_fd);
                    i++; // Skip the filename in the next iteration
                } else {
                    // Add the argument to the exec_args array if it's not a redirection operator
                    exec_args[exec_argc++] = words[i];
                }
            }

            exec_args[exec_argc] = NULL; // Ensure the argument list is NULL-terminated

            // Execute the command
            execvp(exec_args[0], exec_args);
            perror("execvp");
            exit(EXIT_FAILURE);

        // Parent process
        } else if (pid > 0) { 
            int status;
            if (!background) {
                // Wait for foreground process to complete
                do {
                    waitpid(pid, &status, WUNTRACED);
                    if (WIFEXITED(status)) {
                        last_foreground_exit_status = WEXITSTATUS(status);
                        // printf("Foreground process with PID %d exited normally with status %d.\n", pid, WEXITSTATUS(status));
                    } else if (WIFSIGNALED(status)) {
                        last_foreground_exit_status = 128 + WTERMSIG(status);
                        // printf("Foreground process with PID %d was terminated by signal %d.\n", pid, WTERMSIG(status));
                    } else if (WIFSTOPPED(status)) {
                        kill(pid, SIGCONT);
                        last_background_pid = pid;
                        // printf("Foreground process with PID %d was stopped. Automatically continuing in the background.\n", pid);
                        break;
                    }
                } while (!WIFEXITED(status) && !WIFSIGNALED(status));
            } else {
                // For background processes, do not wait
                // printf("Started background process with PID %d.\n", pid);
                last_background_pid = pid;
            }
        } else {
            perror("fork");
        }
    }
}

void handle_sigint(int sig) {
    // Set the flag
    sigint_received = 1; 
    write(STDOUT_FILENO, "\n", 1);
}

// Signal handler for SIGCHLD
void sigchld_handler(int sig) {
    int saved_errno = errno;
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        if (WIFEXITED(status)) {
            fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t)pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t)pid, WTERMSIG(status));
        } else if (WIFSTOPPED(status)) {
            // Automatically continue stopped background process (advised to be non-standard behavior)
            kill(pid, SIGCONT);
            // fprintf(stderr, "Child process %d stopped by signal %d\n", (intmax_t)pid, WSTOPSIG(status));
        } else if (WIFCONTINUED(status)) {
            fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t)pid);
        }
    }
    errno = saved_errno;
}

void setup_signal_handlers() {
    struct sigaction sa_sigchld = {0};
    sa_sigchld.sa_handler = sigchld_handler;
    sa_sigchld.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa_sigchld, NULL) == -1) {
        perror("sigaction(SIGCHLD)");
        exit(EXIT_FAILURE);
    }

    // Ignoring SIGTSTP in the parent process
    signal(SIGTSTP, SIG_IGN);

    // Ignoring SIGINT in the parent process
    signal(SIGINT, SIG_IGN);
}

void segfault_sigaction(int signal, siginfo_t *si, void *arg) {
    exit(0);  // Exit cleanly with status 0
}