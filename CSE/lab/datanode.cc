#include "datanode.h"
#include <arpa/inet.h>
#include "extent_client.h"
#include <unistd.h>
#include <algorithm>
#include "threader.h"

using namespace std;

int DataNode::init(const string &extent_dst, const string &namenode, const struct sockaddr_in *bindaddr) {
  ec = new extent_client(extent_dst);

  // Generate ID based on listen address
  id.set_ipaddr(inet_ntoa(bindaddr->sin_addr));
  id.set_hostname(GetHostname());
  id.set_datanodeuuid(GenerateUUID());
  id.set_xferport(ntohs(bindaddr->sin_port));
  id.set_infoport(0);
  id.set_ipcport(0);

  // Save namenode address and connect
  make_sockaddr(namenode.c_str(), &namenode_addr);
  if (!ConnectToNN()) {
    delete ec;
    ec = NULL;
    return -1;
  }

  // Register on namenode
  if (!RegisterOnNamenode()) {
    delete ec;
    ec = NULL;
    close(namenode_conn);
    namenode_conn = -1;
    return -1;
  }

  /* Add your initialization here */
  NewThread(this, &DataNode::heartbeat);

  return 0;
}

void DataNode::heartbeat()
{
	while(true)
	{
		SendHeartbeat();
		sleep(1);
	}
}

/*
 * DataNode::ReadBlock/WriteBlock: Call read_block/write_block to read/write block on extent server. 
 * Be careful that client may want to read/write only parts of the block.
 */
bool DataNode::ReadBlock(blockid_t bid, uint64_t offset, uint64_t len, string &buf) 
{
	string blockContent;
	if (ec->read_block(bid, blockContent) != extent_protocol::OK)
	{
		return false;
	}
	if(offset > blockContent.length())	// offset too large
	{
		buf = "";
	}
	else
	{
		buf = blockContent.substr(offset, len);
	}
	return true;
}

bool DataNode::WriteBlock(blockid_t bid, uint64_t offset, uint64_t len, const string &buf) 
{
	string writeContent;
	if(ec->read_block(bid, writeContent) != extent_protocol::OK)
	{
		return false;
	}
	writeContent = writeContent.substr(0, offset) + buf + writeContent.substr(offset + len);
	if(ec->write_block(bid, writeContent) != extent_protocol::OK)
	{
		return false;
	}
	return true;
}

