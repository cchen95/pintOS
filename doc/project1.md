Design Document for Project 1: Threads
======================================

## Group Members

* Christopher Chen <christopher.chen95@berkeley.edu>
* Jimmy Cheung <jimmycheung2481@berkeley.edu>
* Sarah Au <sau@berkeley.edu>
* Solah	Yoo <tjf009@berkeley.edu>


### Efficient Alarm Clock

##### Data structures and functions
##### Algorithms
##### Synchronization
##### Rationale

### Priority Scheduler

##### Data structures and functions
##### Algorithms
##### Synchronization
##### Rationale

### Multi-level Feedback Queue Scheduler (MLFQS) 

##### Data structures and functions
##### Algorithms
##### Synchronization
##### Rationale

### Additional Questions
1) 
2)

| timer ticks | R(A) | R(B) | R(C) | P(A) | P(B) | P(C) | thread to run |
|-------------|------|------|------|------|------|------|---------------|
| 0           | 0    | 0    | 0    | 63   | 61   | 59   | A             |
| 4           | 4    | 0    | 0    | 62   | 61   | 59   | A             |
| 8           | 8    | 0    | 0    | 61   | 61   | 59   | A             |
| 12          | 12   | 0    | 0    | 60   | 61   | 59   | B             |
| 16          | 12   | 4    | 0    | 60   | 60   | 59   | A             |
| 20          | 16   | 4    | 0    | 59   | 60   | 59   | B             |
| 24          | 16   | 8    | 0    | 59   | 59   | 59   | A             |
| 28          | 20   | 8    | 0    | 58   | 59   | 59   | B             |
| 32          | 29   | 12   | 0    | 58   | 58   | 59   | C             |
| 36          | 20   | 12   | 4    | 58   | 58   | 58   | A             |

3) 