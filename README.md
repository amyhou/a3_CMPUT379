# CMPUT 379 Assignment 3: Trivial Unix File System - README

## Introduction
This document outlines the design choices made and system calls used to implement the trivial Unix file system. The code was implemented and tested on the lab machines.

## Acknowledgements
FileSystem.h was provided on eClass and has remained untouched.
The tokenize helper function was derived from the original version included in the
starter code of Assignment 1 on eClass.

## Assumptions
Input "size" and "block_num" values from input file must be digits.\
Size will be greater than 0 for fs_resize function.\
Empty lines in input file will be ignored.

## Implementation
### fs_mount
System calls used:
* open - use to (try) opening disk file into temporary file descriptor
* close - close temporary disk file descriptor
* dup2 - copy temporary file descriptor to global file descriptor

Function goes through 6 consistency checks and mounts disk only if it passes
all checks and no errors are reported.

Check 1:
Loops over each bit of the free block list of the superblock. The first bit should
be one to represent the superblock being in use. For all the following bits, we check if it is in use.\
If it is in use, we check each inode to see if the bit index (represents block index in memory) falls within range [start_block, start_block + file_size]. If it is, we make sure it is the first occurrence; if it's not, we report an error as this means the block belongs to two files.\
If it is not in use, we perform the same check to see if the block belongs to any files. If it does, we report an error.

Check 2:
For each inode in the superblock, we add its name to a names list in a map where the key is the inode parent directory. Before adding the name, we check if the name already exists for the parent directory; if so, report an error.

Check 3:


### fs_create

### fs_delete

### fs_read

### fs_write

### fs_buff

### fs_ls

### fs_resize

### fs_defrag

### fs_cd

## Testing
Ran the provided consistency checks to validate the fs_mount function, and received
the expected outputs to stderr.

Ran the sample tests 1-4 provided on eClass, and compared stdout and stderr to the result files given. Also ran a binary compare between the resultant disk file provided and the disk file that was manipulated. No differences were detected.

Ran with valgrind - still accessible blocks but those are due to using STL containers
