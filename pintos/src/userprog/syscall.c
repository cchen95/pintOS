#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);
void check_ptr (void *ptr, size_t size);
void check_string (char *ptr);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_ptr (void *ptr, size_t size) {
  if (is_user_vaddr (ptr) && pagedir_get_page (thread_current ()->pagedir, ptr) != NULL && is_user_vaddr (ptr + size) && pagedir_get_page (thread_current ()->pagedir, ptr + size) != NULL)
  {
    return;
  }
  else
  {
    thread_exit();
  }
}

void check_string (char *ustr) {
  if (is_user_vaddr (ustr)) {
    char *kstr = pagedir_get_page (thread_current ()->pagedir, ustr);
    if (kstr != NULL && is_user_vaddr (ustr + strlen (kstr) + 1) &&
      pagedir_get_page (thread_current ()->pagedir, (ustr + strlen (kstr) + 1)) != NULL)
      return;
  }
  thread_exit ();
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);
  // printf("System call number: %d\n", args[0]);
  switch (args[0])
    {
      case SYS_EXIT:
        f->eax = args[1];
        printf("%s: exit(%d)\n", &thread_current ()->name, args[1]);
        thread_exit();
      case SYS_WRITE:
        if (args[1] == STDOUT_FILENO)
          {
            putbuf(args[2], args[3]);
          }
    }
}
