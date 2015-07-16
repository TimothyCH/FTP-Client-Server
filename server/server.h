#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <cstring>
#include <map>
#include <vector>
#include <iostream>

#include "log.h"

#define USER_DB_PATH "usr.data"
#define MSGLEN 100

static std::map<std::string,std::string> user_pass;//store the map of username and password.
static std::string root_dir;//store the root_dir.

static int openListenfd(int port);
static std::string hash(std::string input);//used to hash username and password.
static int findCommand(std::string command);//find the number of command.
static std::string toUpper(std::string str);
static std::string toLower(std::string str);
static std::string getIP();//get local ip address.

const std::vector<std::string> command_vec
{
	"quit",
	"user",
	"pass",
	"size",
	"cwd",
	"cdup",
	"list",
	"retr",
	"stor",
	"mkd",
	"pwd",
	"dele",
	"rmd",
	"mv",
	"pasv",
	"port",
};

enum COMMAND
{
	QUIT,
	USER,
	PASS,
	SIZE,
	CWD,
	CDUP,
	LIST,
	RETR,
	STOR,
	MKD,
	PWD,
	DELE,
	RMD,
	MV,
	PASV,
	PORT,
};

class ServerBox
{
public:
	explicit ServerBox(int port,std::string root_dir_input);
	~ServerBox();
	int startServe();
private:
	ServerBox();
	int listenfd;
};

class Server
{
public:
	explicit Server(int clientfd_input);
	~Server();
	int startServe();
private:
	std::string current_dir;
	int clientfd;
	int datafd;//datafd = -1 means no connection,datafd = -2 mean port mode.
	std::string port_ip;//record ip in portmode.
	int port_port;//record port in portmode.

	bool is_login;

	Server();
	int sendMsg(std::string msg);
	int recvMsg(std::string& command,std::string& arg);

	int do_quit();
	int do_cdup();
	int do_pwd();
	int do_user(std::string arg);
	int do_size(std::string arg);
	int do_cwd(std::string arg);

	int mk_dir(std::string dir_name);
	int do_mkd(std::string arg);

	int do_dele(std::string arg);
	int rm_dir(std::string dir_full_path);
	int do_rmd(std::string arg);

	int do_mv(std::string arg);

	int getFileInfo(std::string file_path,std::string file_name,std::string& ret);
	int ls(std::string path,std::string& ret);
	int do_list();

	int do_retr(std::string arg);
	int do_stor(std::string arg);

	int do_pasv();
	int doPortRecv(std::string arg);//first part of port mode,recv client data ip port info.
	int doPortConnect();//second part of port,make connection.
	int getIPandPortFromPortInfo(std::string port_info,std::string& ip,int& port);

	int check_filename(std::string filename);
	int check_filename_out_of_bound(std::string filename);
};

#endif
