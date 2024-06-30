/*
  Gonçalo Monteiro  2021217127
  Tiago Almeida     2021221615
*/

#ifndef PROJECT_CONSTS_H
#define PROJECT_CONSTS_H

#define DEBUG

#define BUFFER_SIZE 1024

#define CONSOLE_PIPE_NAME "/tmp/CONSOLE_PIPE"
#define SENSOR_PIPE_NAME "/tmp/SENSOR_PIPE"

#define ID_MIN_LENGTH 3
#define ID_MAX_LENGTH 32

#define KEY_MIN_LENGTH 3
#define KEY_MAX_LENGTH 32

#define CMD_MAX_LENGTH 32

/* Struct that holds a message for the message queue */
typedef struct {
  long msgtype;
  char data[BUFFER_SIZE];
} Message;

int valid_id(char *id);
int valid_key(char *key);

#endif // PROJECT_CONSTS_H