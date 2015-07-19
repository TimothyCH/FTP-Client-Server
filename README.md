FTP-Client-Server
An Implementation of FTP Client and Server in C++ with POSIX API.

Implemented commands:

client:

  quit,passive,user,pass,size,cd,cdup,ls,get,put,mkdir,pwd,rm,rmdir.

Server:

  pasv,port,user,pass,quit,cwd,list,retr,stor,mkd,pwd,dele,rmd.

start:

  client:    ./client ip port

  server:    ./server port root_dir_path
