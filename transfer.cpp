
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

Transfer::~Transfer() {
}

void Transfer::log(string message) {
	ofstream fout;
	fout.open("log.txt", std::ofstream::app);
	fout << message << "\n";
	fout.close();
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

char* Transfer::receiveMessage(bool throws) {
	memset(&szbuffer,0,sizeof(szbuffer));
	//Fill in szbuffer from accepted request.
	/*if((ibytesrecv = recv(s,szbuffer,128,0)) == SOCKET_ERROR)
		throw "Receive error in server program\n";*/
	 //fd_set is a typeFD_ZERO(&readfds); //initialize FD_SET(s, &readfds); //put the socket in the set
	FD_ZERO(&readfds);
	FD_SET(s, &readfds);

	int outfds;
	if((outfds = select (1 , &readfds, NULL, NULL, &timeouts))==SOCKET_ERROR) {//timed out, 
		// socket error 
	} else if(outfds == 0) {
		if(throws)
			throw "timeout";
	}
	fromAddrSize = sizeof(fromAddr);
	if (outfds > 0) //receive frame
		ibytesrecv = recvfrom(s, (char*)& szbuffer, sizeof(szbuffer),0, (struct sockaddr*)&fromAddr, &fromAddrSize);


	//Print reciept of successful message. 
	return szbuffer;
}

string Transfer::generatePutHeader(FILE *stream, string filename) {
	
	// Get the filesize
	long fileSize;
	fseek (stream, 0, SEEK_END);
    fileSize=ftell (stream);
    printf ("Size of myfile.txt: %ld bytes.\n", fileSize);

	// Figure out how many packets are needed
	numPackets = (fileSize / PACKET_SIZE) + 1;
	printf("Number of packets required: %ld. \n", numPackets);

	// Figure out the size of the last packet
	lastPacketSize = (fileSize % PACKET_SIZE);
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

	return header;
}


bool Transfer::sendFile(FILE *stream, string filename, bool put) {

	// Get first seq number
	int seq = (put ? SR : CR) % 2;

	// Read file to memory
	long size;
	size_t result;
	fseek(stream, 0, SEEK_END);
	size = ftell(stream);
	rewind(stream);
	char *fileBuffer = new char[size];
	result = fread(fileBuffer, 1, size, stream);
	printf("Read %ld bytes to memory", result);

	int bytesSent = 0;

	// Send packets
	for(int i = 0; i < numPackets; i++) {

		// set packet size
		int packetSize = PACKET_SIZE - 1;
		if(i == numPackets - 1) packetSize = lastPacketSize;

		char packetBuffer[128];

		// Clear buffer
		memset(&packetBuffer,0,sizeof(packetBuffer));

		// Get the current position in the file
		memcpy(packetBuffer, &fileBuffer[i * packetSize], packetSize);

		// set sequence number
		if(seq == 1) {
			packetBuffer[packetSize] |= 0x01 << 0;
		} else {
			packetBuffer[packetSize] &= ~(0x01 << 0);
		}

		bool ack = false;

		while( ! ack) {

			// Read from fi
			ibytessent=0;
			ibytessent = sendto(s,packetBuffer,sizeof(szbuffer),0, (struct sockaddr *) &fromAddr, sizeof(fromAddr));
			if (ibytessent == SOCKET_ERROR)
				throw "Send failed\n";
			else {
				cout << "\n\r";
				cout << "Sending packet " << i << ", seq #" << seq << ", size " << packetSize << ", bytes sent " << ibytessent;
				log("sent packet " + to_string(i) + ", seq #" + to_string(seq));
			}

			bytesSent += ibytessent;

			// receive ack
			char ackMsg[128] = "";
			FD_ZERO(&readfds);
			FD_SET(s, &readfds);

			int outfds; ;
			if((outfds = select (1 , &readfds, NULL, NULL, &timeouts)) == SOCKET_ERROR) {
				// socket error
			} 
			if(outfds > 0) {
				fromAddrSize = sizeof(fromAddr);
				if (outfds > 0) //receive frame
					ibytesrecv = recvfrom(s, (char*)& ackMsg, sizeof(ackMsg),0, (struct sockaddr*)&fromAddr, &fromAddrSize);

				string ackMsgString(ackMsg);

				if( ackMsgString.compare("") != 0 && ackMsgString.compare("\x1") != 0 ) {

					if(i == 0 && ackMsgString.find("SR:") >= 0 && ackMsgString.find("CR:") >= 0) {

							string responseMessage = "SR:" + to_string(SR) + ";ok";
							char responseChar[128] = "";
							strcpy(responseChar, responseMessage.c_str());
							sendMessage(responseChar);

					}
				} else {

					// ack
					bool sequenceBit = (ackMsg[0] >> 0) & 0x1;
					if((sequenceBit && seq == 1) || (! sequenceBit && seq == 0)) {
						ack = true;
						string sequenceBitString = (sequenceBit ? "1" : "0");
						log("received ack for seq #" + sequenceBitString);
					}
				}
			}
		}

		seq = (seq == 1 ? 0 : 1);
	}

	cout << "Send complete \n";
	log("file transfer completed");
	int effectiveBytes = result;
	log("number of effective bytes sent: " + to_string(effectiveBytes));
	log("number of packets sent: " + to_string(numPackets));
	log("number of bytes sent: " + to_string(bytesSent));
	log("");

	return true;
}


void Transfer::receiveFile(FILE *stream, int numPackets, int lastPacketSize, bool put) {

	// get first seq number
	int seq = (put ? SR : CR) % 2;

	// Calculate total size
	int filesize = ((numPackets - 1) * PACKET_SIZE) + lastPacketSize;
	char *fileBuffer = new char[filesize];

	// Receive packets
	for(int i = 0; i < numPackets; i++) {

		// Set packet size
		int packetSize = PACKET_SIZE - 1;
		if(i == numPackets - 1) packetSize = lastPacketSize;

		bool recvCorrectPacket = false;

		while( ! recvCorrectPacket) {

			cout << "Expecting seq #" << seq << "\n";

			// Reset buffer
			memset(&szbuffer,0,sizeof(szbuffer));


			bool recv = false;

			while(! recv) {
				// Receive packet to buffer
				FD_ZERO(&readfds);
				FD_SET(s, &readfds);

				int outfds;
				if((outfds = select (1 , &readfds, NULL, NULL, & timeouts))==SOCKET_ERROR) {//timed out, 
					//throw "timeout!!!";
				}
				fromAddrSize = sizeof(fromAddr);
				if (outfds > 0) {
					recv = true;
					ibytesrecv = recvfrom(s, (char*)& szbuffer, sizeof(szbuffer),0, (struct sockaddr*)&fromAddr, &fromAddrSize);
				}
			}

			// received sequence #
			bool packetSeq = (szbuffer[packetSize] >> 0) & 0x1;

			string packetSeqString = (packetSeq ? "1" : "0");
			log("received packet " + packetSeqString);

			// send ACK
			char ack[128] = "";

			// set ACK number
			if(packetSeq) {
				ack[0] |= 0x01 << 0;
			} else {
				ack[0] &= ~(0x01 << 0);
			}

			ibufferlen = strlen(ack);
			ibytessent = sendto(s,ack,ibufferlen,0, (struct sockaddr *) &fromAddr, sizeof(fromAddr));
			if (ibytessent == SOCKET_ERROR)
				throw "Send failed\n";  
			else {
				cout << "Sending ACK for seq #" << (packetSeq ? 1 : 0) <<  "\n";
				log("sent ACK for packet " + packetSeqString);
			}
						
			// check if we received the right packet
			if((packetSeq && seq == 1) || (! packetSeq && seq == 0)) {
				recvCorrectPacket = true;
				seq = (seq == 1 ? 0 : 1);
			} else {
				recvCorrectPacket = false;
			}

		}

		// Print received packet
		printf("Received packet %ld, expected size %ld, bytes received %ld\n", i, packetSize, ibytesrecv);

		// Copy to file buffer
		memcpy(&fileBuffer[i * packetSize], szbuffer, packetSize);

	}

	seq = (seq == 1 ? 0 : 1);

	// Empty buffer to file stream
	int byteswritten = fwrite(fileBuffer, 1, filesize, stream);

	printf("Wrote %ld bytes \n", byteswritten);

	log("transfer completed");
	log("bytes received: " + to_string(byteswritten));
	log("");

	for(int i = 0; i < 10; i++) {

		try {
			string message = receiveMessage(true);

			i = -1;
			
			// send ACK
			bool packetSeq = (seq == 1 ? true : false);
			char ack[128] = "";

			// set ACK number
			if(packetSeq) {
				ack[0] |= 0x01 << 0;
			} else {
				ack[0] &= ~(0x01 << 0);
			}

			ibufferlen = strlen(ack);
			ibytessent = sendto(s,ack,ibufferlen,0, (struct sockaddr *) &fromAddr, sizeof(fromAddr));
			if (ibytessent == SOCKET_ERROR)
				throw "Send failed\n";  
			else {
				cout << "Sending ACK for seq #" << (packetSeq ? 1 : 0) <<  "\n";
				string packetSeqString = (packetSeq ? "1" : "0");
				log("sent ACK for packet " + packetSeqString);
			}
		} catch(char* error) {
			// timeout
		}
	}
}

void Transfer::setCRSR(int CR, int SR) {
	this->CR = CR;
	this->SR = SR;
}