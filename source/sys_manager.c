/*
  Gonçalo Monteiro  2021217127
  Tiago Almeida     2021221615
*/

#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/prctl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "alerts_watcher.h"
#include "log.h"
#include "project_consts.h"
#include "semaphores.h"
#include "sys_manager_globals.h"
#include "worker.h"

int set_config(FILE *fp);
void sigint_handler(int sn);
void *dispatcher();
void *sensor_reader();
void *console_reader();

void clean_up();
void shutdown();

int main(int argc, char *argv[]) {
  struct sigaction act_sigint, act_ingore;
  FILE *config_fp;

  int i;

  /* Check if number of arguments is correct before executing */
  if (argc != 2) {
    puts("Wrong number of parameters");
    exit(1);
  }

  /* Set configs */
  if ((config_fp = fopen(argv[1], "r")) == NULL) {
    printf("Cannot open file\n");
    fclose(config_fp);
    exit(1);
  }
  if (set_config(config_fp) == 0) {
    puts("Invalid configs");
    fclose(config_fp);
    exit(1);
  }
  fclose(config_fp);

  /* Get main process PID */
  globals.sys_manager_pid = getpid();

  /* Redirect SIGINT */
  act_sigint.sa_handler = &sigint_handler;
  sigfillset(&act_sigint.sa_mask);
  act_sigint.sa_flags = 0;
  sigaction(SIGINT, &act_sigint, NULL);

  /* Ignore all other signals */
  act_ingore.sa_handler = SIG_IGN;
  sigfillset(&act_ingore.sa_mask);
  act_ingore.sa_flags = 0;
  for (i = 1; i < NSIG; i++) {
    if (i != SIGINT)
      sigaction(i, &act_ingore, NULL);
  }

  /* Open log file*/
  if ((globals.log_fp = fopen(LOG_FILE_NAME, "a")) == NULL) {
    perror("Error opening file");
    exit(1);
  }

  /* Create shared memory */
  globals.shm_size = sizeof(InfoStruct) + sizeof(Key) * globals.max_keys +
                     sizeof(Sensor) * globals.max_sensors +
                     sizeof(Alert) * globals.max_alerts +
                     sizeof(int) * globals.n_worker;
  globals.shmid =
      shmget(IPC_PRIVATE, globals.shm_size, IPC_CREAT | IPC_EXCL | 0700);
  if (globals.shmid < 1) {
    perror("Error creating shared memory");
    clean_up();
    exit(1);
  }

  /* Attach shared memory */
  globals.shm_info = (InfoStruct *)shmat(globals.shmid, NULL, 0);
  if (globals.shm_info < (InfoStruct *)1) {
    perror("Error attaching memory");
    clean_up();
    exit(1);
  }
  globals.shm_keys = (Key *)(globals.shm_info + 1);
  globals.shm_sensors = (Sensor *)(globals.shm_keys + globals.max_keys);
  globals.shm_alerts = (Alert *)(globals.shm_sensors + globals.max_sensors);
  globals.shm_workers = (int *)(globals.shm_alerts + globals.max_alerts);

  /* Set shared memory to zero */
  memset(globals.shm_info, 0, globals.shm_size);

  /* Create Semaphores */
  if (init_semaphores() == 0) {
    perror("Error creating semaphores");
    clean_up();
    exit(1);
  }

  /* Start simulator */
  write_log("HOME_IOT SIMULATOR STARTING");
  globals.prog_ending = 0;

  /* Create message queue */
  globals.msqkey = ftok("sys_manager.c", 'S');
  globals.msqid = msgget(globals.msqkey, IPC_CREAT | 0700);
  if (globals.msqid == -1) {
    perror("Error creating message queue");
    clean_up();
    exit(1);
  }

  /* Create Alerts Watcher process */
  write_log("STARTING ALERTS WATCHER");
  if ((globals.alerts_watcher_pid = fork()) == 0) {
    prctl(PR_SET_NAME, "alerts_watcher", 0, 0, 0);
    alerts_watcher();
    exit(0);
  }

  /* Create a pipe for each Worker process */
  globals.worker_fd = malloc(globals.n_worker * sizeof(int *));
  for (i = 0; i < globals.n_worker; i++) {
    globals.worker_fd[i] = malloc(2 * sizeof(int));
    if (pipe(globals.worker_fd[i]) < 0) {
      perror("Error opening unnamed pipe");
      clean_up();
      exit(1);
    }
  }

  /* Allocate space for worker PID array */
  globals.worker_pid = malloc(globals.n_worker * sizeof(pid_t *));

  /* Create Worker processes */
  write_log("STARTING WORKERS");
  for (i = 0; i < globals.n_worker; i++) {
    if ((globals.worker_pid[i] = fork()) == 0) {
      char name[32];
      sprintf(name, "worker_%d", i + 1);
      prctl(PR_SET_NAME, name, 0, 0, 0);
      worker(i + 1);
      exit(0);
    }
  }

  /* Initialize INTERNAL_QUEUE */
  init(&globals.internal_queue, globals.queue_sz);

  /* Initialize INTERNAL_QUEUE synchronization mechanisms */
  pthread_cond_init(&globals.queue_empty, NULL);
  pthread_cond_init(&globals.queue_full, NULL);
  pthread_mutex_init(&globals.queue_mutex, NULL);

  /* Create and open CONSOLE_PIPE and SENSOR_PIPE */
  unlink(CONSOLE_PIPE_NAME);
  if ((mkfifo(CONSOLE_PIPE_NAME, O_CREAT | O_EXCL | 0600) < 0)) {
    perror("Error creating console pipe");
    clean_up();
    exit(1);
  }
  unlink(SENSOR_PIPE_NAME);
  if ((mkfifo(SENSOR_PIPE_NAME, O_CREAT | O_EXCL | 0600) < 0)) {
    perror("Error creating sensor pipe");
    clean_up();
    exit(1);
  }

  globals.console_pipe_fd = open(CONSOLE_PIPE_NAME, O_RDWR);
  if (globals.console_pipe_fd < 0) {
    perror("Error opening console pipe for reading");
    clean_up();
    exit(1);
  }
  globals.sensor_pipe_fd = open(SENSOR_PIPE_NAME, O_RDWR);
  if (globals.sensor_pipe_fd < 0) {
    perror("Error opening sensor pipe for reading");
    clean_up();
    exit(1);
  }

  /* Create Dispatcher Thread */
  pthread_create(&globals.dispatcher_thread, NULL, dispatcher, NULL);
  write_log("THREAD DISPATCHER CREATED");

  /* Create Sensor Reader, Console Reader threads */
  pthread_create(&globals.sensor_reader_thread, NULL, sensor_reader, NULL);
  write_log("THREAD SENSOR_READER CREATED");
  pthread_create(&globals.console_reader_thread, NULL, console_reader, NULL);
  write_log("THREAD CONSOLE_READER CREATED");

  /* Wait for termination */
  sem_wait(globals.main_sem);
  shutdown();

  return 0;
}

/* Reads the Config file and sets the configs */
int set_config(FILE *fp) {
  /* File format
      Queue size
      N workers
      Max keys
      Max sensors
      Max alerts
  */

  /* Read values */
  if (fscanf(fp, "%d\n%d\n%d\n%d\n%d\n", &globals.queue_sz, &globals.n_worker,
             &globals.max_keys, &globals.max_sensors, &globals.max_alerts) != 5)
    return 0;

  /* Validate values */
  if (globals.queue_sz >= 1 && globals.n_worker >= 1 && globals.max_keys >= 1 &&
      globals.max_sensors >= 1 && globals.max_alerts >= 0) {
    return 1;
  } else {
    return 0;
  }
}

void clean_up() {
  int i;

  /* Close log file */
  fclose(globals.log_fp);

  /* Close semaphores */
  close_semaphores();

  /* Close mutex and condition variables */
  pthread_mutex_destroy(&globals.queue_mutex);
  pthread_cond_destroy(&globals.queue_full);
  pthread_cond_destroy(&globals.queue_empty);

  /* Delete shared memory */
  shmdt(globals.shm_info);
  shmctl(globals.shmid, IPC_RMID, NULL);

  /* Close named pipes */
  close(globals.console_pipe_fd);
  close(globals.sensor_pipe_fd);
  unlink(CONSOLE_PIPE_NAME);
  unlink(SENSOR_PIPE_NAME);

  /* Close worker pipes */
  for (i = 0; i < globals.n_worker; i++) {
    close(globals.worker_fd[i][0]);
    close(globals.worker_fd[i][1]);
    free(globals.worker_fd[i]);
  }
  free(globals.worker_fd);

  /* Free memory allocated for workers PID array */
  free(globals.worker_pid);

  /* Close Message queue*/
  msgctl(globals.msqid, IPC_RMID, NULL);
}

/* TODO - Shuts down system, cleaning used resources */
void shutdown() {
  int i, tmp;
  char buffer[BUFFER_SIZE];
  /* Write to log file */

  /* Kill Dispatcher, Sensor Reader and  Console Reader threads */
  pthread_cancel(globals.dispatcher_thread);
  pthread_cancel(globals.console_reader_thread);
  pthread_cancel(globals.sensor_reader_thread);
  pthread_join(globals.dispatcher_thread, NULL);
  pthread_join(globals.console_reader_thread, NULL);
  pthread_join(globals.sensor_reader_thread, NULL);

  /* Send a signal to worker processes for them to stop working */
  for (i = 0; i < globals.n_worker; i++) {
    kill(globals.worker_pid[i], SIGUSR1);
  }

  /* Send a signal to alerts watcher process for it to stop */
  kill(globals.alerts_watcher_pid, SIGUSR2);

  /* Wait for Alerts Watcher to exit*/
  waitpid(globals.alerts_watcher_pid, NULL, 0);

  /* Wait for all worker processes to finish */
  write_log("HOME_IOT SIMULATOR WAITING FOR LAST TASKS TO FINISH");
  for (i = 0; i < globals.n_worker; i++) {
    waitpid(globals.worker_pid[i], NULL, 0);
  }

  /* Write unfinished tasks to log */
  write_log("UNFINISHED TASKS:");
  while (!is_empty(&globals.internal_queue)) {
    dequeue(&globals.internal_queue, buffer, &tmp);
    write_log("%s", buffer);
  }
  write_log("HOME_IOT SIMULATOR CLOSING\n");

  /* Clean up resources*/
  clean_up();

  /* Exit main process */
  exit(0);
}

void *dispatcher() {
  char data[BUFFER_SIZE - 2];
  int prio;
  char buffer[BUFFER_SIZE];
  int i;

  while (1) {
    /* Lock internal queue before reading */
    pthread_mutex_lock(&globals.queue_mutex);

    /* Wait for queue to not be empty */
    while (is_empty(&globals.internal_queue)) {
      pthread_cond_wait(&globals.queue_empty, &globals.queue_mutex);
    }

    /* Queue is not empty here */
    dequeue(&globals.internal_queue, data, &prio);

    /* Signal queue is not full */
    pthread_cond_signal(&globals.queue_full);

    /* Unlock mutex */
    pthread_mutex_unlock(&globals.queue_mutex);

    /* Data to send */
    sprintf(buffer, "%d#%s", prio, data);

    /* Wait for a worker to be available */
    sem_wait(globals.worker_available_sem);

    /* Check wich worker is free */
    for (i = 0; i < globals.n_worker; i++) {
      /* if worker "i" is free */
      if (globals.shm_workers[i]) {
        /* Set availability to 0 */
        globals.shm_workers[i] = 0;

        /* Send buffer to worker pipe */
        write(globals.worker_fd[i][1], buffer, strlen(buffer) + 1);
        break;
      }
    }
  }
  pthread_exit(NULL);
}

void *sensor_reader() {
  char buffer[BUFFER_SIZE];
  int n_read;

  while (1) {
    /* Read info from sensor pipe */
    n_read = read(globals.sensor_pipe_fd, buffer, BUFFER_SIZE);
    if (n_read > 0) {
      buffer[n_read] = '\0';
    }

    /* Lock access to internal queue */
    pthread_mutex_lock(&globals.queue_mutex);

    /* Check if internal queue is full */
    if (is_full(&globals.internal_queue)) {
      pthread_mutex_unlock(&globals.queue_mutex);           /* Free mutex */
      write_log("QUEUE IS FULL. DROPPED DATA: %s", buffer); /* Drop data */

    } else {
      /* Add info to internal queue */
      enqueue(&globals.internal_queue, buffer, 0); /* Add data to queue*/
      write_log("Sensor reader: Enqueued - 0#%s", buffer);
      pthread_cond_signal(&globals.queue_empty);  /* Signal queue not empty */
      pthread_mutex_unlock(&globals.queue_mutex); /* Free mutex */
    }
  }
  pthread_exit(NULL);
}

void *console_reader() {
  char buffer[BUFFER_SIZE];
  int n_read;

  while (1) {
    /* Read info from console pipe */
    n_read = read(globals.console_pipe_fd, buffer, BUFFER_SIZE);
    if (n_read > 0) {
      buffer[n_read] = '\0';
    }

    /* Lock access to internal queue */
    pthread_mutex_lock(&globals.queue_mutex);

    /* Wait for queue to not be full */
    while (is_full(&globals.internal_queue)) {
      pthread_cond_wait(&globals.queue_full, &globals.queue_mutex);
    }
    /* Queue is not full here */
    enqueue(&globals.internal_queue, buffer, 1);

    /* Signal queue is not empty */
    pthread_cond_signal(&globals.queue_empty);

    /* Unlock mutex */
    pthread_mutex_unlock(&globals.queue_mutex);

    write_log("Console reader: Enqueued - 1#%s", buffer);
  }
  pthread_exit(NULL);
}

void sigint_handler(int signum) {
  if (getpid() == globals.sys_manager_pid) {
    write_log("SIGNAL SIGINT RECEIVED");
    shutdown();
    exit(0);
  }
}