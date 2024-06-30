/*
  Gonçalo Monteiro  2021217127
  Tiago Almeida     2021221615
*/

#include <signal.h>
#include <string.h>
#include <sys/msg.h>
#include <unistd.h>

#include "log.h"
#include "sys_manager_globals.h"

void sigusr2_handler(int sg);

void alerts_watcher() {
  int i, j;
  Message msg;

  struct sigaction act_sigusr2;
  act_sigusr2.sa_handler = &sigusr2_handler;
  sigfillset(&act_sigusr2.sa_mask);
  act_sigusr2.sa_flags = 0;
  sigaction(SIGUSR2, &act_sigusr2, NULL);

  while (1) {
    sem_wait(globals.shm_watch_alerts_sem);
    /* Iterate over all alerts */
    sem_wait(globals.shm_alerts_sem);
    for (i = 0; i < globals.shm_info->n_alerts; i++) {
      /* Check if any is to be triggered */
      if (globals.shm_alerts[i].key->max_val > globals.shm_alerts[i].max_val ||
          globals.shm_alerts[i].key->min_val < globals.shm_alerts[i].min_val) {
        /* Alert triggered */
        msg.msgtype = globals.shm_alerts[i].console_id;
        sprintf(msg.data, "Alert %s (%s %d to %d) triggered\n",
                globals.shm_alerts[i].alert_id, globals.shm_alerts[i].key->key,
                globals.shm_alerts[i].min_val, globals.shm_alerts[i].max_val);
        msgsnd(globals.msqid, &msg, sizeof(msg) - sizeof(long), 0);
        write_log("Alert %s (%s %d to %d) triggered",
                  globals.shm_alerts[i].alert_id,
                  globals.shm_alerts[i].key->key, globals.shm_alerts[i].min_val,
                  globals.shm_alerts[i].max_val);

        /* Remove alert from the list */
        for (j = i; j < globals.shm_info->n_alerts; j++) {
          /* Shift all next alerts to the left */
          globals.shm_alerts[j] = globals.shm_alerts[j + 1];
        }
        /* Decrement number of alerts in array */
        globals.shm_info->n_alerts--;

        /* Set last alert memory to 0 */
        memset(globals.shm_alerts + globals.shm_info->n_alerts, 0,
               sizeof(Alert));
      }
    }
    sem_post(globals.shm_alerts_sem);
  }
}

void sigusr2_handler(int signum) { exit(0); }