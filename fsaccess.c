/***********************************************************************
 
 
 
 This program allows user to do two things: 
   1. initfs: Initilizes the file system and redesigning the Unix file system to accept large 
      files of up tp 4GB, expands the free array to 152 elements, expands the i-node array to 
      200 elemnts, doubles the i-node size to 64 bytes and other new features as well.
   2. Quit: save all work and exit the program.
   
 User Input:
     - initfs (file path) (# of total system blocks) (# of System i-nodes)
     - q
     
 File name is limited to 14 characters.
 ***********************************************************************/

#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<stdlib.h>
#include <sys/stat.h>


#define FREE_SIZE 152  
#define I_SIZE 200
#define BLOCK_SIZE 1024    
#define ADDR_SIZE 11
#define INPUT_SIZE 256


// Superblock Structure

typedef struct {
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
typedef struct {
unsigned short flags;
unsigned short nlinks;
unsigned short uid;
unsigned short gid;
unsigned int size;
unsigned int addr[ADDR_SIZE];
unsigned short actime[2];
unsigned short modtime[2];
} inode_type; 

inode_type inode;

typedef struct {
  unsigned short inode;
  unsigned char filename[14];
} dir_type;

dir_type root;

int fileDescriptor ;		//file descriptor 
const unsigned short inode_alloc_flag = 0100000;
const unsigned short dir_flag = 040000;
const unsigned short dir_large_file = 010000;
const unsigned short dir_access_rights = 000777; // User, Group, & World have all access privileges 
const unsigned short INODE_SIZE = 64; // inode has been doubled


int initfs(char* path, unsigned short total_blcks,unsigned short total_inodes);
void add_block_to_free_list( int blocknumber , unsigned int *empty_buffer );
void create_root();
unsigned short allocateInode();
void update_root_directory(const char *v6FileName, unsigned_short v6File_iNumber);
unsigned short get_free_data_block();
void cpin(const char *externalFileName, const char *v6FileName);
void cpout(const char *v6File, const char *externalFile);

int main() {
 
  char input[INPUT_SIZE];
  char *splitter;
  unsigned int numBlocks = 0, numInodes = 0;
  char *filepath;
  printf("Size of super block = %d , size of i-node = %d\n",sizeof(superBlock),sizeof(inode));
  printf("Enter command:\n");
  
  while(1) {
  
    scanf(" %[^\n]s", input);
    splitter = strtok(input," ");
    
    if(strcmp(splitter, "initfs") == 0){ //if user input == "initfs"
    
        preInitialization();   //get user input to start initializing file system
        splitter = NULL;        
                       
    } else if (strcmp(splitter, "q") == 0) { //if the user input is equal to "q" 
   
       lseek(fileDescriptor, BLOCK_SIZE, 0);
       write(fileDescriptor, &superBlock, BLOCK_SIZE);
       return 0;
     
    } 
  }
}

int preInitialization(){

  char *n1, *n2;
  unsigned int numBlocks = 0, numInodes = 0;
  char *filepath;
  
  //get user input, each separated by a space
  filepath = strtok(NULL, " "); 
  n1 = strtok(NULL, " ");
  n2 = strtok(NULL, " ");
         
      
  if(access(filepath, F_OK) != -1) {  //if file already exists.
      
      if(fileDescriptor = open(filepath, O_RDWR, 0600) == -1){ //if fails to open for reading and writing (O_RDWR) [0600 = 110 000 000 : rw_ ___ ___]
      
         printf("\n filesystem already exists but open() failed with error [%s]\n", strerror(errno));
         return 1;  //exit
      }
      printf("filesystem already exists and the same will be used.\n"); //successfully opened existing file
  
  } else {
  
        	if (!n1 || !n2) //if user have not input n1 and n2
              printf(" All arguments(path, number of inodes and total number of blocks) have not been entered\n");
            
       		else {
          		numBlocks = atoi(n1); //set number of blocks based on user input
          		numInodes = atoi(n2); //set number of inodes based on user input
          		
          		if( initfs(filepath,numBlocks, numInodes )){ //create file system, if 1, then filesystem created sucessfully
          		  printf("The file system is initialized\n");	
          		} else {                                      //else failed to create file system
            		printf("Error initializing file system. Exiting... \n"); 
            		return 1;
          		}
       		}
  }
}

//FUNCTION: Initializes file system
int initfs(char* path, unsigned short blocks,unsigned short inodes) {

   unsigned int buffer[BLOCK_SIZE/4]; 
   int bytes_written;
   
   unsigned short i = 0;
   superBlock.fsize = blocks; //set fsize based on the user input's number of blocks
   unsigned short inodes_per_block= BLOCK_SIZE/INODE_SIZE; // 1024 / 64 = 16 inodes per block
   
   if((inodes % inodes_per_block) == 0) //if user input's # of inodes a multiple of 16 
   superBlock.isize = inodes/inodes_per_block; //set the number of blocks devoted to the i-list
   else                                 //user input's # of nodes NOT a multiple of 16
   superBlock.isize = (inodes/inodes_per_block) + 1; 
   
   if((fileDescriptor = open(path,O_RDWR|O_CREAT,0700))== -1) //if fails to open for rwx
       {
           printf("\n open() failed with the following error [%s]\n",strerror(errno));
           return 0;
       }
       
   for (i = 0; i < FREE_SIZE; i++)
      superBlock.free[i] =  0;			//initializing free array to 0 to remove junk data. free array will be stored with data block numbers shortly.
    
   superBlock.nfree = 0;
   superBlock.ninode = I_SIZE;
   
   for (i = 0; i < I_SIZE; i++)
	    superBlock.inode[i] = i + 1;		//Initializing the inode array to inode numbers. e.g: inode array = [1,2,3,4,5,6]
   
   superBlock.flock = 'a'; 					//flock,ilock and fmode are not used.
   superBlock.ilock = 'b';					
   superBlock.fmod = 0;
   superBlock.time[0] = 0;        //last time when superblock was modified
   superBlock.time[1] = 1970;
   
   lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET); //offset BLOCK_SIZE number of bytes for this file
   write(fileDescriptor, &superBlock, BLOCK_SIZE); // writing superblock to file system
   
   for (i = 0; i < BLOCK_SIZE/4; i++) 
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
   for ( i = 2 + superBlock.isize + 1; i < data_blocks_for_free_list; i++ ) {
      add_block_to_free_list(i , buffer);
   }
   
   return 1;
}

// Assigning Data blocks to free list
void add_block_to_free_list(int block_number,  unsigned int *empty_buffer){

  if ( superBlock.nfree == FREE_SIZE ) { //if the free list is COMPLETELY free, nfree is the number of free elements in the free array

    int free_list_data[BLOCK_SIZE / 4], i;
    free_list_data[0] = FREE_SIZE; //the first element of the free array = FREE_SIZE
    
    for ( i = 0; i < BLOCK_SIZE / 4; i++ ) {
       if ( i < FREE_SIZE ) {
         free_list_data[i + 1] = superBlock.free[i]; //assign unallocated blocks 
       } else {
         free_list_data[i + 1] = 0; // getting rid of junk data in the remaining unused bytes of header block
       }
    }
    
    lseek( fileDescriptor, (block_number) * BLOCK_SIZE, 0 );
    write( fileDescriptor, free_list_data, BLOCK_SIZE ); // Writing free list to header block
    
    superBlock.nfree = 0;
    
  } else {

	  lseek( fileDescriptor, (block_number) * BLOCK_SIZE, 0 );
    write( fileDescriptor, empty_buffer, BLOCK_SIZE );  // writing 0 to remaining data blocks to get rid of junk data
  }

  superBlock.free[superBlock.nfree] = block_number;  // Assigning blocks to free array
  ++superBlock.nfree;
}

// Create root directory
void create_root() {

  int root_data_block = 2 + superBlock.isize; // Allocating first data block to root directory
  int i;
  
  root.inode = 1;   // root directory's inode number is 1.
  root.filename[0] = '.'; //double check tuesday's lecture
  root.filename[1] = '\0';
  
  inode.flags = inode_alloc_flag | dir_flag | dir_large_file | dir_access_rights;   		// flag for root directory 
  inode.nlinks = 0; 
  inode.uid = 0;
  inode.gid = 0;
  inode.size = INODE_SIZE;
  inode.addr[0] = root_data_block;
  
  for( i = 1; i < ADDR_SIZE; i++ ) {
    inode.addr[i] = 0;
  }
  
  inode.actime[0] = 0;
  inode.modtime[0] = 0;
  inode.modtime[1] = 0;
  
  lseek(fileDescriptor, 2 * BLOCK_SIZE, 0);
  write(fileDescriptor, &inode, INODE_SIZE);   
  
  lseek(fileDescriptor, root_data_block * BLOCK_SIZE, 0);
  write(fileDescriptor, &root, 16);
  
  root.filename[0] = '.';
  root.filename[1] = '.';
  root.filename[2] = '\0';
  
  write(fileDescriptor, &root, 16);
 
}

//---------------------------IMPLENTATIONS BELOW------------------------------
//Helper functions
unsigned short allocateInode()
{
  unsigned short inumber;
  unsigned int i = 0;
  superBlock.ninode--; 
  inumber = superBlock.inode[superBlock.ninode];
  return inumber;
}

//function to update root whenever a new file is to be added to or be deleted from root directory
void update_root_directory(const char *v6FileName, unsigned_short v6File_iNumber)
{
  dir_type newFileToRoot;
  newFileToRoot.inode = v6File_iNumber; //assign inumber 
  strncpy(newFileToRoot.filename, v6FileName, 14); //assign filename
  int root_fileDesc = open(root.filename, O_RDWR | O_APPEND); //get root file descriptor
  write(root_fileDesc, &newFileToRoot, 16); //add v6file's inumber and filename to root directory
}

//function to get a free data block. Also decrements nfree for each pass
unsigned short get_free_data_block() 
{
  unsigned short free_block;
  superBlock.nfree--;
  free_block = superBlock.free[superBlock.nfree];
  superBlock.free[superBlock.free] = 0; //not free; 

  //implementation for when nfree = 0?

  return free_block;
}

void cpin(const char *externalFileName, const char *v6FileName)
{
  unsigned int buffer[BLOCK_SIZE/4]; //buffer used to transfer data between files via read
  unsigned short v6File_inumber = allocateInode(); //allocate inode for new v6 file
  unsigned short exFile_size;
  struct stat exFile_Statistics;
  int v6File_fdes, exFile_fdes;
  //only working with small file
  stat(externalFileName, &exFile_Statistics); //get external file characteristics such as inode number, file size, etc... (see stat linux)
  //get size of external file 
  exFile_size = exFile_Statistics.st_size;
  inode_type v6File_inode;
  v6File_inode.flags = inode_alloc_flag | 000077; //second flag = 000777 (dir_acess_rights)?
  v6File_inode.size = exFile_size;
  int numOfBlocks_allocated = exFile_size / BLOCK_SIZE; //number of blocks that need to be allocated for the new v6 file (should match with the external file)

  //create v6 file
  v6File_fdes = creat(v6FileName, 0775); //see 0775 permission
  v6File_fdes = open(v6FileName, O_RDWR | O_APPEND);
  lseek(v6File_fdes, 0, SEEK_SET); //pointer to beginning of file 
  //update root
  update_root_directory(v6FileName, v6File_inumber);
  //get free data blocks for v6 file and sest the block numbers to the address array of its inode
  for(int i = 0; i < numOfBlocks_allocated; i++)
  {
    v6File_inode.addr[i] = get_free_data_block(); 
  }

  write(v6File_fdes, &v6File_inode, INODE_SIZE);
  close(v6File_fdes);

  //now copy the external file's content to this v6 file
  exFile_fdes = open(externalFileName, O_RDONLY);
  v6File_fdes = open(v6FileName, O_RDWR | O_APPEND);

  for(int j = 0; j <= numOfBlocks_allocated; j++)
  {
    lseek(exFile_fdes, 1024*j, SEEK_SET);
    read(exFile_fdes, &buffer, BLOCK_SIZE);
    //move to this file's data block (11 in total), the addr[b] returns the data block number
    //and each data block is 1024 bytes
    lseek(v6File_fdes, 1024*(v6File_inode.addr[j]), SEEK_SET);     
    write(v6File_fdes, &buffer, BLOCK_SIZE);
  }

  //finished copying, close files
  close(exFile_fdes);
  close(v6File_fdes);
  //fileDescriptor = open(v6FileName , O_RDONLY); ?
}

// Copy the contents in new file system to an external file.
void cpout(const char *v6File, const char *externalFile)
{
    /**
    * 1.) Open the file externalFile using open the open function and create if it does not already exist using the O_RDWR and O_CREAT flags.
    *     Store the file descriptor in externFileDes.
    * 2.) Go to the v6File location in the new file system by starting the search at the root data block.
    *     Use lseek to go to root data block ((2 + superBlock.isize) * 1024 bytes in from the start).
    * 3.) Once at the root directory, find the name of the file represented by the v6File argument.
    *     lseek through the root data block 16 bytes at a time, check bytes 2-15 to get file name.
    *     If the file name matches v6File, then get the i-node from bytes 0 and 1 from the entry.  
    * 4.) Go to the i-node for v6File, get it's block number from the addr array.
    *     Each entry in the represents block where the data is stored. May not be in numberical order or contiguous blocks.
    * 5.) 
    **/
    int rootDataBlock = 2 + superBlock.isize;

    int externFileDes = open(externalFile, O_RDWR | O_CREAT, 0700) // RWX permission for the user/owner of the file.
    
    if (externFileDes == -1) {
      printf("\nopen() failed with error [%s]\n", strerror(errno));
    }

    lseek(fileDescriptor, rootDataBlock * BLOCK_SIZE, SEEK_SET)  // lseek to root directory of the file system.

    // Loop through the directory where each directory entry is 16-bytes and check each entry's 14 byte name string.
    int isNameSame = 1;
    int i; int dirSize = 16;
    for(i = 0; i < BLOCK_SIZE; i += dirSize) {
      // Traversing the directory entries for the root directory 16 bytes at a time.
      char *dirEntry; // Temporary buffer to hold the 16 bytes of entry info.
      read(fileDescriptor, dirEntry, dirSize);

      char *fileName = dirEntry + 2;
      
      // Comparing each character of the two strings.
      int i = 0;
      while (fileName[i] == v6File[i]) {
        i++;

        if (fileName[i] != v6File[i]) { // If fileName or v6File ended before the other (XOR)
          isNameSame = 0;
          break;
        }  

        if (*fileName != '\0' && *v6File != '\0') { // Only increment pointers if both do not point to null char.
          fileName++;
          v6File++;
        }
        else break;
      }
    }

    // If the two strings are the same, get the i-node number corresponding to that file name.
    if (isNameSame) {
      int inodeNum = atoi(dir;
       
    }
}

