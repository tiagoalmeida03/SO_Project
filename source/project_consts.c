/*
  Gonçalo Monteiro  2021217127
  Tiago Almeida     2021221615
*/

#include <string.h>

#include "project_consts.h"

int valid_id(char *s) {
  int len = strlen(s);
  int i;

  /* Check length */
  if (len < KEY_MIN_LENGTH || len > KEY_MAX_LENGTH) {
    return 0;
  }
  /* Check characters */
  for (i = 0; i < len; i++) {
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'z') ||
          (s[i] >= 'A' && s[i] <= 'Z'))) {
      return 0;
    }
  }

  return 1;
}

int valid_key(char *s) {
  int len = strlen(s);
  int i;

  /* Check length */
  if (len < KEY_MIN_LENGTH || len > KEY_MAX_LENGTH) {
    return 0;
  }
  /* Check characters */
  for (i = 0; i < len; i++) {
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'z') ||
          (s[i] >= 'A' && s[i] <= 'Z') || (s[i] == '_'))) {
      return 0;
    }
  }

  return 1;
}