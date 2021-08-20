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

int background_proc[MAX_BG_PROCESS];

/* Splits the string by space and returns the array of tokens
 *
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

void bg(char *line) {
  char **tokens = tokenize(line);
  int ret = fork();
  if (ret < 0) {
    printf("Shell: Error while calling fork\n");
  } else if (ret == 0) { // Child process
    setpgid(0, 0);
    int p = execvp(tokens[0], tokens);
    if (p == -1) {
      printf("Shell: Incorrect command\n");
      exit(0);
    }
  } else { // ret > 0, Parent process with ret as Child PID
    for (int i = 0; i < 64; i++) {
      if (background_proc[i] == -1) {
        background_proc[i] = ret;
        break;
      }
    }
  }
}

void work(char line[]) {
  char **tokens = tokenize(line);
  int i;

  if (tokens[0] == NULL) {
    printf("Nothing to do\n");
  } else if (!strcmp(tokens[0], "cd")) {
    // TODO: Add "cd -" command for fun
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
        exit(0);
      }
    } else { // ret > 0, Parent process with ret as Child PID
      int k = waitpid(ret, NULL, 0);
      if (k == -1) {
        printf("Shell: Error while calling waitpid\n");
      }
    }
  }

  for (i = 0; tokens[i] != NULL; i++) {
    printf("found token %s (remove this debug output later)\n", tokens[i]);
  }

  // Freeing the allocated memory
  for (i = 0; tokens[i] != NULL; i++) {
    free(tokens[i]);
  }
  free(tokens);
}

int main(int argc, char *argv[]) {
  char line[MAX_INPUT_SIZE];
  char **tokens;

  while (1) {
    /* BEGIN: TAKING INPUT */
    bzero(line, sizeof(line));
    printf("$ ");
    scanf("%[^\n]", line);
    getchar();

    printf("Command entered: %s (remove this debug output later)\n", line);
    /* END: TAKING INPUT */

    line[strlen(line)] = '\n'; // terminate with new line

    // do whatever you want with the commands, here we just print them
    work(line);
  }

  return 0;
}
