#include "tests/main.h"
#include "lib/user/syscall.h"
#include <stdio.h>
#include <syscall.h>
#include "tests/filesys/extended/mk-tree.h"
#include "tests/lib.h"
#include <random.h>

void test_main(void)
{
  int fd;
  int reads1;
  int reads2;
  int writes1;
  int writes2;
  char buffer[512];
  random_init (0);
  random_bytes (buffer, sizeof buffer);
  CHECK (create ("a", 0), "create \"a\"");
  CHECK ((fd = open ("a")) > 1, "open \"a\"");
  reads1 = cache_reads();
  writes1 = cache_writes();
  int i;
  for(i = 0;i < 100; i++)
  {
    write (fd, buffer, 512);
  }
  reads2 = cache_reads();
  writes2 = cache_writes();
  msg("%d reads in 100 writes", reads2-reads1);
  msg("%d writes in 100 writes", writes2-writes1);
  close (fd);
  remove("a");
}