#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "myfs.h"

int main(int argc, char *argv[])
{
	char diskname[128]; 
	char filename[16][MAXFILENAMESIZE]; 
	int i, n; 
	int fd0, fd1, fd2;       // file handles
	char buf[MAXREADWRITE]; 
	clock_t clock(void);
	strcpy (filename[0], "file0"); 
	strcpy (filename[1], "file1"); 
	strcpy (filename[2], "file2"); 
	clock_t read_start, read_end, read_time, write_start, write_end, write_time;

	if (argc != 2) {
		printf ("usage: app <diskname>\n"); 
		exit (1);
	}

       
	strcpy (diskname, argv[1]); 
	
	if (myfs_mount (diskname) != 0) {
		printf ("could not mound %s\n", diskname); 
		exit (1); 
	}
	else 
		printf ("filesystem %s mounted\n", diskname); 
	

	for (i=0; i<3; ++i) {
		if (myfs_create (filename[i]) != 0) {
			printf ("could not create file %s\n", filename[i]); 
			exit (1); 
		}
		else 
			printf ("file %s created\n", filename[i]); 
	}

	fd0 = myfs_open (filename[0]); 	
	if (fd0 == -1) {
		printf ("file open failed: %s\n", filename[0]); 
		exit (1); 
	}

	write_start = clock();
	for (int k=0;k < 1000; k++){
		fd0 = myfs_open (filename[0]); 	

		for (i=0; i<100; ++i) {
			n = myfs_write (fd0, buf, 1024);  
			if (n != 1024) {
				printf ("vsfs_write failed\n"); 
				exit (1); 
			}
		}
		myfs_close (fd0); 

	}
	printf("a:\n");
	myfs_print_blocks(filename[0]);

	write_end = clock();

   	write_time = (double)(write_end - write_start) / 1000;

    printf("\nTotal time taken by CPU to write: %f\n", write_time  );
    
    printf("fd0 %d",fd0);
    int mysize = myfs_filesize(fd0);
    printf("\n mysize: %d",mysize);
    
	read_start = clock();

	for (i=0; i<(100*500); ++i) 
	{
		n = myfs_read (fd0, buf, 1); 
		if (n != 1) {
			printf ("vsfs_read failed\n"); 
			exit(1); 
		}
	}

	read_end = clock();

   	read_time = (double)(read_end - read_start) / 1000;
   	printf("\nTotal time taken by CPU to read: %f\n", read_time  );

	myfs_print_dir();

	myfs_close (fd0); 

	fd1 = myfs_open (filename[1]); 
	fd2 = myfs_open (filename[2]); 

	myfs_close (fd1);
	myfs_close (fd2); 
	 
	// printf("\n before format:");
	// myfs_print_dir();
	// myfs_makefs(diskname);
	// printf("\n after format:");
	// myfs_print_dir();
	myfs_umount(); 
	
	return (0);		
}