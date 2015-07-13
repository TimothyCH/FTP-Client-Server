#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <sstream> // for do_login()
#include <cstring>

#include "client.h"


static int open_clientfd(char *ip,int port)
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



Client::Client(char* ip,int port):login_flag(false)
{
	if((serverfd = open_clientfd(ip,port)) == -1)
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
	std::string buf = "";
	while(buf.size() < MSGLEN)
	{
		int ret;
		if((ret = recv(serverfd,&ch,sizeof(char),0)) < 0)
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

int Client::getCode(std::string msg)
{
	int code;
	std::stringstream ss;
	ss<<msg;
	ss>>code;
	ss.clear();
	return code;
}

int Client::do_login()
{
	std::cout<<"enter username:";
	std::string username;
	std::cin>>username;
	std::string msg = "USER ";
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
	msg = "PASS "; 
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

bool Client::is_login()
{
	return login_flag;
}

int Client::do_command(std::string msg)
{
	//test
	std::cout<<"msg:"<<msg<<std::endl;
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

