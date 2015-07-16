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
	"delete",
	"rmdir",
	"passive",
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
	DELETE,
	RMDIR,
	PASV,
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
	client.doLogin();
	std::cin.ignore(1,'\n');//remove the last '\n' in the input stream.

	if(client.isLogin() == false)
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
				client.doCommand(msg);
				return 0;
				break;
			case SIZE:
				client.doCommand(msg);
				break;
			case CD:
				client.doCommand(msg,"cwd");
				break;
			case MKDIR:
				client.doCommand(msg,"mkd");
				break;
			case PWD:
				client.doCommand(msg);
				break;
			case CDUP:
				client.doCommand(msg);
				break;
			case RM:
				client.doCommand(msg,"dele");
				break;
			case DELETE:
				client.doCommand(msg,"dele");
				break;
			case RMDIR:
				client.doCommand(msg,"rmd");
				break;
			case PASV:
				client.doPasv();
				break;
			case LS:
				client.doLs();
				break;
			case GET:
				client.doGet(msg);
				break;
			case PUT:
				client.doPut(msg);
				break;
			default:
				break;
		}
	}
}
