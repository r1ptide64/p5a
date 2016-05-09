#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "fs.h"

#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Special device

#define DBG_DISPLAY_SB_DATA 0
#define DBG_ADDRESS_ARITHMETIC 0
#define DBG_INODE_ADDRESSES 1
#define DBG_FL_SIZE 0

void *imgPtr;
int fd;
uint nblocks;
uint ninodes;
uint size;

void *blockPtrFromBNum(uint bnum) {
	return imgPtr + bnum * BSIZE;
}

struct dinode *inodeFromINum(int inum) {
	uint blockNum = IBLOCK(inum);
	void *blockPtr = blockPtrFromBNum(blockNum);
	return (struct dinode *) blockPtr + (inum % IPB);
}

void ezErr(const char *msg, const int noCleanup) {
	perror(msg);
	if (noCleanup) {
		exit(1);
	}
}

void checkSuperBlock() {
	struct superblock *testSB = imgPtr + BSIZE;
	nblocks = testSB->nblocks;
	ninodes = testSB->ninodes;
	size = testSB->size;
	if (DBG_DISPLAY_SB_DATA) {
		printf("siperblock: %p\n", testSB);
		printf("nblocks: %u\n", nblocks);
		printf("ninodes: %u\n", ninodes);
		printf("size: %u\n", size);
	}
}

void checkRoot() {
	struct dinode *root = inodeFromINum(ROOTINO);
	if (root->type != T_DIR) {
		ezErr("root directory does not exist.", 0);
		close(fd);
		exit(1);
	}

}

void iNodeAddressStuff() {
	printf("imgPtr:  %p\n", imgPtr);
	struct dinode *root = inodeFromINum(1);
	printf("root:    %p\n", root);
	printf("root+1:  %p\n", root+1);
//	struct dinode {
//	  short type;           // File type
//	  short major;          // Major device number (T_DEV only)
//	  short minor;          // Minor device number (T_DEV only)
//	  short nlink;          // Number of links to inode in file system
//	  uint size;            // Size of file (bytes)
//	  uint addrs[NDIRECT+1];   // Data block addresses
//	};
	printf("  type:  %d\n", root->type);
	printf("  major: %d\n", root->major);
	printf("  minor: %d\n", root->minor);
	printf("  nlink: %d\n", root->nlink);
	printf("  size:  %u\n", root->size);
	int i;
	for (i = 0; i < NDIRECT+1; i++) {
		printf("addrs[%d]:  %u\n", i, root->addrs[i]);
	}
	void *blockStart = blockPtrFromBNum(root->addrs[0]);
	void *blockEnd = blockStart + root->size;
	struct dirent *curr; // = (struct dirent *)blockPtrFromBNum(root->addrs[0]);
	printf("***dirent***\n");
	for (curr = blockStart; curr < (struct dirent *)blockEnd; curr++) {
		printf("inum:  %u\n", curr->inum);
		printf("name:  %s\n", curr->name);
	}
//

//	close(fd);
//	exit(1);
}

int main(int argc, char *argv[]) {
	if (argc != 2)
		ezErr("usage: fscheck file_system_image", 1);
	struct stat buf;
	if (stat(argv[1], &buf) != 0)
		ezErr("image not found.", 1);
	if (DBG_FL_SIZE) {
		printf("img file size: %lu\n", buf.st_size);
	}
	fd = open(argv[1], O_RDWR);
	if (fd < 0)
		ezErr("image not found.", 1);
	imgPtr = mmap(NULL, buf.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (imgPtr == MAP_FAILED) {
		ezErr("mmap failed", 0);
		close(fd);
		exit(1);
	}
	checkSuperBlock();
	if (DBG_INODE_ADDRESSES) iNodeAddressStuff();
	checkRoot();
	close(fd);
	return 0;
}
