#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAXCOM 1000  
#define MAXLIST 100  

void printDir() {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("\nDir: %s", cwd);
}

void parseInput(char* str, char** parsedArgs) {
    int i = 0;
    char* token = strtok(str, " \n");
    while (token != NULL && i < MAXLIST) {
        parsedArgs[i++] = token; 
        token = strtok(NULL, " \n");
    }
    parsedArgs[i] = NULL; 
}

int handleBuiltins(char** parsedArgs) {
    if (strcmp(parsedArgs[0], "help") == 0) {
        printf("Supported commands:\n");
        printf("cd <dir>\nmkdir <dir>\nexit\n!! (Repeat last command)\n");
        return 1;
    }
    if (strcmp(parsedArgs[0], "cd") == 0) {
        if (chdir(parsedArgs[1]) != 0) {
            perror("cd failed");
        }
        return 1;
    }
    if (strcmp(parsedArgs[0], "mkdir") == 0) {
        if (mkdir(parsedArgs[1], 0777) != 0) {
            perror("mkdir failed");
        }
        return 1;
    }
    if (strcmp(parsedArgs[0], "exit") == 0) {
        exit(0);
    }
    return 0;
}

void execArgs(char** parsedArgs) {
    pid_t pid = fork();
    if (pid == 0) {
        if (execvp(parsedArgs[0], parsedArgs) < 0) {
            printf("Could not ecxecute command\n");
        }
        exit(0);
    } else {
        wait(NULL);
    }
}

void parseSpecial(char* str) {
    int echoFlag = 0;
    int i = 0;
    int len = strlen(str);

    char* lastWord = strrchr(str, ' ');
    if (lastWord != NULL && strcmp(lastWord + 1, "ECHO") == 0) {
        echoFlag = 1;
        str[len - 5] = '\0'; 
    }

    if (echoFlag) {
        int spaceFlag = 0;  
        while (str[i] != '\0') {
            if (str[i] == ' ') {
                if (!spaceFlag) {
                    printf("\nSPACE");
                    spaceFlag = 1;  
                }
            } else if (str[i] == '|') {
                printf("\nPIPE");
                spaceFlag = 0;  
            } else {
                if (spaceFlag) {
                    printf("\n");  
                }
                printf("%c", str[i]);  
                spaceFlag = 0;  
            }
            i++;
        }
    } else {
        printf("ECHO not at the end, ignoring.\n");
    }
}

// Function to handle input/output redirection
void execRedirection(char** parsedArgs) {
    pid_t pid = fork();
    if (pid == 0) {
        int outFile = open(parsedArgs[2], O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (outFile == -1) {
            perror("Failed to open file");
            exit(1);
        }
        dup2(outFile, STDOUT_FILENO);
        close(outFile);

        if (execvp(parsedArgs[0], parsedArgs) < 0) {
            printf("Could not execute command\n");
        }
        exit(0);
    } else {
        wait(NULL);
    }
}

void execPipe(char** parsedArgs1, char** parsedArgs2) {
    int pipefd[2];
    pid_t pid1, pid2;

    if (pipe(pipefd) < 0) {
        perror("Pipe failed");
        exit(1);
    }

    pid1 = fork();
    if (pid1 == 0) {
        close(pipefd[0]);  
        dup2(pipefd[1], STDOUT_FILENO); 
        close(pipefd[1]);
        
        for (int i = 0; parsedArgs1[i] != NULL; i++) {
            printf("Arg[%d]: %s\n", i, parsedArgs1[i]);
        }
        
        if (execvp(parsedArgs1[0], parsedArgs1) < 0) {
            perror("Could not execute command 1");
        }
        exit(0);
    } else {
        pid2 = fork();
        if (pid2 == 0) {
            close(pipefd[1]);  
            dup2(pipefd[0], STDIN_FILENO); 
            close(pipefd[0]);
            
            for (int i = 0; parsedArgs2[i] != NULL; i++) {
                printf("Arg[%d]: %s\n", i, parsedArgs2[i]);
            }
            
            if (execvp(parsedArgs2[0], parsedArgs2) < 0) {
                perror("Could not execute command 2");
            }
            exit(0);
        } else {
            close(pipefd[0]); 
            close(pipefd[1]);
            wait(NULL);
            wait(NULL);
        }
    }
}


int main() {
    char inputString[MAXCOM];
    char* parsedArgs[MAXLIST];
    char* parsedArgs1[MAXLIST];
    char* parsedArgs2[MAXLIST];
    char lastCommand[MAXCOM] = "";

    while (1) {
        printDir();
        printf("\n>>> ");
        fgets(inputString, MAXCOM, stdin);
        inputString[strlen(inputString) - 1] = '\0'; 

        // Handle '!!' command
        if (strcmp(inputString, "!!") == 0) {
            if (strlen(lastCommand) == 0) {
                printf("No commands in history.\n");
                continue;
            }
            printf("Repeating last command: %s\n", lastCommand);
            strcpy(inputString, lastCommand); 
        } else {
            strncpy(lastCommand, inputString, sizeof(lastCommand) - 1);
            lastCommand[sizeof(lastCommand) - 1] = '\0';  
        }
        
        if (strstr(inputString, "ECHO") != NULL && strcmp(inputString + strlen(inputString) - 4, "ECHO") == 0) {
            parseSpecial(inputString);
            continue;
        }

        char* pipePos = strstr(inputString, "|");
        if (pipePos != NULL) {
            *pipePos = '\0';  
            pipePos++;  
            while (*pipePos == ' ') pipePos++; 

            parseInput(inputString, parsedArgs1);  
            parseInput(pipePos, parsedArgs2);      

            execPipe(parsedArgs1, parsedArgs2);
            continue;
        }

        parseInput(inputString, parsedArgs);

        // Handle built-in commands
        if (handleBuiltins(parsedArgs)) {
            continue;
        }

        // Execute regular commands
        execArgs(parsedArgs);
    }

    return 0;
}
