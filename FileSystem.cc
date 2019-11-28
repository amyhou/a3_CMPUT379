#include "FileSystem.h"
#include <string.h>
#include <stdlib.h>

using namespace std;


/* Questions
    1. When updating the buffer, what should we do if input gives more than
      1024 characters?
*/

/* MACROS */
#define TRUE (1)
#define FALSE (0)
#define MAX_INPUT_LENGTH (4000)

/* GLOBAL VARIABLES */
uint8_t buffer[1024];

/* STRUCTURE DEFINITIONS */

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


/* Required Functions */
void fs_mount(char *new_disk_name)
{
  // Check for file existence in current directory

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
      // Should only have two args
      if (tokArgs[3] != NULL || tokArgs[1] == NULL)
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

  return 0;
}
