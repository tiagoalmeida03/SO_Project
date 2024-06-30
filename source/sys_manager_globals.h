/*
  Gonçalo Monteiro  2021217127
  Tiago Almeida     2021221615
*/

#ifndef SYS_MANAGER_GLOBALS_H
#define SYS_MANAGER_GLOBALS_H

#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#include "project_consts.h"
#include "queue.h"

#define LOG_FILE_NAME "log.txt"

/* Struct that holds the info of a key. Will be used in shared memory */
typedef struct {
  char key[KEY_MAX_LENGTH + 1];
  int last_val, min_val, max_val;
  double avg;
  int update_count;
} Key;

/* Struct that holds info of a sensor. Will be used in shared memory */
typedef struct {
  char id[ID_MAX_LENGTH + 1];
} Sensor;

/* Struct that holds an rule */
typedef struct {
  char alert_id[ID_MAX_LENGTH + 1];
  int console_id;
  Key *key;
  int min_val, max_val;
} Alert;

/* Struct that holds info about the shared memory segment */
typedef struct {
  int n_keys;
  int n_sensors;
  int n_alerts;
} InfoStruct;

/* Global variables */
typedef struct {
  /* Configs */
  int queue_sz, n_worker, max_keys, max_sensors, max_alerts;

  /* Shared Memory */
  int shmid;
  InfoStruct *shm_info; /* Pointer to information about shared memory state */
  Key *shm_keys;        /* Pointer to where the info about keys starts */
  Sensor *shm_sensors;  /* Pointer to where the info about sensors starts */
  Alert *shm_alerts;    /* Pointer to where alert rules start */
  int *shm_workers;     /* Pointer to the array with each workers' state */
  size_t shm_size;      /* Size of shared memory */

  /* Synchronization Mechanisms */
  sem_t *log_sem;         /* Semaphore o control access to log file */
  sem_t *main_sem;        /* Semaphore to control main exit */
  sem_t *shm_keys_sem;    /* Control access to keys segment of shared memory */
  sem_t *shm_sensors_sem; /* Control access to sensors info in shared memory */
  sem_t *shm_alerts_sem;  /* Control access to alert rules in shared memory */
  sem_t *shm_watch_alerts_sem; /* Signal alerts watcher changes were made */
  sem_t *worker_available_sem; /* Control if workers are available */

  pthread_cond_t queue_full,
      queue_empty;             /* Condition variables to control queue space */
  pthread_mutex_t queue_mutex; /* Mutex for internal queue access */

  /* Pipes */
  int **worker_fd; /* Array of worker pipes. Needs memory allocation */
  int console_pipe_fd,
      sensor_pipe_fd; /* Names pipes for user console and sensors */

  /* Internal Queue */
  PrioQueue internal_queue;

  /* Log file */
  FILE *log_fp;

  /* Message Queue */
  int msqid;
  key_t msqkey;

  /* System Manager */
  pthread_t dispatcher_thread, sensor_reader_thread, console_reader_thread;
  pid_t sys_manager_pid;    /* Stores Main PID */
  pid_t *worker_pid;        /* Stores Workers PID. Needs allocation */
  pid_t alerts_watcher_pid; /* Stores Alerts Watcher PID */
  int prog_ending;          /* 1 if program is ending */

} SystemManagerGlobals;

extern SystemManagerGlobals globals;

#endif // SYS_MANAGER_GLOBALS_H