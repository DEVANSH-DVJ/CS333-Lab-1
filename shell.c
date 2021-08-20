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

// Splits the string by space and returns the array of tokens
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

void background(char **tokens) {
  int i;

  for (i = 0; i < MAX_BG_PROCESS; i++) {
    if (background_proc[i] == -1) {
      break;
    }
  }
  if (i == MAX_BG_PROCESS) {
    printf("Shell: Can't handle more background processes\n");
    return;
  }
  int ret = fork();
  if (ret < 0) {
    printf("Shell: Error while calling fork\n");
  } else if (ret == 0) { // Child process
    setpgid(0, 0);
    if (tokens[0] == NULL) {
      printf("Shell: Nothing to do\n");
    } else if (!strcmp(tokens[0], "cd")) {
      printf("Shell: cd in background doesn't make sense\n");
    } else {
      int p = execvp(tokens[0], tokens);
      if (p == -1) {
        printf("Shell: Incorrect command\n");
      }
    }
    exit(0);
  } else { // ret > 0, Parent process with ret as Child PID
    background_proc[i] = ret;
  }
}

void normal(char **tokens) {
  if (tokens[0] == NULL) {
    printf("Shell: Nothing to do\n");
  } else if (!strcmp(tokens[0], "cd")) {
    // TODO: Add "cd -" compatibility for fun
    if (tokens[1] == NULL || tokens[2] != NULL)
      printf("Shell: Incorrect command\n");
    else {
      int l = chdir(tokens[1]);
      if (l == -1) {
        printf("Shell: Directory not found\n");
      }
    }
  } else {
    int ret = fork();
    if (ret < 0) {
      printf("Shell: Error while calling fork\n");
    } else if (ret == 0) { // Child process
      int p = execvp(tokens[0], tokens);
      if (p == -1) {
        printf("Shell: Incorrect command\n");
      }
      exit(0);
    } else { // ret > 0, Parent process with ret as Child PID
      int k = waitpid(ret, NULL, 0);
      if (k == -1) {
        printf("Shell: Error while calling waitpid\n");
      }
    }
  }
}

void run(char **tokens) {
  int i;
  int bg = 0;

  if (interrupt == 1) {
    printf("Cancelling: ");
    for (i = 0; tokens[i] != NULL; i++) {
      printf("%s ", tokens[i]);
    }
    printf("\n");
    return;
  }

  printf("Running: ");
  for (i = 0; tokens[i] != NULL; i++) {
    printf("%s ", tokens[i]);
  }
  printf("\n");

  for (i = 0; tokens[i] != NULL; ++i) {
    if (!strcmp(tokens[i], "&") && tokens[i + 1] == NULL) {
      tokens[i] = NULL;
      bg = 1;
      break;
    }
  }

  if (bg) {
    background(tokens);
  } else {
    normal(tokens);
  }
}

void series(char **tokens) {
  char **ptokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
  int i, j;

  j = 0;
  for (i = 0; tokens[i] != NULL; ++i) {
    if (strcmp(tokens[i], "&&")) {
      ptokens[j] = tokens[i];
      ++j;
    } else {
      ptokens[j] = NULL;
      run(ptokens);
      j = 0;
    }
  }
  ptokens[j] = NULL;
  run(ptokens);

  free(ptokens);
}

void parallel(char **tokens) {
  int i;

  for (i = 0; i < MAX_FG_PROCESS; i++) {
    if (foreground_proc[i] == -1) {
      break;
    }
  }
  if (i == MAX_FG_PROCESS) {
    printf("Shell: Can't handle more foreground processes\n");
    return;
  }
  int ret = fork();
  if (ret < 0) {
    printf("Shell: Error while calling fork\n");
  } else if (ret == 0) { // Child process
    // signal(SIGINT, SIG_DFL);
    series(tokens);
    exit(0);
  } else { // ret > 0, Parent process with ret as Child PID
    foreground_proc[i] = ret;
  }
}

void work(char **tokens) {
  char **ptokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
  int i, j;

  j = 0;
  for (i = 0; tokens[i] != NULL; ++i) {
    if (strcmp(tokens[i], "&&&")) {
      ptokens[j] = tokens[i];
      ++j;
    } else {
      ptokens[j] = NULL;
      parallel(ptokens);
      j = 0;
    }
  }
  ptokens[j] = NULL;
  series(ptokens);

  for (i = 0; i < MAX_FG_PROCESS; ++i) {
    if (foreground_proc[i] > 0) {
      int k = waitpid(foreground_proc[i], NULL, 0);
      if (k == -1) {
        printf("Shell: Error while calling waitpid\n");
      } else if (k == foreground_proc[i]) {
        printf("Shell: Foreground process [%i] reaped\n", k);
        foreground_proc[i] = -1;
      }
    }
  }

  free(ptokens);
}

void handle_sig(int sig) {
  printf("\n");
  interrupt = 1;
}

int main(int argc, char *argv[]) {
  char line[MAX_INPUT_SIZE];
  char **tokens;
  int i;

  signal(SIGINT, handle_sig);

  for (i = 0; i < MAX_BG_PROCESS; ++i) {
    background_proc[i] = -1;
  }
  for (i = 0; i < MAX_FG_PROCESS; ++i) {
    foreground_proc[i] = -1;
  }

  while (1) {
    for (i = 0; i < MAX_BG_PROCESS; i++) {
      if (background_proc[i] > 0) {
        int k = waitpid(background_proc[i], NULL, WNOHANG);
        if (k == -1) {
          printf("Shell: Error while calling waitpid\n");
        } else if (k == background_proc[i]) {
          printf("Shell: Background process [%i] reaped\n", k);
          background_proc[i] = -1;
        }
      }
    }

    bzero(line, sizeof(line));
    printf("$ ");
    scanf("%[^\n]", line);
    getchar();

    // printf("Command entered: %s (remove this debug output later)\n", line);

    line[strlen(line)] = '\n'; // terminate with new line
    tokens = tokenize(line);   // convert line to tokens
    if (tokens[0] == NULL) {
      printf("Shell: Nothing to do\n");
    } else if (!strcmp(tokens[0], "exit")) {
      for (i = 0; i < MAX_BG_PROCESS; i++) {
        if (background_proc[i] > -1) {
          kill(background_proc[i], SIGKILL);
          printf("Shell: Background process [%i] killed\n", background_proc[i]);
          background_proc[i] = -1;
        }
      }

      // Freeing the allocated memory
      for (i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);
      }
      free(tokens);

      return 0;
    } else {
      interrupt = 0;
      work(tokens); // work on the tokens
    }

    // for (i = 0; tokens[i] != NULL; i++) {
    //   printf("found token %s (remove this debug output later)\n", tokens[i]);
    // }

    // Freeing the allocated memory
    for (i = 0; tokens[i] != NULL; i++) {
      free(tokens[i]);
    }
    free(tokens);
  }

  return 0;
}
