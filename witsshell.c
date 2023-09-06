#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#define MAX_PATHS 20

//initial search path is set to bin
char* search_paths[MAX_PATHS];
int num_search_paths = 1;
// error message
char error_message[30]="An error has occurred\n";

int exit_function(char **args, int counter);
int cd_function(char **args, int counter);
int path_function(char **args,int counter);

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


//int path_function(char **args){
//    int i = 0;
//    while(args[i+1] != NULL){
////        updating the search path
//        if(i < MAX_PATHS-1){
//            paths[i] = args[i+1];
//            i++;
//        }
//        else{
//            fprintf(stderr, "Path list full\n");
//        }
//    }
//}


int path_function(char **args,int counter){
    // Free the old search paths
    for (int i = 0; i < num_search_paths; i++) {
        free(search_paths[i]);
        search_paths[i] = NULL;
    }
//    search_paths[0] = "/bin/";
    if (counter <= 1) {
//        fprintf(stderr, "path: missing argument(s)\n");
//        NO YOU NEED TO SET IT SO THAT ITLL ONLY USE THE BUILTIN FUNCTIONS
        search_paths[0] = strdup("/bin/");
        num_search_paths = 0;
//        printf("Path set to empty.\n");
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

//    n_processes = counter - 2;
}
void read_input_interactive_mode(char **input, size_t *buffer_size){
    printf("witsshell> ");
    fflush(stdout);
    getline(input, buffer_size, stdin);

}




//void parse_input(char *input, char **toks, const char *delimiter, size_t max_tokens, int *counter){
//    *counter = 0;
//    char* token = NULL;
//
//    while ((token = strsep(&input, delimiter)) !=NULL){
//        if (*counter < max_tokens -1) {
//            toks[*counter] = strdup(token);
//            *(counter)++;
//        }
//    }
//    toks[*counter] = NULL;
//}
void parse_input(char *input, char **toks, const char *delimiter, size_t max_tokens, int *counter) {
    *counter = 0;
    char *input_copy = strdup(input);
    char *token;

    while ((token = strsep(&input_copy, delimiter)) != NULL) {
        if (strlen(token) > 0){

// Check for redirection operator and split it into separate tokens
            char *redirection = NULL;
            while ((redirection = strstr(token, ">")) != NULL) {
//                while there are more ">" characters within the token, continue splitting
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
                // Move the token pointer past the ">"
                token = redirection + 1;
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

//
//void execute_command(char *args[], int counter) {
//    int x = 0;
//    bool command_found = false;
////    Checks if commands are built-in ones
////// you can also do it like this as it makes more sense
////    if(strcmp(args[0], "cd")==0){
////        int status = cd_function(args);
////        if (status != 0) {
////            fprintf(stderr, "Error executing built-in command: %s\n", args[0]);
////        }
////        return;
////    }
//    while (builtins[x].name != NULL) {
//        if (strcmp(args[0], builtins[x].name) == 0) {
//            int status = builtins[x].func(args, counter);
//            if (status != 0) {
//                fprintf(stderr, "Error executing built-in command: %s\n", args[0]);
//            }
//            return;
//        }
//        x++;
//    }
//
////    not a built-in function, search for executable in the search paths
//    pid_t pid = fork();
//    if (pid == 0) {
//        // Child process
//        execvp(args[0], args);  // Use the full path
//        // If execv returns, there was an error
//        perror("Error");
//        exit(1);
//    } else if (pid > 0) {
//        // Parent process
//        wait(NULL);  // Wait for the child process to complete
//    } else {
//        perror("Fork error");
//    }
//}
void execute_command(char *args[], int counter) {
    int x = 0;
    bool command_found = false;
    char *output_file = NULL;
    bool redirect = false;

    // Check if the command is a built-in one
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


    // Detects if Redirection operator is present and valid
    for(int i = 0; i < counter; i++){
        if(strcmp(args[i], ">") == 0){
            if(i+1 == counter-1){
                redirect = true;
                output_file = args[i+1];
                args[i] = NULL; // Removes redirection operator from command
            }
            else if(i+1 == counter || i < counter-1 || i==0){
//                fprintf(stderr, "An error occurred REDIRECT\n");
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(0);
            }
        }
    }


    // Search for the executable in search paths
    for (int i = 0; i < num_search_paths; i++) {
        char executable_path[256];
        strcpy(executable_path, search_paths[i]);
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


//void redirection(char *args[], int counter){
//    char *output_file = NULL;
//    bool redirect = false;
//    for(int i = 0; i < counter; i++){
//        if(strcmp(args[i], ">") == 0){
//            if(i+1 < counter || i == 0 || i < counter-1){
//                fprintf(stderr, "An error occurred REDIRECT\n");            }
//            output_file = args[i+1];
//            args[i] = NULL;
//        }
//    }
//}

void interactive_mode(){
    char *input = NULL;
    size_t buffer_size = 0;

    while(1){
        read_input_interactive_mode(&input, &buffer_size);
//    if the end-of-file marker (EOF) has been reached, exit while loop
        if(feof(stdin)){
            printf("\n");
            exit(0);
        }
//        if the user types "exit"
//        if (strcmp(input, "exit\n") == 0) {
//            exit_function();
//        }
        size_t max_args = 10;
        char *args[max_args];
        int counter = 0;
        parse_input(input,args," \t\n", max_args, &counter);

        if (counter > 0){
            execute_command(args, counter);
        }

//        processing toks array
        for (int i =0; i < counter; i++){
//            printf("Arg %d: %s\n", i, args[i]);
            free(args[i]);
        }

//        if (input) {
//            free(input); // Free the allocated memory
//        }
    }
}

void batch_mode_execute_line(char* line){
    size_t max_args = 10;
    char *args[max_args];
    int counter = 0;
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
    //    to differentiate between interactive mode and batch mode
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
