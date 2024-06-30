/*
  Gonçalo Monteiro  2021217127
  Tiago Almeida     2021221615
*/

#include <fcntl.h>

#include "project_consts.h"
#include "sys_manager_globals.h"

#define LOG_SEM_NAME "LOG_SEM"
#define MAIN_SEM_NAME "MAIN_SEM"
#define SHM_KEYS_SEM_NAME "SHM_KEYS_SEM"
#define SHM_SENSORS_SEM_NAME "SHM_SENSORS_SEM"
#define SHM_ALERTS_SEM_NAME "SHM_ALERTS_SEM"
#define SHM_WORKERS_SEM_NAME "SHM_WORKER_SEM"
#define SHM_WATCH_ALERTS_SEM_NAME "SHM_WATCH_ALERTS_SEM"

int init_semaphores() {
  sem_unlink(LOG_SEM_NAME);
  globals.log_sem = sem_open(LOG_SEM_NAME, O_CREAT | O_EXCL, 0700, 1);
  if (globals.log_sem == SEM_FAILED) {
    return 0;
  }

  sem_unlink(MAIN_SEM_NAME);
  globals.main_sem = sem_open(MAIN_SEM_NAME, O_CREAT | O_EXCL, 0700, 0);
  if (globals.main_sem == SEM_FAILED) {
    return 0;
  }

  sem_unlink(SHM_KEYS_SEM_NAME);
  globals.shm_keys_sem = sem_open(SHM_KEYS_SEM_NAME, O_CREAT | O_EXCL, 0700, 1);
  if (globals.shm_keys_sem == SEM_FAILED) {
    return 0;
  }

  sem_unlink(SHM_SENSORS_SEM_NAME);
  globals.shm_sensors_sem =
      sem_open(SHM_SENSORS_SEM_NAME, O_CREAT | O_EXCL, 0700, 1);
  if (globals.shm_sensors_sem == SEM_FAILED) {
    return 0;
  }

  sem_unlink(SHM_ALERTS_SEM_NAME);
  globals.shm_alerts_sem =
      sem_open(SHM_ALERTS_SEM_NAME, O_CREAT | O_EXCL, 0700, 1);
  if (globals.shm_alerts_sem == SEM_FAILED) {
    return 0;
  }

  sem_unlink(SHM_WORKERS_SEM_NAME);
  globals.worker_available_sem =
      sem_open(SHM_WORKERS_SEM_NAME, O_CREAT | O_EXCL, 0700, 0);
  if (globals.worker_available_sem == SEM_FAILED) {
    return 0;
  }

  sem_unlink(SHM_WATCH_ALERTS_SEM_NAME);
  globals.shm_watch_alerts_sem =
      sem_open(SHM_WATCH_ALERTS_SEM_NAME, O_CREAT | O_EXCL, 0700, 0);
  if (globals.shm_watch_alerts_sem == SEM_FAILED) {
    return 0;
  }

  return 1;
}

void close_semaphores() {
  sem_close(globals.log_sem);
  sem_close(globals.main_sem);
  sem_close(globals.shm_keys_sem);
  sem_close(globals.shm_sensors_sem);
  sem_close(globals.shm_alerts_sem);
  sem_close(globals.worker_available_sem);
  sem_close(globals.shm_watch_alerts_sem);
  sem_unlink(LOG_SEM_NAME);
  sem_unlink(MAIN_SEM_NAME);
  sem_unlink(SHM_KEYS_SEM_NAME);
  sem_unlink(SHM_SENSORS_SEM_NAME);
  sem_unlink(SHM_ALERTS_SEM_NAME);
  sem_unlink(SHM_WORKERS_SEM_NAME);
  sem_unlink(SHM_WATCH_ALERTS_SEM_NAME);
}