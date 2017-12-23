#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "myfs.h"

// Global Variables
char disk_name[128];   // name of virtual disk file
int  disk_size;        // size in bytes - a power of 2
int  disk_fd;          // disk file handle
int  disk_blockcount;  // block count on disk
int  allocated_file_count = 0;
int  remaining_block_count = BLOCKCOUNT * 3 / 4; //for actual data
int  first_free_block;
int  open_file_count = 0;
int  fat_start_block = 1000; // fat table will be started storing at block 1000

int  data_start_block = BLOCKCOUNT / 4;  //starting block of the actual data

typedef struct {
	int used;	
	int open;	
	char filename[128];		
	int size;				// file size
	int initial;			// initial pointer to the block
	int offset;				// offset pointer for read/write indicates where the index is

} file;


// FAT tle
file file_table[MAXFILECOUNT];	//table that will contain file information
int* blocks_next;				//table that will contain blocks' information

/* 
   Reads block blocknum into buffer buf.
   You will not modify the getblock() function. 
   Returns -1 if error. Should not happen.
*/
int getblock (int blocknum, void *buf)
{      
	int offset, n; 
	
	if (blocknum >= disk_blockcount) 
		return (-1); //error

	offset = lseek (disk_fd, blocknum * BLOCKSIZE, SEEK_SET); 
	if (offset == -1) {
		printf ("lseek error\n"); 
		exit(0); 

	}

	n = read (disk_fd, buf, BLOCKSIZE); 
	if (n != BLOCKSIZE) 
		return (-1); 

	return (0); 
}


/*  
    Puts buffer buf into block blocknum.  
    You will not modify the putblock() function
    Returns -1 if error. Should not happen. 
*/
int putblock (int blocknum, void *buf)
{
	int offset, n;
	
	if (blocknum >= disk_blockcount) 
		return (-1); //error

	offset = lseek (disk_fd, blocknum * BLOCKSIZE, SEEK_SET);
	if (offset == -1) {
		printf ("lseek error\n"); 
		exit (1); 
	}
	
	n = write (disk_fd, buf, BLOCKSIZE); 
	if (n != BLOCKSIZE) 
		return (-1); 

	return (0); 
}


/* 
   IMPLEMENT THE FUNCTIONS BELOW - You can implement additional 
   internal functions. 
 */


int myfs_diskcreate (char *vdisk)
{
  return(0); 
}


/* format disk of size dsize */
int myfs_makefs(char *vdisk)
{
	strcpy (disk_name, vdisk); 
	disk_size = DISKSIZE; 
	disk_blockcount = disk_size / BLOCKSIZE; 

	disk_fd = open (disk_name, O_RDWR); 
	if (disk_fd == -1) {
		printf ("disk open error %s\n", vdisk); 
		exit(1); 
	}
	
	// perform your format operations here. 
	printf ("formatting disk=%s, size=%d\n", vdisk, disk_size); 

	free(blocks_next);
	fsync (disk_fd); 
	close (disk_fd); 

	return (0); 
}

/* 
   Mount disk and its file system. This is not the same mount
   operation we use for real file systems:  in that the new filesystem
   is attached to a mount point in the default file system. Here we do
   not do that. We just prepare the file system in the disk to be used
   by the application. For example, we get FAT into memory, initialize
   an open file table, get superblock into into memory, etc.
*/

int myfs_mount (char *vdisk)
{
	struct stat finfo; 

	strcpy (disk_name, vdisk);
	disk_fd = open (disk_name, O_RDWR); 
	if (disk_fd == -1) {
		printf ("myfs_mount: disk open error %s\n", disk_name); 
		exit(1); 
	}
	
	fstat (disk_fd, &finfo); 

	printf ("myfs_mount: mounting %s, size=%d\n", disk_name, (int) finfo.st_size);  
	disk_size = (int) finfo.st_size; 
	disk_blockcount = disk_size / BLOCKSIZE; 


	// initialize global variables
	allocated_file_count = 0;
	remaining_block_count = BLOCKCOUNT * 3 / 4; //for actual data
	data_start_block = BLOCKCOUNT / 4; 
	first_free_block = BLOCKCOUNT / 4;
	open_file_count = 0;
	fat_start_block = 1000; // fat table will be started storing at block 1000 (chosen arbitrarily)


 	blocks_next = (int*) malloc( (BLOCKCOUNT) * sizeof(int));
	for (int i = 0 ; i < BLOCKCOUNT / 4; i++){
		blocks_next[i] = -2; // these blocks are reserved for metadata
	}

	for (int i = BLOCKCOUNT / 4 ; i < BLOCKCOUNT; i++){
		blocks_next[i] = -1; // data blocks are initialized
	}

	// initialize file table
	for (int i = 0; i < MAXFILECOUNT; i++){
		file_table[i].initial = -1;
		file_table[i].offset = -1;
		file_table[i].open = 0;
		file_table[i].used = 0;
		strcpy(file_table[i].filename, "");
	}

	// read table info from disk
	file* fileBuffer;
	fileBuffer = (file*)malloc(BLOCKSIZE);

	// stored on 1000. block
	getblock(fat_start_block, (void*)fileBuffer);

	for (int i = 0; i < MAXFILECOUNT; i++){
		file current_file = fileBuffer[i];
		file_table[i].initial = current_file.initial;
		file_table[i].offset = current_file.offset;
		file_table[i].open = current_file.open;
		file_table[i].used = current_file.used;
		strcpy(file_table[i].filename, current_file.filename);	
		if(file_table[i].used == 1){
			allocated_file_count++;
		}
	}

	//read data blocks from disk
	int block_counter = 1;
	while (block_counter < 26){ // 25 blocks will be used each will contain 1000 integers (holding 4000 bytes each)
			int* blockBuffer;
			blockBuffer = (int*)malloc(BLOCKSIZE);

			getblock(fat_start_block + block_counter, (void*)blockBuffer);
			for (int j = 0 ; j < 1000; j++){
				int currentBlock = data_start_block + (block_counter - 1) * 1000 + j;
				//printf("\nbl:%d ,dsb: %d, bc: %d, j : %d , cb : %d\n",BLOCKCOUNT, data_start_block,block_counter,j,currentBlock);
				if(currentBlock < BLOCKCOUNT){
					blocks_next[currentBlock] = blockBuffer[j];
				}
			}
			block_counter++;
			free(blockBuffer);

	}
	free(fileBuffer);

	first_free_block = updateFirstFreeBlock();

  	return (0); 
 }


int myfs_umount()
{
	// write file information to disk
	file* fileBuffer2;
	fileBuffer2 = (file*)malloc(BLOCKSIZE);

	for (int i = 0 ; i < MAXFILECOUNT; i++){
		file current_file;
		current_file.initial = file_table[i].initial;
		current_file.offset  = file_table[i].offset;
		current_file.open    = file_table[i].open;
		current_file.used 	 = file_table[i].used;
		strcpy(current_file.filename, file_table[i].filename);
		fileBuffer2[i] = current_file;
	}

	putblock(fat_start_block, (void *)fileBuffer2);
	
	// write block information to the disk
	int block_counter = 1;
	while (block_counter < 26){ // 25 blocks will be used
			int* blockBuffer2;
			blockBuffer2 = (int*)malloc(BLOCKSIZE);

			for (int j = 0 ; j < 1000; j++){
				int currentBlock = data_start_block + (block_counter - 1) * 1000 + j;
				//printf("\nbl:%d ,dsb: %d, bc: %d, j : %d , cb : %d\n",BLOCKCOUNT, data_start_block,block_counter,j,currentBlock);
				if(currentBlock < BLOCKCOUNT){
					blockBuffer2[j] = blocks_next[currentBlock];
				}

			}
			putblock(fat_start_block + block_counter, (void *)blockBuffer2);
			block_counter++;
			free(blockBuffer2);
	}


	/*// stored on 1000. block
	getblock(fat_start_block, (void*)fileBuffer);
	printf("test1\n");
	for (int i = 0; i < MAXFILECOUNT; i++){
		file current_file = fileBuffer[i];
		printf("init :%d ,  name: %s" ,current_file.initial, current_file.filename);
			
	}
*/

	/*file* fileBuffer2;
	fileBuffer2 = (file*)malloc(BLOCKSIZE);

	// stored on 1000. block
	getblock(fat_start_block, (void*)fileBuffer2);
	printf("test1\n");
	/*for (int i = 0; i < MAXFILECOUNT; i++){
		file current_file = fileBuffer2[i];
		strcpy(file_table[i].filename, current_file.filename);	
		printf("%d %s\t",i,file_table[i].filename);	

	}
*/

	fsync (disk_fd); 
	close (disk_fd);
	printf("\nunmount finished\n"); 
	return (0); 
}


/* create a file with name filename */
int myfs_create(char *filename)
{
	if ( allocated_file_count >= MAXFILECOUNT || remaining_block_count <= 0){
		printf("error creating file : %s",filename);
		return -1;
	}

	int ret = open (filename,  O_CREAT | O_RDWR, 0666);
		if (ret == -1) {
			printf ("could not create file\n"); 
			return -1; 
	}
	first_free_block = updateFirstFreeBlock();
	for (int i = 0 ; i < MAXFILECOUNT; i++){
		if (file_table[i].used == 0){
			file_table[i].used = 1;
			file_table[i].initial = first_free_block;
			blocks_next[first_free_block] = 99999; 	//it means it has no next but it is allocated 
			file_table[i].offset = 0;
			file_table[i].open = 0;
			strcpy(file_table[i].filename, filename);
			break;
		}
	}
	myfs_print_blocks ("asd");
	remaining_block_count--;
	allocated_file_count++;
	first_free_block = updateFirstFreeBlock();

  	return (0); 
}

int updateFirstFreeBlock(){

	for (int i = data_start_block + 1; i < BLOCKCOUNT; i++){
		if (blocks_next[i] < BLOCKCOUNT/4)
			return i;
	}
	return -1; //no empty block left
}

/* open file filename */
int myfs_open(char *filename)
{
	int index = -1; 
	
	if (open_file_count >= MAXOPENFILES){
		return -1;
	}

	for (int i = 0 ; i < MAXFILECOUNT; i++){
		if (strcmp(file_table[i].filename, filename) == 0){
			file_table[i].open = 1;
			index = i;
			break;
		}
	}   
	open_file_count++;
	return (index); 
}

/* close file filename */
int myfs_close(int fd)
{

	file_table[fd].open = 0;
	open_file_count--;

	return (0); 
}

int myfs_delete(char *filename)
{
	for (int i = 0 ; i < MAXFILECOUNT; i++){
		if (strcmp(file_table[i].filename, filename) == 0){
			//error if the file is open
			if(file_table[i].open ==1 ){
				printf("error while deleting file: %s ,because it is open\n", filename);
				exit(1);
			}
			// update file information
			int block = file_table[i].initial;
			file_table[i].initial = -1;
			file_table[i].offset = -1;
			file_table[i].open = 0;
			file_table[i].used = 0;
			strcpy(file_table[i].filename, "");

			//delete blocks that are allocated to the file
			while (block > BLOCKCOUNT/4-1 && block < BLOCKCOUNT+1 ){
				remaining_block_count++;
				int temp =  blocks_next[block];
				blocks_next[block] = -1;
				block = temp;
			}
		}
	}

	allocated_file_count--;
	first_free_block = updateFirstFreeBlock();
	return (0); 
}

int myfs_read(int fd, void *buf, int n)
{
	int bytes_read = -1; 

	// write your code
	
	return (bytes_read); 

}

int myfs_write(int fd, void *buf, int n)
{
	int bytes_written = 0; 
	
	if ( 4096 - file_table[fd].offset > n){
		//there is enough space, no need to allocate another block

		//find the last block of the file
		int block = file_table[fd].initial;
		while (block > BLOCKCOUNT/4-1 && block < BLOCKCOUNT+1 ){
			block = blocks_next[block];
		}

		putblock(block, (void *)buf);
		file_table[fd].offset += n;
		bytes_written = n;

	}
	else{
		//there is no enough space, need to allocate another block

	}


	return (bytes_written); 
} 

int myfs_truncate(int fd, int size)
{

	// write your code

	return (0); 
} 


int myfs_seek(int fd, int offset)
{
	int position = -1; 
	//if the given file descriptor is invalid
	if(!file_table[fd].used)
		return position;
	//if offset is larger than the file size
	if(offset > file_table[fd].size){
		position = file_table[fd].size;
	}
	else
		position = offset;

	return position;
	

	return (position); 
} 

int myfs_filesize (int fd)
{
	int size = -1; 
	
	// write your code

	return (size); 
}


void myfs_print_dir ()
{
	printf("print dir:");
	for (int i = 0 ; i < MAXFILECOUNT; i++)
		if (file_table[i].used == 1)
			printf ("%s\n", file_table[i].filename);
}


void myfs_print_blocks (char *  filename)
{
	//printing all files for debugging purpose
	for (int i = 0 ; i < MAXFILECOUNT; i++){
		//if (strcmp(file_table[i].filename, filename) == 0){
		if( file_table[i].used == 1){
			int start = file_table[i].initial;
			printf ("\n%s : %d", file_table[i].filename, start);
			int next = blocks_next[start];
			while (next > BLOCKCOUNT/4-1 && next < BLOCKCOUNT+1 ){
				printf(" %d", next);
				next = blocks_next[next];
			}
//			return;
		}

	}

}


