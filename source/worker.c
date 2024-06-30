/*
  Gonçalo Monteiro  2021217127
  Tiago Almeida     2021221615
*/

#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/msg.h>
#include <unistd.h>

#include "log.h"
#include "sys_manager_globals.h"

void sigusr1_handler(int sn);
void process_sensor_info(char *buf);
void update_key_info(char *key, int val);
void update_sensor_list(char *id);
void process_console_command(char *buffer);
void send_stats(int console_id);
void reset_stats(int console_id);
void send_sensors(int console_id);
void add_alert(int console_id, char *params);
void remove_alert(int console_id, char *params);
void list_alerts(int console_id);

int worker_i;
int stop_working;

void worker(int worker_num) {
  int n_read;
  char buffer[BUFFER_SIZE];
  struct sigaction act_sigusr1;

  /* Set worker index */
  worker_i = worker_num - 1;

  /* Handle signal that tells the worker to stop */
  act_sigusr1.sa_handler = &sigusr1_handler;
  sigfillset(&act_sigusr1.sa_mask);
  act_sigusr1.sa_flags = 0;
  sigaction(SIGUSR1, &act_sigusr1, NULL);

  /* Get ready to work */
  write_log("Worker %d READY", worker_num);
  globals.shm_workers[worker_i] = 1;
  sem_post(globals.worker_available_sem);

  while (!stop_working) {
    /* Receive data from pipe */
    n_read = read(globals.worker_fd[worker_i][0], buffer, BUFFER_SIZE);

    /* Process the data */
    if (n_read > 0) {
      buffer[n_read] = '\0';

      write_log("WORKER %d: RECEIVED COMMAND: %s", worker_num, buffer);

      if (buffer[0] == '0') {
        /* Info from sensor */
        process_sensor_info(buffer);

      } else if (buffer[0] == '1') {
        /* Command from user console */
        process_console_command(buffer);
      }

      /* Dont need to the rest if this is the last task */
      if (stop_working)
        break;

      /* Set worker state as available */
      globals.shm_workers[worker_i] = 1;

      /* Post semaphore (worker ready) */
      sem_post(globals.worker_available_sem);
      write_log("Worker %d READY", worker_num);
    }
  }

  exit(0);
}

void process_sensor_info(char *buf) {
  char id[ID_MAX_LENGTH + 1], key[KEY_MAX_LENGTH + 1];
  int val;

  /* Get values from buffer */
  if (sscanf(buf, "%*d#%[^#]#%[^#]#%d", id, key, &val) != 3) {
    /* Error reading */
    write_log("Worker %d: Wrong command \"%s\"", worker_i + 1, buf);
    return;
  }

  /* Update keys info in shared memory */
  update_key_info(key, val);

  /* Update list of sensors in shared memory */
  update_sensor_list(id);
}

void update_key_info(char *key, int val) {
  int is_registered = 0;
  int i;

  /* Block access to this segment of shared memory */
  sem_wait(globals.shm_keys_sem);

  /* Search for key. Update it if found */
  for (i = 0; i < globals.shm_info->n_keys; i++) {
    if (strcmp(globals.shm_keys[i].key, key) == 0) {
      /* Key already exists in the list */
      is_registered = 1;
      break;
    }
  }
  /* If key exists, update it*/
  if (is_registered) {
    globals.shm_keys[i].last_val = val;
    /* Update max value*/
    if (val > globals.shm_keys[i].max_val)
      globals.shm_keys[i].max_val = val;
    /* Update min value*/
    if (val < globals.shm_keys[i].min_val)
      globals.shm_keys[i].min_val = val;
    /* Update average and update count */
    globals.shm_keys[i].avg =
        (globals.shm_keys[i].avg * globals.shm_keys[i].update_count + val) /
        (globals.shm_keys[i].update_count + 1);
    globals.shm_keys[i].update_count++;
    /* Log */
    sem_post(globals.shm_keys_sem);
    sem_post(globals.shm_watch_alerts_sem);
    write_log("Worker %d: %s data processing completed", worker_i + 1, key);
  } else {
    /* Key is not registered */
    if (globals.shm_info->n_sensors < globals.max_sensors) {
      /* There is space in the list */
      Key key_info;
      strcpy(key_info.key, key);
      key_info.avg = val;
      key_info.last_val = val;
      key_info.max_val = val;
      key_info.min_val = val;
      key_info.update_count = 1;
      globals.shm_keys[globals.shm_info->n_keys++] = key_info;
      sem_post(globals.shm_keys_sem);
      write_log("Worker %d: %s data processing completed", worker_i + 1, key);
    } else {
      /* There is no space in the list */
      sem_post(globals.shm_keys_sem);
      write_log("Worker %d: List of keys is full. Could not add %s",
                worker_i + 1, key);
    }
  }
}

void update_sensor_list(char *id) {
  int is_registered = 0;
  int i;

  /* Block access to this segment of shared memory */
  sem_wait(globals.shm_sensors_sem);

  /* Check if sensor is registered */
  for (i = 0; i < globals.shm_info->n_sensors; i++) {
    if (strcmp(globals.shm_sensors[i].id, id) == 0) {
      /* Sensor is in the list */
      is_registered = 1;
      break;
    }
  }

  if (!is_registered) {
    /* Sensor is not registered */
    if (globals.shm_info->n_sensors < globals.max_sensors) {
      /* There is space on the list */
      Sensor sensor_info;
      strcpy(sensor_info.id, id);
      globals.shm_sensors[globals.shm_info->n_sensors++] = sensor_info;
      sem_post(globals.shm_sensors_sem);
      write_log("Worker %d: Registered sensor %s", worker_i + 1, id);
    } else {
      /* There is no space on the list */
      sem_post(globals.shm_sensors_sem);
      write_log("Worker %d: Could not register sensor %s", worker_i + 1, id);
    }
  } else {
    /* Sensor is already registered */
    sem_post(globals.shm_sensors_sem);
  }
}

/* 1#console_id#command#params */
void process_console_command(char *buffer) {
  int console_id;
  char command[CMD_MAX_LENGTH];
  char params[BUFFER_SIZE];

  sscanf(buffer, "%*d#%d#%[^#]#%s", &console_id, command, params);

  if (strcmp(command, "stats") == 0) {
    send_stats(console_id);
  } else if (strcmp(command, "reset") == 0) {
    reset_stats(console_id);
  } else if (strcmp(command, "sensors") == 0) {
    send_sensors(console_id);
  } else if (strcmp(command, "add_alert") == 0) {
    add_alert(console_id, params);
  } else if (strcmp(command, "remove_alert") == 0) {
    remove_alert(console_id, params);
  } else if (strcmp(command, "list_alerts") == 0) {
    list_alerts(console_id);
  } else {
    write_log("Worker %d: Wrong command %s", worker_i + 1, buffer);
  }
}

void send_stats(int console_id) {
  int i;
  int str_index;
  Message msg;

  str_index = sprintf(msg.data, "Key\tLast\tMin\tMax\tAvg\tCount\n");

  sem_wait(globals.shm_keys_sem);
  for (i = 0; i < globals.shm_info->n_keys; i++) {
    /* Write to string and increment last index */
    str_index +=
        sprintf((msg.data + str_index), "%s %d %d %d %.2f %d\n",
                globals.shm_keys[i].key, globals.shm_keys[i].last_val,
                globals.shm_keys[i].min_val, globals.shm_keys[i].max_val,
                globals.shm_keys[i].avg, globals.shm_keys[i].update_count);
  }
  sem_post(globals.shm_keys_sem);

  /* Set msg type to console id and send */
  msg.msgtype = console_id;
  msgsnd(globals.msqid, &msg, sizeof(msg) - sizeof(long), 0);
}

void reset_stats(int console_id) {
  Message msg;

  /* Lock access to shared memory */
  sem_wait(globals.shm_keys_sem);
  sem_wait(globals.shm_sensors_sem);
  sem_wait(globals.shm_alerts_sem);

  /* Set all statistics memory to zero */
  memset(globals.shm_info, 0,
         globals.shm_size - sizeof(int) * globals.n_worker);

  /* Send message to console */
  sprintf(msg.data, "OK\n");
  msg.msgtype = console_id;
  msgsnd(globals.msqid, &msg, sizeof(msg) - sizeof(long), 0);
  write_log("Worker %d: Reset all statistics", worker_i + 1);

  /* Unlock access*/
  sem_post(globals.shm_keys_sem);
  sem_post(globals.shm_sensors_sem);
  sem_post(globals.shm_alerts_sem);
}

void send_sensors(int console_id) {
  int i;
  int str_index;
  Message msg;

  str_index = sprintf(msg.data, "ID\n");

  sem_wait(globals.shm_sensors_sem);
  for (i = 0; i < globals.shm_info->n_sensors; i++) {
    /* Write to string and increment last index */
    str_index +=
        sprintf((msg.data + str_index), "%s\n", globals.shm_sensors[i].id);
  }
  sem_post(globals.shm_sensors_sem);

  /* Set msg type to console id and send */
  msg.msgtype = console_id;
  msgsnd(globals.msqid, &msg, sizeof(msg) - sizeof(long), 0);
}

void add_alert(int console_id, char *params) {
  char alert_id[ID_MAX_LENGTH + 1];
  char key[KEY_MAX_LENGTH + 1];
  int min_val, max_val;
  Message msg;
  int i, j, is_registered = 0, key_exists = 0;

  if (sscanf(params, "%[^#]#%[^#]#%d#%d", alert_id, key, &min_val, &max_val) !=
      4) {
    /* Wrong parameters */

    write_log("Worker %d: Wrong command add_alert#%s", worker_i + 1, params);
    return;
  }

  /* Set message type to console ID*/
  msg.msgtype = console_id;

  sem_wait(globals.shm_alerts_sem);

  /* Check if alert ID is already taken */
  for (i = 0; i < globals.shm_info->n_alerts; i++) {
    if (strcmp(globals.shm_alerts[i].alert_id, alert_id) == 0) {
      /* Alert ID already exists */
      is_registered = 1;
      break;
    }
  }

  /* Add alert to alert list */
  if (!is_registered) {
    /* Alert is not registered */
    if (globals.shm_info->n_alerts < globals.max_alerts) {
      /* There is space in the list */
      sem_wait(globals.shm_keys_sem);
      for (j = 0; j < globals.shm_info->n_keys; j++) {
        if (strcmp(globals.shm_keys[j].key, key) == 0) {
          key_exists = 1;
          break;
        }
      }
      sem_post(globals.shm_keys_sem);

      if (key_exists) {
        /* Key exists */
        Alert new_alert;
        strcpy(new_alert.alert_id, alert_id);
        new_alert.console_id = console_id;
        new_alert.min_val = min_val;
        new_alert.max_val = max_val;
        new_alert.key = &globals.shm_keys[j];
        globals.shm_alerts[i] = new_alert;
        globals.shm_info->n_alerts++;

        sem_post(globals.shm_alerts_sem);
        sem_post(globals.shm_watch_alerts_sem);
        sprintf(msg.data, "OK\n");
        msgsnd(globals.msqid, &msg, sizeof(msg) - sizeof(long), 0);
        write_log("Worker %d: Added alert %s.", worker_i + 1, alert_id);
      } else {
        /* Key does not exist */
        sem_post(globals.shm_alerts_sem);
        sprintf(msg.data, "Key %s is not registered\n", key);
        msgsnd(globals.msqid, &msg, sizeof(msg) - sizeof(long), 0);
        write_log("Worker %d: Could not add alert %s. Key does not exist",
                  worker_i + 1, alert_id);
      }

    } else {
      /* There is no space in the list */
      sem_post(globals.shm_alerts_sem);
      sprintf(msg.data, "Could not add alert %s. Max alerts reached.\n",
              alert_id);
      msgsnd(globals.msqid, &msg, sizeof(msg) - sizeof(long), 0);
      write_log("Worker %d: Could not add alert %s. Max alerts reached.",
                worker_i + 1, alert_id);
    }
  } else {
    /* Alert is already registered */
    sem_post(globals.shm_alerts_sem);
    sprintf(msg.data, "Alert ID %s already exists\n", alert_id);
    msgsnd(globals.msqid, &msg, sizeof(msg) - sizeof(long), 0);
    write_log("Worker %d: Alert ID %s already exists", worker_i + 1, alert_id);
  }
}
void remove_alert(int console_id, char *params) {
  char alert_id[ID_MAX_LENGTH + 1];
  Message msg;
  int i, j, is_registered = 0;

  if (sscanf(params, "%s", alert_id) != 1) {
    /* Wrong parameters */
    write_log("Worker %d: Wrong command remove_alert#%s", worker_i + 1, params);
    return;
  }

  /* Set msg type to console id */
  msg.msgtype = console_id;

  sem_wait(globals.shm_alerts_sem);

  /* Check if alert exists */
  for (i = 0; i < globals.shm_info->n_alerts; i++) {
    if (strcmp(globals.shm_alerts[i].alert_id, alert_id) == 0) {
      is_registered = 1;
      break;
    }
  }

  /* Remove alert from alert list */
  if (is_registered) {
    /* Alert exists. */
    for (j = i; j < globals.shm_info->n_alerts; j++) {
      /* Shift all next alerts to the left */
      globals.shm_alerts[j] = globals.shm_alerts[j + 1];
    }

    /* Decrement number of alerts in array */
    globals.shm_info->n_alerts--;

    /* Set last alert memory to 0 */
    memset(globals.shm_alerts + globals.shm_info->n_alerts, 0, sizeof(Alert));

    sem_post(globals.shm_alerts_sem);
    sprintf(msg.data, "OK\n");
    msgsnd(globals.msqid, &msg, sizeof(msg) - sizeof(long), 0);
    write_log("Worker %d: Alert ID %s removed", worker_i + 1, alert_id);
  } else {
    sem_post(globals.shm_alerts_sem);
    sprintf(msg.data, "Alert %s does not exist\n", alert_id);
    msgsnd(globals.msqid, &msg, sizeof(msg) - sizeof(long), 0);
    write_log("Worker %d: Could not remove alert %s. Does not exist",
              worker_i + 1, alert_id);
  }
}
void list_alerts(int console_id) {
  int i;
  int str_index;
  Message msg;

  str_index = sprintf(msg.data, "ID\tKey\tMin\tMax\n");

  sem_wait(globals.shm_alerts_sem);
  for (i = 0; i < globals.shm_info->n_alerts; i++) {
    /* Write to string and increment last index */
    str_index +=
        sprintf((msg.data + str_index), "%s %s %d %d\n",
                globals.shm_alerts[i].alert_id, globals.shm_alerts[i].key->key,
                globals.shm_alerts[i].min_val, globals.shm_alerts[i].max_val);
  }
  sem_post(globals.shm_alerts_sem);

  /* Set msg type to console id and send */
  msg.msgtype = console_id;
  msgsnd(globals.msqid, &msg, sizeof(msg) - sizeof(long), 0);
}

void sigusr1_handler(int signum) {
  /* Tell the worker to stop working */
  stop_working = 1;
}
