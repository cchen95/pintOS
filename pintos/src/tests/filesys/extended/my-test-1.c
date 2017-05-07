#include "tests/main.h"
#include "lib/user/syscall.h"
#include <stdio.h>
#include <syscall.h>
#include "tests/filesys/extended/mk-tree.h"
#include "tests/lib.h"

void
test_main(void){
	create ("a", 10000);
	char buffer[512];
	int fd = open ("a");
	read (fd, buffer, 512);
	int first_hit_rate = cache_hit_rate ();
	close (fd);
	fd = open ("a");
	read (fd, buffer, 512);
	int second_hit_rate = cache_hit_rate ();
	second_hit_rate >= first_hit_rate ? msg ("Hit rate increases") : msg ("Hit rate decreases");
}