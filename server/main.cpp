#include <iostream>

#include "server.h"

int main(int argc,char*argv[])
{
	if(argc != 3)
	{
		std::cerr<<"usage: server port root_dir"<<std::endl;
	}
	int port = atoi(argv[1]);
	std::string root_dir = argv[2];
	if(*(root_dir.end()-1) != '/')//for example file/*
	{
		root_dir += '/';	
	}
	ServerBox serverbox(port,root_dir);	
	serverbox.startServe();
}
