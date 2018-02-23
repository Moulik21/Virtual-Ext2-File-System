all: ls mkdir rm cp 

ls: 
	gcc -Wall -g -o ext2_ls ext2_ls.c ext2_utils.c

mkdir:
	gcc -Wall -g -o ext2_mkdir ext2_mkdir.c ext2_utils.c

rm:
	gcc -Wall -g -o ext2_rm ext2_rm.c ext2_utils.c

cp:
	gcc -Wall -g -o ext2_cp ext2_cp.c ext2_utils.c

clean:
	rm -rf *.o ext2_ls *~
	rm -rf *.o ext2_mkdir *~
	rm -rf *.o ext2_rm *~
	rm -rf *.o ext2_cp *~