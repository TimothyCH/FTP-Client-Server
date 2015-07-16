#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <ifaddrs.h> // for getIP()

#include <sstream> // for do_login()
#include <cstring>
#include <unistd.h>
#include <fstream>

#include <regex.h>

#include "client.h"

#define NAT//private address will not be accepted by server in port mode.

//connect to server.
//return connected socket if success,-1 if error.
static int openClientfd(char *ip,int port)
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

//open listen socket at @port.
//return listen socket if success,-1 if error.
static int openListenfd(int port)
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

//get local machine's ip address,
//ip = 127.0.0.1 if no internet connected.
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
			if(ip != "127.0.0.1")//return the first ip address that was not 127.0.0.1
			{
				return 0;
			}
		}
		ifAddrStruct = ifAddrStruct->ifa_next;
	}
	ip = "127.0.0.1";//if no internet connected,ip = 127.0.0.1
	return 0;
}


//conenct to server and recv the welcome message.
Client::Client(char* ip,int port):login_flag(false),passive_mode(true)
{
	if((serverfd = openClientfd(ip,port)) == -1)
	{
		std::cerr<<"client open_clientfd error."<<std::endl;	
		exit(-1);
	}	
	std::string msg;
	recvMsg(msg);
	std::cout<<msg<<std::endl;
	int code = getCode(msg);
	if(code != 220)
	{
		exit(-1);
	}
}

Client::~Client(){};

int Client::sendMsg(std::string msg)
{
	char buf[MSGLEN+1];
	strcpy(buf,msg.c_str());
	char tail[] = "\r\n";
	strcat(buf,tail);
	int ret = send(serverfd,buf,strlen(buf),0);
	if(ret <= 0)
	{
		std::cerr<<"send error."<<std::endl;
		return -1;
	}
	return 0;
}

int Client::recvMsg(std::string& msg)
{
	char ch;
	msg = "";
	std::string buf = "";
	while(buf.size() < MSGLEN)
	{
		int ret;
		if((ret = recv(serverfd,&ch,sizeof(char),0)) <= 0)
		{
			return -1;
		}	
		buf = buf + ch;
		if((*(buf.end()-1)) == '\n' && 
				(*(buf.end()-2)) == '\r')
		{
			break;
		}
	}
	msg = buf;
	size_t pos;
	if((pos = msg.find("\r\n")) == std::string::npos)
	{
		return 0;	
	}
	msg = msg.substr(0,pos);
	return 0;
}

//get the code number in the message.
int Client::getCode(std::string msg)
{
	int code;
	std::stringstream ss;
	ss<<msg;
	ss>>code;
	ss.clear();
	return code;
}


int Client::doLogin()
{
	std::cout<<"enter username:";
	std::string username;
	std::cin>>username;
	std::string msg = "user ";
	msg += username;
	sendMsg(msg);
	
	std::string recv_msg;
	recvMsg(recv_msg);
	std::cout<<recv_msg<<std::endl;
	if(getCode(recv_msg) != 331)
	{
		return -1;	
	}
	std::cout<<"enter the password:";
	std::string password;
	std::cin>>password;
	msg = "pass "; 
	msg += password;
	sendMsg(msg);
	
	recvMsg(recv_msg);
	std::cout<<recv_msg<<std::endl;
	int code = getCode(recv_msg);
	if(code == 230)
	{
		login_flag = true;	
		return 0;
	}
	else if(code == 530)
	{
		return -1;
	}
	return -1;
}

bool Client::isLogin()
{
	return login_flag;
}

//passive 
int Client::doPasv()
{
	if(passive_mode == false)
	{
		passive_mode = true;
		std::cout<<"passive mode on."<<std::endl;
	}
	else
	{
		passive_mode = false;
		std::cout<<"passive mode off."<<std::endl;
	}
	return 0;
}


//get the ip address and port in the received pasv message.
int Client::pasvGetIPandPort(std::string msg,std::string& ip,int& port)
{
	char match[100];
	regex_t reg;
	int nm = 10;
	regmatch_t pmatch[nm];	
	char pattern[] = "([0-9]+,[0-9]+,[0-9]+,[0-9]+,[0-9]+,[0-9]+)";
	
	if(regcomp(&reg,pattern,REG_EXTENDED) < 0)
	{
		std::cerr<<"compile error."<<std::endl;
	}
	int err = regexec(&reg,msg.c_str(),nm,pmatch,0);
	if(err == REG_NOMATCH)
	{
		std::cout<<"No ip and port match in pasv message."<<std::endl;
		return -1;
	}
	if(err)
	{
		std::cout<<"pasv message regex match error."<<std::endl;
		return -1;
	}
	for(int i=0;i<nm && pmatch[i].rm_so!=-1;i++)
	{
		int len = pmatch[i].rm_eo-pmatch[i].rm_so;
		if(len)
		{
			memset(match,'\0',sizeof(match));
			memcpy(match,msg.c_str()+pmatch[i].rm_so,len);
			break;
		}
	}

	int d1,d2,d3,d4,d5,d6;
	sscanf(match,"%d,%d,%d,%d,%d,%d",&d1,&d2,&d3,&d4,&d5,&d6);
	std::stringstream ss;
	ss<<d1<<"."<<d2<<"."<<d3<<"."<<d4;
	ss>>ip;
	ss.clear();
	port = d5*256 + d6;

	return 0;
}

//send pasv message to server.
int Client::pasvMode()
{
	sendMsg("pasv");
	std::string msg;
	if(recvMsg(msg) == -1)
	{
		return -1;
	}
	std::string ip;
	int port;
	int ret = pasvGetIPandPort(msg,ip,port);
	if(ret == -1)
	{
		Log("pasv can't get ip and port.");
		return -1;
	}
	int datafd = openClientfd((char*)ip.c_str(),port);
	if(datafd == -1)
	{
		Log("pasv can't connect to server.");
		return -1;
	}	
	return datafd;
}

//first part of port mode.
//send port message,get listen socket:retfd to wait for the connection of server.
//return retfd to accept conection,return -1 if error.
int Client::portModeSend()
{
	//choose first port in [2000,2500] that can be used.
	int port = 2000;		
	int retfd = openListenfd(port);
	while(retfd == -1)
	{
		port++;
		if(port == 2500)
		{
			std::cerr<<"port can't find a usable port in [2000,2500]."<<std::endl;
			return -1;
		}
		retfd = openListenfd(port);
	}

	std::string ip;
	if(getIP(ip) == -1)
	{
		std::cerr<<"port can't get local ip."<<std::endl;
		close(retfd);
		return -1;
	}
#ifdef NAT
	//active mode won't work if the ip is a private ip address.
	ip = "127.0.0.1";
#endif
	int d1,d2,d3,d4;
	sscanf(ip.c_str(),"%d.%d.%d.%d",&d1,&d2,&d3,&d4);
	int d5 = port/256;
	int d6 = port%256;
	std::string msg = "port ";
	std::stringstream ss;
	ss<<d1<<","<<d2<<","<<d3<<","<<d4<<","<<d5<<","<<d6;
	std::string temp;
	ss>>temp;
	ss.clear();
	msg += temp;
	sendMsg(msg);

	std::string recv_msg;
	recvMsg(recv_msg);
	std::cout<<recv_msg<<std::endl;
	if(getCode(recv_msg) != 200)
	{
		close(retfd);
		return -1;
	}
	return retfd;
}

//second part of port mode.
//wait for the server connection.
int Client::portModeConnect(int retfd)
{
	int datafd;
	sockaddr_in serveraddr;
	socklen_t addr_size = sizeof(sockaddr_in);
	if((datafd = accept(retfd,(sockaddr*)&serveraddr,&addr_size)) == -1)
	{
		std::cout<<"local:failed to get connection."<<std::endl;
		shutdown(retfd,SHUT_RDWR);
		close(retfd);
		return -1;
	}
	shutdown(retfd,SHUT_RDWR);
	close(retfd);
	return datafd;
}

int Client::doLs()
{
	int datafd;
	int retfd;//for port mode.
	if(passive_mode == true)//pasv mode.
	{
		datafd = pasvMode();		
		if(datafd == -1)
		{
			Log("pasv error.");
			close(datafd);
			datafd = -1;
			return -1;
		}
	}	
	else//port mode send connect.
	{
		retfd = portModeSend();
		if(retfd == -1)
		{
			return -1;
		}
	}

	sendMsg("list");

	if(passive_mode == false)//port mode.
	{
		datafd = portModeConnect(retfd);
		if(datafd == -1)
		{
			return -1;
		}
	}

	std::string msg;
	recvMsg(msg);
	std::cout<<msg<<std::endl;
	if(getCode(msg) != 150)//connection failed.
	{
		close(datafd);
		datafd = -1;
		close(retfd);
		return -1;
	}

	while(true)
	{
		char buf[MSGLEN];	
		int num;
		if((num = recv(datafd,buf,sizeof(buf),0)) <= 0)
		{
			break;
		}	
		std::string str = buf;
		str = str.substr(0,num);
		std::cout<<str;
	}
	close(datafd);
	recvMsg(msg);
	std::cout<<msg<<std::endl;
	return 0;				
}

int Client::doGet(std::string msg)
{
	int datafd;
	int retfd;//for port mode.
	if(passive_mode == true)
	{
		datafd = pasvMode();		
		if(datafd == -1)
		{
			Log("pasv error.");
			return -1;
		}
	}	
	else//port mode.
	{
		retfd = portModeSend();
		if(retfd == -1)
		{
			return -1;
		}

	}
	//get local_name and remote_name.
	std::stringstream ss;	
	ss<<msg;
	std::string com_temp;
	ss>>com_temp;
	std::string remote_name;
	ss>>remote_name;
	std::string local_name;
	if(!(ss>>local_name))
	{
		local_name = remote_name;
	}

	std::string command = "retr ";	
	command += remote_name;
	sendMsg(command);

	if(passive_mode == false)//port mode.
	{
		datafd = portModeConnect(retfd);
		if(datafd == -1)
		{
			return -1;
		}
	}

	std::string recv_msg;
	recvMsg(recv_msg);
	std::cout<<recv_msg<<std::endl;
	if(getCode(recv_msg) != 150)
	{
		close(datafd);	
		return 0;
	}	
		
	std::fstream fs;
	fs.open(local_name,std::ios::out | std::ios::trunc);	
	if(fs.is_open() == false)
	{
		std::cerr<<"open local file error."<<std::endl;
		close(datafd);
		return -1;
	}

	/*
	char ch;
	while(recv(datafd,&ch,1,0) > 0)
	{
		fs<<ch;	
	}
	fs.close();
	close(datafd);
	*/	
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
	close(datafd);
	recvMsg(recv_msg);
	std::cout<<recv_msg<<std::endl;
	return 0;
}


int Client::doPut(std::string msg)
{
	std::stringstream ss;
	ss<<msg;
	std::string temp_com;
	ss>>temp_com;
	std::string local_name;
	ss>>local_name;
	std::string remote_name;
	if(!(ss>>remote_name))
	{
		remote_name = local_name;
	}

	std::fstream fs;
	fs.open(local_name,std::ios::in);	
	if(fs.is_open() == false)
	{
		std::cerr<<"open local file error."<<std::endl;
		return -1;
	}


	int datafd;
	int retfd;//for port mode.
	if(passive_mode == true)
	{
		datafd = pasvMode();		
		if(datafd == -1)
		{
			Log("pasv error.");
			return -1;
		}
	}	
	else//port mode.
	{
		retfd = portModeSend();
		if(retfd == -1)
		{
			return -1;
		}
	}
		
	std::string command = "stor ";
	command += remote_name;
	sendMsg(command);

	if(passive_mode == false)//port mode.
	{
		datafd = portModeConnect(retfd);
		if(datafd == -1)
		{
			return -1;
		}
	}

	std::string recv_msg;
	recvMsg(recv_msg);
	std::cout<<recv_msg<<std::endl;
	if(getCode(recv_msg) != 150)
	{
		close(datafd);
		return -1;
	}

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
			return -1;
		}
	}

	fs.close();
	shutdown(datafd,SHUT_RDWR);
	close(datafd);

	recvMsg(recv_msg);
	std::cout<<recv_msg<<std::endl;

	return 0;
}

//for those command simply send message and receive a message.
int Client::doCommand(std::string msg)
{
	if(sendMsg(msg) == -1)
	{
		return -1;
	}
	std::string recv_msg;
	if(recvMsg(recv_msg) == -1)
	{
		return -1;
	}
	std::cout<<recv_msg<<std::endl;
	return 0;
}

//for command that is different with the command in message.
int Client::doCommand(std::string msg,std::string new_com)
{
	std::stringstream ss;
	ss<<msg;
	std::string temp_com;
	ss>>temp_com;
	std::string command = new_com + " ";
	std::string arg;
	ss>>arg;
	command = command + arg;
	ss.clear();
	if(sendMsg(command) == -1)
	{
		return -1;
	}

	std::string recv_msg;
	if(recvMsg(recv_msg) == -1)
	{
		return -1;
	}
	std::cout<<recv_msg<<std::endl;
	return 0;
}

