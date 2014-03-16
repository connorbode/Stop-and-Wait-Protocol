#ifndef TRANSFER_H
#define TRANSFER_H
#pragma comment( linker, "/defaultlib:ws2_32.lib" )
#include <WinSock2.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

class Transfer {
public:

	// variables
	SOCKET s;
	int ibufferlen;
	char szbuffer[128];
	int ibytessent;
	int ibytesrecv;
	int PACKET_SIZE;
	int HEADER_LENGTH;
	struct sockaddr_in fromAddr;
	int fromAddrSize;
	struct timeval timeouts;	
	fd_set readfds;
	long numPackets;
	long lastPacketSize;
	int SR;
	int CR;

	// methods
	Transfer::Transfer();
	Transfer::Transfer(SOCKET);
	Transfer::~Transfer();
	bool sendMessage(char*);
	bool sendMessage2(char*);
	bool sendFile(FILE*, std::string, bool);
	std::string generatePutHeader(FILE*, std::string);
	void receiveFile(FILE*, int, int, bool);
	char* receiveMessage();
	char* receiveMessage2();
	void setCRSR(int, int);
	void log(std::string);
};

#endif