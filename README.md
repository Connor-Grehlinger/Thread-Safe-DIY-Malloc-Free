A DIY implementation of thread-safe malloc and free functions using best-fit policy. 

One thread-safe malloc & free function pair (ts_malloc_lock and ts_free_lock) uses mutual exclusion locks to synchronize 
access to a data structure which manages freed blocks. The other thread-safe malloc & free function pair (ts_malloc_nolock and 
ts_free_nolock) uses thread-local storage to eliminate the need for mutual exclusion locks. 

The included report discusses the tradeoffs involved with each malloc & free implementation.
