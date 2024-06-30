/*
  Gonçalo Monteiro  2021217127
  Tiago Almeida     2021221615
*/

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

#include "project_consts.h"

int console_id;

int pipe_fd;
char buffer[BUFFER_SIZE];
int msqid;
key_t msqkey;
pthread_t message_printer_thread;

int end;

void sigint_handler(int sn);
void sigpipe_handler(int sn);
void clean_up();
int process_command(char *line);
int remove_alert_command(char *line);
int add_alert_command(char *line);
void *message_printer();

int main(int argc, char *argv[]) {
  struct sigaction act_sigint, act_sigpipe;

  /* Check if number of arguments is correct before executing */
  if (argc != 2) {
    puts("Use console <id>");
    exit(1);
  }

  /* Set console ID */
  if (sscanf(argv[1], "%d", &console_id) != 1 || console_id < 0) {
    puts("Invalid console ID");
    exit(1);
  }

  /* Redirect SIGINT */
  act_sigint.sa_handler = &sigint_handler;
  sigfillset(&act_sigint.sa_mask);
  act_sigint.sa_flags = 0;

  if (sigaction(SIGINT, &act_sigint, NULL) == -1) {
    perror("Error redirecting SIGINT");
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

  /* Open message queue */
  msqkey = ftok("sys_manager.c", 'S');
  msqid = msgget(msqkey, 0);
  if (msqid == -1) {
    perror("Error creating message queue");
    exit(1);
  }

  /* Start Message Printer thread */
  pthread_create(&message_printer_thread, NULL, message_printer, NULL);

  /* Open pipe for writing */
  if ((pipe_fd = open(CONSOLE_PIPE_NAME, O_WRONLY)) < 0) {
    perror("Error opening pipe");
    exit(1);
  }

  end = 0;
  /* Receive user input */
  while (!end) {
    if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
      /* Remove '\n' character */
      buffer[strcspn(buffer, "\n")] = 0;

      /* Process command. If command was sent, wait for response */
      if (process_command(buffer) == 1) {
        // process response from message queue
      } else {
        puts("Invalid command");
      }
    }
  }
  clean_up();
  exit(0);
}

/* Processes command and sends it to pipe. If command was sent returns 1 */
int process_command(char *line) {
  char cmd[15];

  if (sscanf(line, "%s ", cmd) != 1)
    return 0;

  if (strcmp(cmd, "exit") == 0) {
    puts("Exiting");
    end = 1;

  } else if (strcmp(cmd, "stats") == 0) {
    sprintf(buffer, "%d#stats", console_id);

  } else if (strcmp(cmd, "reset") == 0) {
    sprintf(buffer, "%d#reset", console_id);

  } else if (strcmp(cmd, "sensors") == 0) {
    sprintf(buffer, "%d#sensors", console_id);

  } else if (strcmp(cmd, "add_alert") == 0) {
    if (add_alert_command(line) == 0)
      return 0;

  } else if (strcmp(cmd, "remove_alert") == 0) {
    if (remove_alert_command(line) == 0)
      return 0;

  } else if (strcmp(cmd, "list_alerts") == 0) {
    sprintf(buffer, "%d#list_alerts", console_id);

  } else
    return 0;

  /* Send command to pipe */
  write(pipe_fd, buffer, strlen(buffer) + 1);

  /* Return sucessful write to pipe */
  return 1;
}

/* Copies add_alert command to buffer. Returns 0 for error */
int add_alert_command(char *line) {
  char id[ID_MAX_LENGTH + 1], key[KEY_MAX_LENGTH + 1];
  int min, max;

  /* Check if number of parameters is correct */
  if (sscanf(line, "%*s %s %s %d %d ", id, key, &min, &max) != 4) {
    return 0;
  }

  /* Check if parameters are valid */
  if (!valid_id(id) || !valid_key(key) || min > max)
    return 0;

  /* Write to buffer*/
  sprintf(buffer, "%d#add_alert#%s#%s#%d#%d", console_id, id, key, min, max);

  return 1;
}

/* Copies remove_alert command to buffer. Returns 0 for error */
int remove_alert_command(char *line) {
  char id[ID_MAX_LENGTH + 1];

  /* Check if parameters are correct */
  if (sscanf(line, "%*s %s", id) != 1 || !valid_id(id))
    return 0;

  /* Write to buffer*/
  sprintf(buffer, "%d#remove_alert#%s", console_id, id);

  return 1;
}

void *message_printer() {
  Message msg;

  while (msgrcv(msqid, &msg, sizeof(msg) - sizeof(long), console_id, 0) != -1) {
    printf("%s", msg.data);
    fflush(stdout);
  }
  pthread_exit(NULL);
}

void clean_up() {
  /* Close named pipe */
  close(pipe_fd);

  /* Cancel printer thread and join it */
  pthread_cancel(message_printer_thread);
  pthread_join(message_printer_thread, NULL);
}

void sigint_handler(int signum) {
  clean_up();
  exit(0);
}

void sigpipe_handler(int signum) {
  clean_up();
  exit(0);
}
