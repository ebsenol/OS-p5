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
  	int n, size ,ret, i;
	int fd;  
	char vdiskname[128]; 
	char buf[BLOCKSIZE];     
	int numblocks = 0; 


	strcpy (vdiskname, vdisk); 
        size = DISKSIZE; 
	numblocks = DISKSIZE / BLOCKSIZE; 

	printf ("diskname=%s size=%d blocks=%d\n", 
		vdiskname, size, numblocks); 
       
	ret = open (vdiskname,  O_CREAT | O_RDWR, 0666); 	
	if (ret == -1) {
		printf ("could not create disk\n"); 
		exit(1); 
	}
	
	bzero ((void *)buf, BLOCKSIZE); 
	fd = open (vdiskname, O_RDWR); 
	for (i=0; i < (size / BLOCKSIZE); ++i) {
		printf ("block=%d\n", i); 
		n = write (fd, buf, BLOCKSIZE); 
		if (n != BLOCKSIZE) {
			printf ("write error\n"); 
			exit (1); 
		}
	}	
	close (fd); 
	
	printf ("created a virtual disk=%s of size=%d\n", vdiskname, size);	
	return (0);
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

	
	for (int i = 0 ; i < BLOCKCOUNT; i++){
		blocks_next[i] = 0; // data blocks are initialized
	}

	// initialize file table
	for (int i = 0; i < MAXFILECOUNT; i++){
		file_table[i].initial = 0;
		file_table[i].offset = 0;
		file_table[i].open = 0;
		file_table[i].used = 0;
		file_table[i].size = 0;
		strcpy(file_table[i].filename, "");
	}

	allocated_file_count = 0;
	remaining_block_count = BLOCKCOUNT * 3 / 4; //for actual data
	open_file_count = 0;
	data_start_block = BLOCKCOUNT / 4; 
	first_free_block=updateFirstFreeBlock();

	// write file information to disk
	// first half
	file* fileBuffer1;
	fileBuffer1 = (file*)malloc(MAXFILECOUNT / 2 * sizeof(file));

	for (int i = 0 ; i < MAXFILECOUNT / 2; i++){
		file current_file;
		current_file.initial = file_table[i].initial;
		current_file.offset  = file_table[i].offset;
		current_file.open    = file_table[i].open;
		current_file.used 	 = file_table[i].used;
		current_file.size 	 = file_table[i].size;
		strcpy(current_file.filename, file_table[i].filename);
		fileBuffer1[i] = current_file;
	}
	putblock(fat_start_block, (void *)fileBuffer1);
	fileBuffer1 = (file*)malloc(0);

	// second half
	file* fileBuffer2;
	fileBuffer2 = (file*)malloc(MAXFILECOUNT / 2 * sizeof(file));

	for (int i = MAXFILECOUNT / 2 ; i < MAXFILECOUNT; i++){
		int j = i - MAXFILECOUNT / 2;
		file current_file;
		current_file.initial = file_table[i].initial;
		current_file.offset  = file_table[i].offset;
		current_file.open    = file_table[i].open;
		current_file.used 	 = file_table[i].used;
		current_file.size 	 = file_table[i].size;
		strcpy(current_file.filename, file_table[i].filename);
		fileBuffer2[j] = current_file;
	}

	putblock(fat_start_block + 1, (void *)fileBuffer2);
	fileBuffer2 = (file*)malloc(0);
	fat_start_block++;

	// write block information to the disk
	int* blockBuffer2;
	int block_counter = 1;
	while (block_counter < 26){ // 25 blocks will be 	
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
			blockBuffer2 = (int*)malloc(0);
	}
	fat_start_block--;

	
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
		file_table[i].size = 0;
		strcpy(file_table[i].filename, "");
	}


	// put first half:

	// read table info from disk
	file* fileBuffer1;
	fileBuffer1 = (file*)malloc(BLOCKSIZE);

	// stored on 1000. block
	getblock(fat_start_block, (void*)fileBuffer1);

	for (int i = 0; i < MAXFILECOUNT / 2; i++){
		file current_file = fileBuffer1[i];
		file_table[i].initial = current_file.initial;
		file_table[i].offset = current_file.offset;
		file_table[i].open = current_file.open;
		file_table[i].used = current_file.used;
		file_table[i].size = current_file.size;

		strcpy(file_table[i].filename, current_file.filename);	
		if(file_table[i].used == 1){
			allocated_file_count++;
		}
	}

	// put second half:

	// read table info from disk
	file* fileBuffer2;
	fileBuffer2 = (file*)malloc(BLOCKSIZE);

	// stored on 1001. block
	getblock(fat_start_block + 1, (void*)fileBuffer2);

	for (int i = MAXFILECOUNT / 2 ; i < MAXFILECOUNT; i++){
		int j = i - MAXFILECOUNT / 2;
		file current_file = fileBuffer2[j];
		file_table[i].initial = current_file.initial;
		file_table[i].offset = current_file.offset;
		file_table[i].open = current_file.open;
		file_table[i].used = current_file.used;
		file_table[i].size = current_file.size;
		strcpy(file_table[i].filename, current_file.filename);	
		if(file_table[i].used == 1){
			allocated_file_count++;
		}
	}

	fat_start_block++;
	//read data blocks from disk
	int block_counter = 1;
	while (block_counter < 26){ // 25 blocks will be used each will contain 1000 integers (holding 4000 bytes each)
			int* blockBuffer;
			blockBuffer = (int*)malloc(BLOCKSIZE);

			getblock(fat_start_block + block_counter, (void*)blockBuffer);
			for (int j = 0 ; j < 1000; j++){
				int currentBlock = data_start_block + (block_counter - 1) * 1000 + j;
				if(currentBlock < BLOCKCOUNT){
					blocks_next[currentBlock] = blockBuffer[j];
				}
			}
			block_counter++;
			free(blockBuffer);

	}
	free(fileBuffer1);
	free(fileBuffer2);

	first_free_block = updateFirstFreeBlock();
	fat_start_block--;
  	return (0); 
 }


int myfs_umount()
{
	// write file information to disk
	// first half
	file* fileBuffer1;
	fileBuffer1 = (file*)malloc(MAXFILECOUNT / 2 * sizeof(file));

	for (int i = 0 ; i < MAXFILECOUNT / 2; i++){
		file current_file;
		current_file.initial = file_table[i].initial;
		current_file.offset  = file_table[i].offset;
		current_file.open    = file_table[i].open;
		current_file.used 	 = file_table[i].used;
		current_file.size 	 = file_table[i].size;
		strcpy(current_file.filename, file_table[i].filename);
		fileBuffer1[i] = current_file;
	}
	putblock(fat_start_block, (void *)fileBuffer1);
	fileBuffer1 = (file*)malloc(0);

	// second half
	file* fileBuffer2;
	fileBuffer2 = (file*)malloc(MAXFILECOUNT / 2 * sizeof(file));

	for (int i = MAXFILECOUNT / 2 ; i < MAXFILECOUNT; i++){
		int j = i - MAXFILECOUNT / 2;
		file current_file;
		current_file.initial = file_table[i].initial;
		current_file.offset  = file_table[i].offset;
		current_file.open    = file_table[i].open;
		current_file.used 	 = file_table[i].used;
		current_file.size 	 = file_table[i].size;
		strcpy(current_file.filename, file_table[i].filename);
		fileBuffer2[j] = current_file;
	}

	putblock(fat_start_block + 1, (void *)fileBuffer2);
	fileBuffer2 = (file*)malloc(0);
	fat_start_block++;

	// write block information to the disk
	int* blockBuffer2;
	int block_counter = 1;
	while (block_counter < 26){ // 25 blocks will be 	
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
			blockBuffer2 = (int*)malloc(0);
	}
	fat_start_block--;

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

	/*int ret = open (filename,  O_CREAT | O_RDWR, 0666);
		if (ret == -1) {
			printf ("could not create file\n"); 
			return -1; 
	}*/
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
			file_table[i].offset = 0;
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
	file_table[fd].offset = 0;
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

	//checking if the 
	if(n <= 0 || n > 1024 || file_table[fd].used != 1) {
        return bytes_read;
	}
	
	char *buffer = buf;
	char block[BLOCKSIZE] = "";
	int cur_block = file_table[fd].initial;;
	int cur_offset = file_table[fd].offset;
	
	//getting the current block
	while (cur_offset >= BLOCKSIZE){
		
        cur_block = blocks_next[cur_block];
        cur_offset -= BLOCKSIZE;
	}
	
	//getting the initial block
	getblock(cur_block, (void *)block);
	bytes_read++;
	
	for(int i = cur_offset; i < BLOCKSIZE; i++) {
		
        buffer[bytes_read++] = block[i];
		//checking if enough
        if(bytes_read == (int)n) {
            file_table[fd].offset += bytes_read;
            return bytes_read;
        }
	}
	
	//getting the remaining blocks
	 while(bytes_read < (int)n && cur_block > BLOCKCOUNT/4-1 && cur_block < BLOCKCOUNT+1) {
    	cur_block = blocks_next[cur_block];
        strcpy(block,"");
        getblock(cur_block, block);
		//copying the block from the beginning
        for(int i =0; i < BLOCKSIZE; i++) {
            buffer[bytes_read++] = block[i];
			//checking if enough
            if(bytes_read == (int)n ) {
                file_table[fd].offset += bytes_read;
                return bytes_read;
            }
        }
    }
	file_table[fd].offset += bytes_read;

	
	return (bytes_read); 

}

int myfs_write(int fd, void *buf, int n)
{
	int bytes_written = 0; 

	if(n <= 0 || !file_table[fd].used) {
        return -1;
	}
	int initialSize = file_table[fd].size;
	int block = file_table[fd].initial;
	int cur_offset = file_table[fd].offset;
	char temp_block[BLOCKSIZE];
	char *buffer = buf;
	
	//getting the current block
	while (cur_offset >= BLOCKSIZE){
		if (blocks_next[block] == 99999){
			// allocate a new one
			blocks_next[block] = first_free_block;
			block = first_free_block;
			remaining_block_count--;
			break;
		}
		block = blocks_next[block];
		cur_offset -= BLOCKSIZE;
	}
	first_free_block = updateFirstFreeBlock();
	//writing to the initial block
	getblock(block, (void *)temp_block);
	for(int i = cur_offset; i < BLOCKSIZE; i++) {
		temp_block[i] = buffer[bytes_written];
		bytes_written++; 
		//checking if enough
		if(bytes_written == (int)n) {
			putblock(block, (void *)temp_block);
			file_table[fd].offset += bytes_written;
			return bytes_written;
		}
	}
	putblock(block, (void *)temp_block);
	if(blocks_next[block] > BLOCKCOUNT/4-1 && blocks_next[block] < BLOCKCOUNT+1){
		//writing to the allocated blocks
		while(bytes_written < (int)n && block > BLOCKCOUNT/4-1 && block < BLOCKCOUNT+1) {
			block = blocks_next[block];
			strcpy(block,"");
			getblock(block, (void *)temp_block);
			//copying the block from the beginning
			for(int i =0; i < BLOCKSIZE; i++) {
				temp_block[i] = buffer[bytes_written++]; 
				//checking if enough
				if(bytes_written == (int)n) {
					putblock(block, (void *)temp_block);
					file_table[fd].offset += bytes_written;
					return bytes_written;
				}
			}
			putblock(block, (void *)temp_block);
		}
	}
		
	//writing to the new blocks
	while(bytes_written < (int)n && block > BLOCKCOUNT/4-1 && block < BLOCKCOUNT+1) {
		//linking the first free block
		if (first_free_block == -1){
			return -1;
		}
		blocks_next[block] = first_free_block;
		blocks_next[first_free_block] = 99999;
		first_free_block = updateFirstFreeBlock();
		
		block = blocks_next[block];
		remaining_block_count--;//update remaining blocks
		
		getblock(block, (void *)temp_block);
		
		//copying the block from the beginning
		for(int i =0; i < BLOCKSIZE; i++) { 
			
			temp_block[i] = buffer[bytes_written++]; 
			//checking if enough
			if(bytes_written == (int)n) {
				putblock(block, (void *)temp_block);
				file_table[fd].offset += bytes_written;
				if(file_table[fd].size < file_table[fd].offset){ //update size if necessary
					file_table[fd].size = file_table[fd].offset;
				}
				return bytes_written;
			}
		}
	
		putblock(block, (void *)temp_block);
	}
	
	
	
	file_table[fd].offset += bytes_written;//update offset
	
	if(file_table[fd].size < file_table[fd].offset){ //update size if necessary
		file_table[fd].size = file_table[fd].offset;
	}

	return bytes_written;
} 

int myfs_truncate(int fd, int size)
{
	if(!file_table[fd].used){
        return -1;
	}
	if (size >= file_table[fd].size || size < 0){
        return -1;
	}
	
	int block = file_table[fd].initial;
	int cur_offset = size;
	int next_block;
	
	//getting the current block
	while (cur_offset >= BLOCKSIZE){
		block = blocks_next[block];
		cur_offset -= BLOCKSIZE;
	}
	int lastBlock = block;

	while(block > BLOCKCOUNT/4-1 && block < BLOCKCOUNT+1){
		next_block = blocks_next[block];
		blocks_next[block] = 0;
		block = next_block;
		remaining_block_count++;

	}

	//modify size and offset
	file_table[fd].offset = size;
	file_table[fd].size = size;

	if (block == lastBlock){
		return 0;
	}

	//last block
	remaining_block_count++;
	blocks_next[lastBlock] = 99999;

	return (0); 
} 


int myfs_seek(int fd, int offset)
{
	int position = -1; 

	//if the given file descriptor is invalid
	if(!file_table[fd].used)
		file_table[fd].offset = offset;
		return (position);
	//if offset is larger than the file size
	if(offset > file_table[fd].size){
		file_table[fd].offset = file_table[fd].size;
		position = file_table[fd].size;
	}
	else{
		file_table[fd].offset = file_table[fd].size;
		position = offset;
	}

	return (position); 
} 

int myfs_filesize (int fd)
{
	int size = -1; 
	
	//if exists retrieve the size
	if(file_table[fd].used){
		size = file_table[fd].size;
	}

	return (size); 
}


void myfs_print_dir ()
{
	printf("print dir:\n");
	for (int i = 0 ; i < MAXFILECOUNT; i++)
		if (file_table[i].used == 1)
			printf ("i=%d %s\n",i, file_table[i].filename);
}


void myfs_print_blocks (char *  filename)
{
	for (int i = 0 ; i < MAXFILECOUNT; i++){
		if (strcmp(file_table[i].filename, filename) == 0){
				int start = file_table[i].initial;
				printf ("\n%s: %d", file_table[i].filename, start);
				int next = blocks_next[start];
				while (next > BLOCKCOUNT/4-1 && next < BLOCKCOUNT+1 ){
					printf(" %d", next);
					next = blocks_next[next];
				}
				return;
		}
	}

}


