Final Report for Project 2: User Programs
=========================================


### Argument Parsing

For the argument parsing section, we first placed the logic in `setup_stack()` which involved adding in additional arguments (specifically `filename` and `saveptr`) which meant we also needed to change how it is called in load. This design passed all the tests for argument parsing, but then later on when implementing the process syscalls would trigger kernel panics. This was solved by moving the logic away from `setup_stack()` (bringing it back to its original implementation) and into `start_process()`. In retrospect, it is likely that process syscalls could have still been implemented correctly with argument parsing logic in `setup_stack()` but it proved easier to our group for us to have it in `start_process()`.

### Process syscalls

As per Josh’s suggestion, we used a lock in our `childProc` and also used a semaphore rather than condition variable and monitor to simplify things. We also ended up not implementing a new `exec()` method, instead modifying `process_execute()` for our exec syscall. The semaphore blocks the parent for the child to load. We also ended up not needing a `kernelExited` property in the `childProc` since that was easy for us to determine with the `exit_status`. Other fields in `childProc` were added and removed as necessary (or unnecessary). 

`childProc` may actually be somewhat of a misnomer, since threads spawn their own `childProc` for themselves, so it’s more of just a process status rather than something that refers to the child. This was done to make it easier to associate the child and the parent rather than our previous intended implementation of using a new `get_thread_by_id()` method (which ended up turning into a `get_child_process()` method). This puts the burden of associating the parent and the child on the parent rather than having the child do a reverse lookup for its parent.

Also as per Josh’s suggestion, we implemented a method `check_ptr()` to validate user pointers. Otherwise, we followed the initial design document.

### File syscalls

As per Josh’s suggestion, we use a list instead of hash table. Also as Josh suggested, we didn’t use inode methods, instead using filesys methods (e.g. `filesys_remove()` rather than `inode_remove()`). Other than that, we pretty much followed our design document (eg. handling specific cases such as writing to stdin, and closing already closed files, etc).

### Project Reflection

Chris implemented the process syscalls and some of the file syscalls as well as moved argument parsing syscall logic from `setup_stack()` to `start_process()`.

Solah wrote the initial argument parsing in `setup_stack()` but Chris had to change all that to make it work with the later parts. And she added to some of the file syscalls to make all the tests pass.

Jimmy contributed in writing the syscalls exec and wait. He also wrote test #2 and the corresponding information about it. 

In retrospect, we could have started the project a little earlier because some people had other commitments during spring break, which caused the workload to be a little less balanced this time and also limited contributions to the project. Otherwise it was pretty smooth and most of the work was finished well before the deadline.

### Student Testing Report

#### write-stdout

This test case tests writing to stdout. It first creates and opens a text file and writes the content of that file to stdout. If the bytes written doesn't equal the size of the file, then it outputs a failure message. Then, in the .ck file, it checks that the file content was correctly written to output. 

###### Potential bugs

If the kernel did not have a check for if the file descriptor is `STDOUT_FILENO`, the kernel would instead try to write to file “1” and not print to output, so the test case would have no outputs.  

If the kernel used `printf()` instead of `putbuf()`, since `putbuf()` is thread-safe but `printf()` is not, the output may have interleavings of multiple processes’ outputs. 

#### wait-childterm

This test case tests the wait system call. It allows a simple function to run in a child using `exec()`. Once the call has been completed and exited it checks that the parent can still call `wait()` and retrieve the exit code. In the .ck file it makes sure that `child-simple` runs, then a message is printed indicating that the child process has completed and exited, then checks that a call on `wait()` returns the exit code that the child exited with.

###### Potential bugs

If the kernel did not save any state for a child process and instead relied on child returning the exit code directly to the parent, the parent would not have been able to retrieve any information about the child’s exit code and would have returned -1.

If the child saved state did not indicate whether the child or the parent had exited in some form, then the parent would not know to retrieve the exit code without first waiting on the child to signal that it has exited. As a result, the parent would have waited forever.

#### Experience writing tests for Pintos 

There were some tests that we thought would be better to test Pintos’ functions such as checking that a process wouldn’t be able wait on a child’s child and reading from stdin. But we didn’t know if the Pintos testing system had a simple way to get a child’s child’s pid for the parent to use. We learned that writing tests for Pintos isn’t too hard as long as you know how to use the test functions (eg. `CHECK()`) and that because of the limitations of the testing system, you may not be able to test all features.  

