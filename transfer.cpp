
#include "transfer.h"
#define UTIMER 300000
#define STIMER 0
using namespace std;

Transfer::Transfer() {
}

Transfer::Transfer(SOCKET s) {
	this->s = s;
	PACKET_SIZE = 128;
	HEADER_LENGTH = 43;
	timeouts.tv_sec = STIMER;
	timeouts.tv_usec = UTIMER;
}

bool Transfer::sendMessage(char* message) {
	memset(&szbuffer,0,sizeof(szbuffer));
	sprintf(szbuffer, message); 
	ibytessent=0;
	ibufferlen = strlen(message);
	ibytessent = sendto(s,szbuffer,ibufferlen,0, (struct sockaddr *) &fromAddr, sizeof(fromAddr));
	if (ibytessent == SOCKET_ERROR)
		throw "Send failed\n";  
	else {
		cout << "\n\r";
		cout << "SENDING MESSAGE: " << szbuffer;
		cout << "\n\r\n\r";
	}
}

char* Transfer::receiveMessage() {
	memset(&szbuffer,0,sizeof(szbuffer));
	//Fill in szbuffer from accepted request.
	/*if((ibytesrecv = recv(s,szbuffer,128,0)) == SOCKET_ERROR)
		throw "Receive error in server program\n";*/
	 //fd_set is a typeFD_ZERO(&readfds); //initialize FD_SET(s, &readfds); //put the socket in the set
	FD_ZERO(&readfds);
	FD_SET(s, &readfds);

	int outfds;
	if((outfds = select (1 , &readfds, NULL, NULL, &timeouts))==SOCKET_ERROR) {//timed out, 
		throw "timeout!!!";
	}
	fromAddrSize = sizeof(fromAddr);
	if (outfds > 0) //receive frame
		ibytesrecv = recvfrom(s, (char*)& szbuffer, sizeof(szbuffer),0, (struct sockaddr*)&fromAddr, &fromAddrSize);


	//Print reciept of successful message. 
	return szbuffer;
}

bool Transfer::sendFile(FILE *stream, string filename) {

	// Get the filesize
	long fileSize;
	fseek (stream, 0, SEEK_END);
    fileSize=ftell (stream);
    printf ("Size of myfile.txt: %ld bytes.\n", fileSize);

	// Figure out how many packets are needed
	long numPackets = (fileSize / PACKET_SIZE) + 1;
	printf("Number of packets required: %ld. \n", numPackets);

	// Figure out the size of the last packet
	long lastPacketSize = (fileSize % PACKET_SIZE);
	printf("Last packet size: %ld. \n", lastPacketSize);

	// Compose header message
	char numPacketsChar[128];
	itoa(numPackets, numPacketsChar, 10);
	char lastPacketSizeChar[128];
	itoa(lastPacketSize, lastPacketSizeChar, 10);
	char header[128] = "";
	strcat(header, "put;num_packets:");
	strcat(header, numPacketsChar);
	strcat(header, ";last_packet_size:");
	strcat(header, lastPacketSizeChar);
	strcat(header, ";filename:");
	strcat(header, filename.c_str());
	strcat(header, ";");
	cout << "Header: " << header << "\n";

	// Send header
	sendMessage(header);

	// Wait for response
	char response[128];
	memset(response, '\0', sizeof(char)*128);

	while(strcmp(response, "") == 0) {
		strcpy(response, receiveMessage());
	}

	// if the response is not "ok", exit
	if(strcmp(response, "ok") != 0) {
		cout << "something went wrong... quitting...";
		Sleep(10000);
		exit(0);
	}

	// Read file to memory
	long size;
	size_t result;
	fseek(stream, 0, SEEK_END);
	size = ftell(stream);
	rewind(stream);
	char *fileBuffer = new char[size];
	result = fread(fileBuffer, 1, size, stream);
	printf("Read %ld bytes to memory", result);

	// Send packets
	for(int i = 0; i < numPackets; i++) {

		// set packet size
		int packetSize = PACKET_SIZE;
		if(i == numPackets - 1) packetSize = lastPacketSize;

		// Clear buffer
		memset(&szbuffer,0,sizeof(szbuffer));

		// Get the current position in the file
		memcpy(szbuffer, &fileBuffer[i * PACKET_SIZE], PACKET_SIZE);

		// Read from fi

		ibytessent=0;
		ibytessent = sendto(s,szbuffer,ibufferlen,0, (struct sockaddr *) &fromAddr, sizeof(fromAddr));
		if (ibytessent == SOCKET_ERROR)
			throw "Send failed\n";
		else {
			//cout << "\n\r";
			//cout << "Sending packet " << i << ", size " << packetSize << ", bytes sent " << ibytessent;
		}
	}




	return true;
}


void Transfer::receiveFile(FILE *stream, int numPackets, int lastPacketSize) {

	// Send OK
	sendMessage("ok");

	// Calculate total size
	int filesize = ((numPackets - 1) * PACKET_SIZE) + lastPacketSize;
	char *fileBuffer = new char[filesize];

	// Receive packets
	for(int i = 0; i < numPackets; i++) {

		// Set packet size
		int packetSize = PACKET_SIZE;
		if(i == numPackets - 1) packetSize = lastPacketSize;

		// Reset buffer
		memset(&szbuffer,0,sizeof(szbuffer));

		// Receive packet to buffer
		FD_ZERO(&readfds);
		FD_SET(s, &readfds);

		int outfds;
		if((outfds = select (1 , &readfds, NULL, NULL, & timeouts))==SOCKET_ERROR) {//timed out, 
			throw "timeout!!!";
		}
		fromAddrSize = sizeof(fromAddr);
		if (outfds > 0) //receive frame
			ibytesrecv = recvfrom(s, (char*)& szbuffer, sizeof(szbuffer),0, (struct sockaddr*)&fromAddr, &fromAddrSize);

		// Print received packet
		printf("Received packet %ld, expected size %ld, bytes received %ld\n", i, packetSize, ibytesrecv);

		// Copy to file buffer
		memcpy(&fileBuffer[i * PACKET_SIZE], szbuffer, packetSize);

	}

	// Empty buffer to file stream
	int byteswritten = fwrite(fileBuffer, 1, filesize, stream);

	printf("Wrote %ld bytes \n", byteswritten);
}