// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// one entry in directory consists of filename and inode number, occupying 60 and 4 bytes seperately
#define MAXFILENAMELEN 60
#define INODENUMLEN    4
#define MAXDIRENTRYLEN (MAXFILENAMELEN+INODENUMLEN)

yfs_client::yfs_client()
{
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
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

//===================================================================

bool
yfs_client::isdir(inum inum)
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

bool
yfs_client::issymlink(inum inum)
{   
	extent_protocol::attr a;

	if (ec->getattr(inum, a) != extent_protocol::OK) 
	{
		printf("error getting attr\n");
		return false;
	}

	if (a.type == extent_protocol::T_SYMLINK)
	{
		printf("issymlink: %lld is a symbol link\n", inum);
		return true;
	}
	printf("issymlink: %lld is not a symbol link\n", inum);
	return false;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}

int
yfs_client::getsymlink(inum inum, symlinkinfo &sym)
{
	int r = OK;

	printf("getsymlink %016llx\n", inum);
	extent_protocol::attr a;
	if (ec->getattr(inum, a) != extent_protocol::OK) 
	{
		r = IOERR;
		goto release;
	}
	sym.atime = a.atime;
	sym.mtime = a.mtime;
	sym.ctime = a.ctime;
	sym.size = a.size;

release:
	return r;
}

#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
	// validate parameters
	if(ino <= 0 || ino > INODE_NUM)
	{
		std::cout << "In setattr: invalid inode number!" << std::endl;
		return IOERR;
	}
	if(size < 0)
	{
		std::cout << "In setattr: negative size!" << std::endl;
	}

	std::cout << "In setattr: setting attributes on inode No." << ino << std::endl;
    int r = OK;
	std::string buf;

	if((r = ec->get(ino, buf)) != extent_protocol::OK)
	{
		std::cout << "In setattr: failed to get inode info!" << std::endl;
		return r;
	}
	if(buf.size() < size)
	{
		buf.insert(buf.size(), size-buf.size(), 0);
	}
	else
	{
		buf = buf.substr(0, size);
	}
	if((r = ec->put(ino, buf)) != extent_protocol::OK)
	{
		std::cout << "In setattr: failed to set attribute!" << std::endl;
		return r;
	}

    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
	std::cout << "In create: creating file named " << name << "under inode No." << parent << std::endl;
    int r = OK;

	// check existence
	bool found = false;
	lookup(parent, name, found, ino_out);
	if(found)
	{
		return EXIST;
	}

	// create file and update parent info
	if ((r = ec->create(extent_protocol::T_FILE, ino_out)) != extent_protocol::OK)
	{
		std::cout << "In create: failed to create inode!" << std::endl;
		return r;
	}
	std::string buf;
	if ((r = ec->get(parent, buf)) != extent_protocol::OK)
	{
		std::cout << "In create: failed to get inode info!" << std::endl;
		return r;
	}
	buf.insert(buf.size(), name);
	buf.insert(buf.size(), MAXDIRENTRYLEN-strlen(name), 0);
	*(uint32_t*)(buf.c_str()+buf.size()-INODENUMLEN) = ino_out;
	if ((r = ec->put(parent, buf)) != extent_protocol::OK)
	{
		std::cout << "In create: failed to write inode back!" << std::endl;
		return r;
	}
	std::cout << "In create: file named " << name << "created with inode No." << ino_out <<std::endl;

    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
	std::cout << "In mkdir: creating directory named " << name << "under inode No." << parent << std::endl;
    int r = OK;

	// check existence
	bool found = false;
	lookup(parent, name, found, ino_out);
	if(found)
	{
		return EXIST;
	}

	// create directory and update parent info
	if ((r = ec->create(extent_protocol::T_DIR, ino_out)) != extent_protocol::OK)
	{
		std::cout << "In mkdir: failed to create inode!" << std::endl;
		return r;
	}
	std::string buf;
	if ((r = ec->get(parent, buf)) != extent_protocol::OK)
	{
		std::cout << "In mkdir: failed to get inode info!" << std::endl;
		return r;
	}
	buf.insert(buf.size(), name);
	buf.insert(buf.size(), MAXDIRENTRYLEN-strlen(name), 0);
	*(uint32_t*)(buf.c_str()+buf.size()-INODENUMLEN) = ino_out;
	if ((r = ec->put(parent, buf)) != extent_protocol::OK)
	{
		std::cout << "In mkdir: failed to write inode back!" << std::endl;
		return r;
	}
	std::cout << "In mkdir: directory named " << name << "created with inode No." << ino_out <<std::endl;

    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
	std::cout << "In lookup: searching file named " << name << "under inode No." << parent << std::endl;
    int r = OK;
	std::string buf;

	if ((r = ec->get(parent, buf)) != extent_protocol::OK)
	{
		std::cout << "In lookup: failed to get inode info!" << std::endl;
		return r;
	}
	const char *cbuf = buf.c_str();
	for(int i = 0; i < buf.size(); i += MAXDIRENTRYLEN)
	{
		if(!strcmp(cbuf + i, name))
		{
			ino_out = *(uint32_t *)(cbuf + i + MAXFILENAMELEN);
			found = true;
			return r;
		}
	}
	found = false;
	std::cout << "In lookup: lookup failed!" << std::endl;
    return r;
}
																	
int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;
	std::string buf;

	if ((r = ec->get(dir, buf)) != extent_protocol::OK)
	{
		std::cout << "In readdir: failed to get inode info!" << std::endl;
		return r;
	}
	for(int i = 0; i < buf.size(); i += MAXDIRENTRYLEN)
	{
		struct dirent item;
		item.name = buf.substr(i, MAXFILENAMELEN);
		item.inum = *(uint32_t *)(buf.c_str() + i + MAXFILENAMELEN);
		list.push_back(item);
	}

    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
	int r = OK;
	std::string buf;

	if ((r = ec->get(ino, buf)) != extent_protocol::OK)
	{
		std::cout << "In read: failed to get inode info!" << std::endl;
		return r;
	}
	if (off >= buf.size())					// offset exceeds file size, nothing to read
	{
		data = "";
	}
    else 
	{
        if ((off+size) <= buf.size())		// normal case
		{
            data = buf.substr(off, size);
		}
        else								// less then size bytes are available
		{
            data = buf.substr(off);
		}
	}
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;
    std::string buf;
    bytes_written = 0;
	int maxFileSize = MAXFILE * BLOCK_SIZE;		// max number of bytes in one single file

	// convert data to std::string and get rid of the last '\0'
	// std::string dataString = data; Shit why this line doesn't work?
	std::string dataString(size, 0);
	for(int i = 0; i < size; i++)
	{
		dataString[i] = data[i];
	}
	dataString = dataString.substr(0, size);

	if ((r = ec->get(ino, buf)) != extent_protocol::OK)
	{
		std::cout << "In write: failed to get inode info!" << std::endl;
		return r;
	}
	if(off < maxFileSize)						// offset within the max size of one single file
	{
		if(off+size > maxFileSize)              // the last byte to write exceeds the end of last available block
		{
			// truncate the data to write
			dataString = dataString.substr(0, (maxFileSize-off));
			size = maxFileSize - off;
		}
		if (off > buf.size())					// there is a hole
		{
			bytes_written = off +size - buf.size();
			// fill the hole with '\0'
			buf.insert(buf.size(), (off-buf.size()), '\0');
			buf += dataString;
		}
		else
		{
			buf.replace(off, size, dataString);
	        bytes_written = size;
   		}
		
	}
	else                                        // offset exceeds the max size of one single file
	{
		bytes_written = maxFileSize - buf.size();
		// fill the rest of the file with '\0'
		buf.insert(buf.size(), (maxFileSize-buf.size()), '\0');
	}
	if ((r = ec->put(ino, buf)) != extent_protocol::OK)
	{
		std::cout << "In write: failed to write inode back!" << std::endl;
		return r;
	}
    return r;
}

int yfs_client::unlink(inum parent, const char *name)
{
    int r = OK;
	std::string buf;

	// used to check whether name is equal to filename or a substring of filename
	std::string end;
	end.insert(0, 1, 0);

	if ((r = ec->get(parent, buf)) != extent_protocol::OK)
	{
		std::cout << "In unlink: failed to get inode info!" << std::endl;
		return r;
	}
	for(int i = 0; i < buf.size(); i += MAXDIRENTRYLEN)					// search file by name
	{
		if(buf.substr(i, strlen(name)) == std::string(name) && (MAXFILENAMELEN == strlen(name) || !buf.substr(i+strlen(name), 1).compare(end)))
		{
			inum ino = *(uint32_t *)(buf.c_str()+i+MAXFILENAMELEN);		// retrieve inode number
			buf.erase(i, MAXDIRENTRYLEN);
			if ((r = ec->remove(ino)) != extent_protocol::OK)
			{
				std::cout << "In unlink: failed to remove inode!" << std::endl;
				return r;
			}
			if ((r = ec->put(parent, buf)) != extent_protocol::OK)
			{
				std::cout << "In unlink: failed to write inode back!" << std::endl;
				return r;
			}
			return r;
		}
	}

	return r;
}

int yfs_client::readlink(inum ino, std::string &content)
{
	int r = OK;
	if ((r = ec->get(ino, content)) != extent_protocol::OK)
	{
		std::cout << "In readlink: failed to get inode info!" << std::endl;
		return r;
	}
	return r;
}

int yfs_client::symlink(const char *link, inum parent, const char *name, inum &ino_out)
{
	int r = OK;

	// check existence
	bool found = false;
	inum ino;
	lookup(parent, name, found, ino);
	if(found)
	{
		return EXIST;
	}

	if ((r = ec->create(extent_protocol::T_SYMLINK, ino_out)) != extent_protocol::OK)
	{
		std::cout << "In symlink: failed to create inode!" << std::endl;
		return r;
	}
	std::string buf;
	r = ec->get(parent, buf);
	buf.insert(buf.size(), name);
	buf.insert(buf.size(), MAXDIRENTRYLEN-strlen(name), 0);
	*(uint32_t*)(buf.c_str()+buf.size()-INODENUMLEN) = ino_out;
	if ((r = ec->put(parent, buf)) != extent_protocol::OK)
	{
		std::cout << "In symlink: failed to write inode back!" << std::endl;
		return r;
	}
	if ((r = ec->put(ino_out, std::string(link))) != extent_protocol::OK)
	{
		std::cout << "In symlink: failed to write inode out!" << std::endl;
		return r;
	}
	return r;
}
