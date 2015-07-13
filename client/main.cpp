#include <iostream>
#include <stdlib.h>
#include <algorithm>

#include "client.h"

static const std::vector<std::string> command_vec
{
	"quit",
	"user",
	"pass",
	"size",
	"cd",
	"cdup",
	"ls",
	"get",
	"mget",
	"put",
	"mput",
	"mkdir",
	"pwd",
	"rm",
	"rmdir",
};

enum COMMAND
{
	QUIT,
	USER,
	PASS,
	SIZE,
	CD,
	CDUP,
	LS,
	GET,
	MGET,
	PUT,
	MPUT,
	MKDIR,
	PWD,
	RM,
	RMDIR,
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

static std::string toUpper(std::string input)
{
	std::string str = "";
	for(int i=0;i<(int)input.size();i++)
	{
		str += toupper(input[i]);	
	}
	return str;
}

static std::string toLower(std::string input)
{
	std::string str = "";
	for(int i=0;i<(int)input.size();i++)
	{
		str += tolower(input[i]);
	}
	return str;
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
		command = toLower(command);
		//test
		std::cout<<"command:"<<command<<std::endl;
		int com_num = findCommand(command);
		if(com_num == -1)
		{
			std::cerr<<"client:?Invalid Command."<<std::endl;	
			continue;
		}
		switch(com_num)
		{
			case QUIT:
				client.do_command(msg);
				return 0;
				break;
			case SIZE:
				client.do_command(msg);
				break;
			case CD:
				client.do_cwd(msg);
				break;
			case MKDIR:
				client.do_mkdir(msg);
				break;
			case PWD:
				client.do_command(msg);
				break;
			case CDUP:
				client.do_command(msg);
				break;
			case RM:
				client.do_rm(msg);
			default:
				break;
		}
	}
}
