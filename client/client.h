#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <iostream>
#include <vector>

#include "log.h"

#define MSGLEN 100


static int openClientfd(char*ip,int port);

class Client
{
public:
	explicit Client(char*ip,int port);
	bool isLogin();

	int doLogin();
	int doCommand(std::string msg);
	int doChangeCommand(std::string msg,std::string new_com);

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
