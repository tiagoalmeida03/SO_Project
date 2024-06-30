/*
  Gonçalo Monteiro  2021217127
  Tiago Almeida     2021221615
*/

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "project_consts.h"

float delay;
int min_val, max_val;
char id[ID_MAX_LENGTH + 1], key[KEY_MAX_LENGTH + 1];

int pipe_fd;
char buffer[BUFFER_SIZE];

int num_sent;

void sigint_handler(int sn);
void sigtstp_handler(int sn);
void sigpipe_handler(int sn);
void clean_up();
int set_config(char *s[]);

int main(int argc, char *argv[]) {
  struct sigaction act_sigint, act_sigtstp, act_sigpipe;
  int n_wrote;
  int random_val;

  if (argc != 6) {
    puts("Use: sensor <id> <delay> <key> <min> <max>");
    exit(0);
  }

  /* Parse arguments */
  if (set_config(argv) == 0) {
    puts("Invalid parameters");
    exit(0);
  }

  /* Redirect SIGINT */
  act_sigint.sa_handler = &sigint_handler;
  sigfillset(&act_sigint.sa_mask);
  act_sigint.sa_flags = 0;
  if (sigaction(SIGINT, &act_sigint, NULL) == -1) {
    perror("Error redirecting SIGINT");
    exit(1);
  }

  /* Redirect SIGTSTP */
  act_sigtstp.sa_handler = &sigtstp_handler;
  sigfillset(&act_sigtstp.sa_mask);
  act_sigtstp.sa_flags = 0;
  if (sigaction(SIGTSTP, &act_sigtstp, NULL) == -1) {
    perror("Error redirecting SIGTSTP");
    exit(1);
  }

  /* Redirect SIGPIPE */
  act_sigpipe.sa_handler = &sigpipe_handler;
  sigfillset(&act_sigpipe.sa_mask);
  act_sigpipe.sa_flags = 0;
  if (sigaction(SIGPIPE, &act_sigpipe, NULL) == -1) {
    perror("Error redirecting SIGPIPE");
    exit(1);
  }

  /* Open pipe for reading */
  if ((pipe_fd = open(SENSOR_PIPE_NAME, O_WRONLY)) < 0) {
    perror("Error opening pipe");
    exit(1);
  }

  /* Generate random values and send them to pipe */
  srand(time(NULL));
  while (1) {
    /* Generate random value */
    random_val = rand() % (max_val - min_val + 1) + min_val;

    /* Generate output */
    sprintf(buffer, "%s#%s#%d", id, key, random_val);
#ifdef DEBUG
    printf("%s\n", buffer);
#endif

    /* Send output to pipe */
    n_wrote = write(pipe_fd, buffer, strlen(buffer) + 1);
    if (n_wrote != -1) {
      num_sent++;
    }

    usleep(delay * 1000000);
  }
}

int set_config(char *args[]) {
  /* args:
    {prog, id, delay, key, min, max}
  */

  /* Set ID if it is valid */
  if (!valid_id(args[1]))
    return 0;
  strcpy(id, args[1]);

  /* Set delay (always >= 0) */
  if (sscanf(args[2], "%f", &delay) != 1 || delay < 0)
    return 0;

  /* Set sensor key */
  if (!valid_key(args[3]))
    return 0;
  strcpy(key, args[3]);

  /* Set min and max (max >= min) */
  if (sscanf(args[4], "%d", &min_val) != 1 ||
      sscanf(args[5], "%d", &max_val) != 1 || min_val > max_val)
    return 0;

  /* Return 1 on success */
  return 1;
}

void sigint_handler(int signum) {
  clean_up();
  exit(0);
}
void sigtstp_handler(int signum) {
  printf("Already sent %d messages", num_sent);
}
void sigpipe_handler(int signum) {
  clean_up();
  exit(0);
}
void clean_up() {
  /* Close named pipe */
  close(pipe_fd);
}
