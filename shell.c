#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64
#define MAX_BG_PROCESS 64
#define MAX_FG_PROCESS 64

int background_proc[MAX_BG_PROCESS];
int foreground_proc[MAX_FG_PROCESS];
int interrupt;

/**
 * @fn tokenize
 * @param[in] line
 * @return tokens
 * @brief Split line into tokens
 */
char **tokenize(char *line) {
  char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
  char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
  int i, tokenIndex = 0, tokenNo = 0;

  for (i = 0; i < strlen(line); i++) {

    char readChar = line[i];

    if (readChar == ' ' || readChar == '\n' || readChar == '\t') {
      token[tokenIndex] = '\0';
      if (tokenIndex != 0) {
        tokens[tokenNo] = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
        strcpy(tokens[tokenNo++], token);
        tokenIndex = 0;
      }
    } else {
      token[tokenIndex++] = readChar;
    }
  }

  free(token);
  tokens[tokenNo] = NULL;
  return tokens;
}

/**
 * @fn background
 * @param[in] tokens
 * @brief Run the command by forking and calling executable (except cd).
 *        Don't wait for it to end (run as background process)
 */
void background(char **tokens) {
  int i;

  // Check availability of background process and set i accordingly
  for (i = 0; i < MAX_BG_PROCESS; i++) {
    if (background_proc[i] == -1) {
      break;
    }
  }
  if (i == MAX_BG_PROCESS) {
    printf("Shell: Can't handle more background processes\n");
    return;
  }

  // Fork to run the the command
  int ret = fork();
  if (ret < 0) {
    printf("Shell: Error while calling fork\n");
  } else if (ret == 0) {
    // Child process
    // Assign a different process group id
    setpgid(0, 0);

    if (tokens[0] == NULL) {
      // Nothing to do
    } else if (!strcmp(tokens[0], "cd")) {
      // Special case cd
      // Doesn't make sense to run cd in background
      // Even then, check for format of argument
      if (tokens[1] == NULL || tokens[2] != NULL)
        printf("Shell: Incorrect command\n");
      else {
        int l = chdir(tokens[1]);
        if (l == -1) {
          printf("Shell: Directory not found\n");
        }
      }
    } else {
      // Load and run the executable
      int p = execvp(tokens[0], tokens);
      if (p == -1) {
        printf("Shell: Incorrect command\n");
      }
    }
    exit(0);
  } else { // ret > 0
    // Parent process with ret as Child PID
    background_proc[i] = ret;
  }
}

/**
 * @fn normal
 * @param[in] tokens
 * @brief Run the command by forking and calling executable (except cd).
 *        Wait for it to end (run as foreground process)
 */
void normal(char **tokens) {
  if (tokens[0] == NULL) {
    // Nothing to do
  } else if (!strcmp(tokens[0], "cd")) {
    // Special case cd
    // Check for format of argument
    if (tokens[1] == NULL || tokens[2] != NULL)
      printf("Shell: Incorrect command\n");
    else {
      int l = chdir(tokens[1]);
      if (l == -1) {
        printf("Shell: Directory not found\n");
      }
    }
  } else {
    // Fork to run the the command
    int ret = fork();
    if (ret < 0) {
      printf("Shell: Error while calling fork\n");
    } else if (ret == 0) {
      // Child process
      // Load and run the executable
      int p = execvp(tokens[0], tokens);
      if (p == -1) {
        printf("Shell: Incorrect command\n");
      }
      exit(0);
    } else { // ret > 0
      // Parent process with ret as Child PID
      // Wait for the child process to terminate then reap it
      int k = waitpid(ret, NULL, 0);
      if (k == -1) {
        printf("Shell: Error while calling waitpid\n");
      }
    }
  }
}

/**
 * @fn run
 * @param[in] tokens
 * @brief Detect background process and accordingly run the command
 */
void run(char **tokens) {
  int i;
  int bg = 0;

  // Don't run if interrupt is set to 1
  if (interrupt == 1) {
    return;
  }

  // Check if it is background (ends with "&") or not
  for (i = 0; tokens[i] != NULL; ++i) {
    if (!strcmp(tokens[i], "&") && tokens[i + 1] == NULL) {
      tokens[i] = NULL;
      bg = 1;
      break;
    }
  }

  // Separately run background process
  if (bg) {
    background(tokens);
  } else {
    normal(tokens);
  }
}

/**
 * @fn series
 * @param[in] tokens
 * @brief Splits the token into components based on "&&".
 *        Run them one after another in foreground
 */
void series(char **tokens) {
  char **ptokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
  int i, j;

  // Split into components
  j = 0;
  for (i = 0; tokens[i] != NULL; ++i) {
    if (strcmp(tokens[i], "&&")) {
      ptokens[j] = tokens[i];
      ++j;
    } else {
      // If encountered a "&&", segment till now is run and waited for
      ptokens[j] = NULL;
      run(ptokens);
      j = 0;
    }
  }
  // Same for last segment
  ptokens[j] = NULL;
  run(ptokens);

  // Free the allocated memory
  free(ptokens);
}

/**
 * @fn parallel
 * @param[in] tokens
 * @brief Run the command on a new shell as a foreground process
 */
void parallel(char **tokens) {
  int i;

  // Check availability of foreground process and set i accordingly
  for (i = 0; i < MAX_FG_PROCESS; i++) {
    if (foreground_proc[i] == -1) {
      break;
    }
  }
  if (i == MAX_FG_PROCESS) {
    printf("Shell: Can't handle more foreground processes\n");
    return;
  }

  // Fork another shell to run the command
  int ret = fork();
  if (ret < 0) {
    printf("Shell: Error while calling fork\n");
  } else if (ret == 0) {
    // Child process
    series(tokens);
    exit(0);
  } else { // ret > 0
    // Parent process with ret as Child PID
    foreground_proc[i] = ret;
  }
}

/**
 * @fn work
 * @param[in] tokens
 * @brief Splits the token into components based on "&&&".
 *        Run them parallelly in foreground
 */
void work(char **tokens) {
  char **ptokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
  int i, j;

  // Split into components
  j = 0;
  for (i = 0; tokens[i] != NULL; ++i) {
    if (strcmp(tokens[i], "&&&")) {
      ptokens[j] = tokens[i];
      ++j;
    } else {
      // If encountered a "&&&", segment till now is run on a new shell
      ptokens[j] = NULL;
      parallel(ptokens);
      j = 0;
    }
  }
  // Last segment is run on the current shell itself
  ptokens[j] = NULL;
  series(ptokens);

  // Wait for all foreground processes to end
  for (i = 0; i < MAX_FG_PROCESS; ++i) {
    if (foreground_proc[i] > 0) {
      int k = waitpid(foreground_proc[i], NULL, 0);
      if (k == -1) {
        printf("Shell: Error while calling waitpid\n");
      } else if (k == foreground_proc[i]) {
        foreground_proc[i] = -1;
      }
    }
  }

  // Free the allocated memory
  free(ptokens);
}

/**
 * @fn handle_sig
 * @param[in] sig
 * @brief SIGINT handler
 *        Does nothing, just sets interrupt to 1 (no new command will run)
 */
void handle_sig(int sig) {
  printf("\n");
  interrupt = 1;
  // SIGINT passed to children in same process group as well
}

int main(int argc, char *argv[]) {
  char line[MAX_INPUT_SIZE];
  char **tokens;
  int i;

  // SIGINT handler added
  signal(SIGINT, handle_sig);

  // Initialize list of background and foreground processes
  for (i = 0; i < MAX_BG_PROCESS; ++i) {
    background_proc[i] = -1;
  }
  for (i = 0; i < MAX_FG_PROCESS; ++i) {
    foreground_proc[i] = -1;
  }

  while (1) {
    // Scan the line
    bzero(line, sizeof(line));
    printf("$ ");
    scanf("%[^\n]", line);
    getchar();

    // Reap background child processes which have ended
    for (i = 0; i < MAX_BG_PROCESS; i++) {
      if (background_proc[i] > 0) {
        int k = waitpid(background_proc[i], NULL, WNOHANG);
        if (k == -1) {
          printf("Shell: Error while calling waitpid\n");
        } else if (k == background_proc[i]) {
          printf("Shell: Background process finished\n");
          background_proc[i] = -1;
        }
      }
    }

    // Break the line into tokens
    line[strlen(line)] = '\n';
    tokens = tokenize(line);

    if (tokens[0] == NULL) {
      // Nothing to do
    } else if (!strcmp(tokens[0], "exit")) {
      if (tokens[1] == NULL) {
        // Kill background processes before exit
        for (i = 0; i < MAX_BG_PROCESS; i++) {
          if (background_proc[i] > -1) {
            kill(background_proc[i], SIGKILL);
            background_proc[i] = -1;
          }
        }

        // Free the allocated memory
        for (i = 0; tokens[i] != NULL; i++) {
          free(tokens[i]);
        }
        free(tokens);

        // Just exit
        return 0;
      } else {
        printf("Shell: exit command doesn't have any arguments\n");
      }
    } else {
      // Set interrupt to 0 (new command will run)
      interrupt = 0;
      // Work on the tokens
      work(tokens);
    }

    // Free the allocated memory
    for (i = 0; tokens[i] != NULL; i++) {
      free(tokens[i]);
    }
    free(tokens);
  }

  return 0;
}
