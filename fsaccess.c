/***********************************************************************
/*Author: Nguyen Le, Sung Hyun Hwang
* UTD ID: nhl170002, sxh200013
* CS 5348.001 Operating Systems
* Prof. S Venkatesan
* Project - 2* 
***************
* Compilation :-$ gcc -o fsaccess fsaccess.c 
* Run using :- $ ./fsaccess

----------PRE SETUP BEFORE RUNNING THE PROGRAM--------------------
Step 1) Create a file test.txt (vi test.txt). Type in anything (e.g. "test")
Step 2) Create a blank file copy.txt (vi copy.txt). Leave it blank
Step 3) Compile and run ./fsaccess 

EXAMPLE EXECUTION:
    1) vi test.txt 
        a) Type any string (e.g "test")
        b) e
 
 This program allows user to do two things: 
   1. initfs: Initilizes the file system and redesigning the Unix file system to accept large 
      files of up tp 4GB, expands the free array to 152 elements, expands the i-node array to 
      200 elemnts, doubles the i-node size to 64 bytes and other new features as well.
   2. Quit: save all work and exit the program.
   
 User Input (based on the PRE SETUP above):
     - initfs (file path) (# of total system blocks) (# of System i-nodes)
     - cpin (external file path) (v6 file path) 
     - cpout (v6 file path) (external file path)
     - q
     
 File name is limited to 14 characters.
 ***********************************************************************/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#define FREE_SIZE 152
#define I_SIZE 200
#define BLOCK_SIZE 1024
#define ADDR_SIZE 11
#define INPUT_SIZE 256

// Superblock Structure

typedef struct
{
    unsigned short isize;
    unsigned short fsize;
    unsigned short nfree;
    unsigned int free[FREE_SIZE];
    unsigned short ninode;
    unsigned short inode[I_SIZE];
    char flock;
    char ilock;
    unsigned short fmod;
    unsigned short time[2];
} superblock_type;

superblock_type superBlock;

// I-Node Structure
typedef struct
{
    unsigned short flags; // 0b1xxxxxxxxxxxxxxx
    unsigned short nlinks;
    unsigned short uid;
    unsigned short gid;
    unsigned int size;
    unsigned int addr[ADDR_SIZE];
    unsigned short actime[2];
    unsigned short modtime[2];
} inode_type;

inode_type inode;

typedef struct
{
    unsigned short inode;
    unsigned char filename[14];
} dir_type;

dir_type root;

int fileDescriptor; //file descriptor
const unsigned short inode_alloc_flag = 0100000;
const unsigned short dir_flag = 040000;
const unsigned short dir_large_file = 010000;
const unsigned short dir_access_rights = 000777; // User, Group, & World have all access privileges
const unsigned short INODE_SIZE = 64;            // inode has been doubled
const unsigned short plain_file = 000000;

int initfs(char *path, unsigned short total_blcks, unsigned short total_inodes);
void add_block_to_free_list(int blocknumber, unsigned int *empty_buffer);
void create_root();
unsigned short allocateInode();
void update_root_directory(const char *v6FileName, unsigned short v6File_iNumber);
unsigned short get_free_data_block();
void cpin(const char *externalFileName, const char *v6FileName);
void cpout(const char *v6File, const char *externalFile);

int cpout_num_of_blocks = 0;

int main()
{

    char input[INPUT_SIZE];
    char *splitter;
    unsigned int numBlocks = 0, numInodes = 0;
    char *filepath;
    printf("Size of super block = %d , size of i-node = %d\n", sizeof(superBlock), sizeof(inode));
    printf("Enter command:\n");

    char *externalFile, *v6File;

    while (1)
    {

        scanf(" %[^\n]s", input);
        splitter = strtok(input, " ");

        if (strcmp(splitter, "initfs") == 0)
        { //if user input == "initfs"

            preInitialization(); //get user input to start initializing file system
            splitter = NULL;
        }
        else if (strcmp(splitter, "q") == 0)
        { //if the user input is equal to "q"

            lseek(fileDescriptor, BLOCK_SIZE, 0);
            write(fileDescriptor, &superBlock, BLOCK_SIZE);
            return 0;
        }
        else if (strcmp(splitter, "cpin") == 0)
        { //if the user input is equal to "cpin"
            externalFile = strtok(NULL, " ");
            v6File = strtok(NULL, " ");
            cpin(externalFile, v6File);       
        }
        else if (strcmp(splitter, "cpout") == 0)
        { 
            v6File = strtok(NULL, " ");
            externalFile = strtok(NULL, " ");
            cpout(v6File, externalFile);
        }
    }
}

int preInitialization()
{

    char *n1, *n2;
    unsigned int numBlocks = 0, numInodes = 0;
    char *filepath;

    //get user input, each separated by a space
    filepath = strtok(NULL, " ");
    n1 = strtok(NULL, " ");
    n2 = strtok(NULL, " ");

    if (access(filepath, F_OK) != -1)
    { //if file already exists.

        if (fileDescriptor = open(filepath, O_RDWR, 0600) == -1)
        { //if fails to open for reading and writing (O_RDWR) [0600 = 110 000 000 : rw_ ___ ___]

            printf("\n filesystem already exists but open() failed with error [%s]\n", strerror(errno));
            return 1; //exit
        }
        printf("filesystem already exists and the same will be used.\n"); //successfully opened existing file
    }
    else
    {

        if (!n1 || !n2) //if user have not input n1 and n2
            printf(" All arguments(path, number of inodes and total number of blocks) have not been entered\n");

        else
        {
            numBlocks = atoi(n1); //set number of blocks based on user input
            numInodes = atoi(n2); //set number of inodes based on user input

            if (initfs(filepath, numBlocks, numInodes))
            { //create file system, if 1, then filesystem created sucessfully
                printf("The file system is initialized\n");
            }
            else
            { //else failed to create file system
                printf("Error initializing file system. Exiting... \n");
                return 1;
            }
        }
    }
}

//FUNCTION: Initializes file system
int initfs(char *path, unsigned short blocks, unsigned short inodes)
{

    unsigned int buffer[BLOCK_SIZE / 4];
    int bytes_written;

    unsigned short i = 0;
    superBlock.fsize = blocks;                                 //set fsize based on the user input's number of blocks
    unsigned short inodes_per_block = BLOCK_SIZE / INODE_SIZE; // 1024 / 64 = 16 inodes per block

    if ((inodes % inodes_per_block) == 0)             //if user input's # of inodes a multiple of 16
        superBlock.isize = inodes / inodes_per_block; //set the number of blocks devoted to the i-list
    else                                              //user input's # of nodes NOT a multiple of 16
        superBlock.isize = (inodes / inodes_per_block) + 1;

    if ((fileDescriptor = open(path, O_RDWR | O_CREAT, 0700)) == -1) //if fails to open for rwx
    {
        printf("\n open() failed with the following error [%s]\n", strerror(errno));
        return 0;
    }

    for (i = 0; i < FREE_SIZE; i++)
        superBlock.free[i] = 0; //initializing free array to 0 to remove junk data. free array will be stored with data block numbers shortly.

    superBlock.nfree = 0;
    superBlock.ninode = I_SIZE;

    for (i = 0; i < I_SIZE; i++)
        superBlock.inode[i] = i + 1; //Initializing the inode array to inode numbers. e.g: inode array = [1,2,3,4,5,6]

    superBlock.flock = 'a'; //flock,ilock and fmode are not used.
    superBlock.ilock = 'b';
    superBlock.fmod = 0;
    superBlock.time[0] = 0; //last time when superblock was modified
    superBlock.time[1] = 1970;

    lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET);    //offset BLOCK_SIZE number of bytes for this file
    write(fileDescriptor, &superBlock, BLOCK_SIZE); // writing superblock to file system

    for (i = 0; i < BLOCK_SIZE / 4; i++)
        buffer[i] = 0;

    for (i = 0; i < superBlock.isize; i++)
        write(fileDescriptor, buffer, BLOCK_SIZE);

    //size of data blocks
    //if user input for blocks is 800,
    int data_blocks = blocks - 2 - superBlock.isize;
    int data_blocks_for_free_list = data_blocks - 1;

    // Create root directory
    create_root();

    //add free data blocks to free list
    for (i = 2 + superBlock.isize + 1; i < superBlock.fsize; i++)
    {
        add_block_to_free_list(i, buffer);
    }

    return 1;
}

// Assigning Data blocks to free list
void add_block_to_free_list(int block_number, unsigned int *empty_buffer)
{

    if (superBlock.nfree == FREE_SIZE)
    { //if the free list is COMPLETELY free, nfree is the number of free elements in the free array

        int free_list_data[BLOCK_SIZE / 4], i;
        free_list_data[0] = FREE_SIZE; //the first element of the free array = FREE_SIZE

        for (i = 0; i < BLOCK_SIZE / 4; i++)
        {
            if (i < FREE_SIZE)
            {
                free_list_data[i + 1] = superBlock.free[i]; //assign unallocated blocks
            }
            else
            {
                free_list_data[i + 1] = 0; // getting rid of junk data in the remaining unused bytes of header block
            }
        }

        lseek(fileDescriptor, (block_number)*BLOCK_SIZE, 0);
        write(fileDescriptor, free_list_data, BLOCK_SIZE); // Writing free list to header block

        superBlock.nfree = 0;
    }
    else
    {

        lseek(fileDescriptor, (block_number)*BLOCK_SIZE, 0);
        write(fileDescriptor, empty_buffer, BLOCK_SIZE); // writing 0 to remaining data blocks to get rid of junk data
    }

    superBlock.free[superBlock.nfree] = block_number; // Assigning blocks to free array
    ++superBlock.nfree;
}

// Create root directory
void create_root()
{

    int root_data_block = 2 + superBlock.isize; // Allocating first data block to root directory
    int i;

    root.inode = 1;         // root directory's inode number is 1.
    root.filename[0] = '.'; //double check tuesday's lecture
    root.filename[1] = '\0';

    inode.flags = inode_alloc_flag | dir_flag | dir_large_file | dir_access_rights; // flag for root directory
    inode.nlinks = 0;
    inode.uid = 0;
    inode.gid = 0;
    inode.size = INODE_SIZE;
    inode.addr[0] = root_data_block;

    for (i = 1; i < ADDR_SIZE; i++)
    {
        inode.addr[i] = 0;
    }

    inode.actime[0] = 0;
    inode.modtime[0] = 0;
    inode.modtime[1] = 0;

    lseek(fileDescriptor, 2 * BLOCK_SIZE, 0);
    write(fileDescriptor, &inode, INODE_SIZE);

    lseek(fileDescriptor, root_data_block * BLOCK_SIZE, 0);
    write(fileDescriptor, &root, 16);

    // Now R/W pointer points 16 bytes in from start of root data block (second root dir. entry)
    root.filename[0] = '.';
    root.filename[1] = '.';
    root.filename[2] = '\0';

    write(fileDescriptor, &root, 16);
}

//---------------------------IMPLENTATIONS BELOW------------------------------

/*
Function: cpin
---------------
Description: Copies the content of a given external file onto the newly created v6 file within this file systen
Parameters: externalFileName - Name of the external file to be copied from
            v6FileName       - Name of the v6 file to be created and copied to within this filesystem
*/
void cpin(const char *externalFileName, const char *v6FileName)
{
        //printf("\nInside cpin...");
    //-----VARIABLES----------------
        int v6File_fdes, exFile_fdes;
        int i, j;
        //-----------------------------
        unsigned int buffer[BLOCK_SIZE / 4]; //buffer used to transfer data between files via read
        
    //allocate new inode for new v6 file START
        printf("\nFetch inumber...");
        unsigned short inumber;
        superBlock.ninode--; 
        inumber = superBlock.inode[superBlock.ninode];
        unsigned short v6File_inumber = inumber;
        printf("\ninumber Fetched: %d", inumber);
    //------------------------------------END
    //--get statistics of external file START
        printf("\nFetch external file's statistics...");
        unsigned short exFile_size;
        struct stat exFile_Statistics;
        stat(externalFileName, &exFile_Statistics); //get external file characteristics such as inode number, file size, etc... (see stat linux)
        exFile_size = exFile_Statistics.st_size; //get size of external file
    //------------------------------------ END
    //---Create new inode for new v6 file and write to file system START
        inode_type v6File_inode;
        v6File_inode.flags = inode_alloc_flag | dir_access_rights | plain_file; //second flag = 000777 (dir_acess_rights)?
        v6File_inode.size = exFile_size;
        v6File_inode.uid = 0;
        v6File_inode.gid = 0;
        printf("\nExternal file's size: %d", exFile_size); 
        int numOfBlocks_allocated; 
        //number of blocks that need to be allocated for the new v6 file (should match with the external file)
        if ((exFile_size % BLOCK_SIZE) == 0)             
            numOfBlocks_allocated = (exFile_size / BLOCK_SIZE);  
        else                                              
            numOfBlocks_allocated = (exFile_size / BLOCK_SIZE) + 1; 
        
        //printf("\nnumOfBlocks_allocated: %d", numOfBlocks_allocated); 
        cpout_num_of_blocks = numOfBlocks_allocated; //to be used in cpout for loop
        //printf("\ncpout_num_of_blocks: %d", cpout_num_of_blocks); 

        unsigned short free_block; //block number to be assigned to address array
        //get free data blocks for v6 file and sest the block numbers to the address array of its inode
        printf("\nStart assigning free block numbers and store the numbers to the inode's addr[]");
        for (i = 0; i < numOfBlocks_allocated; i++)
        {
            superBlock.nfree--; //get the index to free array of the next free block number
            free_block = superBlock.free[superBlock.nfree];
            //printf("\nAssigning free block %d to inode's addr[]", free_block);
            superBlock.free[superBlock.nfree] = 0; //now this block is not free anymore as it has been allocated
            v6File_inode.addr[i] = free_block;
            
        }
        printf("\nFinished assigning free blocks to v6file, now writing inode to file system....");
        //write inode to file system
        lseek(fileDescriptor, (2 * BLOCK_SIZE) + (v6File_inumber * INODE_SIZE), SEEK_SET);
        write(fileDescriptor, &v6File_inode, INODE_SIZE);
    //------------------------------------ END
        printf("\nFinished writing inode to file system, now adding v6 file to root directory...");
    //--Add the newly created v6 file to root directory (inumber 2bytes, filename 14 bytes) START
        int rootDataBlock = 2 + superBlock.isize;
        lseek(fileDescriptor, rootDataBlock * BLOCK_SIZE + 32, SEEK_SET); //skip the first two entries in root
        write(fileDescriptor, &v6File_inumber, 2); //2 = sizeof(v6File_inumber)
        write(fileDescriptor, v6FileName, 14); //14 = sizeof(v6FileName)
        printf("\nFinished updating root directory...");
    //------------------------------------------------------------------------------------ END
        
    //update superblock's inode array?

    //--now copy the external file's content to this newly created v6 file START
    printf("\n------------COPYING CONTENT FROM EXTERNAL FILE TO V6 FILE START--------------------");
        exFile_fdes = open(externalFileName, O_RDONLY);
        if(exFile_fdes == -1)
        {
            printf("\nUnable to open external file");
        } else {
            printf("\nExternal File opened sucessfully!");
            printf("\n------Start copying now.....--------");
            for (j = 0; j < numOfBlocks_allocated; j++)
            {
                //printf("\n%d) Copying content from external file to v6 file...", j);
                lseek(exFile_fdes, BLOCK_SIZE * j, SEEK_SET);
                read(exFile_fdes, &buffer, BLOCK_SIZE);
                //move to this file's data block (11 in total), the addr[b] returns the data block number
                //and each data block is 1024 bytes
                lseek(fileDescriptor, BLOCK_SIZE * (v6File_inode.addr[j]), SEEK_SET);
                write(fileDescriptor, &buffer, BLOCK_SIZE);
            }
            //finished copying, close files
        }
        printf("\nFinished copying, closing external file....\n\n");
        close(exFile_fdes);
    //-------------------------------------------------------------- END
}

/*
Function: cpout
---------------
Description: Copies the content of a v6 file onto the external file in the UNIX system
Parameters: v6File       - Name of the v6 file to be created and copied to within this filesystem
            externalFile - Name of the external file to be copied from
*/
void cpout(const char *v6File, const char *externalFile)
{
    /**
    * 1.) Open the file externalFile using the open function and create if it does not already exist using the O_RDWR and O_CREAT flags.
    *     Store the file descriptor in externFileDes.
    * 2.) Go to the v6File location in the new file system by starting the search at the root data block.
    *     Use lseek to go to root data block ((2 + superBlock.isize) * 1024 bytes in from the start).
    * 3.) Once at the root directory, find the name of the file represented by the v6File argument.
    *     lseek through the root data block 16 bytes at a time, check bytes 2-15 to get file name.
    *     If the file name matches v6File, then get the i-node from bytes 0 and 1 from the entry.  
    * 4.) Store the size of the file from the i-node.
    * 5.) Go to the i-node for v6File, get it's block number from the addr array.
    *     Each entry in array the represents the block where the data is stored. May not be in numerical order or contiguous blocks.
    * 6.) Write the data in the blocks to the externalFile blocks.
    **/

    //----- Open the externalFile and getting its fileDescriptor --------------------------------------------------------------------------------

    int rootDataBlock = 2 + superBlock.isize;

    int externFileDes = open(externalFile, O_RDWR | O_CREAT, 0700); // RWX permission for the user/owner of the file.

    if (externFileDes == -1)
    {
        printf("\nopen() failed with error [%s]\n", strerror(errno));
    }

   //-----END------------------------------------------------------------------------------------------------------------------------------------

   //-----Compare the two file names-------------------------------------------------------------------------------------------------------------

   lseek(fileDescriptor, rootDataBlock * BLOCK_SIZE, SEEK_SET); // lseek to root directory data block of the file system.

    // Loop through the directory where each directory entry is 16-bytes and check each entry's 14 byte name string. ----------------------------
    // Find the beginning of the 16 byte entries and read the i-node number from the first two bytes and the file name from the next 14 bytes. --
    // NOTE: loop through 1024/16 or 64 times (total number of entries possible for a directory data block) -------------------------------------
    
    unsigned char dirEntry[16]; // Buffer for the read function to read into.

    int i;
      for (i = 0; i < 16; i++)
      {
        dirEntry[i] = '\0';
      }

    int isNameSame = 1;
    int entryNum = 0;
    int foundMatch = 0;
    while (entryNum < 64 && foundMatch == 0)
    { // Loop while the directory entry name is different from the user v6File input.
        read(fileDescriptor, dirEntry, 16);

        // Use a pointer assigned to the address of dirEntry 2 bytes in, and use it to compare to the v6File input.
        unsigned char *fileName = &dirEntry[2];

        // Loop through both strings to see if all characters match. If not, set isNameSame to false and break out of the loop.
        // If either string hits a null char before the end of the loop, break out of the loop.
        for (i = 0; i < 13; i++)
        {
            if (fileName[i] != v6File[i])
            { // Catches for any differing characaters or length.
                isNameSame = 0;
                break;
            }
            else if (fileName[i] == '\0' && v6File[i] == '\0') {
                foundMatch = 1;
                isNameSame = 1;
                break; // Catches when identical strings both end.
            }
        }
        printf("\n");
        entryNum++;
    }
    entryNum = 0;

    // Get the i-node number if the two strings matched.
    unsigned short inodeNum;
    if (isNameSame)
        // TODO: Double check this inodeNum assignment.
        inodeNum = (dirEntry[1] << 8) | dirEntry[0]; // Use bitwise OR operation to get the first two bytes into a short.
    else
    {
        printf("\nThe file '%s' does not exist in the root directory!\n", v6File);
        return;
    }

    printf("inodeNum from root directory: %d\n\n", inodeNum);

    //-----END-----------------------------------------------------------------------------------------------------------------------------------

   //------Get information on the file to copy from----------------------------------------------------------------------------------------------
   
    // Go to the i-node number that we just got and the corresponding addr array.
    lseek(fileDescriptor, 2*BLOCK_SIZE + inodeNum*INODE_SIZE, SEEK_SET);

    // Get the i-node field flag information (2 bytes).
    unsigned short *flagFields;
    read(fileDescriptor, flagFields, 2);

    // Get size of the file and then start looking at the address blocks that the file is stored in.
    lseek(fileDescriptor, 2 * BLOCK_SIZE + inodeNum * INODE_SIZE + 8, SEEK_SET); // size is 8 bytes into the i-node

    unsigned int fileSize;
    read(fileDescriptor, &fileSize, 4); // size is an int in the struct so read 4 bytes.

    printf("Size of the file to copy: %d\n\n", fileSize);

    // Read the addr array (right after the size). The array is an int array, so read 4 bytes at a time to get a single data block number.
    unsigned int addrArray[ADDR_SIZE];

    read(fileDescriptor, &addrArray, 44); // Array is int addr[11] so total 44 bytes need to be read.
    
    //-----END-----------------------------------------------------------------------------------------------------------------------------------

    //-----Copying v6 file into external file----------------------------------------------------------------------------------------------------
    
    // Use the addrArray created to find the addresses of the blocks that the data is in.
    // Get the stats of the externalFile to use when writing.
    struct stat exFile_Statistics;
    stat(externalFile, &exFile_Statistics);
    
    
    unsigned int buffer[256];
    int k;
    for (k = 0; k < cpout_num_of_blocks; k++)
    {
        lseek(fileDescriptor, addrArray[k] * BLOCK_SIZE, SEEK_SET);
        read(fileDescriptor, &buffer, BLOCK_SIZE);
        lseek(externFileDes, BLOCK_SIZE * k, SEEK_SET);
        write(externFileDes, &buffer, fileSize);    // Only write out the number of bytes needed for the fileSize.
    }
    
    close(externFileDes);

    //-----END-----------------------------------------------------------------------------------------------------------------------------------
}