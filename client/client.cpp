#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <sstream> // for do_login()
#include <cstring>
#include <regex.h>
#include <unistd.h>
#include <fstream>

#include "client.h"


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
/*
	msg = "";
	while(true)
	{
		int num;
		char buf[MSGLEN];
		//test
		std::cout<<"getgetget"<<std::endl;
		if((num = recv(serverfd,buf,sizeof(buf),0)) <= 0)
		{
			break;	
		}	
		std::string temp = buf;	
		temp = temp.substr(0,num);
		msg = msg + temp;
	}
	size_t pos;
	if((pos = msg.find("\r\n")) == std::string::npos)
	{
		return 0;	
	}
	msg = msg.substr(0,pos);
	return 0;
*/
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

int Client::doPasv()
{
	passive_mode = true;
	std::cout<<"passive mode on."<<std::endl;
	return 0;
}


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

int Client::doLs()
{
	int datafd;
	if(passive_mode == true)
	{
		datafd = pasvMode();		
		if(datafd == -1)
		{
			Log("pasv error.");
			return -1;
		}
	}	
	else
	{
		datafd = -1;	
	}

	sendMsg("list");
	std::string msg;
	recvMsg(msg);
	std::cout<<msg<<std::endl;
	//too slow to recv one by one.
	/*
	char ch;
	while(recv(datafd,&ch,1,0) > 0)
	{
		std::cout<<ch;
	}
	*/
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
	if(passive_mode == true)
	{
		datafd = pasvMode();		
		if(datafd == -1)
		{
			Log("pasv error.");
			return -1;
		}
	}	
	else
	{
		datafd = -1;	
	}

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
	if(passive_mode == true)
	{
		datafd = pasvMode();		
		if(datafd == -1)
		{
			Log("pasv error.");
			return -1;
		}
	}	
	else
	{
		datafd = -1;	
	}
		
	std::string command = "stor ";
	command += remote_name;
	sendMsg(command);

	std::string recv_msg;
	recvMsg(recv_msg);
	if(getCode(recv_msg) != 150)
	{
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

