#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#define MAX_PATHS 20
#define MAX_COMMANDS 20

char* search_paths[MAX_PATHS];
int num_search_paths = 1;

// error message
char error_message[30]="An error has occurred\n";

int exit_function(char **args, int counter);
int cd_function(char **args, int counter);
int path_function(char **args,int counter);
void read_input_interactive_mode(char **input, size_t *buffer_size);
void parse_input(char *input, char **toks, const char *delimiter, size_t max_tokens, int *counter);
void execute_parallel_commands(char *args[], int counter);
void execute_command(char *args[], int counter);
void interactive_mode();
void batch_mode(const char *batch_file_path);

// struct for built-in commands
typedef struct{
    char *name;
    int (*func)(char **args, int counter);
} builtin;

builtin builtins[] = {
        {"exit",exit_function},
        {"cd", cd_function},
        {"path", path_function},
        {NULL, NULL}
};

int exit_function(char **args, int counter){
    if(counter != 1){
        write(STDERR_FILENO, error_message, strlen(error_message));
    }
    else{
        exit(0);
    }
    return 0;
}

int cd_function(char **args, int counter) {
    if (args[1] == NULL) {
//        fprintf(stderr, "cd: missing argument\n");
        write(STDERR_FILENO, error_message, strlen(error_message));
    }
    else if (counter > 2) {
//        fprintf(stderr, "cd: too many arguments\n");
        write(STDERR_FILENO, error_message, strlen(error_message));
    }
    else {
        if (chdir(args[1]) != 0) {
//            perror("cd");
            write(STDERR_FILENO, error_message, strlen(error_message));
        }
    }
    return 0;
}

int path_function(char **args,int counter){
    // Free the old search paths
    for (int i = 0; i < num_search_paths; i++) {
        free(search_paths[i]);
        search_paths[i] = NULL;
    }
    if (counter <= 1) {
        search_paths[0] = strdup("/bin/");
        num_search_paths = 0;
        return 0;
    }

    num_search_paths = counter - 1;
    for(int i=0; i< num_search_paths;i++){
        search_paths[i] = strdup(args[i+1]);
    }
//    for(int i = 0; i < 10; i++){
//        printf("Updated path: %s\n", search_paths[i]);
//
//    }
    return 0;
}

void read_input_interactive_mode(char **input, size_t *buffer_size){
    printf("witsshell> ");
    fflush(stdout);
    getline(input, buffer_size, stdin);

}

void parse_input(char *input, char **toks, const char *delimiter, size_t max_tokens, int *counter) {
    *counter = 0;
    char *input_copy = strdup(input);
    char *token;

    while ((token = strsep(&input_copy, delimiter)) != NULL) {
        if (strlen(token) > 0){

//          checks for redirection operator and split it into separate tokens
            char *redirection = NULL;
            while ((redirection = strstr(token, ">")) != NULL) {
                if(strcmp(redirection, ">")==0){
                    break;
                }
//              while there are more ">" characters within the token, continue splitting
                if (redirection > token) {
                    if (*counter < max_tokens - 1) {
                        toks[*counter] = strndup(token, redirection - token);
                        (*counter)++;
                    } else {
                        break;
                    }
                }
                if (*counter < max_tokens - 1) {
                    toks[*counter] = strdup(">");
                    (*counter)++;
                } else {
                    break;
                }
                token = redirection + 1;
            }

            // checks for '&' character and split it into separate tokens
            char *parallel = NULL;
            while ((parallel = strstr(token, "&")) != NULL) {
                if(strcmp(parallel, "&")==0){
                    break;
                }
                if (parallel > token) {
                    if (*counter < max_tokens - 1) {
                        toks[*counter] = strndup(token, parallel - token);
                        (*counter)++;
                    } else {
                        break;
                    }
                }
                if (*counter < max_tokens - 1) {
                    toks[*counter] = strdup("&");
                    (*counter)++;
                } else {
                    break;
                }
                token = parallel + 1;
            }

            if (*counter < max_tokens - 1) {
                toks[*counter] = strdup(token);
                (*counter)++;
            }
        }
    }
    free(input_copy);
    toks[*counter] = NULL;
}

void execute_parallel_commands(char *args[], int counter) {
    int num_commands = 0;
    char *commands[MAX_COMMANDS];
    bool command_found[MAX_COMMANDS];
    char *output_file[MAX_COMMANDS];
    bool redirect[MAX_COMMANDS];

    for (int i = 0; i < MAX_COMMANDS; i++) {
        command_found[i] = false;
        output_file[i] = NULL;
        redirect[i] = false;
    }

    // Reset old commands
    for (int i = 0; i < MAX_COMMANDS; i++) {
        commands[i] = NULL;
    }

    int current_command = 0;

    // separates commands using "&" and store them in the commands array
    for (int i = 0; i < counter; i++) {
        if (strcmp(args[i], "&") == 0) {
            if (current_command > 0) {
                commands[current_command] = NULL;
                current_command++;
            }
        } else if (strcmp(args[i], ">") == 0) {
            // redirection
            if (current_command > 0) {
                output_file[current_command - 1] = args[i + 1];
                redirect[current_command - 1] = true;
                i++;
            } else {
                // no command before ">"
                write(STDERR_FILENO, error_message, strlen(error_message));
                return;
            }
        } else {
            commands[current_command] = args[i];
            current_command++;
        }
    }

    // executing commands
    for (int i = 0; i < current_command; i++) {
        if (commands[i] != NULL) {
            // checks if the command is a built-in one
            int x = 0;
            while (builtins[x].name != NULL) {
                if (strcmp(commands[i], builtins[x].name) == 0) {
                    int status = builtins[x].func(args, counter);
                    if (status != 0) {
                        fprintf(stderr, "Error executing built-in command: %s\n", args[0]);
                    }
                    command_found[i] = true;
                    break;
                }
                x++;
            }

            // searching for the executable in the search paths
            if (!command_found[i]) {
                for (int j = 0; j < num_search_paths; j++) {
                    char executable_path[256];
                    strcpy(executable_path, search_paths[j]);
                    if (strlen(executable_path) > 0 && executable_path[strlen(executable_path) - 1] != '/') {
                        strcat(executable_path, "/");
                    }
                    strcat(executable_path, commands[i]);

                    if (access(executable_path, X_OK) == 0) {
                        pid_t pid = fork();
                        if (pid == 0) {
                            if (redirect[i]) {
                                int filed = open(output_file[i], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                                if (filed == -1) {
                                    write(STDERR_FILENO, error_message, strlen(error_message));
                                    exit(1);
                                }
                                dup2(filed, STDOUT_FILENO);
                                dup2(filed, STDERR_FILENO);
                                close(filed);
                            }

                            args[0] = executable_path;
                            execvp(args[0], args);
                            perror("Error");
                            exit(1);
                        }
                    }
                }
            }
        }
    }

    // waiting for all child processes to complete
    for (int i = 0; i < current_command; i++) {
        if (commands[i] != NULL && !command_found[i]) {
            wait(NULL);
        }
    }
}

// executes a single command
void execute_command(char *args[], int counter) {
    int x = 0;
    bool command_found = false;
    char *output_file = NULL;
    bool redirect = false;
    bool is_parallel = false;

    // checks for '&' and execute parallel commands
    for (int i = 0; i < counter; i++) {
        if (strcmp(args[i], "&") == 0) {
            is_parallel = true;
        }
    }
    if(is_parallel){
        execute_parallel_commands(args, counter);
        return;
    }

    // checks if it is a built-in command
    while (builtins[x].name != NULL) {
        if (strcmp(args[0], builtins[x].name) == 0) {
            int status = builtins[x].func(args, counter);
            if (status != 0) {
                fprintf(stderr, "Error executing built-in command: %s\n", args[0]);
            }
            return;
        }
        x++;
    }

    // is redirection present
    for(int i = 0; i < counter; i++){
        if(strcmp(args[i], ">") == 0){
            if(i+1 == counter-1){
                redirect = true;
                output_file = args[i+1];
                args[i] = NULL;
            }
            else if(i+1 == counter || i < counter-1 || i==0){
//                fprintf(stderr, "An error occurred REDIRECT\n");
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(0);
            }
        }
    }

    // only redirection symbol
    if (args[0] == NULL) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(0);
    }

    // searching for the executable in search paths
    for (int i = 0; i < num_search_paths; i++) {
        char executable_path[256];
        strcpy(executable_path, search_paths[i]);
        if (strlen(executable_path) > 0 && executable_path[strlen(executable_path) - 1] != '/') {
            strcat(executable_path, "/");
        }
        strcat(executable_path, args[0]);
//        printf("test %s\n", search_paths[i]);
//        snprintf(executable_path, sizeof(executable_path), "%s%s", search_paths[i], args[0]);
//        printf("Full path for %s: %s\n", args[0], executable_path);
        if (access(executable_path, X_OK) == 0) {
            pid_t pid = fork();
            if (pid == 0) {
                if(redirect){
                    // O_CREAT creates file if it doesn't exist
                    int filed = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if(filed == -1){
//                        perror("Error opening output file");
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        exit(1);
                    }
                    dup2(filed, STDOUT_FILENO);
                    dup2(filed, STDERR_FILENO);
                    close(filed);
                }

                args[0] = executable_path;
//                args[0] = args[0];
                execvp(args[0], args);
                perror("Error");
                exit(1);
            } else if (pid > 0) {
                wait(NULL);
                command_found = true;
                break;
            } else {
                perror("Fork error");
            }
        }
    }

    if (!command_found) {
//        fprintf(stderr, "Command not found: %s\n", args[0]);
        write(STDERR_FILENO, error_message, strlen(error_message));

    }
}

void interactive_mode(){
    char *input = NULL;
    size_t buffer_size = 0;

    while(1){
        read_input_interactive_mode(&input, &buffer_size);
//    if the end-of-file marker (EOF) has been reached
        if(feof(stdin)){
            printf("\n");
            exit(0);
        }
//        if the user types "exit"
//        if (strcmp(input, "exit\n") == 0) {
//            exit_function();
//        }
        size_t max_args = 20;
        char *args[max_args];
        int counter = 0;
        for(int i = 0; i < counter; i++){
            args[i] = NULL;
        }

        parse_input(input,args," \t\n", max_args, &counter);

        if (counter > 0){
            execute_command(args, counter);
        }

        for (int i =0; i < counter; i++){
//            printf("Arg %d: %s\n", i, args[i]);
            free(args[i]);
        }
    }
}

void batch_mode_execute_line(char* line){
    size_t max_args = 10;
    char *args[max_args];
    int counter = 0;
    for(int i = 0; i < counter; i++){
        args[i] = NULL;
    }
    parse_input(line, args, " \t\n", max_args, &counter);
    if (counter > 0) {
        execute_command(args, counter);
    }
    for (int i = 0; i < counter; i++) {
        free(args[i]);
    }
}

void batch_mode(const char *batch_file_path){
    FILE *batch_file = fopen(batch_file_path, "r");
    if (batch_file == NULL){
//        perror("Error opening batch file");
        write(STDERR_FILENO, error_message, strlen(error_message));

        exit(1);
    }

    char* line = NULL;
    size_t length = 0;
    while ((getline(&line, &length, batch_file)) > 0) {
        if(line[strlen(line) - 1] == '\n') {
            line[strlen(line) - 1] = '\0';
        }
        if(strcmp(line, "exit") == 0){
            free(line);
            break;
        }
        batch_mode_execute_line(line);
    }
    fclose(batch_file);
    exit(0);
}





int main(int MainArgc, char *MainArgv[]){
    search_paths[0] = strdup("/bin/");
    //    differentiating between interactive mode and batch mode
    //    Interactive mode
    if (MainArgc == 1){
        interactive_mode();
    }
    //    Batch mode
    else if (MainArgc == 2){
        batch_mode(MainArgv[1]);
    }
    //    Error: neither chosen
    else{
//        fprintf("invalid usage of shell");
//        fprintf(stderr, "Usage: %s [batch_file]\n", MainArgv[0]);
        write(STDERR_FILENO, error_message, strlen(error_message));


        exit(1);
    }

    return(0);
}