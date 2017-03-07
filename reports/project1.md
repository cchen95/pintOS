Final Report for Project 1: Threads
===================================
### Changes

#### Efficient Alarm Clock

We kept a sorted list of sleeping threads instead of an unsorted one, as we discussed with our TA during the design review. Instead of storing the time left to sleep, we stored the timer tick the thread should wake in a thread variable `time_to_wake`. At each timer tick, we check the front of the list to see if the first thread’s `time_to_wake` is equal to the current time, so we don't have to iterate through all sleeping threads, but only a few. If we find an equal time, we dequeue the front of the list, unblock the thread, and keep checking the front for equal wake times. We also moved the `sleeping_threads_list` into `timer.c`. When `sleeping_threads_list` was in `thread.c` we received a page fault on a test, probably because of a memory error. We also wrote a comparator `wake_value_less`, that compares the wake times of threads in `sleeping_threads_list` so we could insert ordered into the list.

#### Priority Scheduler

We ended up not using a donated variable in the thread, since that can easily be calculated whenever it’s needed by checking whether its current priority is equal to its base priority. We had also planned on keeping the `ready_list` sorted and using `list_insert_ordered` instead of `list_push_back` but it turns out that it isn't necessary; at one point we planned on just finding the highest priority of the threads within that list for `next_thread_to_run` but it ended up being easier to just use `list_sort` before calling `list_pop_front`. This did mean that we had to implement some comparison functions. This behavior is mirrored in the `waiters_list` for semaphores, although we did use a `list_max` call in addition to `list_sort` when we could since it has a faster runtime. During our design review, it was suggested that we didn’t need to use a priority for our struct lock since that priority could be calculated, but we ended up doing so anyways since it greatly cleaned up priority donation when releasing locks. We also abstracted priority donation out into 2 separate methods (`thread_update_priorities` and `thread_donate_priority`) instead of encoding the logic in multiple places (`thread_get_priority` and `lock_release/lock_acquire`). In addition to what we planned to do in our design doc for `synch.c`, we also adjusted `sema_up` to unblock the next thread to run.

#### Multi-level Feedback Queue Scheduler (MLFQS) 

Instead of using a list of 64, we decided to use an unsorted list (`ready_list`), which would save runtime and space as we wouldn’t have to check empty queues. We updated the threads' priorities as described in the spec. So to get the highest priority thread, we used `list_max` with a function we defined to compare the threads’ priorities (`priority_less`). If multiple threads had the same priority, we would run the first occurring one. If a thread was switched out, it would be pushed back to the back of the list. 

### Reflection

Jimmy and Chris worked on the priority scheduler. They pair coded both together and remotely. After the main logic was implemented, they both debugged until `priority_fifo` passed. Afterwards, Chris worked on getting the remaining tests to pass, which involved re-writing some of the logic and abstracting some implementation into its own methods. 
Solah worked on writing the main parts of MLFQS (ie. functions to calculate niceness, average cpu, etc., algorithm in `timer_tick()` and `next_thread_to_run()`) and both Sarah and Solah debugged to get all the tests to pass. Sarah worked on implementing the efficient alarm clock.

If there was anything to improve, we wished that we started on the project sooner. Overall, we also found that abstracting logic into their own functions helped a lot with debugging the code.