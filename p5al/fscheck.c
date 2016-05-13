#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
//#include <stdint.h>
#include "fs.h"

#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Special device

#define DBG_DISPLAY_SB_DATA 0
#define DBG_ADDRESS_ARITHMETIC 0
#define DBG_INODE_ADDRESSES 0
#define DBG_FL_SIZE 0
#define DBG_SIZEOF_UINT_T 0
#define DBG_POINTERS_AND_ARRAYS 0
#define DBG_BFS 1

void parseOneINode(uint, uint);
void *imgPtr;
int fd = -1;
uint nblocks;
uint ninodes;
uint size;
uint *bitmap;

void *blockPtrFromBNum(uint bnum) {
	return imgPtr + bnum * BSIZE;
}

struct dinode *inodeFromINum(uint inum) {
	uint blockNum = IBLOCK(inum);
	void *blockPtr = blockPtrFromBNum(blockNum);
	return (struct dinode *) blockPtr + (inum % IPB);
}

void ezErr(const char *msg) {
	perror(msg);
	if (fd != -1) close(fd);
	exit(1);
}

typedef struct __inode__joeb {
	struct dinode *			inode;
	uint 					numLinks;
//	sem_t					lock;
} inode_joeb;

inode_joeb root;
inode_joeb *iary;

typedef struct __qnode__joeb {
	void *					data;
	struct __qnode__joeb *	next;
} qnode_joeb;

typedef struct __queue__joeb {
	qnode_joeb *				head;
	qnode_joeb *				tail;
} queue_joeb;

void queue_push(queue_joeb *theQueue, void *data) {
	qnode_joeb *newNode = malloc(sizeof(qnode_joeb));
	newNode->data = data;
	newNode->next = NULL;
	if (theQueue->head == NULL) {
		theQueue->head = newNode;
	}
	else {
		theQueue->tail->next = newNode;
	}
	theQueue->tail = newNode;
}

void *pop(queue_joeb *theQueue) {
	qnode_joeb *retNode = theQueue->head;
	theQueue->head = retNode->next;
	if (theQueue->head == NULL) {
		theQueue->tail = NULL;
	}
	return retNode->data;
}

void checkSuperBlock() {
	struct superblock *testSB = imgPtr + BSIZE;
	nblocks = testSB->nblocks;
	ninodes = testSB->ninodes;
	size = testSB->size;
	iary = calloc(ninodes, sizeof(inode_joeb));
//	iary = malloc(sizeof(inode_joeb) * ninodes);
//	bzero(iary, sizeof(inode_joeb) * ninodes);
	bitmap = calloc(size, sizeof(uint));
//	bzero(bitmap, sizeof(uint) * size);
//	int i;
//	for (i = 1; i < ninodes; i++) {
//		iary[i].inode = inodeFromINum()
//	}
	if (DBG_DISPLAY_SB_DATA) {
		printf("siperblock: %p\n", testSB);
		printf("nblocks: %u\n", nblocks);
		printf("ninodes: %u\n", ninodes);
		printf("size: %u\n", size);
		printf("BPB: %u\n", BPB);
		uint i;
		for (i = 0; i < size; i++) {
//			printf("bblock(%u): %u\n", i, BBLOCK(i,ninodes));
		}
	}
}

void checkRoot() {
	root.inode = inodeFromINum(ROOTINO);
	if (root.inode->type != T_DIR) {
		ezErr("root directory does not exist.");
	}

}

void markBlockInUse(uint bnum, uint inum) {
	if (bitmap[bnum] != 0 && bitmap[bnum] != inum) {
		ezErr("address used more than once.");
	}
	bitmap[bnum] = inum;
	unsigned char *bitBlock = blockPtrFromBNum(BBLOCK(bnum, ninodes));
	if ( (bitBlock[bnum / 8] & (0x1 << (bnum % 8))) != (0x1 << (bnum % 8))) {
		ezErr("address used by inode but marked free in bitmap.");
	}

}

void parseOneDirent(uint blockNum, uint iBytesRead, uint inum, uint parentINum) {
	struct dirent *currDirent = (struct dirent *) blockPtrFromBNum(blockNum);
	uint i;
	for (i = 0; i < BSIZE/sizeof(struct dirent) && currDirent[i].inum > 0; i++) {
		if (DBG_BFS) {
			printf("examining entry number %u (%s)\n", i, currDirent[i].name);
			printf("bytes read = %u\n", iBytesRead);
		}
		if (iBytesRead == 0) {
			if (i == 0) {
				if (strcmp(currDirent[i].name, ".") != 0) {
					ezErr("directory not properly formatted.");
				}
			}
			else if (i == 1) {
				if (strcmp(currDirent[i].name, "..") != 0) {
					ezErr("directory not properly formatted.");
				}
				else if (currDirent[i].inum != parentINum) {
					ezErr("parent directory mismatch.");
				}
			}
			else {
				parseOneINode(currDirent[i].inum, inum);
			}
		}
	}
}

void readIndirectAddresses(uint blockNum, uint inum, uint parentINum,
						   uint bytesRead, uint fsize) {

}

uint getBlockNumFromINode(struct dinode *inode, uint flBlockNum, uint inum) {
	if (flBlockNum < NDIRECT) {
		return inode->addrs[flBlockNum];
	}
	uint indirectBlockNum = inode->addrs[NDIRECT];
	markBlockInUse(indirectBlockNum, inum);
	uint *indrAddresses = (uint *)blockPtrFromBNum(indirectBlockNum);
	if (indrAddresses[flBlockNum - NDIRECT] > size) {
		ezErr("bad address in inode.");
	}
	return indrAddresses[flBlockNum - NDIRECT];
}

void parseOneINode(uint inum, uint parentINum) {
	if (DBG_BFS) {
		printf("parsing inode %u...\n", inum);
	}
	iary[inum].numLinks++;
	struct dinode *inode = inodeFromINum(inum);
	if (inode->type < T_DIR || inode->type > T_DEV) {
		ezErr("bad inode.");
	}
	if (inode->type == T_DIR && iary[inum].numLinks > 1) {
		ezErr("directory appears more than once in file system.");
	}
	uint bytesRead;
	uint blockNum;
	for (bytesRead = 0; bytesRead < inode->size; bytesRead += BSIZE) {
		blockNum = getBlockNumFromINode(inode, bytesRead / BSIZE, inum);
		if (blockNum > size) {
			ezErr("bad address in inode.");
		}
		markBlockInUse(blockNum, inum);
		if (inode->type == T_DIR) {
			parseOneDirent(blockNum, bytesRead, inum, parentINum);
		}
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
	if (DBG_POINTERS_AND_ARRAYS) {
		bitmap = malloc(sizeof(ushort) * 10);
		bitmap[0] = 2;
		bitmap[1] = 3;
		printf("[0]: %u\n", bitmap[0]);
		printf("[1]: %u\n", bitmap[1]);
		printf("*bitmap: %u\n", *bitmap);
		printf("++*bitmap: %u\n", ++*bitmap);
		exit(0);
	}
	if (argc != 2)
		ezErr("usage: fscheck file_system_image");
	struct stat buf;
	if (stat(argv[1], &buf) != 0)
		ezErr("image not found.");
	if (DBG_FL_SIZE) {
		printf("img file size: %lu\n", buf.st_size);
	}
	fd = open(argv[1], O_RDWR);
	if (fd < 0)
		ezErr("image not found.");
	imgPtr = mmap(NULL, buf.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (imgPtr == MAP_FAILED) {
		ezErr("mmap failed");
	}
	checkSuperBlock();
	if (DBG_INODE_ADDRESSES) iNodeAddressStuff();
	checkRoot();
	parseOneINode(1, 1);
	close(fd);
	return 0;
}
