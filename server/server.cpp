#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <ctype.h>

#include "server.h"
#include "md5.h"

static int open_listenfd(int port)
{
	int listenfd,optval = 1;
	sockaddr_in serveraddr;
	if((listenfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
	{
		Log("open_listenfd:get socket error.");
		return -1;
	}
	if(setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,
				static_cast<const void*>(&optval),sizeof(int)) < 0)
	{
		return -1;
	}
	memset(&serveraddr,0,sizeof(sockaddr_in));	

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(port);
	if(bind(listenfd,(sockaddr*)&serveraddr,sizeof(sockaddr_in)) < 0)
	{
		Log("open_listenfd:bind serveraddr to listen error.");
		return -1;
	}
	if(listen(listenfd,SOMAXCONN) < 0)
	{
		Log("open_listenfd:listen error.");
		return -1;
	}
	return listenfd;
}

static std::string hash(std::string input)
{
	MD5 md5;
	return md5.digestString((char*)input.c_str());	
}

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

ServerBox::ServerBox(int port,std::string root_dir_input)
{
	if((listenfd = open_listenfd(port)) < 0)
	{
		Log("ServerBox open_listenfd error.");	
		exit(-1);
	}	

	user_pass.clear();	
	std::fstream fs;
	fs.open(USER_DB_PATH,std::ios::in);
	if(fs.is_open() == false)
	{
		Log("ServerBox open user_db error.");
		exit(-1);
	}
	std::string username; std::string password;	
	while(fs>>username)
	{
		fs>>password;
		user_pass[username] = password;
	}
	fs.close();
	root_dir = root_dir_input;
}

ServerBox::~ServerBox(){}


int ServerBox::startServe()
{
	sockaddr_in clientaddr;
	socklen_t addr_size = sizeof(sockaddr_in);
	int clientfd;
	while(true)
	{
		if((clientfd = accept(listenfd,(sockaddr*)&clientaddr,&addr_size)) == -1)
		{
			continue;
		}
		pid_t pid;
		if((pid = fork()) < 0)
		{
			Log("ServeBox fork error.");
		}
		else if(pid == 0)
		{
			Server server(clientfd,inet_ntoa(clientaddr.sin_addr));	
			server.startServe();
		}
	}
}

Server::Server(int clientfd_input,std::string clientip_input):current_dir(root_dir),
								clientfd(clientfd_input),clientip(clientip_input),is_login(false)
{
}

Server::~Server(){};

int Server::sendMsg(std::string msg)
{
	char buf[MSGLEN+1];
	strcpy(buf,msg.c_str());
	char tail[] = "\r\n";
	strcat(buf,tail);
	int ret;
	if((ret = send(clientfd,buf,strlen(buf),0)) <= 0)
	{
		return -1;
	}
	/*
	int left = strlen(buf);
	while(left > 0)//make sure to send all the message.
	{
		int ret;
		if((ret = send(clientfd,buf,left,0)) == -1)
		{
			return -1;
		}	
		left = left - ret;
	}
	*/
	return 0;
}

int Server::recvMsg(std::string& command,std::string& arg)
{
	command = "";
	arg = "";
	std::string temp;
	char ch;
	while(arg.size() < MSGLEN)
	{
		int ret;
		if((ret = recv(clientfd,&ch,sizeof(char),0)) <= 0)
		{
			return -1;
		}	
		temp = temp + ch;	
		if(*(temp.end()-1) == '\n' && 
				(*(temp.end()-2)) == '\r')
		{
			break;
		}
	}

	size_t pos;	
	if((pos = temp.find("\r\n")) == std::string::npos)
	{
		return 0;
	}
	temp = temp.substr(0,pos);//get the message.
	if((pos = temp.find(" ")) == std::string::npos)
	{
		command = temp;
		return 0;
	}
	command = temp.substr(0,pos);
	arg = temp.substr(pos+1,temp.size());
	return 0;
}

int Server::startServe()
{
	if(sendMsg("220 (TimoFTP)") == -1)
	{
		return -1;
	}
	
	std::string command,arg;
	while(true)
	{
		if(recvMsg(command,arg) == -1)
		{
			return -1;
		}	
		command = toLower(command);	
		int com_num = findCommand(command);	
		if(com_num == -1)
		{
			sendMsg("?Invalid command.");
			continue;
		}
		if(com_num > 1 || com_num == 0)
		{
			if(is_login  == false)	
			{
				sendMsg("530 Please login with USER and PASS.");	
				continue;
			}
		}
		switch(com_num)
		{
			case QUIT:
				do_quit();
				break;
			case USER:
				do_user(arg);
				break;
			case SIZE:
				do_size(arg);
				break;
			case CWD:
				do_cwd(arg);
				break;
			case CDUP:
				do_cdup();
				break;
			case MKD:
				do_mkd(arg);
				break;
			case PWD:
				do_pwd();
				break;
			case DELE:
				do_dele(arg);
			default:
				break;
		}
	}
}

int Server::do_quit()
{
	sendMsg("221 Goodbye.");	
	exit(0);
}

int Server::do_user(std::string arg)
{
	bool login_flag = true;
	std::string hash_username = hash(arg);
	auto iter = user_pass.find(hash_username);
	if(iter == user_pass.end())
	{
		login_flag = false;
	}
	sendMsg("331 Please specify the password.");
	std::string command,password;
	recvMsg(command,password);
	int com_num = findCommand(command);
	while(com_num != 2)
	{
		sendMsg("331 Need password to login in.");
		recvMsg(command,password);
		com_num = findCommand(command);
	}
	std::string hash_password = hash(password);	
	if(login_flag == false)
	{
		sendMsg("530 Login incorrect.");
		return 0;
	}
	if((*iter).second == hash_password)
	{
		sendMsg("230 Login successfully.");
		is_login = true;
		return 0;
	}
	else
	{
		is_login = false;
		sendMsg("530 Login incorrect.");
		return 0;
	}
}

int Server::do_size(std::string arg)
{
	std::string file_path = current_dir + arg;	
	if(check_filename(file_path) != 1)
	{
		sendMsg("550 Could not get file size.");
		return -1;
	}
	struct stat st;
	if(lstat(file_path.c_str(),&st) == -1)
	{
		sendMsg("550 Could not get file size.");
		return -1;
	}
	int size = st.st_size;
	std::string msg = "213 ";
	std::stringstream ss;
	std::string size_str;
	ss<<size;
	ss>>size_str;
	ss.clear();

	msg = msg + size_str;
	if(sendMsg(msg) == -1)
	{
		return -1;
	}
	return 0;
}

int Server::do_cwd(std::string arg)
{
	std::string file_path = current_dir + arg;
	int file_type = check_filename(file_path);	
	if(file_type == 0 || file_type == 1)
	{
		sendMsg("550 failed to change directory.");
		return 0;
	}
	if(arg == "..")
	{
		if(current_dir == root_dir)
		{
			sendMsg("550 already at the root directory.");
			return 0;
		}
		//remove the last '/' in the current_dir string.
		current_dir = current_dir.substr(0,current_dir.size()-1);
		//remove chars after the last '/'.
		current_dir = current_dir.substr(0,
						current_dir.find_last_of("/")+1);
	}
	else if(arg == ".")
	{
		current_dir = current_dir;
	}
	else
	{
		current_dir = current_dir + arg + '/';
	}
	sendMsg("250 Directory successfully changed.");
	return 0;
}

int Server::mk_dir(std::string dir_name)
{
	std::string dir_path = current_dir + dir_name;
	if(check_filename_out_of_bound(dir_path) == 1)
	{
		return -1;
	}	
	return mkdir(dir_path.c_str(),S_IRWXU|S_IRWXG);
}

int Server::do_mkd(std::string arg)
{
	std::string dir_name;
	std::stringstream ss;	
	ss<<arg;
	ss>>dir_name;
	ss.clear();
	if(mk_dir(dir_name) == -1)
	{
		sendMsg("550 Create directory operation failed.");
		return -1;
	}
	sendMsg("257 Create directory successfully.");
	return 0;
}

int Server::do_pwd()
{
	std::string msg = "257 ";
	std::string dir = current_dir;
	dir = dir.substr(root_dir.size()-1,dir.size() - (root_dir.size()-1));
	msg = msg + "\"" + dir + "\"";
	if(sendMsg(msg) == -1)
	{
		return -1;
	}
	return 0;
}

int Server::do_cdup()
{
	return do_cwd("..");
}

int Server::do_dele(std::string arg)
{
	std::string file_path = current_dir + arg;		
	struct stat st;	
	if(lstat(file_path.c_str(),&st) == -1)
	{
		sendMsg("550 Delete operation failed.");
		return -1;
	}
	if(S_ISREG(st.st_mode))
	{
		if(unlink(file_path.c_str()) == -1)
		{
			sendMsg("550 Delete operation failed.");
			return -1;
		}	
		sendMsg("250 Delete operation successful.");	
		return 0;
	}
	else if(S_ISDIR(st.st_mode))
	{
		sendMsg("550 File is a directory,use RMDIR to delete.");
		return -1;
	}
	return -1;
}


//Check wether the file exist and the type of the file.
//0 for not exist or out of bound,1 for regular file,2 for directory.
int Server::check_filename(std::string file_path)
{
	if(check_filename_out_of_bound(file_path) == 1) //check whether the filename_path is out of the root_dir.
	{
		return 0;
	}
	struct stat st;
	if(lstat(file_path.c_str(),&st) == -1)
	{
		return 0;
	}
	else
	{
		if(S_ISREG(st.st_mode))
		{
			return 1;
		}
		if(S_ISDIR(st.st_mode))
		{
			return 2;
		}
	}
	return 0;
}

//this function is used to check whether the filepath is within the root_dir(out of bound).
//1 for out of bound.
int Server::check_filename_out_of_bound(std::string file_path)
{
	//get the full path without the root_dir.
	std::string full_path = file_path;	
	full_path = full_path.substr(std::string(root_dir).size()-1,
									full_path.size());
	int count = 0;
	size_t pos = -1;
	while((pos = full_path.find('/',pos+1)) != std::string::npos)
	{
		if(full_path[pos+1] != '.')
		{
			count++;
		}
		// if "/.." or "/../"
		else if(full_path[pos+2] == '.' && 
					(full_path[pos+3] == '/' || full_path[pos+3] == '\0'))
		{
			count--;
		}
		if(count < 0)
		{
			return 1;
		}
	}
	return 0;
}
