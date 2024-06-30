/*
  Gonçalo Monteiro  2021217127
  Tiago Almeida     2021221615
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "project_consts.h"
#include "sys_manager_globals.h"

void write_log(char *format, ...) {
  va_list args;
  va_start(args, format);
  time_t t = time(NULL);
  struct tm date = *localtime(&t);
  char log_message[BUFFER_SIZE];
  char log[BUFFER_SIZE + 10];
  int len;

  /* Create log */
  vsprintf(log_message, format, args);
  len = sprintf(log, "%02d:%02d:%02d %s\n", date.tm_hour, date.tm_min,
                date.tm_sec, log_message);

  /* Wait for log semaphore */
  sem_wait(globals.log_sem);

  /* Write to log file */
  fwrite(log, sizeof(char), len, globals.log_fp);
  fflush(globals.log_fp);

  /* Free log semaphore */
  sem_post(globals.log_sem);

  /* Print to the screen */
  write(STDOUT_FILENO, log, len);
  fflush(stdout);
}
