#include "namenode.h"
#include "extent_client.h"
#include "lock_client.h"
#include <sys/stat.h>
#include <unistd.h>
#include "threader.h"

using namespace std;

#define MAXFILENAMELEN 60
#define INODENUMLEN    4
#define MAXDIRENTRYLEN 64

void NameNode::init(const string &extent_dst, const string &lock_dst) {
  ec = new extent_client(extent_dst);
  lc = new lock_client_cache(lock_dst);
  yfs = new yfs_client(extent_dst, lock_dst);

  /* Add your init logic here */
  heartbeatCounter = 0;
  NewThread(this, &NameNode::heartbeatCount);
}

void NameNode::heartbeatCount()
{
	while(true)
	{
		this->heartbeatCounter++;
		sleep(1);
	}
}

/*
 * NameNode::GetBlockLocations: Call get_block_ids and convert block ids to LocatedBlocks.
 */
list<NameNode::LocatedBlock> NameNode::GetBlockLocations(yfs_client::inum ino) 
{
	cout << "get block location: inode " << ino << endl;
	cout.flush();

	list<NameNode::LocatedBlock> locatedBlocks;
	list<blockid_t> blockids;
	extent_protocol::attr attr;
	ec->get_block_ids(ino, blockids);
	ec->getattr(ino, attr);
	uint64_t blocksize;
	uint64_t currentsize = 0;

	list<blockid_t>::iterator iter;
	for (iter = blockids.begin(); iter != blockids.end(); iter++)
	{
		// cope with size of the last block
		iter++;
		blocksize = (iter == blockids.end()) ? (attr.size - currentsize) : BLOCK_SIZE;
		iter--;

		LocatedBlock lb(*iter, currentsize, blocksize, GetDatanodes());
		locatedBlocks.push_back(lb);
		currentsize += BLOCK_SIZE;
	}
	return locatedBlocks;
}

/*
 * NameNode::Complete: Call complete and unlock the file.
 */
bool NameNode::Complete(yfs_client::inum ino, uint32_t new_size) 
{
	cout << "complete: inode " << ino << " with size: " << new_size << endl;
	cout.flush();

	if((ec->complete(ino, new_size)) != extent_protocol::OK)
	{
		return false;
	}
	lc->release(ino);
	return true;
}

/*
 * NameNode::AppendBlock: Call append_block and convert block id to LocatedBlock.
 */
NameNode::LocatedBlock NameNode::AppendBlock(yfs_client::inum ino) 
{
	cout << "append block: inode " << ino << endl;
	cout.flush();

	extent_protocol::attr attr;
	ec->getattr(ino, attr);
	blockid_t newBlock;
	ec->append_block(ino, newBlock);

	uint64_t blocksize = (attr.size % BLOCK_SIZE) ? (attr.size % BLOCK_SIZE) : BLOCK_SIZE;
	LocatedBlock lb(newBlock, attr.size, blocksize, GetDatanodes());
	writtenBlocks.insert(newBlock);
	return lb;
}

/*
 * NameNode::Rename: Move a directory entry. Note that src_name/dst_name is entry name, not full path.
 */
bool NameNode::Rename(yfs_client::inum src_dir_ino, string src_name, yfs_client::inum dst_dir_ino, string dst_name) 
{
	cout << "rename: source: inode " << src_dir_ino << " with name " << src_name << " destination: inode " << dst_dir_ino << " with name " << dst_name << endl;
	cout.flush();

	// read directories
	string srcContent, dstContent;
	ec->get(src_dir_ino, srcContent);
	ec->get(dst_dir_ino, dstContent);

	// search src_name in src_dir
	bool found = false;
	int dst_ino;
	const char *name;
	for(int pos = 0; pos < srcContent.size(); pos += MAXDIRENTRYLEN)
	{
		name = srcContent.c_str() + pos;
		if(!strcmp(name, src_name.c_str()))
		{
			found = true;
			dst_ino = *(uint32_t*)(name + MAXFILENAMELEN);
			srcContent.erase(pos, MAXDIRENTRYLEN);
			break;
		}
	}
	if (found)
	{
		if (src_dir_ino == dst_dir_ino)
		{
			dstContent = srcContent;
		}
		if (dst_name.length() > MAXFILENAMELEN)
		{
			cout<<"\nfile name too long!!!!!!\n";
		}
		dstContent += dst_name;
		dstContent.insert(dstContent.size(), MAXDIRENTRYLEN-dst_name.length(), 0);
		*(uint32_t*)(dstContent.c_str()+dstContent.size()-INODENUMLEN) = dst_ino;
		ec->put(src_dir_ino, srcContent);
		ec->put(dst_dir_ino, dstContent);
	}
	return found;
}

bool NameNode::Mkdir(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out)
{	
	cout << "mkdir: in inode " << parent << " with name " << name << " and mode " << mode << endl;
	cout.flush();

	int res = yfs->mkdir(parent, name.c_str(), mode, ino_out);
	if(res != yfs_client::OK)
	{
		return false;
	}
	return true;
}

/*
 * NameNode::Create: Create a file, lock it before return.
 */
bool NameNode::Create(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out) 
{
	cout << "create: in inode " << parent << " with name " << name << " and mode " << mode << endl;
	cout.flush();

	int res = yfs->create(parent, name.c_str(), mode, ino_out);
	if(res != yfs_client::OK)
	{
		return false;
	}
	lc->acquire(ino_out);
	return true;
}

/*
 * NameNode::Isfile/Isdir/Getfile/Getdir/Readdir/Unlink
 * The same as the functions in yfs_client, but the framework will call these functions with the locks held,
 * so you shouldn't try to lock them again. Otherwise there will be a deadlock.
 * Reuse code in yfs_client and remove the locks
 */
bool NameNode::Isfile(yfs_client::inum inum) 
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
		return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
		return true;
    } 
    printf("isfile: %lld is not a file\n", inum);
	return false;
}

bool NameNode::Isdir(yfs_client::inum inum) 
{
    extent_protocol::attr a;

	if (ec->getattr(inum, a) != extent_protocol::OK) 
	{
	  	printf("error getting attr\n");
		return false;
	}

	if (a.type == extent_protocol::T_DIR) 
	{
		printf("isdir: %lld is a dir\n", inum);
		return true;
	} 
	printf("isdir: %lld is not a dir\n", inum);
	return false;
}

bool NameNode::Getfile(yfs_client::inum inum, yfs_client::fileinfo &fin) 
{
    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) 
	{
        return false;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

	return true;
}

bool NameNode::Getdir(yfs_client::inum inum, yfs_client::dirinfo &din) 
{
    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) 
	{
        return false;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

    return true;
}

bool NameNode::Readdir(yfs_client::inum ino, std::list<yfs_client::dirent> &dir) 
{
	std::string buf;

	if (ec->get(ino, buf) != extent_protocol::OK)
	{
		std::cout << "In readdir: failed to get inode info!" << std::endl;
		return false;
	}
	for(int i = 0; i < buf.size(); i += MAXDIRENTRYLEN)
	{
		struct yfs_client::dirent item;
		item.name = buf.substr(i, MAXFILENAMELEN);
		item.inum = *(uint32_t *)(buf.c_str() + i + MAXFILENAMELEN);
		dir.push_back(item);
	}

    return true;
}

bool NameNode::Unlink(yfs_client::inum parent, string name, yfs_client::inum ino) 
{
	std::string buf;

	// used to check whether name is equal to filename or a substring of filename
	std::string end;
	end.insert(0, 1, 0);

	if (ec->get(parent, buf) != extent_protocol::OK)
	{
		std::cout << "In unlink: failed to get inode info!" << std::endl;
		return false;
	}
	for(int i = 0; i < buf.size(); i += MAXDIRENTRYLEN)					// search file by name
	{
		if(buf.substr(i, name.length()) == std::string(name) && (MAXFILENAMELEN == name.length() || !buf.substr(i+name.length(), 1).compare(end)))
		{
			yfs_client::inum ino = *(uint32_t *)(buf.c_str()+i+MAXFILENAMELEN);		// retrieve inode number
			buf.erase(i, MAXDIRENTRYLEN);
			if (ec->remove(ino) != extent_protocol::OK)
			{
				std::cout << "In unlink: failed to remove inode!" << std::endl;
				return false;
			}
			if (ec->put(parent, buf) != extent_protocol::OK)
			{
				std::cout << "In unlink: failed to write inode back!" << std::endl;
				return false;
			}
			return true;
		}
	}

	return false;
}

// receive heartbeats from datanodes and keep them alive by updating their heartbeat number with heartbeat counter
void NameNode::DatanodeHeartbeat(DatanodeIDProto id) 
{
	datanodes[id] = this->heartbeatCounter;
}

// register a newly started datanode and update its blocks
void NameNode::RegisterDatanode(DatanodeIDProto id)
{
	for(auto block : writtenBlocks)
	{
		ReplicateBlock(block, master_datanode, id);
	}
	datanodes.insert(make_pair(id, this->heartbeatCounter));
}

list<DatanodeIDProto> NameNode::GetDatanodes() {
  list<DatanodeIDProto> datanodeList;
  // datanodeList.push_back(master_datanode);
  for(auto dnode : datanodes)
  {
	  // get rid of dead datanodes
	  if (dnode.second > this->heartbeatCounter - 3)
	  {
		  datanodeList.push_back(dnode.first);
	  }
  }
  return datanodeList;
}
