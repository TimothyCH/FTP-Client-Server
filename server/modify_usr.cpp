#include <iostream>
#include <string>
#include <sstream>
#include "md5.h"

#include <sqlite3.h>
#define USR_DB_PATH "usr.db"
#define TABLE_NAME "usr_info"

static int callback(void* not_used,int argc,char* argv[],char *azColName[])
{
	if(not_used){};
	for(int i = 0;i < argc;i++)
	{
		std::cout<<azColName[i]<<" = "<<(argv[i]? argv[i]:"NULL")<<std::endl;
	}
	std::cout<<std::endl;
	return 0;
}

int list(sqlite3* sql)
{
	std::string statement = "select * from ";
	statement =statement + TABLE_NAME + ";";
	char *err = 0;
	if(sqlite3_exec(sql,statement.c_str(),callback,0,&err) != SQLITE_OK)
	{
		std::cout<<"sql err:"<<err<<std::endl;	
		return -1;
	}	
	return 0;
}

int add_usr(sqlite3* sql,std::string usrname,std::string password_hash)
{
	std::string statement = "insert into ";
	statement = statement + TABLE_NAME + " values (\"" + usrname
										+ "\",\"" + password_hash + "\");";
	char *err;
	if(sqlite3_exec(sql,statement.c_str(),callback,0,&err) != SQLITE_OK)
	{
		std::cout<<"sql err:"<<err<<std::endl;
		return -1;
	}
	return 0;
}

int delete_usr(sqlite3* sql,std::string usrname)
{
	std::string statement = "delete from ";
	statement = statement + TABLE_NAME + " where username = \"" + usrname + "\";";
	char *err;
	if(sqlite3_exec(sql,statement.c_str(),callback,0,&err) != SQLITE_OK)
	{
		std::cout<<"sql err:"<<err<<std::endl;
		return -1;
	}
	return 0;
}


int main()
{
	sqlite3* sql;	
	if(sqlite3_open(USR_DB_PATH,&sql) != SQLITE_OK)
	{
		std::cerr<<"open usr.db error."<<std::endl;	
		return -1;
	}
	while(std::cout<<">")
	{
		std::string msg;	
		getline(std::cin,msg);
		std::stringstream ss;
		ss<<msg;
		std::string command;
		ss>>command;	
		if(command == "list")
		{
			list(sql);
		}
		else if(command == "delete")
		{
			std::string usrname;
			while(ss>>usrname)
			{
				delete_usr(sql,usrname);
			}
		}
		else if(command == "add")
		{
			std::string usrname;
			std::string password;
			ss>>usrname;
			if(!(ss>>password))
			{
				std::cout<<"password should not be empty."<<std::endl;
				continue;
			}
			if(usrname.size() == 0)
			{
				std::cout<<"username should not be empty."<<std::endl;
				continue;
			}
			MD5 md;
			std::string hash_password = md.digestString((char*)password.c_str());
			add_usr(sql,usrname,hash_password);
		}
		else if(command == "quit")
		{
			return 0;
		}
		else
		{
			std::cout<<"command:quit,list,add <username> <password>,delete <username>."<<std::endl;
		}
		ss.clear();
	}
	return 0;
}
