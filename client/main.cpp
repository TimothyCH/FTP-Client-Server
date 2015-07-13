#include <iostream>
#include <stdlib.h>
#include <algorithm>

#include "client.h"

static const std::vector<std::string> command_vec
{
	"QUIT",
	"USER",
	"PASS",
	"SIZE",
	"CD",
	"LS",
	"GET",
	"MGET",
	"PUT",
	"MPUT",
	"MKDIR",
	"PWD",
	"RM",
	"RMDIR",
};

static int findCommand(std::string command)
{
	auto iter = find(command_vec.begin(),command_vec.end(),command);	
	if(iter == command_vec.end())
	{
		return -1;
	}
	return iter - command_vec.begin();
}

int main(int argc,char* argv[])
{
	if(argc != 3)
	{
		std::cout<<"usage client <ip> <port>"<<std::endl;
		return 0;
	}	
	char*ip = argv[1];
	int port = atoi(argv[2]);
	Client client(ip,port);
	client.do_login();
	std::cin.ignore(1,'\n');//remove the last '\n' in the input stream.

	if(client.is_login() == false)
	{
		return 0;
	}
	std::string msg;
	while(true)
	{
		std::cout<<"TimoFTP>";

		getline(std::cin,msg);

		std::string command;
		std::stringstream ss;
		ss<<msg;
		ss>>command;
		ss.clear();
		int com_num = findCommand(command);
		if(com_num == -1)
		{
			std::cerr<<"client:?Invalid Command."<<std::endl;	
			continue;
		}
		switch(com_num)
		{
			case 3:
				client.do_command(msg);
				break;
			case 4:
				client.do_command(msg);
				break;
			default:
				break;
		}
	}
}
