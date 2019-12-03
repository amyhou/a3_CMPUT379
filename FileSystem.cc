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


/* MACROS */
#define MAX_INPUT_LENGTH (4000)


/* STRUCTURE DEFINITIONS */
/* Struct for additional info about disk file */
typedef struct {
	map<int, vector<string> > directories; // key: parent dir num, val: list of dirs and files inside
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
      tempUsedSize = bitset<8>(tempSuperblock.inode[40].used_size).set(7, 0).to_ulong();
      lowerLim = tempSuperblock.inode[i].start_block;
      if (tempUsedSize > 0)
      {
        upperLim = lowerLim + tempUsedSize - 1;
      } else
      {
        upperLim = 0;
      }

      // cout << "tempUsedSize" << tempUsedSize << endl;
      // cout << "lowerLim" << lowerLim << endl;
      // cout << "upperLim" << upperLim << endl;

      if (curByte[bitIdx])
      {
        if ((freeByteCounter >= lowerLim) && (freeByteCounter <= upperLim))
        {
          if (!properAlloc)
          {
            properAlloc = true;
          }
          else // block is used by more than one file
          {
            properAlloc = false;
            break;
          }
        }
      }
      else // if block is supposed to be free
      {
        // printf("here");
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
  map<int, vector<string> > dirNames;
  vector<int> availableInodes;
  for (int i = 0; i < sizeof(tempSuperblock.inode)/sizeof(Inode); i++)
  {
    // Check if inode is marked "in use"
    bitset<8> tempBitset = bitset<8>((int)tempSuperblock.inode[i].used_size);
    if (tempBitset[7] == 0)
    {
      // add to free inodes list
      availableInodes.push_back(i);
      sort(availableInodes.begin(), availableInodes.end());
      continue;
    }
    else
    {
      cout << tempBitset[7] << endl;
    }

    // Use a multimap Map<directory number, vector <string>>
    uint8_t tempDir = tempSuperblock.inode[i].dir_parent;
    printf("Using inode %d\n", i);
    bin(tempDir);
    bin(tempSuperblock.inode[i].used_size);
    string strName(tempSuperblock.inode[i].name);
    if (dirNames.find(tempDir) == dirNames.end())
    {
      // add parent directory to map
      vector<string> vecName;
      dirNames[tempDir] = vecName;
      dirNames[tempDir].push_back(strName);
    }
    else
    {
      // parent directory already a key; check if name in vector and append if not
      if (find(dirNames[tempDir].begin(), dirNames[tempDir].end(), strName) != dirNames[tempDir].end())
      {
        consistent = false;
        inconsistency = 2;
        break;
      }
      else
      {
        dirNames[tempDir].push_back(strName);
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

  /* END CONSISTENCY CHECK 3 */
  // Consistent?
  if (!consistent)
  {
    fprintf(stderr, "Error: File system in %s is inconsistent (error code: %d)\n", new_disk_name, inconsistency);
  }
  else
  {
    superblock = tempSuperblock;
    info.directories = dirNames;
    info.freeInodeIndexes = availableInodes;
    dup2(fd, fsfd);
    fsMounted = true;
  }
  close(fd);
}

void fs_create(char name[5], int size)
{

}

void fs_delete(char name[5])
{

}

void fs_read(char name[5], int block_num)
{

}

void fs_write(char name[5], int block_num)
{

}

void fs_buff(uint8_t buff[1024])
{

}

void fs_ls(void)
{

}

void fs_resize(char name[5], int new_size)
{

}

void fs_defrag(void)
{

}

void fs_cd(char name[5])
{

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
    close(fsfd);
  }

  return 0;
}
