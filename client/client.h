#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <iostream>
#include <vector>

#include "log.h"

#define MSGLEN 100

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

static int openClientfd(char*ip,int port);
static int openListenfd(int port);
static int getIP(std::string& ip);

class Client
{
public:
	explicit Client(char*ip,int port);
	bool isLogin();

	int doLogin();
	int doPasv();
	int doLs();
	int doGet(std::string msg);
	int doPut(std::string msg);
	int doCommand(std::string msg);
	int doCommand(std::string msg,std::string new_com);

	~Client();
private:
	Client();
	int serverfd;
	bool login_flag;
	bool passive_mode;//true for passive mode,false for port mode.
	int recvMsg(std::string& msg);
	int sendMsg(std::string msg);
	int getCode(std::string msg);

	int pasvMode();
	int portModeSend();//send port and return socket.
	int portModeConnect(int retfd);//listen on retfd and return datafd.
	int pasvGetIPandPort(std::string msg,std::string& ip,int&port);
};

#endif
