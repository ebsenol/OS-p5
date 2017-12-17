

all:  clean2 clean libmyfs.a  app createdisk formatdisk createVdisk

libmyfs.a:  	myfs.c
	gcc -Wall -c myfs.c
	ar -cvq  libmyfs.a myfs.o
	ranlib libmyfs.a

app: 	app.c
	gcc -Wall -o app app.c  -L. -lmyfs

createdisk: createdisk.c
	gcc -Wall -o createdisk createdisk.c

formatdisk: formatdisk.c
	gcc -Wall -o formatdisk formatdisk.c -L. -lmyfs

clean: 
	rm -fr *.o *.a *~ a.out app createdisk formatdisk

clean2:
	rm -rf vdisk

createVdisk:
	./createdisk vdisk