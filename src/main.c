#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* =======================
   HISTORY STORAGE
   This part stores previous commands
======================= */
#define HISTORY_SIZE 100

char *history[HISTORY_SIZE];
int history_count = 0;

/* Used for ENV command */
extern char **environ;

/*
  Function Declarations for builtin shell commands
*/
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);

/* =======================
   Added Builtin Commands
   pwd   -> print current directory
   echo  -> print text
   history -> print previous commands
   env   -> print environment variables
======================= */
int lsh_pwd(char **args);
int lsh_echo(char **args);
int lsh_history(char **args);
int lsh_env(char **args);

/*
  List of builtin commands
*/
char *builtin_str[] = {
  "cd",
  "help",
  "exit",

  /* Added commands */
  "pwd",
  "echo",
  "history",
  "env"
};

int (*builtin_func[]) (char **) = {
  &lsh_cd,
  &lsh_help,
  &lsh_exit,

  /* Functions for added commands */
  &lsh_pwd,
  &lsh_echo,
  &lsh_history,
  &lsh_env
};

int lsh_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}

/* =======================
   BUILTIN: CD
   Change current directory
======================= */
int lsh_cd(char **args)
{
  if (args[1] == NULL) {
    fprintf(stderr, "lsh: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("lsh");
    }
  }

  return 1;
}

/* =======================
   BUILTIN: HELP
   Print all builtin commands
======================= */
int lsh_help(char **args)
{
  int i;

  printf("LSH Shell\n");
  printf("Builtin commands:\n");

  for (i = 0; i < lsh_num_builtins(); i++) {
    printf("  %s\n", builtin_str[i]);
  }

  return 1;
}

/* =======================
   BUILTIN: EXIT
   Exit from shell
======================= */
int lsh_exit(char **args)
{
  return 0;
}

/* =======================
   BUILTIN: PWD
   Print current working directory
   Uses getcwd()
======================= */
int lsh_pwd(char **args)
{
  char cwd[1024];

  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    printf("%s\n", cwd);
  } else {
    perror("pwd");
  }

  return 1;
}

/* =======================
   BUILTIN: ECHO
   Print text entered by user
======================= */
int lsh_echo(char **args)
{
  int i = 1;

  while (args[i] != NULL) {
    printf("%s ", args[i]);
    i++;
  }

  printf("\n");

  return 1;
}

/* =======================
   BUILTIN: HISTORY
   Print previous commands
======================= */
int lsh_history(char **args)
{
  int i;

  for (i = 0; i < history_count; i++) {
    printf("%d %s\n", i + 1, history[i]);
  }

  return 1;
}

/* =======================
   BUILTIN: ENV
   Print environment variables
======================= */
int lsh_env(char **args)
{
  int i = 0;

  while (environ[i] != NULL) {
    printf("%s\n", environ[i]);
    i++;
  }

  return 1;
}

/* =======================
   Launch external programs
   مثل ls و cat وغيرها
======================= */
int lsh_launch(char **args)
{
  pid_t pid;
  int status;

  pid = fork();

  if (pid == 0) {

    /* Child process */
    if (execvp(args[0], args) == -1) {
      perror("lsh");
    }

    exit(EXIT_FAILURE);

  } else if (pid < 0) {

    /* Fork error */
    perror("lsh");

  } else {

    /* Parent process waits */
    do {
      waitpid(pid, &status, WUNTRACED);
    }

    while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

/* =======================
   Execute commands
   Check builtin or external
======================= */
int lsh_execute(char **args)
{
  int i;

  if (args[0] == NULL) {
    return 1;
  }

  for (i = 0; i < lsh_num_builtins(); i++) {

    if (strcmp(args[0], builtin_str[i]) == 0) {
      return (*builtin_func[i])(args);
    }
  }

  return lsh_launch(args);
}

/* =======================
   Read input line
======================= */
char *lsh_read_line(void)
{
#define LSH_RL_BUFSIZE 1024

  int bufsize = LSH_RL_BUFSIZE;
  int position = 0;

  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {

    c = getchar();

    if (c == EOF) {
      exit(EXIT_SUCCESS);

    } else if (c == '\n') {

      buffer[position] = '\0';
      return buffer;

    } else {

      buffer[position] = c;
    }

    position++;

    if (position >= bufsize) {

      bufsize += LSH_RL_BUFSIZE;

      buffer = realloc(buffer, bufsize);

      if (!buffer) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"

/* =======================
   Split command into tokens
======================= */
char **lsh_split_line(char *line)
{
  int bufsize = LSH_TOK_BUFSIZE;
  int position = 0;

  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;

  if (!tokens) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, LSH_TOK_DELIM);

  while (token != NULL) {

    tokens[position] = token;
    position++;

    if (position >= bufsize) {

      bufsize += LSH_TOK_BUFSIZE;

      tokens = realloc(tokens, bufsize * sizeof(char*));

      if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, LSH_TOK_DELIM);
  }

  tokens[position] = NULL;

  return tokens;
}

/* =======================
   Main shell loop
======================= */
void lsh_loop(void)
{
  char *line;
  char **args;
  int status;

  do {

    printf("> ");

    line = lsh_read_line();

    /* Save commands in history */
    if (strlen(line) > 0) {

      if (history_count < HISTORY_SIZE) {
        history[history_count++] = strdup(line);
      }
    }

    args = lsh_split_line(line);

    status = lsh_execute(args);

    free(line);
    free(args);

  } while (status);
}

/* =======================
   Main function
======================= */
int main(int argc, char **argv)
{
  lsh_loop();

  return EXIT_SUCCESS;
}
