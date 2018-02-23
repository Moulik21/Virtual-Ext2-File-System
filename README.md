How to use:

- Make sure to copy all the disk images from the &quot;Original Virtual Disk&quot; Folder to the source folder.

- ext2\_ls: This program takes two command line arguments. The first is the name of an ext2 formatted virtual disk. The second is an absolute path on the ext2 formatted disk. Should work similar to linux&#39;s ls -l except . and .. will be included.
  - Eg.  ./ext2\_ls onefile.img /

- ext2\_mkdir: This program takes two command line arguments. The first is the name of an ext2 formatted virtual disk. The second is an absolute path on your ext2 formatted disk.
  - Eg. ./ext2\_mkdir emptydisk.img /afolder

- ext2\_rm: This program takes two command line arguments. The first is the name of an ext2 formatted virtual disk, and the second is an absolute path to a file or hard link (not a directory) on that disk. The program should work like rm, removing the specified file from the disk. If the file does not exist or if it is a directory, then the program should return the appropriate error.
  - Eg. ./ext2\_rm onefile.img /afile

- ext2\_cp: This program takes three command line arguments. The first is the name of an ext2 formatted virtual disk. The second is the path to a file on that disk, and the third is an absolute path on your ext2 formatted disk. The program should work like cp, copying the file on ext2 formatted disk onto the specified location on the disk. If the specified file or target location does not exist, then the program should return the appropriate error (ENOENT). If the target is a file with the same name that already exists, you should not overwrite it (as cp would), just return EEXIST instead.
  - Eg. ./ext2\_cp onefile.img /afile /afolder/
