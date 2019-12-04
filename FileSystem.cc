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

using namespace std;

/* Questions
    When do we invalidated a file name such as "\0\0a\0\0"?
*/

/* MACROS */
#define MAX_INPUT_LENGTH (4000)


/* STRUCTURE DEFINITIONS */
/* Struct for additional info about disk file */
typedef struct {
  int currWorkDir;
  char *diskName;
	map<int, vector<string> > directories; // key: parent dir num, val: list of dirs and files inside
  map<int, vector<int> > dirChildInodes; // key: parent dir num, val: list of dirs and files inside
  vector<int> freeInodeIndexes; // sorted set of free inodes (sort after each insert)
} Info;


/* GLOBAL VARIABLES */
uint8_t buffer[1024];   // buffer of 1KB
int fsfd;              // file descriptor of emulator disk file currently mounted
Super_block superblock; // superblock of disk file currently mounted
Info info;
bool fsMounted = false;


/* FUNCTION DEFINITIONS */
/* Helper Functions */
void tokenize(char* str, const char* delim, char ** argv) {
  /**
   * @brief Tokenize a C string
   *
   * @param str - The C string to tokenize
   * @param delim - The C string containing delimiter character(s)
   * @param argv - A char* array that will contain the tokenized strings
   * Make sure that you allocate enough space for the array.
   */
  char* token;
  token = strtok(str, delim);
  for(size_t i = 0; token != NULL; ++i){
    argv[i] = token;
    token = strtok(NULL, delim);
  }
}

void bin(char n)
{
  // helper function that prints out a byte (8 bits) in hex

  printf("0x%02x\n", n & 0xFF);
}

// bool inodeInUse(Inode * inode, int inodeIdx)
// {
//   // Returns if inode state is free (false) or in use (true)
//   cout << "in use: " << bitset<8>(inode->used_size)[7] << endl;
//   return bitset<8>(inode->used_size)[7];
// }


/* Required Functions */
void fs_mount(char *new_disk_name)
{
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
  Info tempInfo;
  int inconsistency = 0;

  read(fd, &(tempSuperblock), 1024);
  // bin(tempSuperblock.free_block_list[0]);
  // bin(tempSuperblock.inode[0].name[0]);

  /* CONSISTENCY CHECK 1 */
  int freeByteCounter = 0;
  bitset<8> curByte;
  while (freeByteCounter < 128)
  {
    int modCount = freeByteCounter % 8;
    if (modCount == 0)
    {
      cout << freeByteCounter / 8 << endl;
      curByte = bitset<8>((unsigned char)tempSuperblock.free_block_list[freeByteCounter/8]);
    }

    int bitIdx = 7 - modCount;
    cout << curByte[bitIdx] << endl;

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
    // int tempUsedSize = bitset<8>(tempSuperblock.inode[40].used_size).set(7, 0).to_ulong();

    // cout << "tempUsedSize: " << tempUsedSize << endl;

    for (int i = 0; i < sizeof(tempSuperblock.inode)/sizeof(Inode); i++)
    {
      // int tempUsedSize = (int) bitset<8>(tempSuperblock.inode[i].used_size).set(7, 0).to_ulong();
      // cout << "check for block number " << freeByteCounter << " and inode "<< i << endl;
      int tempUsedSize, lowerLim, upperLim;
      tempUsedSize = bitset<8>(tempSuperblock.inode[i].used_size).set(7, 0).to_ulong();
      lowerLim = tempSuperblock.inode[i].start_block;
      if (tempUsedSize > 0)
      {
        upperLim = lowerLim + tempUsedSize - 1;
      } else
      {
        upperLim = 0;
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
            cout << "block used by more than one file" << endl;
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
  /* END CONSISTENCY CHECK 1 */

  /* CONSISTENCY CHECK 2 */
  // For each dir, add all names to vector and check if name already in vector before adding
  // map<int, vector<string> > dirNames;
  // map<int, vector<int> > dirInodes;

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
    printf("Using inode %d\n", i);
    bin(tempDir);
    bin(tempSuperblock.inode[i].used_size);
    char tempName[6] = {tempSuperblock.inode[i].name[0], tempSuperblock.inode[i].name[1], tempSuperblock.inode[i].name[2], tempSuperblock.inode[i].name[3], tempSuperblock.inode[i].name[4], '\0'};
    string strName = string(tempName);

    if (tempInfo.directories.find(tempDir) == tempInfo.directories.end())
    {
      // add parent directory to map
      vector<string> vecName;
      tempInfo.directories[tempDir] = vecName;
      cout << "strName: " << strName << endl;
      tempInfo.directories[tempDir].push_back(strName);

      vector<int>vecName2;
      tempInfo.dirChildInodes[tempDir] = vecName2;
      tempInfo.dirChildInodes[tempDir].push_back(i);
    }
    else
    {
      // parent directory already a key; check if name in vector and append if not
      if (find(tempInfo.directories[tempDir].begin(), tempInfo.directories[tempDir].end(), strName) != tempInfo.directories[tempDir].end())
      {
        consistent = false;
        inconsistency = 2;
        break;
      }
      else
      {
        cout << "strName: "<< strName << endl;
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
  /* END CONSISTENCY CHECK 2 */
  /* CONSISTENCY CHECK 3 */
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

      // add to free inodes list
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
  /* END CONSISTENCY CHECK 3 */
  /* CONSISTENCY CHECKS 4, 5, and 6*/
	for (int i = 0; i < unavailableInodes.size(); i++)
	{
		// Check only for inodes marked "in use"
		uint8_t tempDirParent = tempSuperblock.inode[unavailableInodes[i]].dir_parent;
		bitset<8> tempBitset = bitset<8>(tempDirParent);
		if (!tempBitset[7]) // is file, start_block must be between 1 and 127
		{
			// cout << "is file: " << endl;
			// cout << "start_block: " << tempSuperblock.inode[unavailableInodes[i]].start_block << endl;

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
			// cout << "is directory: " << endl;
			// cout << "start_block" << tempSuperblock.inode[unavailableInodes[i]].start_block << endl;
			// cout << "used_size" << (int)(tempSuperblock.inode[unavailableInodes[i]].used_size & 0x7F) << endl;
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
			// not valid parent inode index
		  consistent = false;
		  if (inconsistency == 0)
		  {
		    inconsistency = 6;
		  }
		}
		else if (tempParentNodeIdx != 127)
		{
			// check if parent inode is in use and marked as directory
		  bitset<8> tempParentDirParent = bitset<8>((int)tempSuperblock.inode[tempParentNodeIdx].dir_parent);
		  bitset<8> tempParentUsedSize = bitset<8>((int)tempSuperblock.inode[tempParentNodeIdx].used_size);

		  if ( (!tempParentUsedSize[7]) || (!tempParentUsedSize[7]) )
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
  /* END CONSISTENCY CHECK 4, 5 and 6*/

  // We've come this far... time to actually mount and start using the disk for real!
  superblock = tempSuperblock;
	lseek(fd, 0, SEEK_SET); // return fp to point to beginning of file because why not?
  dup2(fd, fsfd);
  fsMounted = true;
  tempInfo.currWorkDir = 127; // set working directory to root
  tempInfo.diskName = new_disk_name;

  // cout << "before setting info to tempinfo, tempinfo size is " << tempInfo.directories.size() << endl;
  info = tempInfo;
  // cout << "after setting info to tempinfo, info.dir size if " << info.directories.size() << endl;

	close(fd); // close temp fp

}

void fs_create(char name[5], int size)
{

  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted\n");
    return;
  }
  // cout << "from fs_create, currWorkDir is " << info.currWorkDir << endl;
  // cout << "Elements in directories map: " << info.directories.size() << endl;
  int neededBlocks = size;

  if (info.freeInodeIndexes.empty())
  {
    fprintf(stderr, "Error: Superblock in disk %s is full, cannot create %s\n", info.diskName, name);
  }

  vector<string> tempDirs = info.directories[127];
  // cout << "length of tempDirs: " << tempDirs.size() << endl;

  char tempName[6] = {name[0], name[1], name[2], name[3], name[4], '\0'};
  // cout << "tempName: " << string(tempName) << endl;
  if ( (find(tempDirs.begin(), tempDirs.end(), string(tempName)) != tempDirs.end()) ||
       (strcmp(name, ".") == 0) || (strcmp(name, "..") == 0) )
  {
    // not unique name in this directory
    fprintf(stderr, "Error: File or directory %s already exists\n", name);
    return;
  }

  if (size == 0) // creating a directory
  {
    // store attributes into first available inode
    Inode tempInode;
    for (int i = 0; i < 5; i++)
    {
      tempInode.name[i] = name[i];
    }
    tempInode.used_size = (uint8_t)size | 0x80;
    tempInode.dir_parent = info.currWorkDir | 0x80;

    superblock.inode[info.freeInodeIndexes[0]] = tempInode;
    // update directories info
    info.directories[info.currWorkDir].push_back((string)name);;
    info.dirChildInodes[info.currWorkDir].push_back(info.freeInodeIndexes[0]);

    info.freeInodeIndexes.erase(info.freeInodeIndexes.begin());
  }
  else if ((size < 0) || (size > 127)) // impossible to store files of size outside [1,127] blocks
  {
    fprintf(stderr, "Error: Cannot allocate %d on %s", size, name);
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
      int modCount = freeByteCounter % 8;
      if (modCount == 0)
      {
        cout << freeByteCounter / 8 << endl;
        curByte = bitset<8>((unsigned char)superblock.free_block_list[freeByteCounter/8]);
      }

      int bitIdx = 7 - modCount;
      cout << curByte[bitIdx] << endl;

      if (curByte[bitIdx])
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
      // store attributes into first available inode
      Inode tempInode;
      for (int i = 0; i < 5; i++)
      {
        tempInode.name[i] = name[i];
      }
      tempInode.used_size = (uint8_t)size | 0x80;
      tempInode.dir_parent = info.currWorkDir & 0x7F;
      tempInode.start_block = freeByteCounter - neededBlocks + 1;

      superblock.inode[info.freeInodeIndexes[0]] = tempInode;

      // update directories info
      info.directories[info.currWorkDir].push_back((string)name);;
      info.dirChildInodes[info.currWorkDir].push_back(info.freeInodeIndexes[0]);

      info.freeInodeIndexes.erase(info.freeInodeIndexes.begin());

      // update superblock's free block list
      for (int j = tempInode.start_block; j < (tempInode.start_block + neededBlocks); j++)
      {
        curByte = bitset<8>((unsigned char)superblock.free_block_list[j/8]); // get byte containing now used block
        curByte.set(7-(j%8)); // set now used block as in use
        superblock.free_block_list[j/8] = (unsigned char)curByte.to_ulong();
      }

    }
    else
    {
      fprintf(stderr, "Error: Cannot allocate %d on %s\n", size, info.diskName);
      return;
    }
  }

}

void fs_delete(char name[5])
{
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted\n");
    return;
  }
}

void fs_read(char name[5], int block_num)
{
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted");
    return;
  }
}

void fs_write(char name[5], int block_num)
{
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted");
    return;
  }
}

void fs_buff(uint8_t buff[1024])
{
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted");
    return;
  }
}

void fs_ls(void)
{
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted");
    return;
  }
}

void fs_resize(char name[5], int new_size)
{
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted");
    return;
  }
}

void fs_defrag(void)
{
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted");
    return;
  }
}

void fs_cd(char name[5])
{
  if (!fsMounted)
  {
    fprintf(stderr, "Error: No file system is mounted");
    return;
  }
}

int main(int argc, char **argv)
{
  memset(buffer, 0, 1024);

  char input[MAX_INPUT_LENGTH]; // command from file
  int lineCounter = 0;
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
        fs_create(tokArgs[1], atoi(tokArgs[2]));
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
        fs_read(tokArgs[1], atoi(tokArgs[2]));
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
        fs_write(tokArgs[1], atoi(tokArgs[2]));
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
        fs_write(tokArgs[1], atoi(tokArgs[2]));
      }
    }
    else if (strcmp(tokArgs[0], "B") == 0)
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
        fs_write(tokArgs[1], atoi(tokArgs[2]));
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
        fs_resize(tokArgs[1], atoi(tokArgs[2]));
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
        fs_delete(tokArgs[1]);
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
    write(fsfd, &superblock, 1024);
    close(fsfd);
  }

  return 0;
}
