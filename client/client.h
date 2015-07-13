#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <iostream>
#include <vector>

#include "log.h"

#define MSGLEN 100


static int open_clientfd(char*ip,int port);

class Client
{
public:
	explicit Client(char*ip,int port);
	bool is_login();

	int do_login();
	int do_command(std::string msg);
	int do_cwd(std::string msg);
	int do_mkdir(std::string msg);

	~Client();
private:
	Client();
	int serverfd;
	bool login_flag;
	int recvMsg(std::string& msg);
	int sendMsg(std::string msg);
	int getCode(std::string msg);
};

#endif
