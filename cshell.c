#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

/* Global variables */
#define MAX_CHARS 2048
#define MAX_ARGS 512

/* I referenced the thread conversation in this link to enumerate a bool type: https://stackoverflow.com/questions/4159713/how-to-use-boolean-datatype-in-c */
typedef enum {false=0, true=1} bool;

int noBackground = 0;                                                                       /* Variable to hold 0 or 1, which indicates whether the shell can run commands in the background */

/* Function prototypes */
void catchSIGINT(int sig);
void catchSIGSTP(int sig);

/**************************************************************** 
* Name: catchSIGINT()
* Description: This function will print out the number of the signal that killed the foreground child process. Please note, since we are not allowed to use printf(), I could not figure out a way to convert the int
*               into a string to print out the signal number with write(). I successfully converted an int to a char later in the program but it was via snprintf(), which I 
*               believe is also non-reentrant. I ran my program numerous times and each time the signal number was 2 so I have hardcoded it in, however, I understand that
*               this is not the correct way to approach this problem because if the signal value changes, the correct value will not be printed to the screen
****************************************************************/
void catchSIGINT(int signo){
    char *sigintString = "terminated by signal 2\n";                                        /* Variable to hold the string informing the user of the signal that killed the foreground child process */

    write(1, sigintString, 23);                                                             /* I used write() and number of chars (23) as printf() and strlen(sigintString) are non-reentrant. 1 is the file descriptor for stdout */
    fflush(stdout);                                                                         /* Flush out output buffer */
}

/**************************************************************** 
* Name: catchSIGSTP()
* Description: This function will display an informative message immediately if it's sitting at the prompt, or immediately after any currently running foreground process has 
*               terminated. If this is an odd-numbered time that the function has been called, such as the first or third time, it will display a message and set noBackground 
*               to 1, which will enter the shell into a state where subsequent commands can no longer be run in the background. In this state the & operator is ignored, which means 
*               that all commands are run as if they were foreground processes. If this is an even-numbered time that the function has been called, such as the second or fourth 
*               time, a different message will be displayed and noBackground will be set to 0, which means that the & operator will once again cause commands to be placed in the
*               background
****************************************************************/
void catchSIGSTP(int signo){
    if(!noBackground){
        char *foregroundString = "\nEntering foreground-only mode (& is now ignored)\n";    /* Variable to hold the string informing the user that foreground-only mode has been entered */

        write(1, foregroundString, 50);                                                     /* I used write() and number of chars (49) as printf() and strlen(foregoundString) are non-reentrant. 1 is the file descriptor for stdout */
        fflush(stdout);                                                                     /* Flush out output buffer */

        char* promptString1 = ": ";                                                         /* Print out ": " as a prompt to the command line */

        write(1, promptString1, 2);                                                         /* I used write() and number of chars (2) as printf() and strlen(promptString1) are non-reentrant. 1 is the file descriptor for stdout */
        fflush(stdout);                                                                     /* Flush out the output buffer */
        noBackground = 1;                                                                   /* Set noBackground to 1 as new commands cannot be run in the background */
    }
    else{
        char *backgroundString = "\nExiting foreground-only mode\n";                        /* Variable to hold the string informing the user that foreground-only mode has been exited */

        write(1, backgroundString, 30);                                                     /* I used write() and number of chars (30) as printf() and strlen(backgroundString) are non-reentrant. 1 is the file descriptor for stdout */
        fflush(stdout);                                                                     /* Flush out output buffer */

        char* promptString2 = ": ";                                                         /* Print out ": " as a prompt to the command line */

        write(1, promptString2, 2);                                                         /* I used write() and number of chars (2) as printf() and strlen(promptString2) are non-reentrant. 1 is the file descriptor for stdout */
        fflush(stdout);                                                                     /* Flush out the output buffer */
        noBackground = 0;                                                                   /* Set noBackground to 0 as new commands can be run in the background */
    }
}

int main(){
    char userString[MAX_CHARS];                                                             /* Variable to hold current line that is entered by the user */
    char* userStringPtr[MAX_ARGS];                                                          /* Array of pointers to strings */
    char* inputFile = NULL;                                                                 /* File pointer for the input file */
    char* outputFile = NULL;                                                                /* File pointer for the output file */
    int stillRunning = 1;                                                                   /* Variable to hold 0 or 1 for the while loop, which indicates whether the user has indicated they would like to exit the shell */
    int backgroundProcess = 0;                                                              /* Variable to hold 0 or 1. Set to 1 when user enters a "&" character to indicate that a command is to be run in background */
    int i = 0;
    int childExitMethod = 0;                                                        
    int alreadyProgressed = 0;                                                              /* Variable to hold 0 or 1. Set to 1 if the token variable has already been instructed to point to the next position in the string */
    int currentPID = -1;                                                                    /* Variable to hold the current PID */
    pid_t spawnPid = -5;                                                                    /* Variable to hold the PID for child processes */                                                         

    /* Please note, I utilized p.102-110 of the "All Block 3 Lectures in One PDF" lecture for this section of my program */
    struct sigaction SIGINT_action = {0}, SIGSTOP_action = {0};                             /* Initialize structs to be empty */

    SIGINT_action.sa_handler = catchSIGINT;                                                 /* Set to catchSIGINT function, which is called when this signal is received */
    sigfillset(&SIGINT_action.sa_mask);                                                     /* Block or delay all signals arriving while this mask is in place */
    SIGINT_action.sa_flags = SA_RESTART;                                                    /* Tell the system call to automatically restart */                                       

    SIGSTOP_action.sa_handler = catchSIGSTP;                                                /* Set to catchSIGSTP function, which is called when this signal is received */                   
    sigfillset(&SIGSTOP_action.sa_mask);                                                    /* Block or delay all signals arriving while this mask is in place */
    SIGSTOP_action.sa_flags = SA_RESTART;                                                   /* Tell the system call to automatically restart */ 

    sigaction(SIGINT, &SIGINT_action, NULL);                                                /* Register SIGINT_action to do what is listed above in the sigaction SIGINT_action struct */                                         
    sigaction(SIGTSTP, &SIGSTOP_action, NULL);                                              /* Register SIGSTP_action to do what is listed above in the sigaction SIGSTP_action struct */ 


    while(stillRunning){                                                                    /* While loop will continue until the user types in "exit", which will cause the program to set stillRunning to 0 */
        memset(userStringPtr, '\0', sizeof(userStringPtr));                                 /* Clear out the array before using it */
        memset(userString, '\0', sizeof(userString));                                       /* Clear out the array before using it */
        inputFile = NULL;                                                                   /* Point the input file pointer to NULL */
        outputFile = NULL;                                                                  /* Point the input file pointer to NULL */
        backgroundProcess = 0;                                                              /* Reset backgroundProces to 0, indicating that the current command is not to be run in the background */

        printf(": ");                                                                       /* Print out ": " as a prompt for each command line */
        fflush(stdout);                                                                     /* Flush out the output buffer */
        fgets(userString, MAX_CHARS, stdin);                                                /* Get the data input by the user into userString */

        userString[strlen(userString) - 1] = '\0';                                          /* Remove the newline character from the string input by the user */

        if(userString[strlen(userString) - 1] == '$' && userString[strlen(userString) - 2] == '$'){     /* If statement to assess whether the last 2 characters in the user string are "$$"*/
            userString[strlen(userString) - 1] = '\0';                                                  /* Remove the current last "$" character from the string input by the user */
            userString[strlen(userString) - 1] = '\0';                                                  /* Remove the new current last "$" character from the string input by the user */

            int currentPID = getpid();                                                      /* Set currentPID to the current PID value */
            /* Please note, for the following section I utilized: https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c */
            int tempVal = -1;                                                                                 
            int tempLength = snprintf(NULL, 0, "%d", currentPID);                           /* Find the length of the current PID and set tempLength equal to that value */
            char* tempString = malloc(tempLength + 1);                                      /* Create an array of pointers named tempString and allocate a sufficient block of memory */
            snprintf(tempString, tempLength + 1, "%d", currentPID);                         /* Store the characters for currentPID in the array buffer tempString */

            strcat(userString, tempString);                                                 /* Concatenate the string entered by the user with tempString, which will result in the expansion of "$$" to the current PID */

            free(tempString);                                                               /* Free the block of memory allocated to tempString */
        }

        /* Please note, I utilized: https://www.geeksforgeeks.org/strtok-strtok_r-functions-c-examples/ */
        char* token = strtok(userString, " ");                                              /* Character pointer, which will point to userString. The variable currently points to the first word in the string before a space */
        
        i = 0;

        while(token != NULL){                                                               /* Keep progressing to the next string separated by a space while one of the delimiters is present in userString */
            if(!strcmp(token, "<")){                                                        /* Compare what token points to with "<". I used !strcmp because strcmp will return 0 if the two arguments match */
                token = strtok(NULL, " ");                                                  /* If the character is "<", move to the next position in the string after "< " */                                            
                inputFile = strdup(token);                                                  /* Point the input file pointer to the next string separated by a space within userString. strdup() returns a pointer */
            }                                                                               /* to a null-terminated byte string. Please note, I utilized: https://www.geeksforgeeks.org/strdup-strdndup-functions-c/ */
            else if(!strcmp(token, ">")){                                                   /* Compare what token points to with ">". I used !strcmp because strcmp will return 0 if the two arguments match */
                token = strtok(NULL, " ");                                                  /* If the character is ">", move to the next position in the string after "> " */  
                outputFile = strdup(token);                                                 /* Point the input file pointer to the next string separated by a space within userString. strdup() returns a pointer */
            }                                                                               /* to a null-terminated byte string. Please note, I utilized: https://www.geeksforgeeks.org/strdup-strdndup-functions-c/ */
            else if(!strcmp(token, "&")){                                                   /* Compare what token points to with "&". I used !strcmp because strcmp will return 0 if the two arguments match */   
                token = strtok(NULL, " ");                                                  /* If the character is "&", move to the next position in the string after "& " */ 
                if(token == NULL){                                                          /* If there are no more space-separated strings within userString after the "&" character, it is the last character input by the user*/
                    backgroundProcess = 1;                                                  /* Set backgroundProcess to 1 to indicate that the command is to be run in background */
                }
                alreadyProgressed = 1;                                                      /* Set alreadyProgressed to 1 to indicate that token already points to the next string */
            }
            else{
                userStringPtr[i] = strdup(token);                                           /* Point the array of pointers to strings to the string within userString pointed at by token. strdup() returns a pointer */
                i++;                                                                        /* to a null-terminated byte string. Please note, I utilized: https://www.geeksforgeeks.org/strdup-strdndup-functions-c/ */
            }

            if(!alreadyProgressed){                                                         /* Assess whether the token has already moved to the next position within the else if "&" statement */
                token = strtok(NULL, " ");                                                  /* Move to the next position in the string following the next " " */
            }
            alreadyProgressed = 0;                                                          /* Reset alreadyProgressed */
        }

        if(strncmp(userString, "\0", 1) == 0){                                              /* Check to see if the user entered a blank line. If so, continue through the while loop */
            ;
        }
        else if(strncmp(userString, "#", 1) == 0){                                          /* Check to see if the user entered a comment. If so, continue through the while loop */
            ;
        }
        else if(strncmp(userString, "exit", 4) == 0){                                       /* Compare the first 4 letters of the user input with "exit" to check if user wants to exit the program */
            stillRunning = 0;                                                               /* Set stillRunning to 0 to terminate the while loop */
        }
        else if(strncmp(userString, "cd", 2) == 0){                                         /* Compare the first 2 letters of the user input with "cd" to check if the user wants to change the directory */
            if(userStringPtr[1]){                                                           /* Assess whether the string following the "cd" command, which is pointed to by userStringPtr[1], is NULL or not */
                if(chdir(userStringPtr[1]) != 0){                                           /* chdir() returns 0 if successful. Pleas note, I utilized: https://www.geeksforgeeks.org/chdir-in-c-language-with-examples/ */
                    printf("Directory %s not found.\n", userStringPtr[1]);                  /* Inform user that the entered directory does not exist */
                    fflush(stdout);                                                         /* Flush out output buffer */
                }
            }
            else{                                                                           /* Otherwise, change to the home directory. Please note, I utilized: https://www.tutorialspoint.com/c_standard_library/c_function_getenv.htm */
                char* homePath = getenv("HOME");                                            /* getenv searches for environment string pointed to by "HOME" and return the associated value to the string, which homePath points to */
                chdir(homePath);                                                            /* Change to the home directory */
            }
        }
        else if(strncmp(userString, "status", 6) == 0){                                     /* Compare the first 6 letters of the user input with "status" to check if the user wants the status printed out */
            if(WIFEXITED(childExitMethod)){                                                 /* Check if it terminated normally. If so, proceed in the if statement */                                           
                int exitStatus = WEXITSTATUS(childExitMethod);                              /* Get the actual exit status as an int */
                printf("exit value %d\n", exitStatus);                                      /* Print out the exit status*/
                fflush(stdout);                                                             /* Flush out output buffer */
            }
            else{                                                                           /* Otherwise, it was terminated by a signal */
                int termSignal = WTERMSIG(childExitMethod);                                 /* Get the actual terminating signmal as an int */
                printf("terminated by signal %d\n", termSignal);                            /* Print out the signal that terminated the process */
                fflush(stdout);                                                             /* Flush out output buffer */
            }
        }
        else{
            spawnPid = fork();                                                              /* Fork the process */
            switch(spawnPid){                                                               /* Switch statement to assess spawnPid */
                case -1: {                                                                  /* If something goes wrong, fork() returns -1 */                                                    
                    printf("Hull Breach!\n");                                               /* Inform the user that an error occurred */
                    fflush(stdout);                                                         /* Flush out output buffer */
                    exit(1);                                                                /* Set the exit status to 1 */
                    break;                                                                  /* Break out of the switch statement */
                }
                case 0: {                                                                   /* In the child process, fork() returns 0 */
                    SIGINT_action.sa_handler = SIG_DFL;                                     /* SIG_DFL tells SIGNINT_action to take the default action for the signal */
                    sigaction(SIGINT, &SIGINT_action, NULL);                                /* Register SIGINT_action to do what is now listed in the sigaction SIGINT_action struct */  

                    if(inputFile != NULL){                                                  /* Assess whether the inputFile pointer points to a string of characters */
                        int targetFID = open(inputFile, O_RDONLY);                          /* Open a file with the name pointed to by inputFile for reading. The open() function will return -1 if it was unsuccessful */

                        if(targetFID == -1){                                                /* Assess if a file was unable to be opened for reading with the name pointed to by inputFile */
                            printf("cannot open %s for input\n", inputFile);                /* Inform the user that the file could not be opened */
                            fflush(stdout);                                                 /* Flush out output buffer */
                            exit(1);                                                        /* Set the exit status to 1 */
                        }

                        int resultIn = dup2(targetFID, 0);                                  /* Call dup2() to change stdin to point to where targetFID points. The dup2() function will return -1 if it was unsuccessful */                          

                        if(resultIn == -1){                                                 /* Assess if dup2() was unsuccessful in redirecting stdin */
                            printf("cannot redirect stdin");                                /* Inform the user that stdin could not be redirected */
                            fflush(stdout);                                                 /* Flush out output buffer */
                            exit(1);                                                        /* Set exit status to 1 */
                        }
                        close(targetFID);                                                   /* Close file */
                    }

                    if(outputFile != NULL){                                                 /* Assess whether the outputFile pointer points to a string of characters */
                        int targetFOD = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0664);   /* Open a file with the name pointed to by outputFilefor writing. It will be truncated if it already exists or created if it doesn't */

                        if(targetFOD == -1){                                                /* Assess if a file was unable to be opened for writing with the name pointed to by outputFile */
                            printf("cannot open %s for output\n", outputFile);              /* Inform the user that the file could not be opened */
                            fflush(stdout);                                                 /* Flush out output buffer */
                            exit(1);                                                        /* Set exit status to 1 */
                        }
                        int resultOut = dup2(targetFOD, 1);                                 /* Call dup2() to change stdout to point to where targetFOD points. The dup2() function will return -1 if it was unsuccessful */ 

                        if(resultOut == -1){                                                /* Assess if dup2() was unsuccessful in redirecting stdout */
                            printf("cannot redirect stdout");                               /* Inform the user that stdout could not be redirected */
                            fflush(stdout);                                                 /* Flush out output buffer */
                            exit(1);                                                        /* Set exit status to 1 */
                        }
                        close(targetFOD);                                                   /* Close file */
                    }

                    if(execvp(userStringPtr[0], userStringPtr) < 0){                        /* First parameter is the pathname of the new program, which is the first argument of the array. Second parameter is an array of pointers to strings */
                        printf("%s: no such file or directory\n", userStringPtr[0]);        /* Inform the user that an error occurred */
                        fflush(stdout);                                                     /* Flush out output buffer */
                        exit(1);                                                            /* Set exit status to 1 */
                    }
                    break;                                                                  /* Break out of the switch statement */
                }
                default: {                                                                  /* In the parent process, fork() returns the PID of the child process that was just created */
                    if(backgroundProcess && !noBackground){                                 /* If backgroundProcess = 1 and noBackground = 0, the command can be run in background*/
                        printf("background pid is %d\n", spawnPid);                         /* Inform the user of the background PID that is running */
                        fflush(stdout);                                                     /* Flush out output buffer */
                    }
                    else{                                                                   /* Otherwise, the command cannot be run in the background */
                        waitpid(spawnPid, &childExitMethod, 0);                             /* Block the parent until the specific child process terminates */
                    }
                    break;                                                                  /* Break out of the switch statement */ 
                }
            }   
        }

        spawnPid = waitpid(-1, &childExitMethod, WNOHANG);                                  /* Check if any process has completed. This will return with 0 if none have */

        while(spawnPid > 0){                                                                /* If a background process has completed, print out a message stating which PID process completed as well as the exit value or what signal terminated it */
            printf("background pid %d is done: ", spawnPid);
            fflush(stdout);                                                                 /* Flush out output buffer */

            if(WIFEXITED(childExitMethod)){                                                 /* Check if it terminated normally. If so, proceed in the if statement */                                              
                int exitStatus = WEXITSTATUS(childExitMethod);                              /* Get the actual exit status as an int */
                printf("exit value %d\n", exitStatus);                                      /* Print out the exit status*/
                fflush(stdout);                                                             /* Flush out output buffer */
            }
            else{
                int termSignal = WTERMSIG(childExitMethod);                                 /* Otherwise, it was terminated by a signal */
                printf("terminated by signal %d\n", termSignal);                            /* Print out the exit status*/
                fflush(stdout);                                                             /* Flush out output buffer */
            }
                
            spawnPid= waitpid(-1, &childExitMethod, WNOHANG);                               /* Check if any process has completed. This will return with 0 if none have */                       

            if(spawnPid == -1){
                clearerr(stdin);
            }
        } 
    }

    return 0;
}