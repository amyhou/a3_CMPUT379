#include "FileSystem.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <bitset>
#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <queue>

using namespace std;

/* Questions
    Will you test with non-number inputs as the "size" arguments?
*/

/* -------------------------------- MACROS ---------------------------------- */
#define MAX_INPUT_LENGTH (1050)  // Maximum length of input
#define BLOCK_SIZE (1024)        // 1 KB

/* ------------------------- STRUCTURE DEFINITIONS -------------------------- */
/* Struct for additional info about disk file */
typedef struct {
  int currWorkDir;                       // current working directory
  string diskName;                       // name of disk file mounted
	map<int, vector<string> > directories; // key: parent dir num, val: names of items inside
  map<int, vector<int> > dirChildInodes; // key: parent dir num, val: inode indexes of items inside
  vector<int> freeInodeIndexes;          // list of free inodes
} Disk;

/* ---------------------------- GLOBAL VARIABLES ---------------------------- */
uint8_t buffer[BLOCK_SIZE];   // buffer of 1KB
int fsfd;                     // file descriptor of emulator disk file currently mounted
Super_block superblock;       // superblock of disk file currently mounted
Disk info;                    // additional information about the disk file
bool fsMounted = false;       // indicates if a disk is currently mounted

/* -------------------------- FUNCTION DEFINITIONS -------------------------- */
/* Helper Functions ----------------------------------------------------------*/
void tokenize(char* str, const char* delim, char ** argv)
{
  /* Takes character array and splits into tokens, with splits occuring
     at delim characters. If first token is "B", we save all characters
     following it to be the second argument.
  */
  char* token;
  token = strtok(str, delim);

  // If updating buffer, everything following 'B' is part of input to buffer
  if (strcmp(token, "B") == 0)
  {
    argv[0] = token;
    token = strtok(NULL, "");
    argv[1] = token;
    return;
  }

  // Otherwise, split into arguments
  for(size_t i = 0; token != NULL; ++i){
    argv[i] = token;
    token = strtok(NULL, delim);
  }
}

bool getFreeBlockBit(int n)
{
  /* Return bit with index n from the free block list of superblock */
  bitset<8> curByte;
  int modCount = n % 8;
  curByte = bitset<8>((unsigned char)superblock.free_block_list[n/8]);
  int bitIdx = 7 - modCount;
  return (curByte[bitIdx]);
}

void setFreeBlockBit(int n, int val)
{
  /* Set free block bit at index n to value of val */
  bitset<8> curByte;
  int modCount, bitIdx;

  modCount = n % 8;
  curByte = bitset<8>((unsigned char)superblock.free_block_list[n/8]);
  bitIdx = 7 - modCount;
  curByte[bitIdx] = val;
  superblock.free_block_list[n/8] = (unsigned char) curByte.to_ulong();
}

bool inodeIsDirectory(int inodeIndex)
{
  /* Checks if stored member in inode is a directory */
  return bitset<8>(superblock.inode[inodeIndex].dir_parent)[7];
}

int getIndexInDir(char name[5])
{
  /* returns -1 if no dir or file with name is in current directory */
  vector<string>::iterator it = find(info.directories[info.currWorkDir].begin(), info.directories[info.currWorkDir].end(), string(name));

  // Check if specified file or directory is in current working directory
  if ( it == info.directories[info.currWorkDir].end() )
  {
    return -1;
  }
  return distance(info.directories[info.currWorkDir].begin(), it);
}

int getInodeIndex(int sharedIdx)
{
  /* Returns inode number in superblock given its index in the map */
  return info.dirChildInodes[info.currWorkDir][sharedIdx];
}

int getFileSize(int inodeIndex)
{
  /* Returns size of file of given inode */
  return (superblock.inode[inodeIndex].used_size & 0x7F);
}

int getStartBlock(int inodeIndex)
{
  /* Returns start block of file of given inode */
  return (superblock.inode[inodeIndex].start_block);
}

class cmpStartBlock
{
  /*
    Comparator class for sorting a priority queue by start_block
    with smallest start_block first in the queue.
  */
  public:
    bool operator()(int a, int b)
    {
      return (superblock.inode[a].start_block > superblock.inode[b].start_block);
    }
};

/* Required Functions ------------------------------------------------------- */
void fs_mount(char *new_disk_name)
{
  /* fs_mount performs 6 consistency checks on the disk file provided.
     Mounts the disk if all checks pass.
     Input: new_disk_name - name of the disk file being mounted
     Output: None
  */
  // Check for file existence in current directory
  int fd = open(new_disk_name, O_RDWR);

  if (fd < 0)
  {
    fprintf(stderr, "Error: Cannot find disk %s\n", new_disk_name);
    return;
  }

  // Consistency checks
  bool consistent = true;
  Super_block tempSuperblock;
  Disk tempInfo;
  int inconsistency = 0;

  read(fd, &(tempSuperblock), BLOCK_SIZE);

  /* CONSISTENCY CHECK 1 ---------------------------------------------------- */
  int freeByteCounter = 0;
  bitset<8> curByte;
  while (freeByteCounter < 128)
  {
    int modCount = freeByteCounter % 8;
    if (modCount == 0)
    {
      curByte = bitset<8>((unsigned char)tempSuperblock.free_block_list[freeByteCounter/8]);
    }

    int bitIdx = 7 - modCount;

    // Superblock must not be free in free_block_list
    if (freeByteCounter == 0)
    {
      if (curByte[bitIdx])
      {
        freeByteCounter++;
        continue;
      } else
      {
        consistent = false;
        inconsistency = 1;
        break;
      }
    }

    // If not looking at superblock bit
    bool properAlloc = false;

    // get index range using size and start index indicated in inodes
    for (int i = 0; i < sizeof(tempSuperblock.inode)/sizeof(Inode); i++)
    {
      int tempUsedSize, lowerLim, upperLim;
      tempUsedSize = bitset<8>(tempSuperblock.inode[i].used_size & 0x7F).to_ulong();
      lowerLim = tempSuperblock.inode[i].start_block;
      if (tempUsedSize > 0)
      {
        upperLim = lowerLim + tempUsedSize - 1;
      } else // could be a directory
      {
        upperLim = 0;
        if (bitset<8>(tempSuperblock.inode[i].dir_parent)[7]) // directory
        {
          continue; // skip it as we're checking if marked blocks belong to files
        }
      }

      if (curByte[bitIdx]) // block is in use
      {
        if ((freeByteCounter >= lowerLim) && (freeByteCounter <= upperLim))
        {
          if (!properAlloc)
          {
            properAlloc = true;
          }
          else // block is used by more than one file
          {
            // cout << "block used by more than one file" << endl;
            properAlloc = false;
            break;
          }
        }
      }
      else // if block is supposed to be free
      {
        if ((freeByteCounter >= lowerLim) && (freeByteCounter <= upperLim))
        {
          properAlloc = false;
          break;
        }
        else
        {
          properAlloc = true;
        }
      }
    }

    if (!properAlloc)
    {
      consistent = false;
      inconsistency = 1;
      break;
    }

    freeByteCounter++;
  }

  if (!consistent)
  {
    fprintf(stderr, "Error: File system in %s is inconsistent (error code: %d)\n", new_disk_name, inconsistency);
    close(fd);
    return;
  }

  /* CONSISTENCY CHECK 2 ---------------------------------------------------- */
  // For each dir, check if name already in vector before adding to vector
  for (int i = 0; i < sizeof(tempSuperblock.inode)/sizeof(Inode); i++)
  {
    // Check if inode is marked "in use"
    bitset<8> tempBitset = bitset<8>((int)tempSuperblock.inode[i].used_size);
    if (tempBitset[7] == 0)
    {
      continue;
    }

    // Use a multimap Map<directory number, vector <string>>
    uint8_t tempDir = tempSuperblock.inode[i].dir_parent & 0x7F;
    // printf("Using inode %d\n", i);
    char tempName[6] = {tempSuperblock.inode[i].name[0], tempSuperblock.inode[i].name[1], tempSuperblock.inode[i].name[2], tempSuperblock.inode[i].name[3], tempSuperblock.inode[i].name[4], '\0'};
    string strName = string(tempName);

    if (tempInfo.directories.find(tempDir) == tempInfo.directories.end())
    {
      // add parent directory to map
      vector<string> vecName;
      tempInfo.directories[tempDir] = vecName;
      tempInfo.directories[tempDir].push_back(strName);

      vector<int>vecName2;
      tempInfo.dirChildInodes[tempDir] = vecName2;
      tempInfo.dirChildInodes[tempDir].push_back(i);
    }
    else
    {
      // parent directory already key; check and append if name not in vector
      if (find(tempInfo.directories[tempDir].begin(), tempInfo.directories[tempDir].end(), strName) != tempInfo.directories[tempDir].end())
      {
        consistent = false;
        inconsistency = 2;
        break;
      }
      else
      {
        tempInfo.directories[tempDir].push_back(strName);
        tempInfo.dirChildInodes[tempDir].push_back(i);
      }
    }
  }
  if (!consistent)
  {
    fprintf(stderr, "Error: File system in %s is inconsistent (error code: %d)\n", new_disk_name, inconsistency);
    close(fd);
    return;
  }

  /* CONSISTENCY CHECK 3 ----------------------------------------------------- */
  vector<int> availableInodes;
	vector<int> unavailableInodes;
  for (int i = 0; i < sizeof(tempSuperblock.inode)/sizeof(Inode); i++)
  {
    // Check if inode is marked "in use"
    bitset<8> tempBitset = bitset<8>(tempSuperblock.inode[i].used_size);
    if (tempBitset[7] == 0)
    {
      // Make sure all members of inode struct are 0
      if ((tempSuperblock.inode[i].name[0] != 0) ||
          (tempSuperblock.inode[i].name[1] != 0) ||
          (tempSuperblock.inode[i].name[2] != 0) ||
          (tempSuperblock.inode[i].name[3] != 0) ||
          (tempSuperblock.inode[i].name[4] != 0) ||
          (tempSuperblock.inode[i].used_size != 0) ||
          (tempSuperblock.inode[i].start_block != 0) ||
          (tempSuperblock.inode[i].dir_parent != 0)
          )
      {
          consistent = false;
          inconsistency = 3;
          break;
      }

      // Add to free inodes list
      tempInfo.freeInodeIndexes.push_back(i);
    }
    else
    {
      if ((tempSuperblock.inode[i].name[0] == 0) &&
          (tempSuperblock.inode[i].name[1] == 0) &&
          (tempSuperblock.inode[i].name[2] == 0) &&
          (tempSuperblock.inode[i].name[3] == 0) &&
          (tempSuperblock.inode[i].name[4] == 0)
          )
      {
          consistent = false;
          inconsistency = 3;
          break;
      }
			unavailableInodes.push_back(i);
    }
  }
  if (!consistent)
  {
    fprintf(stderr, "Error: File system in %s is inconsistent (error code: %d)\n", new_disk_name, inconsistency);
    close(fd);
    return;
  }

  /* CONSISTENCY CHECKS 4, 5, and 6 ----------------------------------------- */
	for (int i = 0; i < unavailableInodes.size(); i++)
	{
		// Check only for inodes marked "in use"
		uint8_t tempDirParent = tempSuperblock.inode[unavailableInodes[i]].dir_parent;
		bitset<8> tempBitset = bitset<8>(tempDirParent);
		if (!tempBitset[7]) // is file, start_block must be between 1 and 127
		{
			if ((tempSuperblock.inode[unavailableInodes[i]].start_block < 1) ||
					(tempSuperblock.inode[unavailableInodes[i]].start_block > 127))
			{
				consistent = false;
				inconsistency = 4;
				break;
			}
		}
		else // is dir, size and start_block must both be 0
		{
			if ( ((tempSuperblock.inode[unavailableInodes[i]].used_size & 0x7F) != 0) ||
					 (tempSuperblock.inode[unavailableInodes[i]].start_block != 0))
			{
				consistent = false;
				inconsistency = 5;
			}
		}

		int tempParentNodeIdx = (int) (tempDirParent & 0x7F);
		if ( (tempParentNodeIdx != 127) && ( (tempParentNodeIdx < 0) || (tempParentNodeIdx > 125) ) )
		{
			// Not valid parent inode index
		  consistent = false;
		  if (inconsistency == 0)
		  {
		    inconsistency = 6;
		  }
		}
		else if ( (tempParentNodeIdx != 127) && ( (tempParentNodeIdx >= 0) || (tempParentNodeIdx <= 125) ) )
		{
			// Check if parent inode is in use and marked as directory
		  bitset<8> tempParentDirParent = bitset<8>((int)tempSuperblock.inode[tempParentNodeIdx].dir_parent);
		  bitset<8> tempParentUsedSize = bitset<8>((int)tempSuperblock.inode[tempParentNodeIdx].used_size);

		  if ( (!tempParentDirParent[7]) || (!tempParentUsedSize[7]) )
		  {
		    consistent = false;
        if (inconsistency == 0)
        {
          inconsistency = 6;
        }
		  }
		}
	}

	if (!consistent)
	{
		fprintf(stderr, "Error: File system in %s is inconsistent (error code: %d)\n", new_disk_name, inconsistency);
		close(fd);
		return;
	}

  // Mount that sucker
  if (fsMounted)
  {
    lseek(fsfd, 0, SEEK_SET);
    write(fsfd, &superblock, BLOCK_SIZE);
    close(fsfd);
  }

  superblock = tempSuperblock;
	lseek(fd, 0, SEEK_SET); // return fp to point to beginning of file because why not?
  dup2(fd, fsfd);
  fsMounted = true;
  tempInfo.currWorkDir = 127; // set working directory to root
  tempInfo.diskName = string(new_disk_name);
  info = tempInfo;

	close(fd); // close temp fp

}

void fs_create(char name[5], int size)
{
  /* fs_create creates a file of a given size. If the given size is 0, it
     creates a directory. When creating a file, we use the first available
     inode to store the attributes.
     Input: name - name of the directory or file being mounted
            size - size of the file. If 0, creating a directory.
     Output: None
  */
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted\n");
    return;
  }

  int neededBlocks = size;

  if (info.freeInodeIndexes.empty())
  {
    fprintf(stderr, "Error: Superblock in disk %s is full, cannot create %s\n", info.diskName.c_str(), name);
  }

  vector<string> tempDirs = info.directories[info.currWorkDir];

  char tempName[6] = {name[0], name[1], name[2], name[3], name[4], '\0'};

  if ( (find(tempDirs.begin(), tempDirs.end(), string(tempName)) != tempDirs.end()) ||
       (strcmp(name, ".") == 0) || (strcmp(name, "..") == 0) )
  {
    // Not unique name in this directory
    fprintf(stderr, "Error: File or directory %s already exists\n", name);
    return;
  }

  if (size == 0) // creating a directory
  {
    // Store attributes into first available inode
    Inode tempInode;
    for (int i = 0; (i < 5) && (name[i] != '\0'); i++)
    {
      tempInode.name[i] = name[i];
    }
    tempInode.used_size = (uint8_t)size | 0x80;
    tempInode.dir_parent = info.currWorkDir | 0x80;

    superblock.inode[info.freeInodeIndexes[0]] = tempInode;

    // Update directories info
    info.directories[info.currWorkDir].push_back((string)name);;
    info.dirChildInodes[info.currWorkDir].push_back(info.freeInodeIndexes[0]);

    info.freeInodeIndexes.erase(info.freeInodeIndexes.begin());
  }
  else if ((size < 0) || (size > 127)) // impossible to store files of size outside [1,127] blocks
  {
    fprintf(stderr, "Error: Cannot allocate %d on %s\n", size, info.diskName.c_str());
    return;
  }
  else // creating a file. find first set of continuous free blocks that can store file
  {
    // Go through free block section and read each bit until N consecutive free blocks are found
    int freeByteCounter = 0;
    int consecFree = 0;
    bool saveable = false;
    bitset<8> curByte;
    while (freeByteCounter < 128)
    {
      bool blockInUse = getFreeBlockBit(freeByteCounter);
      if (blockInUse)
      {
        consecFree = 0;
      }
      else
      {
        consecFree++;
        if (consecFree >= neededBlocks)
        {
          saveable = true;
          break;
        }
      }
      freeByteCounter++;
    }
    if (saveable)
    {
      // Store attributes into first available inode
      Inode tempInode;
      for (int i = 0; i < 5; i++)
      {
        tempInode.name[i] = name[i];
      }
      tempInode.used_size = (uint8_t)size | 0x80;
      tempInode.dir_parent = info.currWorkDir & 0x7F;
      tempInode.start_block = freeByteCounter - neededBlocks + 1;

      superblock.inode[info.freeInodeIndexes[0]] = tempInode;

      // Update directories info
      info.directories[info.currWorkDir].push_back((string)name);;
      info.dirChildInodes[info.currWorkDir].push_back(info.freeInodeIndexes[0]);
      info.freeInodeIndexes.erase(info.freeInodeIndexes.begin());

      // Update superblock's free block list
      for (int j = tempInode.start_block; j < (tempInode.start_block + neededBlocks); j++)
      {
        setFreeBlockBit(j, 1);
      }
    }
    else
    {
      fprintf(stderr, "Error: Cannot allocate %d on %s\n", size, info.diskName.c_str());
      return;
    }
  }
}

void fs_delete(char name[5])
{
  /* fs_delete checks for and delete file/directory of name in the current
     working directory.
     Input: name - name of the directory or file being deleted
     Output: None
  */
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted\n");
    return;
  }

  int sharedIdx = getIndexInDir(name);
  char tempName[6] = {name[0], name[1], name[2], name[3], name[4], 0};

  // Check if specified file or directory is in current working directory
  if ( sharedIdx < 0 )
  {
    fprintf(stderr, "Error: File or directory %s does not exist\n", tempName);
    return;
  }

  // File or dir exists! Time to delete
  int inodeIndex = getInodeIndex(sharedIdx);

  if (!inodeIsDirectory(inodeIndex))
  {
    // Is a file. Need to zero out used mem blocks and update free block list
    int startBlockIdx = getStartBlock(inodeIndex);
    int fileSize = getFileSize(inodeIndex);

    char tempBuff[BLOCK_SIZE];
    memset(tempBuff, 0, BLOCK_SIZE);

    for (int i = 0; i < fileSize; i++)
    {
      lseek(fsfd, BLOCK_SIZE*(startBlockIdx+i), SEEK_SET);
      write(fsfd, tempBuff, BLOCK_SIZE);
      setFreeBlockBit((startBlockIdx+i), 0);
    }
  }
  else // is a directory, need to recursively delete
  {
    // Update current work directory
    int tempCurrWorkDir = info.currWorkDir;
    info.currWorkDir = inodeIndex;

    // Get list of names for directory and loop through them with fs_delete
    while (info.directories[info.currWorkDir].size() > 0)
    {
      string str = info.directories[info.currWorkDir][0];
      char tempName[5];
      strncpy(tempName, str.c_str(), 5);
      fs_delete(tempName);
    }

    // Restore current work directory
    info.currWorkDir = tempCurrWorkDir;
  }

  memset(&(superblock.inode[inodeIndex]), 0, sizeof(Inode));  // Set inode to 0

  // Update info on directories
  info.directories[info.currWorkDir].erase(info.directories[info.currWorkDir].begin() + sharedIdx);
  info.dirChildInodes[info.currWorkDir].erase(info.dirChildInodes[info.currWorkDir].begin() + sharedIdx);

  // Update and sort list of free inodes
  info.freeInodeIndexes.push_back(inodeIndex);
  sort(info.freeInodeIndexes.begin(), info.freeInodeIndexes.end());
}

void fs_read(char name[5], int block_num)
{
  /* fs_read checks for and reads a KB block from file in the current
     working directory into the buffer.
     Input: name - name of the file being read
            block_num - index of block in file to read
     Output: None
  */
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted\n");
    return;
  }

  int sharedIdx = getIndexInDir(name);
  char tempName[6] = {name[0], name[1], name[2], name[3], name[4], 0};

  if (sharedIdx < 0)
  {
    fprintf(stderr, "Error: File %s does not exist\n", tempName);
    return;
  }

  int inodeIndex = getInodeIndex(sharedIdx);
  if (inodeIsDirectory(inodeIndex))
  {
    fprintf(stderr, "Error: File %s does not exist\n", tempName);
    return;
  }

  int fileSize = getFileSize(inodeIndex);

  if ( (block_num < 0) || (block_num > (fileSize - 1)) )
  {
    fprintf(stderr, "Error: %s does not have block %d\n", tempName, block_num);
    return;
  }

  // Otherwise, block of file exists. Read it into the buffer
  int startBlockIdx = getStartBlock(inodeIndex);

  lseek(fsfd, BLOCK_SIZE*(startBlockIdx+block_num), SEEK_SET);
  read(fsfd, &buffer, BLOCK_SIZE);
}

void fs_write(char name[5], int block_num)
{
  /* fs_write writes current contents of buffer into specified block of file.
     Input: name - name of the file being written to
            block_num - index of block in file to write to
     Output: None
  */
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted\n");
    return;
  }

  int sharedIdx = getIndexInDir(name);
  char tempName[6] = {name[0], name[1], name[2], name[3], name[4], 0};

  if (sharedIdx < 0)
  {
    fprintf(stderr, "Error: File %s does not exist\n", tempName);
    return;
  }

  int inodeIndex = getInodeIndex(sharedIdx);
  if (inodeIsDirectory(inodeIndex))
  {
    fprintf(stderr, "Error: File %s does not exist\n", tempName);
    return;
  }

  int fileSize = getFileSize(inodeIndex);

  if ( (block_num < 0) || (block_num > (fileSize - 1)) )
  {
    fprintf(stderr, "Error: %s does not have block %d\n", tempName, block_num);
    return;
  }

  // Otherwise, block of file exists. Read it into the buffer
  int startBlockIdx = getStartBlock(inodeIndex);

  lseek(fsfd, BLOCK_SIZE*(startBlockIdx+block_num), SEEK_SET);
  write(fsfd, &buffer, BLOCK_SIZE);
}

void fs_buff(uint8_t buff[BLOCK_SIZE])
{
  /* fs_buff flushes and writes new characters into the buffer.
     Input: buff - character array to replace contents of buffer with
     Output: None
  */
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted\n");
    return;
  }

  // Flush buffer
  memset(buffer, 0, BLOCK_SIZE);

  // Write new bytes into buffer
  for(int i=0; buff[i] != '\0'; i++)
  {
    buffer[i] = buff[i];
  }
}

void fs_ls(void)
{
  /* fs_ls prints out all items in the current working directory. Prints names
     of files with their sizes and directories with their number of items.
     Input: None
     Output: None
  */
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted\n");
    return;
  }

  int numChildren, size;
  int parentDir = info.currWorkDir;

  // Print . for current directory and number of items inside
  numChildren = info.directories[info.currWorkDir].size() + 2; // number of items inside directory, plus . and ..
  printf("%-5s %3d\n", ".", numChildren);

  // Print .. and number of items inside
  // If currWorkDir is not root (127), need to find num of items in parent directory
  if (info.currWorkDir != 127)
  {
    parentDir = superblock.inode[info.currWorkDir].dir_parent & 0x7F;
    numChildren = info.dirChildInodes[parentDir].size() + 2;
  }
  printf("%-5s %3d\n", "..", numChildren);

  vector<int> tempChildInodes = info.dirChildInodes[info.currWorkDir];
  sort(tempChildInodes.begin(), tempChildInodes.end());

  // Print name and size for each item in currWorkDir
  for (int i = 0; i < info.dirChildInodes[info.currWorkDir].size(); i++)
  {
    int child = tempChildInodes[i];
    char * name = superblock.inode[child].name;
    char tempName[6] = {name[0], name[1], name[2], name[3], name[4], '\0'};

    if (inodeIsDirectory(child))
    {
      // Is directory, find number of items inside it
      numChildren = info.dirChildInodes[child].size() + 2;

      printf("%-5s %3d\n", tempName, numChildren);
    }
    else
    {
      // Is file, get file size
      size = getFileSize(child);
      printf("%-5s %3d KB\n", tempName, size);
    }
  }
}

void fs_resize(char name[5], int new_size)
{
  /* fs_resize resizes file of given name in the current directory to the given
     new size.
     Input: name - name of file to be resized
            new_size - size to resize to
     Output: None
  */
  int sharedIdx, inodeIndex, fileSize, startBlockIdx;
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted\n");
    return;
  }

  char tempName[6] = {name[0], name[1], name[2], name[3], name[4], 0};
  sharedIdx = getIndexInDir(name);

  if (sharedIdx < 0)
  {
    fprintf(stderr, "Error: File %s does not exist\n", tempName);
    return;
  }

  inodeIndex = getInodeIndex(sharedIdx);
  if (inodeIsDirectory(inodeIndex))
  {
    fprintf(stderr, "Error: File %s does not exist\n", tempName);
    return;
  }

  fileSize = getFileSize(inodeIndex);
  startBlockIdx = getStartBlock(inodeIndex);

  if (new_size == fileSize)
  {
    // Already this size, no need to resize
    return;
  }
  else if (new_size < fileSize)
  {
    // Delete and zero out blocks from tail of block sequence for this file and update free block list
    char tempBuff[BLOCK_SIZE];
    memset(tempBuff, 0, BLOCK_SIZE);

    for (int i = 0; i < fileSize; i++)
    {
      if (i >= new_size)
      {
        lseek(fsfd, BLOCK_SIZE*(startBlockIdx+i), SEEK_SET);
        write(fsfd, tempBuff, BLOCK_SIZE);
        setFreeBlockBit((startBlockIdx+i), 0);
      }
    }
    superblock.inode[inodeIndex].used_size = new_size | 0x80;
  }
  else // new_size > fileSize
  {
    int freeByteCounter = startBlockIdx + fileSize;
    bool saveable = false;
    int consecFree = 0;

    while (freeByteCounter < 128)
    {
      bool blockInUse = getFreeBlockBit(freeByteCounter);
      if (blockInUse)
      {
        consecFree = 0;
        break;
      }
      else
      {
        consecFree++;
        if (consecFree >= (new_size - fileSize))
        {
          saveable = true;
          break;
        }
      }
      freeByteCounter++;
    }
    if (saveable) // can append new blocks to end without moving start_block
    {
      for (int j = (startBlockIdx+fileSize); j < (startBlockIdx+new_size); j++)
      {
        setFreeBlockBit(j, 1);
      }
      superblock.inode[inodeIndex].used_size = new_size | 0x80;
    }
    else // can't extend; need to move start block
    {
      freeByteCounter = 0;
      saveable = false;
      while (freeByteCounter < 128)
      {
        bool blockInUse = getFreeBlockBit(freeByteCounter);
        if (blockInUse)
        {
          consecFree = 0;
        }
        else
        {
          consecFree++;
          if (consecFree >= new_size)
          {
            saveable = true;
            break;
          }
        }
        freeByteCounter++;
      }

      if (saveable) // contiguous number of free blocks found
      {
        // Copy mem from old start block to new and set free block bits
        int newStartBlockIdx = freeByteCounter - new_size + 1;
        char tempBuff[BLOCK_SIZE];

        for (int k = 0; k < fileSize; k++)
        {
          lseek(fsfd, BLOCK_SIZE*(startBlockIdx+k), SEEK_SET);
          read(fsfd, tempBuff, BLOCK_SIZE);

          lseek(fsfd, BLOCK_SIZE*(newStartBlockIdx+k), SEEK_SET);
          write(fsfd, tempBuff, BLOCK_SIZE);
        }

        for (int k=0; k < new_size; k++)
        {
          setFreeBlockBit((newStartBlockIdx+k), 1);
        }

        // Clear old mem blocks and free block bits
        memset(tempBuff, 0, BLOCK_SIZE);

        for (int k = 0; k < fileSize; k++)
        {
          lseek(fsfd, BLOCK_SIZE*(startBlockIdx+k), SEEK_SET);
          write(fsfd, tempBuff, BLOCK_SIZE);

          setFreeBlockBit((startBlockIdx+k), 0);
        }

        // Update start block and size
        superblock.inode[inodeIndex].start_block = newStartBlockIdx;
        superblock.inode[inodeIndex].used_size = new_size | 0x80;
      }
      else
      {
        // Not saveable; print error message
        fprintf(stderr, "Error: File %s cannot expand to size %d\n", tempName, new_size);
      }
    }
  }
}

void fs_defrag(void)
{
  /* fs_defrag rearranges file blocks in memory so that there are no free
     blocks between used blocks, and between the superblock and used blocks.
     Input: None
     Output: None
  */
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted\n");
    return;
  }

  // Create list of file inodes sorted by smallest start_block to largest
  priority_queue<int, vector<int>, cmpStartBlock> q;
  for (int i = 0; i < 126; i++)
  {
    if (superblock.inode[i].start_block != 0)
    {
      q.push(i);
    }
  }

  int firstFreeBlock = 1;
  while (!q.empty())
  {
    int newStartBlockIdx, fileSize;
    int inodeIndex = q.top();
    int startBlockIdx = getStartBlock(inodeIndex);

    while (firstFreeBlock < startBlockIdx)
    {
      if (!getFreeBlockBit(firstFreeBlock))
      {
        break;
      }
      firstFreeBlock++;
    }

    if (firstFreeBlock == startBlockIdx+fileSize) // no need to shift, already at smallest free data block
    {
      q.pop();
      firstFreeBlock = startBlockIdx+fileSize;
      continue;
    }
    else
    {
      newStartBlockIdx = firstFreeBlock;
      fileSize = getFileSize(inodeIndex);
      char tempBuff[BLOCK_SIZE];

      // Copy over mem blocks and reset old free block list bits
      for (int j = 0; j < fileSize; j++)
      {
        // Copy oldFileSize+j'th mem block to newStartBlockIdx+j'th block
        lseek(fsfd, BLOCK_SIZE*(startBlockIdx+j), SEEK_SET);
        read(fsfd, &tempBuff, BLOCK_SIZE);

        lseek(fsfd, BLOCK_SIZE*(newStartBlockIdx+j), SEEK_SET);
        write(fsfd, &tempBuff, BLOCK_SIZE);

        // Zero out free block bit and old block that we just copied
        memset(tempBuff, 0, BLOCK_SIZE);
        lseek(fsfd, BLOCK_SIZE*(startBlockIdx+j), SEEK_SET);
        write(fsfd, &tempBuff, BLOCK_SIZE);

        setFreeBlockBit((startBlockIdx+j), 0);

        // newStartBlockIdx should always be lower than startBlockIdx, so set the new free block bit
        setFreeBlockBit((newStartBlockIdx+j), 1);
      }
      superblock.inode[inodeIndex].start_block = newStartBlockIdx;
    }
    firstFreeBlock = newStartBlockIdx+fileSize;
    q.pop();
  }

}

void fs_cd(char name[5])
{
  /* fs_cd changes current working directory to the one specified.
     Input: name - name of directory to change into
     Output: None
  */
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted\n");
    return;
  }

  if (strcmp(name, ".") == 0) // current directory
  {
    // Already there
    return;
  }
  else if (strcmp(name, "..") == 0) // parent directory
  {
    if (info.currWorkDir == 127)
    {
      // At root; nowhere to go
      return;
    }
    else
    {
      // Set current working directory to parent directory of current inode
      info.currWorkDir = superblock.inode[info.currWorkDir].dir_parent & 0x7F;
    }
  }
  else // child directory (maybe)
  {
    int sharedIdx = getIndexInDir(name);

    if (sharedIdx < 0)
    {
      fprintf(stderr, "Error: Directory %s does not exist\n", name);
      return;
    }

     int inodeIndex = getInodeIndex(sharedIdx);

     // Is it, in fact, a child directory?
     if (inodeIsDirectory(inodeIndex))
     {
       info.currWorkDir = inodeIndex;
     }
     else // nah
     {
       fprintf(stderr, "Error: Directory %s does not exist\n", name);
     }
  }
}
/* ------------------------ END FUNCTION DEFINITIONS ------------------------ */

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    fprintf(stderr, "Incorrect number of input files provided\n");
    return -1;
  }

  memset(buffer, 0, sizeof(buffer));

  char input[MAX_INPUT_LENGTH]; // command from file
  int lineCounter = 1;
  char *filename = argv[1];

  // Try opening input file
  FILE *fp = fopen(filename, "r");

  if (fp == NULL)
  {
    fprintf(stderr, "Could not open input file\n");
    return -1;
  }

  // Read input file line by line
  while (fgets(input, sizeof(input), fp) != NULL) {
    char *tokArgs[MAX_INPUT_LENGTH] = {NULL};

    // Ignore line if just a newline character
    if (input[0] == '\n')
    {
      lineCounter++;
      continue;
    }

    // Remove trailing newline char
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n')
    {
      input[len - 1] = '\0';
    }

    // Split into space-separated strings
    tokenize(&input[0], " ", &tokArgs[0]);

    // Check to see if proper command format and execute
    if (strcmp(tokArgs[0], "M") == 0)
    {
      // Should only have one arg
      if (tokArgs[2] != NULL || tokArgs[1] == NULL)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      else
      {
        fs_mount(tokArgs[1]);
      }
    }
    else if (strcmp(tokArgs[0], "C") == 0)
    {
      // Should only have two args
      if (tokArgs[3] != NULL || tokArgs[1] == NULL || tokArgs[2] == NULL)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      // Check if filename longer than 5 chars
      else if (strlen(tokArgs[1]) > 5)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      else
      {
        int size = atoi(tokArgs[2]);
        if (size < 0 || size > 127)  // no way can we have a file of this size
        {
          fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
        }
        else
        {
          fs_create(tokArgs[1], size);
        }
      }
    }
    else if (strcmp(tokArgs[0], "D") == 0)
    {
      // Should only have one arg
      if (tokArgs[2] != NULL || tokArgs[1] == NULL)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      // Check if filename longer than 5 chars
      else if (strlen(tokArgs[1]) > 5)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      else
      {
        fs_delete(tokArgs[1]);
      }
    }
    else if (strcmp(tokArgs[0], "R") == 0)
    {
      // Should only have two args
      if (tokArgs[3] != NULL || tokArgs[1] == NULL || tokArgs[2] == NULL)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      // Check if filename longer than 5 chars
      else if (strlen(tokArgs[1]) > 5)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      else
      {
        int block_num = atoi(tokArgs[2]);
        // Block number  must be in range [0, 126]
        if ( (block_num > 126) || (block_num < 0) )
        {
          fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
        }
        else
        {
          fs_read(tokArgs[1], block_num);
        }
      }
    }
    else if (strcmp(tokArgs[0], "W") == 0)
    {
      // Should only have two args
      if (tokArgs[3] != NULL || tokArgs[1] == NULL || tokArgs[2] == NULL)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      // Check if filename longer than 5 chars
      else if (strlen(tokArgs[1]) > 5)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      else
      {
        int block_num = atoi(tokArgs[2]);
        // Block number must be in range [0, 126]
        if ( (block_num > 126) || (block_num < 0) )
        {
          fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
        }
        else
        {
          fs_write(tokArgs[1], block_num);
        }
      }
    }
    else if (strcmp(tokArgs[0], "B") == 0)
    {
      // Should only have two args
      if (tokArgs[2] != NULL || tokArgs[1] == NULL)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      // Check if filename longer than 1024 chars
      else if (strlen(tokArgs[1]) > BLOCK_SIZE)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      else
      {
        uint8_t tempBuff[BLOCK_SIZE];
        memcpy(tempBuff, tokArgs[1], BLOCK_SIZE);
        fs_buff(tempBuff);
      }
    }
    else if (strcmp(tokArgs[0], "L") == 0)
    {
      // Should have no args
      if (tokArgs[1] != NULL)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      else
      {
        fs_ls();
      }
    }
    else if (strcmp(tokArgs[0], "E") == 0)
    {
      // Should only have two args
      if (tokArgs[3] != NULL || tokArgs[1] == NULL || tokArgs[2] == NULL)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      // Check if filename longer than 5 chars
      else if (strlen(tokArgs[1]) > 5)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      else
      {
        int size = atoi(tokArgs[2]);
        if (size < 1) // assumption
        {
          fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
        }
        else
        {
          fs_resize(tokArgs[1], size);
        }
      }
    }
    else if (strcmp(tokArgs[0], "O") == 0)
    {
      // Should have no args
      if (tokArgs[1] != NULL)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      else
      {
        fs_defrag();
      }
    }
    else if (strcmp(tokArgs[0], "Y") == 0)
    {
      // Should only have one arg
      if (tokArgs[2] != NULL || tokArgs[1] == NULL)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      // Check if filename longer than 5 chars
      else if (strlen(tokArgs[1]) > 5)
      {
        fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
      }
      else
      {
        fs_cd(tokArgs[1]);
      }
    }
    else
    {
      // Not valid command
      fprintf(stderr, "Command Error: %s, %d\n", filename, lineCounter);
    }
    // increment line counter
    lineCounter++;
  }

  // Close input file
  fclose(fp);

  // Close mounted disk file if open
  if (fsMounted)
  {
    lseek(fsfd, 0, SEEK_SET);
    write(fsfd, &superblock, BLOCK_SIZE);
    close(fsfd);
  }

  return 0;
}
