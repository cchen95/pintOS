Final Report for Project 3: File System
=======================================

### Student Testing Report

##### my-test-1

my-test-1 tests for hit rate in the cache. Two calls to read are made. After each, calls to `cache_hit_rate()` are made. `cache_hit_rate()` is a new syscall. In `cache.c`, there are 2 static variables `cache_hit `and `cache_miss` - `syscall.c` pulls both of these values in and then calculates `cache_hit`/(`cache_hit` + `cache_miss`) which is passed to `cache_hit_rate()`. Thus, the 2nd time `cache_hit_rate()` is called should return a much higher cache hit rate because of temporal recency. The message “Hit rate increases” is given if the 2nd `cache_hit_rate()` is higher and “Hit rate decreases” if the 2nd `cache_hit_rate()` is lower.

Output: 

(my-test-1) begin

(my-test-1) Hit rate increases

(my-test-1) end


Potential kernel bugs:

1. If the buffer cache doesn’t properly check if a sector already exists in the cache then the cache hit rate may not increase in consecutive reads of the same data.
2. If the buffer cache doesn’t properly add sectors in the cache when it’s empty, then the cache hit rate may not increase in consecutive reads of the same data.
3. If cache blocks are not evicted properly, then the cache hit rate may become unpredictable.

##### my-test-2

my-test-2 tests the cache’s ability to write without reading first. In devices/block we’ve added methods that return the `block->write_cnt` and `block->read_cnt`. 100 writes are made and then the number of reads and writes are returned.

Output

(my-test-2) begin

(my-test-2) create "a"

(my-test-2) open "a"

(my-test-2) 100 reads in 100 writes

(my-test-2) 73 writes in 100 writes

(my-test-2) end


Potential kernel bugs: 

I suppose technically we’ve failed this test because there should only be 1 read and 100 writes. Not sure why it’s doing this though. It’s possible that the ordering of the opens and closes in the test file is incorrect.

If the there are unnecessary calls to `block_read` every time a block is written to disk, then there would be too many reads (which is possible what happened)

The other explanation for why there are fewer than 100 write counts is that our kernel somehow fails to support extensible files, since we would actually exceed the maximum file size this way. This still doesn’t completely make sense because the other tests for extensible files pass.

##### Experience

Writing tests for Pintos is kind of a pain. In order to write the test for hit rate, we had to first implement the appropriate methods in cache.c and then update userprog/syscall.c and also update lib/user/syscall.c and lib/user/syscall.h as well. Then the tests utilize Perl to do the checking, which is also annoying because I wasn’t familiar with Perl to begin with. There are also many files - my-test-x.c, my-test-x.ck, and my-test-x-persistence.ck which meant there were many files to sort through.

However, writing tests did force me to think about the true output of syscalls and methods, since previously I mainly focused on getting tests to pass without really thinking about the true capability of Pintos. Afterwards, I was more able to comprehend the practical usage of Pintos instead of just trying to get tests to pass.

### Buffer Cache

Things we did differently from the design doc:

We used a statically allocated array for the data for the cache block of size `BLOCK_SECTOR_SIZE`. We didn’t use `acquire_cache_block` and `read_cache_block` because they weren’t necessary for synchronization since we already had locks. We also got rid of the hash list and only used 1 list to keep track of the cache blocks for efficiency. And we added code to initialize the cache by allocating 64 `struct cache_block` and pushing them to the lru list when the file system was being created and to free the allocated blocks when the filesys was being closed. 

### Extensible files

We added 1 indirect, 1 doubly indirect pointers and an array of size 120 for the direct pointer. We also had to modify `byte_to_sector` to calculate the correct sector the position corresponded with. 

And in `inode_open()`, instead of using `free_map_allocate()`, we added a new function called `inode_allocate()` that iterated through the direct pointers first to find open sectors, then the indirect and doubly indirect. To find free memory, we used `free_map_allocate(1)` to find 1 sector at a time so we could store a file at any sectors of the block. After finding the next free sector we zeroed the data out and after allocating all the sectors we needed, we wrote the new list of sectors back into their respective pointers. (e.g. `write_cache_block (disk_inode->indirect, &indirect, 0, BLOCK_SECTOR_SIZE);`) so that we would be able to read off of it to get the sector information. 

We also modified `inode_write_at()` so if EOF (e.g `offset + size > inode.data.length`) was reached, it would allocate more memory for the file. 

We also wrote functions to use when `inode_close()` was called to release all the sectors that the inode was currently using and to write back all the inode_disk information. `inode_release()` is similar to `inode_allocate()` in that it goes through all the direct, indirect, and doubly indirect pointers to release their sectors using `free_map_release()`.

## Subdirectories

We got rid of the global locks and used locks for inodes instead. We detailed a complicated locking algorithm in the design doc that we actually didn’t end up using, since almost all the file operations worked on disk sectors, and the checks for removing a directory were simpler (a directory can’t be removed if it’s not empty, so no need to check if subfiles or subdirectories were in use). We still kept inode locks that we acquired when changing inode data such as open counts, and added an `inode_list_lock` for the open inode list accesses. For synchronization, we just relied on cache block locking since each read/write operation goes through the cache, and a cache block holds the data for one sector. When opening an inode we read it from the cache, and when closing an inode, we would write its data back to disk as well.

We added `int is_dir` should to `struct inode_disk` since it had to be persistent, and also kept it in `struct file_pointer` to make syscall checking easier. We moved a parent pointer `block_sector_t parent` to `struct inode_disk` as well, and set it whenever a directory was added.

As per Josh’s suggestion, we created `.` and `..` entries in each directory as default at first, but removed them and hardcoded the cases in `dir_lookup ()` instead for efficiency. When checking if a directory has a parent, we check to see if it is not `ROOT_DIR_SECTOR`, and then retrieve it from `inode_disk` data.

### Reflection

Solah worked on all parts of the project and wrote most/all of the cache and extend. She also helped Sarah with some syscalls.

Sarah worked on subdirectories and adding/changing syscalls.

Chris wrote both tests and the associated syscalls, as well as a (mostly unused) first draft of extend.

We could have started the project a lot earlier so we wouldn’t have had to submit it late/stress about finishing it. Chris also learned about the dangers of git reset --hard. 