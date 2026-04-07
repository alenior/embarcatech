#ifndef FIREBASE_H
#define FIREBASE_H

void firebase_init();
void firebase_task();
void firebase_log(const char *state, const char *event);
void firebase_set_status(const char *status);

#endif