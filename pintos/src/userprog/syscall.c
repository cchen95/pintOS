#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <string.h>
#include "threads/malloc.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "threads/init.h"

static void syscall_handler (struct intr_frame *);
void check_ptr (void *ptr, size_t size);
void check_string (char *ptr);
struct lock file_lock;
struct file_pointer *get_file (int fd);
struct dir *get_dir(const char *filepath);

void
syscall_init (void)
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void
check_ptr (void *ptr, size_t size)
{
  if (is_user_vaddr (ptr) && pagedir_get_page (thread_current ()->pagedir, ptr) != NULL && is_user_vaddr (ptr + size) && pagedir_get_page (thread_current ()->pagedir, ptr + size) != NULL)
  {
    return;
  }
  else
  {
    thread_exit();
  }
}

void
check_string (char *ustr)
{
  if (is_user_vaddr (ustr)) {
    char *kstr = pagedir_get_page (thread_current ()->pagedir, ustr);
    if (kstr != NULL && is_user_vaddr (ustr + strlen (kstr) + 1) &&
      pagedir_get_page (thread_current ()->pagedir, (ustr + strlen (kstr) + 1)) != NULL)
      return;
  }
  thread_exit ();
}

struct file_pointer *
get_file (int fd)
{
  struct list *list_ = &thread_current ()->file_list;
  struct list_elem *e = list_head (list_);
  while ((e = list_next(e)) != list_tail(list_))
  {
    struct file_pointer *f = list_entry (e, struct file_pointer, elem);
    if (f->fd == fd)
    return f;
  }
  return NULL;
}

struct dir *
get_dir(const char *filepath)
{
  if (filepath == NULL) return NULL;

  struct dir *curr_dir;
  char filename[NAME_MAX + 1];

  if (filepath[0] == '/')
    curr_dir = dir_open_root();
  else
    curr_dir = thread_current()->wd;

  struct dir *dir = dir_find(curr_dir, filepath, filename);

  struct inode *inode = NULL;
  bool found_dir = dir_lookup(dir, filename, &inode);

  if (found_dir)
    return dir_open(inode);

  return NULL;
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);

  check_ptr (args, sizeof (uint32_t));
  switch (args[0]) {
    case SYS_READ: case SYS_WRITE:
      check_ptr (&args[3], sizeof (uint32_t));
    case SYS_CREATE: case SYS_SEEK:
      check_ptr (&args[2], sizeof (uint32_t));
    case SYS_PRACTICE: case SYS_EXIT: case SYS_EXEC: case SYS_WAIT: case SYS_REMOVE:
    case SYS_OPEN: case SYS_FILESIZE: case SYS_TELL: case SYS_CLOSE:
      check_ptr (&args[1], sizeof (uint32_t));
  }

  if (args[0] == SYS_EXEC || args[0] == SYS_CREATE || args[0] ==  SYS_REMOVE || args[0] == SYS_OPEN)
  {
    check_string ((char *) args[1]);
  }
  else if (args[0] == SYS_WRITE || args[0] == SYS_READ)
  {
    check_ptr((void *) args[2], args[3]);
  }

  switch(args[0]) {
    case SYS_PRACTICE:
      f->eax = args[1] + 1;
      break;
    case SYS_WAIT:
      f->eax = process_wait (args[1]);
      break;
    case SYS_HALT:
      shutdown_power_off ();
      break;
    case SYS_EXIT:
    {
      thread_current ()->proc->exit_status = args[1];
      thread_exit ();
      break;
    }
    case SYS_EXEC:
      f->eax = process_execute ((char *) args[1]);
      break;
    case SYS_READ:
    {
      if (args[1] == STDIN_FILENO)
      {
        uint8_t *buffer = (uint8_t *) args[2];
        size_t i = 0;
        while (i < args[3]) {
          buffer[i] = input_getc ();
          if (buffer[i++] == '\n')
            break;
        }
        f->eax = i;
      }
      else if (args[1] == STDOUT_FILENO)
      {
        f->eax = 0;
      }
      else
      {
        lock_acquire (&file_lock);
        struct file_pointer *fn = get_file (args[1]);
        if (fn == NULL)
          {
            lock_release (&file_lock);
            break;
          }
        f->eax = file_read (fn->file, (void *) args[2], args[3]);
        lock_release (&file_lock);
      }
      break;
    }
    case SYS_WRITE:
    {
      if (args[1] == STDOUT_FILENO)
      {
        putbuf ((void *) args[2], args[3]);
        f->eax = args[3];
      }
      else if (args[1] == STDIN_FILENO)
      {
        f->eax = 0;
      }
      else
      {
        lock_acquire (&file_lock);
        struct file_pointer *fn = get_file (args[1]);
        if (fn == NULL)
          {
            lock_release (&file_lock);
            break;
          }
        f->eax = file_write (fn->file, (void *) args[2], args[3]);
        lock_release (&file_lock);
      }
      break;
    }
    case SYS_CREATE:
    {
      lock_acquire(&file_lock);
      f->eax = filesys_create ((char *) args[1], args[2]);
      lock_release (&file_lock);
      break;
    }
    case SYS_REMOVE:
    {
      lock_acquire(&file_lock);
      f->eax = filesys_remove ((char *) args[1]);
      lock_release (&file_lock);
      break;
    }
    case SYS_OPEN:
    {
      lock_acquire(&file_lock);
      struct file *temp_file = filesys_open ((char *) args[1]);
      if (temp_file)
      {
        struct thread *t = thread_current ();
        struct file_pointer *f_temp = malloc (sizeof (struct file_pointer));
        f_temp->file = temp_file;
        f_temp->fd = t->next_fd++;
        list_push_back (&t->file_list, &f_temp->elem);
        f->eax = f_temp->fd;
      }
      else
      {
        f->eax = -1;
      }
      lock_release (&file_lock);
      break;
    }
    case SYS_FILESIZE:
    {
      lock_acquire(&file_lock);
      struct file_pointer *fn = get_file (args[1]);
      f->eax = file_length (fn->file);
      lock_release (&file_lock);
      break;
    }
    case SYS_SEEK:
    {
      lock_acquire(&file_lock);
      struct file_pointer *fn = get_file (args[1]);
      file_seek (fn->file, args[2]);
      lock_release (&file_lock);
      break;
    }
    case SYS_TELL:
    {
      lock_acquire(&file_lock);
      struct file_pointer *fn = get_file (args[1]);
      f->eax = file_tell (fn->file);
      lock_release (&file_lock);
      break;
    }
    case SYS_CLOSE:
    {
      if (args[1] == STDOUT_FILENO || args[1] == STDIN_FILENO)
        {
          break;
        }
      lock_acquire (&file_lock);
      struct file_pointer *fn = get_file (args[1]);
      if (fn == NULL)
        {
          lock_release (&file_lock);
          break;
        }
      file_close (fn->file);
      list_remove (&fn->elem);
      free (fn);
      lock_release (&file_lock);
      break;
    }
    case SYS_CHDIR:
    {
      struct dir *dir = get_dir ((char *) args[1]);
      break;
    }
    case SYS_MKDIR:
    {
      break;
    }
    case SYS_READDIR:
    {
      break;
    }
    case SYS_ISDIR:
    {
      break;
    }
    case SYS_INUMBER:
    {
      break;
    }
  }
}
