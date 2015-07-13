#include <iostream>

#include "server.h"

int main()
{
	ServerBox serverbox(12345,"file/");	
	serverbox.startServe();
}
