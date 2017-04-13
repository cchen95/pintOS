Design Document for Project 3: File System
==========================================

## Group Members

* Christopher Chen <christopher.chen95@berkeley.edu>
* Jimmy Cheung <jimmycheung2481@berkeley.edu>
* Sarah Au <sau@berkeley.edu>
* Solah Yoo <tjf009@berkeley.edu>

##  Task 1: Buffer cache

### Data Structures and Functions

* `struct cache_block` in cache.h/cache.c
	* `int dirty`
	* `struct lock block_lock`
	* `struct list_elem lru_elem` for LRU policy
	* `struct hash_elem block_elem`
	* `int users`
	* `void *data`
* `struct list lru`
* `struct hash blocks`
* `void acquire_cache_block`
* `void release_cache_block`
* `off_t read_cache_block`
* `off_t write_cache_block`
* `struct lock cache_lock`
* Remove `struct inode_disk data` from `struct inode`

### Algorithms

* Acquire `cache_lock` before modifying `list lru` and `hash blocks`
* For LRU, every time a block is accessed, remove from list and add to head of list. To evict a block, remove element at end of list.
	* Before evicting, acquire `block_lock` so other users don’t attempt to read/write from it and check that users = 0 to ensure that you are not evicting a block that is in use.
* Load block from disk using `block_read`.
	* Acquire `block_lock` and release after loading into `data`
	* Save data into `cache_block->data`
* Write to disk when evicting dirty block or shutdown (when `filesys_done()` is called) using `block_write`
* Use palloc_get_multiple to allocate memory to the buffer cache
* Modify `inode_read_at` and `inode_write_at` to read from buffer cache using cache functions
	* Acquire `cache_block->block_lock` for every read and write
	* `write_cache_block` - find block in `hash blocks` if it exists (if not use LRU policy to evict a block and bring a new block in) and use memcpy to directly write buffer to cache_block->data
	* `read_cache_block` - find block in `hash blocks` and use memcpy to memcpy to write cache_block->data to buffer
* `acquire_cache_block` increments users by 1
* `release_cache_block` decrements users by 1 and calls `cache_queue` if it hits 0 - prevents block from being evicted by cache until kernel is done (move to rationale)

### Synchronization

Shared resources - `list lru`, `hash blocks`, `cache_block`
We acquire a lock before modifying any hash or lists or the cache blocks all have their own locks that are acquired whenever they are being used. 

### Rationale

We thought using LRU instead of clock would be simpler because with a clock algorithm, you would need to keep track of the block your hand is pointing to and also iterate through the list to find a block to evict. But with LRU, you would have to iterate through the list to find the block you have accessed but you wouldn’t need to keep track of the hand pointer. And to evict, you could pop the last element from the list. 

## Task 2: Extensible files

### Data Structures and Functions

* In `inode.c`:
	* `struct inode_disk`
	* add `direct`, `indirect`, and `doubly_indirect` `block_sector_t`
* `bool inode_resize()`
	* extends or shortens inode length as necessary
	* returns true if successful
* In `free_map.c`
	* add a lock

### Algorithms

* `free_map_allocate` - add support for allocating multiple blocks of memory to prevent external fragmentation, counts number of free blocks, and fails if number of free blocks < cnt. Otherwise, allocates cnt blocks
* change `inode_disk` to read in data from buffer cache
* `inode_create`, `inode_close`, `inode_write_at` (when it reaches EOF) all call `inode_resize`
* new syscall `inumber` calls `inode_get_number`

### Synchronization

adding a lock to `free_map` means that allocating blocks is an all-or-nothing operation and that we will not run into situations where some of the blocks are allocated and others are not

### Rationale

Many things that are done here were requested by the spec

`inode_resize` may need to be split into 2 different functions, one for increasing size and another for decreasing size

Might need to split new logic in `free_map_allocate` to a new function as well, since the current `free_map_allocate` allocates consecutive blocks

##  Task 3: Subdirectories

### Data Structures and Functions

* Add `struct condition cv`
* Keep global lock `file_lock` from part 2 but only use with cv
* Modify `struct file_pointer` so it has fields:
	* `int fd`
	* `const char *name`
	* `bool is_dir`
	* `struct dir *dir`
	* `struct file *file`
	* `struct list_elem elem`
* Add fields to `struct inode`:
	* `struct dir *parent`
	* `struct lock lock`
	* `bool in_use`
	* `bool sub_use`
* Add `struct file_pointer *wd` field to `struct thread` if USERPROG
* Add `struct lock lock` to `struct inode_disk`
* Add helper function `struct *dir find_dir(const char *dir, char **filename)` to parse a filepath and get lowermost directory 
* Add helper function `bool can_use(struct *inode)` that checks whether we can operate on an inode

### Algorithms

##### Working directory

* Create new `wd` for a child process when it starts
* Set `wd->dir` of first user process to root directory by calling `dir_open_root()`
* Set `wd->dir` of other user processes to by calling `dir_reopen()` on parent’s `wd->dir`

##### Helper functions 

* `struct dir *find_dir(const char *filepath, char **filename)`
	* Parse input file path with `get_next_part()` (given code in spec) to get components
	* For each component call `dir_lookup(curr_dir, component, **inode)` to get its inode if it exists, and use `dir_open()` on inode to get matching `struct dir` to look through next 
		* `curr_dir` starts from `wd` or root depending on absolute or relative path
	* Sets `filename` to be last component of filepath
	* Returns NULL if any component in the path cannot be found
	* Resolving paths
		* For syscalls with input `char *` file or dir arguments, check if first character is  ‘/’, which indicates it is an absolute path
		* Search for files or dirs starting from `wd` if relative or root if absolute
		* Special characters 
			* `../` - get parent directory and search starting from there
			* `./` - start from `wd`
* `bool can_use(struct *inode)` (used for synchronization)
	* Follow inode parent pointers to make sure itself and no parent directory is `in_use`, and that `sub_use` for current level is false (checks whether operations are happening at a lower level), and return true if nothing is in use

##### New syscalls: modify `syscall.c` for syscall handling

* `bool chdir(const char *dir)`
	* arse input file path with `get_next_part()` (given code in spec) to get components
	* For each component call `dir_lookup(curr_dir, component, **inode)` to get its inode if it exists, and use `dir_open()` on inode to get matching `struct dir` to look through next
	* Returns false if `dir_lookup()` fails or if `dir` is not a directory
	* Else, change `wd` of process thread to last `struct dir` found with `dir_lookup()` and return true
* `bool mkdir (const char *dir)`	
	* Call `get_next_part()` to find directories and fail
	* Get a sector number with `free_map_allocate()`
	* Call `dir_create()` on sector number (entry_cnt = 16?)
	* Call `dir_add()` to add the directory with name and sector number
* `bool readdir (int fd, char *name) `
	* Call `get_file(fd)` to get corresponding `file_pointer->dir` 
	* Call `dir_readdir()` where `*dir` is the directory corresponding to `fd` and `name[NAME_MAX + 1]` is `*name`, while `name` is not “.” or “..”
* `bool isdir (int fd)`
	* Call `get_file(fd)` and check corresponding `file_pointer->is_dir`
* `int inumber (int fd)`
	* Call `get_file(fd)` to get corresponding `struct file_pointer` and check `is_dir` field
	* If directory, call `dir_get_inode()` on `file_pointer->dir` and call `inode_get_inumber()`
	* If file call `file_get_inode` on `file_pointer->file` and call `inode_get_inumber()`

##### Old syscalls to modify
* `int open (const char *file)`
	* Edit `filesys_open()` to take in additional `struct dir` argument, which is the directory it looks into for the file
	* Use `find_dir()` to find subdirectory and filename if it exists
	* Create new `struct file_pointer` and set `fd`, `name`, `is_dir`, either `file` or `dir`
	* Set inode’s parent and add to `file_list`
	* Increment fd count like before and return
* `close (int fd)` 
	* Use `get_file(fd)` to get `struct file_pointer` and check `file_pointer->is_dir`
	* If directory, call `dir_close()` on `file_pointer->dir`
	* If file, call `file_close() on `file_pointer->file`
* Exec
	* Modify `load()` to call`find_dir()` to get `dir` and `filename`, and call `filesys_open(filename, dir)` to get a `struct file` to execute 
* `bool remove (const char *file)` 
	* Edit `filesys_remove()` to take in additional `struct dir` argument
	* Get directory and name with `find_dir()`
	* For files, call `filesys_remove()` on dir and filename
	* For directories, use `dir_readdir()` to see if there are any directory entries and only continue if directory is empty
		* Go to `dir->inode->parent`, and call `dir_remove(dir->inode->parent, dir_name)` to remove directory
	* Don’t allow deletion of directory that is open by a process or is in use as a process’s current working directory
		* Check `dir->inode->open_cnt` to make sure only current process is using it
* `bool create (const char *file, unsigned initial size) `
	* Edit `filesys_create()` signature to take in an additional `struct dir` argument
	* Get directory and name with `find_dir ()` and call `filesys_create(name, size, dir)`

### Synchronization

Before calling an operation on whole files or directories (e.g remove()), acquire `file_lock` and then check `can_use()` in a loop. We keep waiting on `file_lock` if use conditions are not met with `cond_wait(cv, lock)`

After `can_use()` returns true, we set current level `in_use` to be true and `sub_use` for higher levels to be true, then signal and release `file_lock` so other threads can continue checking, then acquire `inode->lock` for inode operations

After operating on inode, we release `inode_lock`, acquire the `cv` and `file_lock` again and set parents’ `sub_use` to be false and own `in_use` to false, then release `file_lock`

Also acquire lock on `inode_disk->lock` before operating on inode disk data to enforce exclusion on same sectors of a file

Our shared resources are `inode` and `inode_disk` structures, since files and directories are created per process. We put locks in these structs to ensure only one process can modify them when necessary.

By adding boolean use states to the inode, we can check if operations working on overlapping disk sectors, files, or directories can be run at the same time. This lets us run concurrent file operations on different parts of the filesystem. The locks and condition variables for checking are used so that we won’t get race conditions.

### Rationale

We thought it would be best to have a helper function to parse the file name since we would have to call it in most of the syscalls. And we use the function given in the spec to parse, instead of a function like `strtok_r()` because it modifies the original string, which we don’t want. 
All other parts are pretty straight forward - ie. checking if `int fd` corresponds to a directory or a file to call the correct functions on it (`dir_open` vs `filesys_open`) and for syscalls only dealing with directories, we call the corresponding functions in `directory.c`.

## Additional Questions

### Write-behind cache

Write a `flush` function in cache.c that writes every dirty element in the hash table to disk. Then, we can call `flush` in `timer_sleep()` (with non-busy waiting) so flush whenever `timer_sleep` is called. And to prevent the cache being flushed too frequently (ie. if `timer_sleep()` is called every tick), we can add a variable to timer.c - `int64_t last_flush`, which keeps track of the last time that the cache was flushed. And if `timer_ticks() - last_flush > SOME_NUM`, then flush. If not, don’t flush. 

### Read-ahead cache

We can try to implement a read-ahead cache by loading most of a file into the cache on its first access on the assumption that processes will usually read a file at once without getting context switched out. In addition, blocks that haven’t been used recently will probably not be used for a while, so we can place cache blocks in a list and get rid of “older” blocks from the cache by dequeuing, and enqueuing blocks that have been used recently. 
