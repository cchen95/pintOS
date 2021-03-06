#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

struct file_pointer
  {
    int fd;
    const char *name;
    bool is_dir;
    struct file *file;
    struct dir *dir;
    struct list_elem elem;
  };

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
