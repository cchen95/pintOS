Design Document for Project 2: User Programs
============================================

## Group Members

* Christopher Chen <christopher.chen95@berkeley.edu>
* Jimmy Cheung <jimmycheung2481@berkeley.edu>
* Sarah Au <sau@berkeley.edu>
* Solah Yoo <tjf009@berkeley.edu>

### Argument Parsing
#### Data Structures/Functions
None needed
#### Algorithms
1. Parse string argument given to take out all whitespace
2. Want to push arguments to stack one by one and decrement stack pointer (follow algorithm in project spec)
### Process Control Syscalls
#### Data Structures/Functions
1. Create a new struct childProc
    * `p_id processID` = the process ID of the child process
    * `int status` =  the exit status of the process, NULL otherwise
    * `bool hasBeenExited` if the process has already exited
    * `bool hasBeenWaited` if the process has been waited on by a parent
    * `bool kernelExited` if process has been exited by kernel
    * `struct list_elem elem` = a list elem for it to be referenced by the parent thread
2. Modify struct thread
    * `struct list childProcesses`, the set of all child processes created by this thread
    * `struct thread get_thread_by_id(tid_t id)`: returns the thread given an input thread id
    * lock and condition `lock_child` and `cond_child`
    * `child_load_status`, holds the child load status, which is set by the child
3. Modify `syscall_handler` to check for other system call numbers using else if statements. Check for other system call numbers that correspond to the respective system calls - practice, halt, exit, exec, wait, etc.. Pass in the corresponding arguments and call the appropriate function.
4. Create a new function called `syscall_practice`, that takes in one argument. Increment the argument by 1 and returns the value of the argument.
5. New function `syscall_halt`, just calls `shutdown_power_off`
6. New function `syscall_wait`, calls `process_wait`
7. New function `is_valid_pointer`, checks if pointer is valid
8. New function `syscall_exit`, calls `thread_exit` and sets the correct statuses for children
9. New function `exec(const char *cmd_line)` executes `cmd_line` executable
#### Algorithms
1. In `syscall_wait`: call `process_wait`
2. In `process_wait`: create a `childProc` whenever a new child is created and add it to `childProcesses`, then wait the parent
  * child is responsible for waking parent and returning status unless it has exited
    * if child calls `syscall_exit`, then set the `has_been_exited boolean` in its `childProc`
  * if parent is woken or sees child `has_been_exited`, then checks status and continues
  * if child is exited by kernel, then sets `bool kernelExited` to true
  * if parent terminates early, then all relevant structs are free, and since the child can find its parent and realize it has been exited, then it will no longer do anything to `childProc` and continue to execute
3. In `syscall_halt`, just calls `shutdown_power_off` from devices
4. In `syscall_exit`, uses `get_thread_by_id` to identify the parent, then sets all the appropriate fields in `childProc` before calling `thread_exit` on itself
5. In exec, checks whether the child successfully loaded (via `child_load_status`) and returns -1 if it doesn&#39;t. Otherwise will set the proper `childProc` fields and will call `process_execute`
6. In `process_execute`, in addition to what is already there, creates a child and a `childProc` using `thread_create`, returning what `thread_create` returned (the tid) and updating `childProc` fields before pushing it to the back of the `childProcesses` list
7. Update `startProcess` to update `load_status` of the parent thread
#### Synchronization
1. Any time any updating of an existing childProc must be made, the thread updating the childProc must obtain the monitor. Once the update is successfully made, then we can use cond_signal to signal to the parent. This synchronization structure is used in `start_process` (along with `cond_wait`), `process_wait`, and `process_exit`. In the `syscall_exit`, we only need to use the lock without the condition variable. Exec needs the monitor because it also updates `childProc`. Wait needs nothing since it just calls `process_wait` and `syscall_halt` needs nothing since it just calls `shutdown_power_off`.
#### Rationale
1. We figured we definitely needed the `childProc` structure because of `syscall_wait` - we needed to know when a child has started, when it&#39;s terminated, and when a parent is waiting on it. This is because condition variables aren&#39;t sufficient for our needs here - if a child has terminated and a parent is waiting, then the parent will wait forever. Similarly, if a child signals after exiting, we can&#39;t guarantee that the parent will know which `syscall_wait` to continue with. We use a list of `childProcesses` in thread because we need to know the child processes in exit. If exit is called on the parent, we must be able to exit all the children. Also, O(n) time is not a concern for us, so the slightly longer runtime is not an issue.
### File Operation Syscalls
#### Data Structures/Functions
1. Have a `struct lock` `file_lock` that locks when any file operation syscall is called
2. Modify `syscall_handler()` to add handling for syscalls `SYS_CREATE`, `SYS_REMOVE`, `SYS_OPEN`, `SYS_FILESIZE`, `SYS_READ`, `SYS_WRITE`, `SYS_SEEK`, `SYS_TELL`, `SYS_CLOSE`
3. Add an int variable `next_fd` in `struct thread` to keep track of next file descriptor to be assigned
4. Create a `struct hash` variable `open_files_map` in `struct thread`, which maps file descriptors (`fd`) to files (`file_pointer`) the process has open
5. Create a helper function `struct file *get_file(int fd)`, which gets a `struct file *` from the hash table given a file descriptor
6. Create a new `struct file_pointer` as a wrapper for `struct file` so we can store files inside a hash map. It has variables
  * `struct file` `file`
  * `struct hash_elem` `elem`
#### Algorithms
1. Error handling
  * For any syscall that operates on an input `fd`, we error if we cannot find it in `open_files_map` (-1)
  * Error if any of the file operation calls are unsuccessful
  * Need error handling for fds 0, 1, 2 on certain syscalls
2. For all the file system calls we need to implement, we call the corresponding functions in file.c and filesys.c (ie. call `file_seek` for `void seek(int fd, unsigned position)`)
3. Syscall create
  * Call `filesys_create(file, size)`
  * Error handling if it returns false
4. syscall remove
  * Call `inode_remove()` on the input `file`&#39;s inode
5. syscall open
  * Open a file with `filesys_open(file)` to get a file object
  * Create a new `file_pointer` struct with `file_pointer-&gt;file` as file object
  * Assign the file&#39;s file descriptor value `fd` to be the value of `next_fd`, and then increment the `next_fd` value to be used for the next open operation.
  * Put the &lt;`fd, file_pointer`&gt; key-value pair in `open_files_map`
  * Return `fd`
6. Syscall filesize
  * Get the right file with `get_file(fd)`
  * Call `file_length(file)` to get the file size in bytes
7. Syscall read
  * Use `input_getc()` to read from the keyboard if `fd` is 0
    * Have  a `count` variable to keep track of number of characters read
    * Keep reading characters with `input_getc()` into `buffer` until we have read `size` number of bytes or we reach `EOF`
    * Return `count`
  * Else, get the right file with `get_file(fd)`
    * Call `file_read(file, buffer, size)` to read file into buffer
    * Return result of `file_read()` call
8. Syscall write
  * Throw error if `fd=0` (stdin)
  * If `fd=1`, use `putbuf(buffer, size)` to write to console and return `size`
  * Else, get `file` with &#39;get\_file(fd)&#39;
    * Call `file_write(file, buffer, size)`
    * Return result of `file_write()`
9. Syscall seek
  * Get `file` with `get_file(fd)`
  * Return result of `file_seek(file, position)`
10. Syscall tell
  * Get `file` with `get_file(fd)`
  * Return result of `file_tell(file)`
11. Syscall close
  * Error handling for closing 0, 1, 2 - fail silently or terminate with exit code -1
  * Remove any locks on the file
  * Remove `fd` and corresponding `file_pointer` from `open_files_map`
  * Call `file_close(file)`
#### Synchronization
1. Use `file_deny_write` on file that is being used by user process. Call `file_allow_write` after user is done with this file.
2. Lock `file_lock` when a file operation function is called and unlock when it is done. With this lock we can ensure that only one filesystem operation is executed at a time, and each must finish before another one is executed.
3. Each file can be opened by multiple threads at once. However, each thread has its own `open_files_map` and `next_fd`, so each fd maps to a unique file per thread. When we call `open()` on a file, we create its `struct file` pointer in the thread stack, so each thread also has its own unique place in an open file.
#### Rationale
1. For most of the syscalls, we can use already implemented file operations. Since it&#39;s not recommended to modify the file and filesys files, we create a new struct to easily store the file and its `hash_elem`. We use a hash map to map file descriptors to files for constant access time, so it&#39;s faster than a linked list in which searching for a file would be in O(n) time. It also uses less space than a static array with a predefined size.
### Additional Questions
1. sc-bad-sp.c
    * Line 18 moves the stack pointer to a bad address (about 64 MB below code segment)
2. boundary.c
    1. Line 28 gets the page boundary and stores it in p
    2. Line 29 decrements p by:
        * If length of `src` is less than 4096, decrement p by the length divided by 2.
        * Else, decrement by 4096
    3. Line 30 attempts to write `src` at address p.
    4. Should fail because it tries to copy `src` across boundary
#### GDB Questions
1. The name of the thread running is `main` and its address is 0xc000ee0c (from the stack pointer). The threads present at this time are:
    1. (current thread) dumplist #0: 0xc000e000 {tid = 1, status = THREAD\_RUNNING, name = &quot;main&quot;, &#39;\000&#39; &lt;repeats 11 times&gt;, stack = 0xc000ee0c &quot;\210&quot;, &lt;incomplete sequence \357&gt;, priority = 31, allelem = {prev = 0xc0034b50 &lt;all\_list&gt;, next = 0xc0104020}, elem = {prev = 0xc0034b60 &lt;ready\_list&gt;, next = 0xc0034b68 &lt;ready\_list+8&gt;}, pagedir = 0x0, magic = 3446325067}
    2. dumplist #1: 0xc0104000 {tid = 2, status = THREAD\_BLOCKED, name = &quot;idle&quot;, &#39;\000&#39; &lt;repeats 11 times&gt;, stack = 0xc0104f34 &quot;&quot;, priority = 0, allelem = {prev = 0xc000e020, next = 0xc0034b58 &lt;all\_list+8&gt;}, elem = {prev = 0xc0034b60 &lt;ready\_list&gt;, next = 0xc0034b68 &lt;ready\_list+8&gt;}, pagedir =0x0, magic = 3446325067}
2. The backtrace for the current thread is:
    1. #0  process\_execute (file\_name=file\_name@entry=0xc0007d50 &quot;args-none&quot;) at ../../userprog/process.c:32
        * Called with line:288 `process_wait (process_execute (task))`
    2. #1  0xc002025e in run\_task (argv=0xc0034a0c &lt;argv+12&gt;) at ../../threads/init.c:288
        * Called with line:340 `a-&gt;function (argv);`
    3. #2  0xc00208e4 in run\_actions (argv=0xc0034a0c &lt;argv+12&gt;) at ../../threads/init.c:340
        * Called with line:133 `run_actions (argv)`
    4. #3  main () at ../../threads/init.c:133
3. The name of the thread running `start_process()` is `args-none` and its address is `0xc010afd4`. The threads present at this time are:
    1. dumplist #0: 0xc000e000 {tid = 1, status = THREAD\_BLOCKED, name = &quot;main&quot;, &#39;\000&#39; &lt;repeats 11 times&gt;, stack = 0xc000eebc &quot;\001&quot;, priority= 31, allelem = {prev = 0xc0034b50 &lt;all\_list&gt;, next = 0xc0104020}, elem = {prev = 0xc0036554 &lt;temporary+4&gt;, next = 0xc003655c &lt;temporary+12&gt;}, pagedir = 0x0, magic = 3446325067}
    2. dumplist #1: 0xc0104000 {tid = 2, status = THREAD\_BLOCKED, name = &quot;idle&quot;, &#39;\000&#39; &lt;repeats 11 times&gt;, stack = 0xc0104f34 &quot;&quot;, priority = 0, allelem = {prev = 0xc000e020, next = 0xc010a020}, elem = {prev = 0xc0034b60 &lt;ready\_list&gt;, next = 0xc0034b68 &lt;ready\_list+8&gt;}, pagedir = 0x0, magic =3446325067}
    3. (current thread) dumplist #2: 0xc010a000 {tid = 3, status = THREAD\_RUNNING, name = &quot;args-none\000\000\000\000\000\000&quot;, stack = 0xc010afd4 &quot;&quot;, priority =31, allelem = {prev = 0xc0104020, next = 0xc0034b58 &lt;all\_list+8&gt;}, elem = {prev = 0xc0034b60 &lt;ready\_list&gt;, next = 0xc0034b68 &lt;ready\_list+8&gt;}, pagedir= 0x0, magic = 3446325067}
4. The thread running `start_process` is created at line:424 `function (aux)` in the `kernel_thread()` function.
5. 0x0804870c caused the page fault.
6. \_start (argc=&lt;error reading variable: can&#39;t compute CFA for this frame&gt;, argv=&lt;error reading variable: can&#39;t compute CFA for this frame&gt;) at ../../lib/user/entry.c:9, which is `exit (main (argc, argv))`
7. The error is cause because argument passing hasn&#39;t been implemented yet and it tries to call `main (argc, argv)`  on arguments that are supposed to be pushed onto the stack.