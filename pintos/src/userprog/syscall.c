#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "filesys/inode.h"
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
struct file_pointer *get_file (int fd);

void
syscall_init (void)
{
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
      {
        f->eax = process_execute ((char *) args[1]);
        break;
      }
    case SYS_READ:
      {
        if (args[1] == STDIN_FILENO)
          {
            uint8_t *buffer = (uint8_t *) args[2];
            size_t i = 0;
            while (i < args[3])
              {
                buffer[i] = input_getc ();
                if (buffer[i++] == '\n')
                  break;
              }
            f->eax = i;
          }
        else if (args[1] == STDOUT_FILENO)
          f->eax = 0;
        else
          {
            struct file_pointer *fn = get_file (args[1]);
            if (fn == NULL)
              {
                f->eax = -1;
                break;
              }
            inode_add_user(file_get_inode (fn->file), false)  ;
            f->eax = file_read (fn->file, (void *) args[2], args[3]);
            inode_remove_user(file_get_inode (fn->file), false);
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
            f->eax = 0;
        else
          {
            struct file_pointer *fn = get_file (args[1]);
            if (fn == NULL || fn->is_dir)
            {
              f->eax = -1;
              break;
            }
            inode_add_user(file_get_inode (fn->file), false);
            f->eax = file_write (fn->file, (void *) args[2], args[3]);
            inode_remove_user(file_get_inode (fn->file), false);
          }
        break;
      }
    case SYS_CREATE:
      {
        char filename[NAME_MAX + 1];
        struct dir *dir = dir_find (thread_current ()->wd, (char *) args[1], filename);
        if (dir == NULL)
          {
            f->eax = false;
            break;
          }
        inode_add_user (dir_get_inode (dir), false);
        f->eax = filesys_create_dir (dir, filename, args[2]);
        inode_remove_user (dir_get_inode(dir), false);

        dir_close (dir);
        break;
      }
    case SYS_REMOVE:
      {
        /* Locks in inode_close (), also need to do stuff in dir_remove() */
        f->eax = filesys_remove ((char *) args[1]);
        break;
      }
    case SYS_OPEN:
      {
        char filename[NAME_MAX + 1];
        struct dir *dir = dir_find (thread_current ()->wd, (char *) args[1], filename);
        if (dir == NULL)
          {
            f->eax = -1;
            break;
          }

        struct inode *inode;
        bool found = dir_lookup (dir, filename, &inode);

        /* Ugly handling for "/" */
        if (strcmp(filename, "") == 0)
        {
          inode = inode_reopen (dir_get_inode (dir));
          found = true;
        }
        
        dir_close (dir);
        if (found)
          {
            inode_add_user(inode, false);
            struct file_pointer *fp = malloc (sizeof (struct file_pointer));
            struct thread *t = thread_current ();
            if (inode_is_dir (inode))
              {
                struct dir *dir = dir_open (inode);
                fp->dir = dir;
                fp->is_dir = true;
              }
            else
              {
                struct file *file = file_open (inode);
                fp->file = file;
                fp->is_dir = false;
              }
            fp->fd = t->next_fd++;
            list_push_back(&t->file_list, &fp->elem);
            f->eax = fp->fd;
            inode_remove_user (inode, false);
          }
        else
          {
            f->eax = -1;
          }
        break;
      }
    case SYS_FILESIZE:
      {
        struct file_pointer *fn = get_file (args[1]);
        inode_add_user(file_get_inode(fn->file), true);
        f->eax = file_length (fn->file);
        inode_remove_user(file_get_inode(fn->file), true);
        break;
      }
    case SYS_SEEK:
      {
        struct file_pointer *fn = get_file (args[1]);
        inode_add_user(file_get_inode(fn->file), false);
        file_seek (fn->file, args[2]);
        inode_remove_user(file_get_inode(fn->file), false);
        break;
      }
    case SYS_TELL:
    {
      struct file_pointer *fn = get_file (args[1]);
      inode_add_user(file_get_inode(fn->file), false);
      f->eax = file_tell (fn->file);
      inode_remove_user(file_get_inode(fn->file), false);
      break;
    }
    case SYS_CLOSE:
    {
      if (args[1] == STDOUT_FILENO || args[1] == STDIN_FILENO)
        {
          break;
        }
      struct file_pointer *fn = get_file (args[1]);
      if (fn == NULL)
        break;
      // lock in inode_close ()
      file_close (fn->file);
      list_remove (&fn->elem);
      free (fn);
      break;
    }
    case SYS_CHDIR:
      {
        char filename[NAME_MAX + 1];
        struct dir *dir = dir_find (thread_current ()->wd, (char *) args[1], filename);
        struct inode *inode = NULL;
        bool found_dir = dir_lookup (dir, filename, &inode);
        dir_close (dir);
        if (!found_dir || !inode_is_dir(inode))
          {
            f->eax = false;
            break;
          }
        dir_close (thread_current ()->wd);
        thread_current()->wd = dir_open (inode);
        f->eax = true;
        break;
      }
    case SYS_MKDIR:
      {
        char filename[NAME_MAX + 1];
        struct dir *dir = dir_find (thread_current ()->wd, (char *) args[1], filename);
        f->eax = dir_add_dir (dir, filename);
        dir_close (dir);
        break;
      }
    case SYS_READDIR:
      {
        break;
      }
    case SYS_ISDIR:
      {
        struct file_pointer *fp = get_file (args[1]);
        if (fp == NULL || !fp->is_dir)
          {
            f->eax = false;
            break;
          }
        f->eax = true;
        break;
      }
    case SYS_INUMBER:
      {
        struct file_pointer *fp = get_file (args[1]);
        if (fp == NULL)
          {
            f->eax = -1;
            break;
          }
        struct inode *inode = NULL;
        if (fp->is_dir)
          inode = dir_get_inode(fp->dir);
        else
          inode = file_get_inode(fp->file);
        f->eax = inode_get_inumber(inode);
        break;
      }
  }
}
