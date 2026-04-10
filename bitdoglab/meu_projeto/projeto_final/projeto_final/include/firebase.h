#ifndef FIREBASE_H
#define FIREBASE_H

void firebase_init(void);
void firebase_task(void);
void firebase_fetch_control(void);

void firebase_log(const char *state, const char *event);
void firebase_set_status(const char *status);

#endif