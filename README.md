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
### System calls used:
* open - use to (try) opening disk file into temporary file descriptor
* close - close temporary disk file descriptor
* dup2 - copy temporary file descriptor to global file descriptor
* read - get memory blocks of disk file
* write - write to memory blocks of disk file
* lseek - move around disk file

### fs_mount
Mount function goes through 6 consistency checks and mounts disk only if it passes all checks and no errors are reported. We initally read the superblock of the disk file into a temporary Super_block struct.

**Check 1**:
Loops over each bit of the free block list of the superblock. The first bit should
be one to represent the superblock being in use. For all the following bits, we check if it is in use.\
If it is in use, we check each inode to see if the bit index (represents block index in memory) falls within range [start_block, start_block + file_size]. If it is, we make sure it is the first occurrence; if it's not, we report an error as this means the block belongs to two files.\
If it is not in use, we perform the same check to see if the block belongs to any files. If it does, we report an error.

**Check 2**:
Looping over each inode in the superblock, we add its name to a names list in a map where the key is the inode parent directory. Before adding the name, we check if the name already exists for the parent directory; if so, report an error. During this check, we also save some of the inode information to a map. The map has keys as index of directory inode, and values as vectors of names and inode indexes of the directory's items.

**Check 3**:
Looping over each inode, if the in use is marked as "not in use", we ensure all name bits, used size, start block, and parent directory fields are all 0s.\
If it is marked as in use, we check to make sure at least one name field bit must not be 0 and append the inode index to a list of inodes in use.

***Check 4, 5, 6 (respectively)***:
Using the list from Check 3 to loop over each inode in use, we do the following. First, we check if inode is for a file. If so, we perform check 4. Otherwise, we perform check 5.

**Check 4**: first check the value stored in start_block member. If it does not fall between 1 and 127, we report an error, mark that check 4 failed, and break from the loop.

**Check 5**: check if either size or start_block is not zero. If it isn't zero, we mark that check 5 failed and continue on with the loop.

If check 4 passed, we continue onto check 6. At this point we know if we passed check 3, then only an in-use inode can have parent directory set to a value. This is why we can still use the same for-loop.

**Check 6**: check parent inode; return error if it is outside range [0,127] or equal to 126. If it passed these conditions, get parent inode information and make sure it's marked in use and marked as directory. If any conditions were not met, we check if we have previously marked any checks as failed. If not, we mark that check 6 failed.

If all 6 checks have passed, we mount the disk by saving changes of previously mounted disk to its file before then saving tempSuperblock into global superblock variable.

Otherwise, we leave the current global superblock as is and return.

### fs_create
If a disk is mounted....\
We check if the given file name is ".", "..", or already exists in the current working directory. If not, we check the given size. If size is 0, we make a directory: store all attribute information into first free inode If size is in range [1, 127] then it's a file: we iterate through the free block list until we either find N consecutive free blocks to store a file of size "size", or report an error. If it can be stored, we store all attribute info into the first free inode.

### fs_delete
If a disk is mounted....\
If the given name is in the list of names within the current directory, we get the inode for the named file/directory. If it's a directory, we recursively call fs_delete for each name in that directory. Otherwise, if it's a file, we zero out the inode and the file blocks. Afterwards, we update our directory mappings.

### fs_read
If a disk is mounted....\
First check if the given name exists inside the current directory. If so, check if the corresponding inode is that of a file. If so, we read the specified block of the file (using block_num) into the buffer.

### fs_write
If a disk is mounted....\
First check if given name exists inside current directory. If so, check if the corresponding inode is that of a file. If so, we write the current buffer contents to the specified block of the file.

### fs_buff
If a disk is mounted....\
We flush the buffer, and then copy into it new buff characters.

### fs_ls
If a disk is mounted....\
First prints "." and ".." directories, then uses map to get list of items inside current directory, sorted by inode index (ascending). Looping over each, if current item to be printed is directory we get the number of items inside it using the map and also print that. If it's a file, we print the file size in addition to its name.

### fs_resize
If a disk is mounted....\
First check if name is for a file within current directory. If so, continue.
If the new size of the file is equal to old size, we return. If the new size is less than old size, we update the file start block and size, and zero out the tail blocks no longer within the scope of the new file size. If the new size is greater than old size, we first check if we can append the new blocks to the tail of the original file blocks in memory. If there is, we do so, mark the blocks as in use, update the size of the inode, and return. If there are not enough free blocks right after the file, we temporarily "remove" the file (zero out the file blocks' bits in free block list). Then we go through and look for the first instance of enough consecutive free blocks for the new size. If such an instance does not exist, we print an error and return. If such an instance exists, we move start block of inode to the beginning of this free blocks section, and copy all blocks of file from old blocks to new blocks. Then we update the inode size, start block, free block list, and return.

### fs_defrag
If a disk is mounted....\
We use a priority queue to insert and sort all inodes of superblock with start block greater than 0 by start block. Then, while queue is not empty, we get the top inode and pop it off the queue. We find the first free block; if such a block doesn't exist, the memory is full and we return. Otherwise, after finding the first free block, we see if the inode start block is smaller. If so, it's already at lowest possible block and we continue to the next inode in queue. If not, we move the start block to the first free block and shift all blocks of memory from old locations to new locations. Then we update the inode information, set index to start searching for first free block from to end of current inode's used blocks, and continue to the next inode in queue.

### fs_cd
If a disk is mounted....\
If given directory name to change into is ".", return as we're already in current directory. If given name is ".." and current directory is not root, we get parent directory from the inode for the current directory and change to it. Otherwise, we go through all of the child inodes of current directory to see if one has the given name. If so, we check if it's a directory and change current directory to it if it is. Otherwise, report an error and return;

### Parsing input file
If not exactly one input file was provided we print and error statement and return. Otherwise, we read one line at a time from the input file. If a empty line is read, we ignore it. For each command read, we first tokenize it with spaces as the deliminator. If the first token is "B", we are trying to update the buffer so all characters following B are saved into second token. Otherwise, tokenize completes parsing and returns the command and its arguments.\
We check to see if the given command was provided the right number of arguments, and that the arguments meet any restrictions placed on them. Any time a file/directory name is provided, we do a check to make sure it is 5 or less characters. Any time a block number is provided, we make sure it's in range [0, 126]. Any time a file size is provided, we make sure it's in range [0, 127].


### Helper functions
I created the following helper functions to improve overall readability of the code, and save lines of code when a certain procedure had to be repeated often.

**Tokenize**: returns array of character arrays split by deliminator (only splits once if first token is "B").

**getFreeBlockBit**: returns value of specified block *n* in the free block list of the superblock.

**getFreeBlockBit**: sets value of specified block *n* in the free block list of the superblock to given value.

**inodeIsDirectory**: checks if most significant bit of dir_parent byte is set (directory) or not (file).

**getIndexInDir**: returns index within map for file/directory with the given name in the current directory. Returns -1 if no such file/directory found.

**getInodeIndex**: returns index of inode within superblock with the given map index value.

**getFileSize**: gets fileSize from inode of a given index in the superblock.

**getStartBlock**: gets start block index from inode of a given inode index in the superblock.


## Testing
I tested my implementation in the following ways:
* Ran the provided consistency checks to validate the fs_mount function, and received
the expected outputs to stderr.

* Ran the sample tests 1-4 provided on eClass, and compared stdout and stderr to the result files given. Also ran a binary compare between the resultant disk file provided and the disk file that was manipulated. No differences were detected.

* Created my own test case input files to cover every single edge case outlined in the assignment description. Edge cases included testing command formats; creating nested directories with files inside and deleting them recursively; creating many small files, resizing some and deleting every other one before defrag-ing.

* Ran with valgrind - still reachable blocks are present but those are due to using C++ STL containers
