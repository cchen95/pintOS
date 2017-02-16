Design Document for Project 1: Threads
======================================

## Group Members

* Christopher Chen <christopher.chen95@berkeley.edu>
* Jimmy Cheung <jimmycheung2481@berkeley.edu>
* Sarah Au <sau@berkeley.edu>
* Solah Yoo <tjf009@berkeley.edu>


### Efficient Alarm Clock

#### Data structures and functions

* Add global variable `struct list sleeping_threads` in `thread.c`: a linked list of all threads that are sleeping
* Add field `sleep_ticks` to `struct thread`: a count of how many ticks left until thread wakes up


#### Algorithms

1. Putting a thread to sleep in `timer_sleep()`
    * Set `sleep_ticks` of current thread to number of ticks sleeping
    * Add current thread to `sleeping_threads` list
    * Call `thread_block()` to block thread and return control to scheduler, which chooses the next thread to run
2. Checking sleeping threads
    * `timer_interrupt()` runs for every tick of the OS clock
    * Every time `timer_interrupt()` is called, iterate through all threads in `sleeping_threads` with `thread_foreach()` and decrement their `sleep_ticks` count
3. Waking sleeping threads
    * If `sleep_ticks` reaches 0 for any thread, remove it from `sleeping_threads` 
    * Unblock any waking thread and then call `thread_yield()` after checking all threads, so scheduler can choose the next thread to run

#### Synchronization

The list `sleeping_threads` is not thread-safe, but the timer device is the only one that is manipulating it and accessing it. The timer device is also the only one manipulating any thread’s `sleep_ticks`, and it does so on every tick of the clock. The code that iterates through the list of `sleeping_threads` happens on every tick. We disable interrupts right before iterating and updating the thread sleep count and enable them again after unblocking waking threads to avoid synchronization issues. 

#### Rationale

We needed a way to keep track of when to wake up sleeping threads so we used a list to store them and another variable to keep track of the time left to sleep. Some alternative methods were to not use a sleeping threads list but to check if threads’ `sleep_ticks` > 0 but that would have a greater runtime of O(m) where m is the number of threads.
Time complexity = O(n), where n is the number of sleeping threads
Space complexity = O(n)

### Priority Scheduler

#### Data structures and functions

`struct thread` variables
* `int basePriority`: If priority was donated, save the original priority to `basePriority`.
* `bool donated`: If priority was donated, this value is true. Else, this value is false.
* `struct lock *heldLocks`: list of locks that the thread currently holds.
* `struct lock blocker`: the lock that is currently blocking this thread.
`struct lock`: Variables
* `int priority`: The highest priority out of the threads waiting for this lock.

`thread.c` methods
* Modify `thread_unblock()` to use `list_insert_ordered()` instead of `list_push_back()` on `ready_list()`.
* Modify `thread_yield()` to use `list_insert_ordered()` instead of `list_push_back on ready_list()`.
* Modify method `next_thread_to_run()`
  * Threads can be assumed to be sorted (`list_insert_ordered()`)
  * Find the highest “effective” priority thread to run
* Create method `void updatePriority(int newPriority, struct thread *thread)`, which updates the priority with a new “donated priority” and calls recursively to apply this donated priority.
* Modify `void thread_set_priority(int new_priority)` to account for donated priorities and original priorities.
* Modify `void thread_get_priority(int new_priority)` to account for donated priorities and original priorities.

Lock Methods
* Modify `void lock_acquire(struct lock *lock)` to call `updatePriority`, set blocker and unset blocker before and after lock is acquired.
* Modify `void lock_release(struct lock *lock)` to update the priority of the lock, as well as update the `heldLocks` struct and use the `heldLocks` struct to set the thread’s new priority.
#### Algorithms

1) Acquiring a lock

Let’s set up some variables - `cur` refers to current thread, and `hold` refers to `lock->holder`. We call the new method, `updatePriority` on the holder object, passing in the new priority. This function recursively checks for locks that are being waited on, and updates their priorities accordingly. This includes the priorities of the locks, as well as the threads.

* If the lock is not acquired, find the thread `*holder` ptr.
* Call `updatePriority(int newPriority, *thread)` on the holder
  * Sets `basePriority` (non donated) = `priority` and `priority` = `newPriority` if `newPriority` > `priority`
  * Sets `donated` = `true` if `newPriority` > `priority`
  * Find the value at `blocker` for the current thread. (Iterates through `lockList` for each lock) For the blocker lock, look at the holder ptr for the lock, and call `updatePriority(newPriority, holder)`.
* Set `blocker` to the lock parameter right before the `sema_down()` call.
* Set `blocker` to null right after the `sema_down()` call. The lock is no longer blocking the thread.

2) Releasing a lock

First, disable interrupts. Set `lock->holder` to Null. If not using the MLFQS algorithm, then continue. Each thread has a list of locks that it is currently holding. Remove the lock that is to be released. If that list is now empty, then set thread priority to its original (non donated) priority and we’re good. If that list is not empty, then we need to find the lock with that max priority (which we most likely will have implemented as a sorted list, so just take the first element). We then set the current thread’s priority to the higher of the max priority or original priority.  If we set the original priority, donated is false. If we set the max priority (max > original), donated is true. Determine whether we need to yield the thread or not

* In `lock_acquire()`, after acquiring the lock (`holder -> currentThread()`), update the priority of the lock to the new thread
* Remove the lock from the thread’s `heldLocks`
* Update the priority of the current thread based on the `heldLocks` it has, setting it to the highest priority lock. Update donated correspondingly.
* In `lock_release()`, after releasing it, set lock priority to 0.
* Check at the priority of the current thread relative to the priorities of all the threads in the “ready” stage.
* If this one has less priority than one of the ready stages, yield the thread. Else, continue running on this thread.

3) Getting and setting priority

* `thread_set_priority()`: `cur` is current thread. If `cur->donated` is false, then just set the priority (we can directly set the original priority). Else, if `new_priority` is less than `cur->priority`, then we can set `cur->base_priority` to be new_priority. If not, then set `cur->priority` and `cur->base_priority` to `new_priority`. If `cur->status` is `THREAD_RUNNING` and `ready_list` is not empty, then find the highest priority ready thread (`ready_list` can be an ordered list? and yield if that thread has higher priority than `cur->priority`.
* `thread_get_priority()`: `cur` is the current thread. If `cur->donated` is `true`, then return `cur->priority`. Else, return `cur->basePriority`.

#### Synchronization

Race conditions might happen in a variety of ways - we were worried about threads trying to set priorities at the same time, threads trying to change their priority at the same time as a priority donation, or list data structures having inconsistent data due to different thread execution timings. For this, we disabled interrupts where priorities of threads were being set. We tried to avoid using list structures unnecessarily. Threads do hold a list of locks currently held but that data isn’t being modified by what other threads do, and is used solely for addressing multiple donations, so we think that race conditions were avoided there. For donations and priority scheduling we primarily use locks - most of the logic for donation is in `lock_release()` and `lock_acquire()`. They help us ensure that threads are blocked and executed at the right time (based on priority).


#### Rationale

We believe that our design is simple, yet provides us the flexibility to adapt and extend our design if needed. A couple of things were the most difficult, and took up the most time to design. These were some of the most challenging questions:

1. How will threads know what priority to revert to after a lock has been released?
2. How can a thread know where to recursively donate?
3. Should we store a list of waiting threads in lock? If not, how will a thread holding a lock know the priority of that given lock? (E.x. Thread A and Thread C blocked by Thread B, who hold locks A and C. Lock A may have different priority than lock C).
4. Should we store locks that are forcing the thread to wait? How should we store them?
  
Our code aims to change as little as possible, to eliminate as many potential problem points as possible. As a result, we’ve attempted to use simple data structures as often as possible. To avoid potential issues with synchronization, we attempted to avoid lists when possible. However, we believe that heldLocks is the exception; we must have a list of currently held locks for several reasons, including question 1 and 3 above. 

Having locks that hold the priority also causes our biggest shortcoming. How do we update the priority of a held lock, after it has been released and acquired by another thread? We can’t rely on the priority donation code to do the updating, as lock_acquired could have been called a long time ago, waiting for the lock to be released. A potential solution is to put the blocked lock priority as a variable in threads, and update the lock priority immediately after the thread has acquired the lock. This idea needs to be investigated further though.

Adding elements to lists will most likely take some additional time, as we try to maintain sorted lists so that accessing elements (which is done in scheduler), will be fast and the transition smooth. Recursive calls done in donations will most likely cause a few more stack frames than an iterative solution, but we believe that the cleanliness of the code will make up for that tradeoff.

**Assumptions**:

If thread A releases a lock, that implies that it cannot be waiting for a lock. - 99% sure this is true

Each thread cannot be waiting on more than one lock held by the same thread. - True because #3 is true.

For the paragraph description of lock_acquire, a thread can have at most 1 blocker. - confirmed by Josh

**Potential issue**: 

We set lock priority to 0 when the lock is released. If other threads are already waiting for a lock, it could acquire the lock, release a separate lock it owns, and then set its own priority to 0. Maybe stored the blocked lock priority?


### Multi-level Feedback Queue Scheduler (MLFQS) 

#### Data structures and functions
#### Algorithms
#### Synchronization
#### Rationale

### Additional Questions
1) 
2)


timer ticks | R(A) | R(B) | R(C) | P(A) | P(B) | P(C) | thread to run
------------|------|------|------|------|------|------|---------
0          |   0   |   0   |  0    |  63    |  61    |  59    |A
4          |   4   |   0   |  0    |  62    |   61   |  59    |A
8          |   8   |   0   |  0    |  61    |   61   |  59    |A
12         |   12  |  0    | 0     |  60    |   61   | 59     |B
16         |   12  |  4    | 0     |  60    |   60   | 59     |B
20         |   12  |  8    | 0     |  60    |   59   | 59     |A
24        |   16  |  8    | 0   |   59   |   59   |  59    |A
28        |   20  |   8   | 0   |   58   |   59   |  59    |C
32        |   20  |   8   | 4   |    58  |   59   |  58    |B
36        |   20  |   12  | 4   |    58  |   58   | 58     |B


3) There are ambiguities when 2 or more threads have the same highest priorities. In this case, keep running the current thread if it is one of the threads with the highest priority (don’t yield since that requires more work, ie. calling `schedule()`, etc). If the threads with the same highest priorities aren’t one of the running threads, choose the thread that has been least recently run.  