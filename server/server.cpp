#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <grp.h>
#include <pwd.h>

#include <cstring>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <ctype.h>
#include <dirent.h>

#include "server.h"
#include "md5.h"

//get listen socket on port.
//return listen socket if success,-1 if error.
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

//in port mode,used to connect to client data socket.
static int open_clientfd(char*ip,int port)
{
	int serverfd;
	sockaddr_in serveraddr;	
	if((serverfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
	{
		Log("open_clientfd:get socket serverfd error.");
		return -1;
	}

	memset(&serveraddr,0,sizeof(sockaddr_in));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
	in_addr sin;
	int temp;
	if((temp = inet_pton(AF_INET,ip,&sin)) == 0)
	{
		Log("open_clientfd:invalid ip address.");
		return -1;
	}
	else if(temp == -1)
	{
		Log("open_clientfd:inet_pton error.");
		return -1;
	}
	serveraddr.sin_addr = sin;

	if(connect(serverfd,(sockaddr*)&serveraddr,sizeof(sockaddr)) < 0)
	{
		Log("open_clientfd:connect to serveraddr error.");
		return -1;
	}
	return serverfd;
}

//hash username and password.
static std::string hash(std::string input)
{
	MD5 md5;
	return md5.digestString((char*)input.c_str());	
}

//find number of command.
static int findCommand(std::string command)
{
	command = toLower(command);
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

//get the first ip address that is not 127.0.0.1,
//if not connected to internet,ip address is 127.0.0.1.
static int getIP(std::string& ip)
{
	struct ifaddrs *ifAddrStruct = NULL;
	void *tmpAddPtr = NULL;
	if(getifaddrs(&ifAddrStruct) == -1)
	{
		return -1;
	}


	while(ifAddrStruct != NULL)
	{
		if(ifAddrStruct->ifa_addr->sa_family == AF_INET)
		{
			tmpAddPtr = &((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
			char addressBuffer[INET_ADDRSTRLEN];
			inet_ntop(AF_INET,tmpAddPtr,addressBuffer,INET_ADDRSTRLEN);
			//printf("%s IP Address %s\n",ifAddrStruct->ifa_name,addressBuffer);
			ip = addressBuffer;
			if(ip != "127.0.0.1")
			{
				return 0;
			}
		}
		ifAddrStruct = ifAddrStruct->ifa_next;

	}
	ip = "127.0.0.1";
	return 0;
}


ServerBox::ServerBox(int port,std::string root_dir_input)
{
	if((listenfd = open_listenfd(port)) < 0)
	{
		Log("ServerBox open_listenfd error.");	
		exit(-1);
	}	

	if(sqlite3_open(USER_DB_PATH,&sql) != SQLITE_OK)
	{
		Log("ServerBox sqlite database open error.");
		exit(-1);
	}
//changed to database.
/*
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
*/
	root_dir = root_dir_input;
}

ServerBox::~ServerBox()
{
	sqlite3_close(sql);//close sql.
}


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
			Server server(clientfd);	
			server.startServe();
		}
	}
}

Server::Server(int clientfd_input):current_dir(root_dir),clientfd(clientfd_input),
										datafd(-1),is_login(false)
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
	return 0;
}

//receive message,and separate the command and arg.
int Server::recvMsg(std::string& command,std::string& arg)
{
	command = "";
	arg = "";
	std::string temp;
	char ch;
	while(arg.size() < MSGLEN)//message length must be less than MSGLEN:100
	{
		int ret;
		if((ret = recv(clientfd,&ch,sizeof(char),0)) <= 0)//recv once a char,not very good.
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
	temp = temp.substr(0,pos);//retrive \r\n.
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
				break;
			case RMD:
				do_rmd(arg);
				break;
			case LIST:
				do_list();
				break;
			case PASV:
				do_pasv();
				break;
			case RETR:
				do_retr(arg);
				break;
			case STOR:
				do_stor(arg);
				break;
			case PORT:
				doPortRecv(arg);
				break;
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

//the callback function used in do_user() sql requery.
static int sqlCallBack(void* arg,int argc,char* argv[],char *azColName[])
{
	if(argv){};//unused.
	if(azColName){};//unused.
	bool *flag = (bool*)arg;//the callback arg.
	if(argc != 2)
	{
		*flag = false;
	}
	else
	{
		*flag = true;
	}
	return 0;
}


//receive usr and pass message,check for login.
int Server::do_user(std::string arg)
{
	std::string username = arg;
	sendMsg("331 Please specify the password.");
	std::string command,password;
	recvMsg(command,password);
	int com_num = findCommand(command);
	while(com_num != PASS)
	{
		sendMsg("331 Need password to login in.");
		recvMsg(command,password);
		com_num = findCommand(command);
	}
	if(username.size() == 0 || password.size() == 0)
	{
		return 0;
	}
	std::string hash_password = hash(password);//get the hashed password.
	
	std::string statement = "select * from ";
	statement = statement + DB_TABLE_NAME + " where username = "
					+ "\"" + username + "\" and password = "
				   + "\"" + hash_password + "\";";	
	char *db_err;
	bool temp_flag = false;//arg into the callback function.
	if(sqlite3_exec(sql,statement.c_str(),sqlCallBack,
			&temp_flag,&db_err) != SQLITE_OK)
	{
		sendMsg("530 server database error.");
		return -1;
	}

	if(temp_flag == true)
	{
		sendMsg("230 Login successful.");
		is_login = true;
		return 0;
	}
	else
	{
		is_login = false;
		sendMsg("530 Login incorrect.");
		return 0;
	}

	return 0;
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

int Server::do_rmd(std::string arg)
{
	std::string file_path = current_dir + arg;	
	struct stat st;
	if(lstat(file_path.c_str(),&st) == -1)
	{
		sendMsg("550 Remove directory operation failed.");
		return -1;
	}
	if(S_ISREG(st.st_mode))
	{
		sendMsg("550 Not a directory name.");
		return -1;
	}
	else if(S_ISDIR(st.st_mode))
	{
		if(arg == "." || arg == "..")
		{
			sendMsg("550 Remove directory operation failed.");
			return -1;
		}	
		if(rmdir(file_path.c_str()) == -1)
		{
			sendMsg("550 Remove directory operation failed.");
			return -1;
		}
	}
	sendMsg("250 Remove directory operation successfully.");
	return 0;
}

//open port to listen and accept client connection.
int Server::do_pasv()
{
	int port = 2000;		
	int retfd = open_listenfd(port);
	while(retfd == -1)
	{
		port++;
		if(port == 2500)
		{
			sendMsg("425 Can't start pasv mode.");
			return -1;
		}
		retfd = open_listenfd(port);
	}
	
	std::string ip;
	if(getIP(ip) == -1)
	{
		sendMsg("425 Can't start pasv mode.");
		return -1;
	}	

	int d1,d2,d3,d4;
	sscanf(ip.c_str(),"%d.%d.%d.%d",&d1,&d2,&d3,&d4);
	int d5 = port/256;
	int d6 = port%256;
	std::string msg = "227 Entering Passive Mode (";
	std::stringstream ss;
	ss<<d1<<","<<d2<<","<<d3<<","<<d4<<","<<d5<<","<<d6<<").";
	std::string temp;
	ss>>temp;
	ss.clear();	
	msg = msg + temp;
	sendMsg(msg);
	
	sockaddr_in clientaddr;
	socklen_t addr_size = sizeof(sockaddr_in);
	if((datafd = accept(retfd,(sockaddr*)&clientaddr,&addr_size)) == -1)
	{
		sendMsg("425 PASV can't receive connection.");
		return -1;
	}

	shutdown(retfd,SHUT_RDWR);//shutdown the listenfd.
	close(retfd);
	return 0;
}

//first part of port mode,recv port mode message.
int Server::doPortRecv(std::string arg)
{
	std::string port_info = arg;
	if(getIPandPortFromPortInfo(port_info,port_ip,port_port) == -1)
	{
		sendMsg("500 Illegal port command.");
		return -1;
	}	
	sendMsg("200 port command successful.");	
	datafd = -2;
	return 0;
}

//second part of port mode,connect to client.
int Server::doPortConnect()
{
	datafd = open_clientfd((char*)port_ip.c_str(),port_port);
	if(datafd == -1)
	{
		sendMsg("425 failed to establish connection.");
		return -1;
	}
	return 0;
}


int Server::getIPandPortFromPortInfo(std::string port_info,std::string& ip,int& port)
{
	ip = "";
	port = -1;
	int d1,d2,d3,d4,d5,d6;
	if(sscanf(port_info.c_str(),"%d,%d,%d,%d,%d,%d",
				&d1,&d2,&d3,&d4,&d5,&d6) == -1)
	{
		return -1;
	}
	char buf[100];
	sprintf(buf,"%d.%d.%d.%d",d1,d2,d3,d4);
	ip = buf;
	port = d5*256 + d6;
	return 0;
}

int Server::getFileInfo(std::string file_path,std::string file_name,std::string& ret)
{
	
	ret = "";
	struct stat st;	
	struct passwd *pw;
	struct group *gr;
	struct tm *tm;

	if(stat(file_path.c_str(),&st) == -1)
	{
		return -1;
	}
	
	std::string mode = "";
	switch(st.st_mode & S_IFMT)
	{
		case S_IFREG:
			mode = "-";
			break;
		case S_IFDIR:
			mode = "d";
			break;
		case S_IFLNK:
			mode = "l";
			break;
		case S_IFBLK:
			mode = "b";
			break;
		case S_IFCHR:
			mode = "c";
			break;
		case S_IFIFO:
			mode = "p";
			break;
		case S_IFSOCK:
			mode = "s";
			break;
	}

	ret +=mode;


	for(int i=8;i >= 0;i--)
	{
		if(st.st_mode & (1<<i))
		{
			switch(i%3)
			{
				case 2:
					ret += "r";
					break;				
				case 1:
					ret += "w";
					break;
				case 0:
					ret += "x";
					break;
			}
		}	
		else
		{
			ret += "-";
		}
	}

	pw = getpwuid(st.st_uid);
	gr = getgrgid(st.st_gid);

	int pw_num,gr_num;
	char root_buf[] = "root";
	if(strcpy(pw->pw_name,root_buf) == 0)
	{
		pw_num = 0;
	}
	else
	{
		pw_num = 1000;
	}
	if(strcpy(gr->gr_name,root_buf) == 0)
	{
		gr_num = 0;
	}
	else
	{
		gr_num = 1000;
	}

	char temp1[100];
	sprintf(temp1,"%2d %d %d %4ld",(int)st.st_nlink,pw_num,gr_num,st.st_size);

	ret += temp1;

	tm = localtime(&st.st_ctime);
	char temp2[100];
	sprintf(temp2," %04d-%02d-%02d %02d:%02d",tm->tm_year + 1900,tm->tm_mon + 1,tm->tm_mday,tm->tm_hour,tm->tm_min);

	ret += temp2;
	ret = ret + " " + file_name;

	return 0;
}

//list the information of all the files in @dir_path.
int Server::ls(std::string dir_path,std::string& ret)
{
	ret = "";	
	DIR* dirp = opendir(dir_path.c_str());
	if(!dirp)
	{
		return -1;
	}
	struct dirent *dir;
	while((dir = readdir(dirp)) != NULL)
	{
		if(strcmp(dir->d_name,".") == 0 ||
				strcmp(dir->d_name,"..") == 0)
		{
			continue;
		}
		std::string full_path = dir_path + dir->d_name;
		std::string file_info;

		if(getFileInfo(full_path,dir->d_name,file_info) == -1)
		{
			continue;
		}
		ret += file_info + "\r\n";//each information end with \r\n
	}
	closedir(dirp);
	return 0;
}

int Server::do_list()
{
	if(datafd == -1)
	{
		sendMsg("425 Use PORT or PASV first.");	
		return -1;
	}
	if(datafd == -2)
	{
		doPortConnect();	
	}

	sendMsg("150 Here comes the directory list.");

	std::string ret;		
	if(ls(current_dir,ret) == -1)
	{
		close(datafd);
		datafd = -1;
		sendMsg("226 Directory Send Ok.");
		return -1;
	}

	//can't do with send all data once,may cause unpredicted result.
	/*
	ret = ret + '\n';
	char buf[] = "";
	strcat(buf,ret.c_str());
	if(send(datafd,buf,strlen(buf),0) <= 0)
	{
		sendMsg("425 data connection failed.");
		return -1;
	}
	*/

	//send each piece at a time.
	if(ret.size() == 0)//incase ret is empty and nothing send.
	{
		ret += "\r\n";
	}
	std::vector<std::string> str_vec;
	while(ret.size() > MSGLEN)
	{
		str_vec.push_back(ret.substr(0,MSGLEN));	
		ret = ret.substr(MSGLEN,ret.size() - MSGLEN);
	}
	str_vec.push_back(ret);
	for(auto i = str_vec.begin();i != str_vec.end();i++)
	{
		char buf[MSGLEN+1];
		strcpy(buf,(*i).c_str());
		if(send(datafd,buf,strlen(buf)*sizeof(char),0) <= 0)
		{
			sendMsg("425 data connection failed.");
			return -1;
		}		
	}

	shutdown(datafd,SHUT_RDWR);
	close(datafd);
	datafd = -1;

	sendMsg("226 Directory Send OK.");

	return 0;
}

int Server::do_retr(std::string arg)
{
	if(datafd == -1)//datafd = -1 means no connection,datafd > 0 means pasv mode.
	{
		sendMsg("425 Use PORT or PASV first.");
		return -1;
	}	
	if(datafd == -2)//datafd = -2 means port mode.
	{
		doPortConnect();
	}
	if(check_filename(current_dir + arg) != 1)
	{
		sendMsg("550 Failed to open file.");
		return -1;
	}
	
	std::string file_path = current_dir + arg;
	std::fstream fs;
	fs.open(file_path,std::ios::in);
	if(!fs.is_open())
	{
		sendMsg("550 Failed to open file.");
		close(datafd);
		datafd = -1;
		return -1;
	}	
	sendMsg("150 Opening data connection for file.");	
	
	char tempc;
	while(true)
	{
		if(!fs)
		{
			break;
		}
		char buf[MSGLEN];
		int count = 0;
		while(fs.get(tempc))
		{
			buf[count++] = tempc;
			if(count == MSGLEN)
			{
				break;
			}
		}

		if(send(datafd,buf,count,0) == -1)
		{
			sendMsg("425 data connection failed.");	
			close(datafd);
			datafd = -1;
			return -1;
		}
	}
	fs.close();
	shutdown(datafd,SHUT_RDWR);
	close(datafd);
	datafd = -1;
	sendMsg("226 Transfer complete.");

	return 0;

}


int Server::do_stor(std::string arg)
{
	if(datafd == -1)//datafd = -1 means no connection,datafd > 0 means pasv mode.
	{
		sendMsg("425 Use PORT or PASV first.");
		return -1;
	}	
	if(datafd == -2)//datafd = -2 means port mode.
	{
		doPortConnect();
	}
	
	std::string file_path = current_dir + arg;
	std::fstream fs;
	fs.open(file_path,std::ios::out);
	if(fs.is_open() == false)
	{
		sendMsg("553 Could not create file.");	
		close(datafd);
		datafd = -1;
		return -1;
	}

	sendMsg("150 ok to send data.");

	while(true)
	{
		char buf[MSGLEN];
		int num;
		if((num = recv(datafd,buf,sizeof(buf),0)) <= 0)
		{
			break;
		}
		for(int i=0;i<num;i++)
		{
			fs<<buf[i];
		}
	}
	fs.close();
	shutdown(datafd,SHUT_RDWR);
	close(datafd);
	datafd = -1;
	sendMsg("226 Transfer complete.");
	return 0;	
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
