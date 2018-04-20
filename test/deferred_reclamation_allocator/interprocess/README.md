This test makes sure that we can place a `deferred_reclamation_allocator`
in a memory mapped file and load it from a different process. This allows
data structures using the allocator to be stored in memory mapped files.
