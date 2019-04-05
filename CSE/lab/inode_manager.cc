#include "inode_manager.h"
#include <time.h>

#define MAXFILENAMELEN 60
#define INODENUMLEN 4
#define MAXDIRENTRYLEN (MAXFILENAMELEN+INODENUMLEN)

// disk layer -----------------------------------------

disk::disk()
{
	bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
	// check boundary
	if (NULL == buf)
	{
		std::cout << "\tIn im-read_block: read buffer is NULL!" << std::endl;
		return;
	}
	if (id < 0 || id >= BLOCK_NUM)
	{
		std::cout << "\tIn im-read_block: invalid block id to read!" << std::endl;
		return;
	}
	memcpy(buf, (char *)blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
	// check boundary
	if (NULL == buf)
	{
		std::cout << "\tIn im-write_block: write buffer is NULL!" << std::endl;
		return;
	}
	if (id < 0 || id >= BLOCK_NUM)
	{
		std::cout << "\tIn im-write_block: invalid block id to write!" << std::endl;
		return;
	}
	memcpy((char *)blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
	// block id of the first available block, one more then the last block in inode table
	blockid_t firstBlockId = 3 + BLOCK_NUM/BPB + INODE_NUM/IPB;

	for (blockid_t i = firstBlockId; i < BLOCK_NUM; i++)
	{
		if (0 == using_blocks[i])
		{
			using_blocks[i] = 1;
			std::cout << "\tIn im-alloc_block: block[" << i << "] allocated" << std::endl;
			return i;
		}
	}
	std::cout << "\t In im-alloc_block: failed to allocate block!" << std::endl;
	return 0;
}

void
block_manager::free_block(uint32_t id)
{
	blockid_t firstBlockId = 3 + BLOCK_NUM/BPB + INODE_NUM/IPB;
	if (id < firstBlockId || id >= BLOCK_NUM)
	{
		std::cout << "\tIn im-free_block: invalid block id!" << std::endl;
		return;
	}
	using_blocks[id] = 0;
	std::cout << "\tIn im-free_block: block[" << id << "] freed" << std::endl;
	return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
// |1block|2+9blocks            |512blocks      |remain  |
// bitmaps left empty, use std::map in d as data bitmap, and directly traverse through all inodes when inode bitmap is needed
block_manager::block_manager()
{
	d = new disk();

	// format the disk
	sb.size = BLOCK_SIZE * BLOCK_NUM;
	sb.nblocks = BLOCK_NUM;
	sb.ninodes = INODE_NUM;
}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
	bm = new block_manager();
	uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
	if (root_dir != 1) 
	{
		std::cout << "\tIn im-inode_manager: error! alloc first inode " << root_dir <<", should be 1" << std::endl;
    	exit(0);
	}
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
	for(int i = 1; i <= INODE_NUM; i++)
	{
		if(!get_inode(i))
		{
			std::cout << "\tIn im-alloc_inode: inode No." << i << " allocated" << std::endl;
			unsigned int current = (unsigned int)time(NULL);
			inode_t ino;
			ino.type = type;
			ino.size = 0;
			ino.atime = current;
			ino.mtime = current;
			ino.ctime = current;
			put_inode(i, &ino);
			return i;
		}
	}
	std::cout << "\tIn im-alloc_inode: failed to allocate inode!" << std::endl;
	return 0;
}

void
inode_manager::free_inode(uint32_t inum)
{
	if (inum <= 0 || inum > INODE_NUM)
	{
		std::cout << "\tIn im-free_inode: invalid inode number!" << std::endl;
	}

	inode_t *ino = get_inode(inum);
	if(!ino)
	{
		std::cout << "\tIn im-free_inode: failed to find inode!" << std::endl;
		return;
	}

	ino->type = 0;
	put_inode(inum, ino);
	free(ino);
	return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
inode_t* 
inode_manager::get_inode(uint32_t inum)
{
	inode_t *ino, *ino_disk;
	char buf[BLOCK_SIZE];

	std::cout << "\tIn im-get_inode: get_inode " << inum << std::endl;

	if (inum < 0 || inum >= INODE_NUM) 
	{
		std::cout << "\tIn im-get_inode: inum out of range" << std::endl;
		return NULL;
	}
	bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);

	ino_disk = (struct inode*)buf + (inum-1)%IPB;		// inum starts from 1
	if (ino_disk->type == 0) 
	{
		std::cout << "\tIn im-get_inode: inode not exist" << std::endl;
		return NULL;
	}

	ino = (inode_t*)malloc(sizeof(inode_t));
	*ino = *ino_disk;

	return ino;
}

void
inode_manager::put_inode(uint32_t inum, inode_t *ino)
{
	char buf[BLOCK_SIZE];
	inode_t *ino_disk;

	std::cout << "\tIn im-put_inode: put_inode " << inum << std::endl;
	if (NULL == ino)
	{
		return;
	}
	ino->ctime = (unsigned int)time(NULL);

	bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
	ino_disk = (inode_t*)buf + (inum-1)%IPB;		// inum starts from 1
	*ino_disk = *ino;
	bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
	inode_t *ino = get_inode(inum);
	if(!ino)
	{
		std::cout << "\tIn im-read_file: failed to find inode!" << std::endl;
		return;
	}

	std::cout << "\tIn im-read_file: reading inode No." << inum << std::endl;
	int blockNum = ino->size / BLOCK_SIZE;      // number of blocks used
	if(ino->size % BLOCK_SIZE > 0)
	{
		blockNum++;
	}
	char *buf = (char*)malloc(blockNum * BLOCK_SIZE);

	if(NDIRECT >= blockNum)                     // indirect block unused
	{
		for(int i = 0; i < blockNum; i++)
		{
			bm->read_block(ino->blocks[i], buf + i * BLOCK_SIZE);
		}
	}
	else if(NDIRECT < blockNum)                 // indirect block used
	{
		for(int j = 0; j < NDIRECT; j++)
		{
			bm->read_block(ino->blocks[j], buf + j * BLOCK_SIZE);
		}

		uint indirectBlockid[NINDIRECT];        // blockids in indirect block
		bm->read_block(ino->blocks[NDIRECT], (char*)indirectBlockid);
		for(int k = 0; k < (blockNum-NDIRECT); k++)
		{
			bm->read_block(indirectBlockid[k], buf + (NDIRECT + k) * BLOCK_SIZE);
		}
	}

	*buf_out = buf;
	*size = ino->size;
	ino->atime = (unsigned int)time(NULL);
	put_inode(inum, ino);
	free(ino);
	std::cout << "\tIn im-read_file: complete reading inode No." << inum << " of size " << *size << std::endl;
	return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
	inode_t *ino = get_inode(inum);
	if (!ino)
	{
		std::cout << "\tIn im-write_file: failed to find inode!" << std::endl;
		return;
	}

	std::cout << "\tIn im-write_file: writing " << size << " bytes to inode No." << inum << std::endl;
	int blockNum = ino->size / BLOCK_SIZE;              // number of blocks used
	if(ino->size % BLOCK_SIZE > 0)
	{
		blockNum++;
	}
	int blockNumToWrite = size / BLOCK_SIZE;            // number of blocks to use
	if(size % BLOCK_SIZE > 0)
	{
		blockNumToWrite++;
	}
	if(blockNumToWrite > MAXFILE)
	{
		std::cout << "\tIn im-write_file: too much data for one file to contain!" << std::endl;
		return;
	}

	// allocate new blocks or free redundant blocks, depending on the relationship between blockNum and blockNumToWrite
	uint indirectBlockid[NINDIRECT];                                // blockids in indirect block
	bm->read_block(ino->blocks[NDIRECT], (char*)indirectBlockid);
	if(blockNum < blockNumToWrite)
	{
		// blockNum < blockNumToWrite <= NDIRECT
		if(blockNumToWrite <= NDIRECT)
		{
			for(int i = blockNum; i < blockNumToWrite; i++)
			{
				ino->blocks[i] = bm->alloc_block();
			}
		}
		// NDIRECT < blockNumToWrite
		else
		{
			// blockNum <= NDIRECT < blockNumToWrite
			if(blockNum <= NDIRECT)
			{
				for(int i = blockNum; i <= NDIRECT; i++)			// one more block for indirect block
				{
					ino->blocks[i] = bm->alloc_block();
				}
				for(int j = NDIRECT; j < blockNumToWrite; j++)      // allocate blocks in indirect block
				{
					indirectBlockid[j-NDIRECT] = bm->alloc_block();
				}
			}
			// NDIRECT < blockNum < blockNumToWrite
			else
			{
				for(int i = blockNum; i < blockNumToWrite; i++)
				{
					indirectBlockid[i-NDIRECT] = bm->alloc_block();
				}
			}
			// write blockids into indirect block
			bm->write_block(ino->blocks[NDIRECT], (char*)indirectBlockid);
		}
	}
	else if(blockNum > blockNumToWrite)
	{
		// blockNumToWrite < blockNum <= NDIRECT
		if(blockNum <= NDIRECT)
		{
			for(int i = blockNumToWrite; i < blockNum; i++)
			{
				bm->free_block(ino->blocks[i]);
			}
		}
		// NDIRECT < blockNum
		else
		{
			bm->read_block(ino->blocks[NDIRECT], (char*)indirectBlockid);
			// blockNumToWrite <= NDIRECT < blockNum
			if(blockNumToWrite <= NDIRECT)
			{
				for(int i = blockNumToWrite; i <= NDIRECT; i++)     // one more block for indirect block
				{
					bm->free_block(ino->blocks[i]);
				}
				for(int j = NDIRECT; j < blockNum; j++)             // free blocks in indirect block
				{
					bm->free_block(indirectBlockid[j-NDIRECT]);
				}
			}
			// NDIRECT < blockNumToWrite < blockNum
			else
			{
				for(int i = blockNumToWrite; i < blockNum; i++)
				{
					bm->free_block(indirectBlockid[i-NDIRECT]);
				}
			}
		}
	}

	// actually write file
	if(NDIRECT >= blockNumToWrite)
	{
		for(int i = 0; i < blockNumToWrite; i++)
		{
			std::cout << "\tblocks[" << i << "], corresponding to blockid " << ino->blocks[i] << std::endl;
			bm->write_block(ino->blocks[i], buf + i * BLOCK_SIZE);
		}
	}
	else
	{
		for(int i = 0; i < NDIRECT; i++)
		{
			std::cout << "\tblocks[" << i << "], corresponding to blockid " << ino->blocks[i] << std::endl;
			bm->write_block(ino->blocks[i], buf + i * BLOCK_SIZE);
		}
		for(int j = NDIRECT; j < blockNumToWrite; j++)
		{
			std::cout << "\tindirect[" << (j-NDIRECT) << "], corresponding to blockid " << indirectBlockid[j-NDIRECT] << std::endl;
			bm->write_block(indirectBlockid[j-NDIRECT], buf + j * BLOCK_SIZE);
		}
	}

	ino->size = size;
	ino->mtime = (unsigned int)time(NULL);
	put_inode(inum, ino);
	//free(ino);
	std::cout << "\tIn in-write_file: " << size << " bytes written to inode No." << inum << std::endl;
	return;

}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
	inode_t *ino = get_inode(inum);
	if (!ino)
	{
		std::cout << "\tIn im-get_attr: failed to find inode!" << std::endl;
		return;
	}
	a.type = ino->type;
	a.size = ino->size;
	a.atime = ino->atime;
	a.mtime = ino->mtime;
	a.ctime = ino->ctime;
	//free(ino);
	return;
}

void
inode_manager::remove_file(uint32_t inum)
{
	inode_t *ino = get_inode(inum);
	if(!ino)
	{
		std::cout << "\tIn im-remove_file: failed to find inode!" << std::endl;
		return;
	}

	std::cout << "\tIn im-remove_file: removing inode " << inum << std::endl;

	// if remove a dir, remove everything within at the same time
	if(extent_protocol::T_DIR == ino->type)
	{
		char *buf;
		int size = 0;
		read_file(inum, &buf, &size);
		for(int i = 0; i < size; i += MAXDIRENTRYLEN)
		{
			uint32_t subino = *(uint32_t *)(buf + i + MAXFILENAMELEN);
			remove_file(subino);
		}
	}

	int blockNum = ino->size / BLOCK_SIZE;
	if(ino->size % BLOCK_SIZE > 0)
	{
		blockNum++;
	}

	// free data blocks
	if(blockNum <= NDIRECT)
	{
		for(int i = 0; i < blockNum; i++)
		{
			bm->free_block(ino->blocks[i]);
		}
	}
	else
	{
		uint indirectBlockid[NINDIRECT];
		bm->read_block(ino->blocks[NDIRECT], (char*)indirectBlockid);
		for(int i = 0; i <= NDIRECT; i++)       // one more block for indirect block
		{
			bm->free_block(ino->blocks[i]);
		}
		for(int j = NDIRECT; j < blockNum; j++) // free blocks in indirect block
		{
			bm->free_block(indirectBlockid[j-NDIRECT]);
		}
	}

	free_inode(inum);
	free(ino);
	return;
}

/*
 * append_block: Given an inode number, allocate and append a block to the inode and return its id. 
 * Note that in HDFS, namenode only manage the metadata, so the actual data (even the size) won't be given in this request.
 */
void
inode_manager::append_block(uint32_t inum, blockid_t &bid)
{
	std::cout << "\tIn im-append_block: appending block on inode " << inum << std::endl;
	inode_t *ino = get_inode(inum);
	int blockNum = ino->size / BLOCK_SIZE;
	if (ino->size % BLOCK_SIZE > 0)
	{
		blockNum++;
	}
	bid = bm->alloc_block();

	// file too large
	if (blockNum > MAXFILE)
	{
		return;
	}
	// small file without appending indirect block
	else if (blockNum < NDIRECT)
	{
		ino->blocks[blockNum] = bid;
	}
	// file with indirect block
	else
	{
		// allocate indirect block if needed
		if (NDIRECT == blockNum)
		{
			ino->blocks[NDIRECT] = bm->alloc_block();
		}
		uint indirectBlockID[NINDIRECT];
		bm->read_block(ino->blocks[NDIRECT], (char*)indirectBlockID);
		indirectBlockID[blockNum-NDIRECT] = bid;
		bm->write_block(ino->blocks[NDIRECT], (char*)indirectBlockID);
	}
	ino->size += BLOCK_SIZE;
	put_inode(inum, ino);
}

/*
 * get_block_ids: Given an inode number, return id of all data blocks of the file.
 */
void
inode_manager::get_block_ids(uint32_t inum, std::list<blockid_t> &block_ids)
{
	inode_t *ino = get_inode(inum);
	if (!ino || 0 == ino->type)
	{
		return;
	}
	int blockNum = ino->size / BLOCK_SIZE;
	if (ino->size % BLOCK_SIZE > 0)
	{
		blockNum++;
	}
	
	if (blockNum <= NDIRECT)
	{
		for (int i = 0; i < blockNum; i++)
		{
			block_ids.push_back(ino->blocks[i]);
		}
	}
	else
	{
		uint indirectBlockID[NINDIRECT];
		bm->read_block(ino->blocks[NDIRECT], (char*)indirectBlockID);
		for (int i = 0; i < NDIRECT; i++)
		{
			block_ids.push_back(ino->blocks[i]);
		}
		for (int j = NDIRECT; j < blockNum; j++)
		{
			block_ids.push_back(indirectBlockID[j-NDIRECT]);
		}
	}
	return;
}

/*
 * read_block: Given a block id, return the data of the disk block.
 */
void
inode_manager::read_block(blockid_t id, char buf[BLOCK_SIZE])
{
	bm->read_block(id, buf);
}

/*
 * write_block: Given a block id and data, write the data to the disk block.
 */
void
inode_manager::write_block(blockid_t id, const char buf[BLOCK_SIZE])
{
	bm->write_block(id, buf);
}

/*
 * complete: Given an inode number and size, this request indicates that the writes to the file is finished, so the metadata (file size, modification time) in inode could be updated safely.
 */
void
inode_manager::complete(uint32_t inum, uint32_t size)
{
	inode_t *ino = get_inode(inum);
	ino->size = size;
	put_inode(inum, ino);
}
