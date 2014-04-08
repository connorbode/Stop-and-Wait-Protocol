
#include "transfer.h"
#define UTIMER 1000000
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
	numPackets = (fileSize / (PACKET_SIZE - 1)) + 1;
	printf("Number of packets required: %ld. \n", numPackets);

	// Figure out the size of the last packet
	lastPacketSize = (fileSize % (PACKET_SIZE - 1));
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

	// Retrieve window size
	cout << "Enter window size: ";
	int windowSize;
	cin >> windowSize;

	// Get first seq number
	int maxSeqNum = 100;
	int seq = (put ? SR : CR) % maxSeqNum;
	cout << "First sequence number: " << seq << "\n";

	// Read file to memory
	long size;
	size_t result;
	fseek(stream, 0, SEEK_END);
	size = ftell(stream);
	rewind(stream);
	char *fileBuffer = new char[size];
	result = fread(fileBuffer, 1, size, stream);
	printf("Read %ld bytes to memory\n", result);

	int bytesSent = 0;

	int curr = seq;
	int packetsACKed = 0;

	while(true) {

		// Send window
		if(curr > maxSeqNum) {
			curr -= (maxSeqNum + 1);
		}
		int windowMax = (packetsACKed + windowSize >= numPackets - 1 ? numPackets : packetsACKed + windowSize);
		seq = curr;
		for(int i = packetsACKed; i < windowMax; i++) {

			// set packet size
			int packetSize = (i == numPackets - 1 ? lastPacketSize: PACKET_SIZE - 1);

			char packetBuffer[128];

			// Clear buffer
			memset(&packetBuffer,0,sizeof(packetBuffer));

			// Get the current position in the file
			memcpy(packetBuffer, &fileBuffer[i * packetSize], packetSize);

			// set sequence number
			packetBuffer[packetSize] = seq;
			/*int seqNum = seq;
			if(seq == 1) {
				packetBuffer[packetSize] |= 0x01 << 0;
			} else {
				packetBuffer[packetSize] &= ~(0x01 << 0);
			}*/

			// Read from fi
			ibytessent=0;
			ibytessent = sendto(s,packetBuffer,sizeof(szbuffer),0, (struct sockaddr *) &fromAddr, sizeof(fromAddr));
			if (ibytessent == SOCKET_ERROR)
				throw "Send failed\n";
			else {
				cout << "Sending packet " << i << ", seq #" << seq << ", size " << packetSize << ", bytes sent " << ibytessent << "\n";
				log("sent packet " + to_string(i) + ", seq #" + to_string(seq));
			}

			bytesSent += ibytessent;
			seq = (seq >= maxSeqNum ? 0 : seq + 1);
		}

		if(packetsACKed < numPackets) {
			// receive responses
			int limit = (numPackets - packetsACKed < windowSize ? curr + numPackets - packetsACKed : curr + windowSize);
			for(curr; curr < limit;) {
			
				// receive ack
				char ackMsg[128] = "";
				FD_ZERO(&readfds);
				FD_SET(s, &readfds);

				int outfds;
				outfds = select (1 , &readfds, NULL, NULL, &timeouts);
				if(outfds == SOCKET_ERROR) {
					// socket error
				} else if (outfds == 0) {
					// timeout
				} else if(outfds > 0) {
					fromAddrSize = sizeof(fromAddr);
					memset(ackMsg, 0, sizeof(ackMsg));
					ibytesrecv = recvfrom(s, (char*)& ackMsg, sizeof(ackMsg),0, (struct sockaddr*)&fromAddr, &fromAddrSize);

					string ackMsgString(ackMsg);

					// if we're still receiving headers
					if(packetsACKed == 0 && ackMsgString.find("SR:") != -1 && ackMsgString.find("CR:") != -1) {

							string responseMessage = "SR:" + to_string(SR) + ";ok";
							char responseChar[128] = "";
							strcpy(responseChar, responseMessage.c_str());
							sendMessage(responseChar);
					} else {

						// is ack
						bool isControl = (ackMsg[0] >> 0) & 0x1;

						// ack
						bool ACK = (ackMsg[1] >> 0) & 0x1;

						// get sequence number
						int seqNum = ackMsg[2];

						if(isControl) {
							if(ACK == 1) {
							//ACK 
								cout << "received ACK for sequence " << seqNum << "\n";
								int diff;
								if(curr > maxSeqNum) {
									diff = (curr - seqNum - maxSeqNum); 
								} else {
									diff = seqNum - curr + 1;
								}
								if(diff >= 0) {
									packetsACKed += diff;
									curr += diff;
								}
							} else {
							//NAK
								cout << "received NAK for sequence " << seqNum << "\n";
								int diff = curr - seqNum;
								if(curr > maxSeqNum) {
									diff -= maxSeqNum + 1;
								}
								packetsACKed -= diff;
								curr = seqNum;
								break;
							}
						}
					}
				}

			}
		} else {
			break;
		}
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
	int maxSeqNum = 100;
	int seq = (put ? SR : CR) % maxSeqNum;

	cout << "First sequence number: " << seq << "\n";

	// Calculate total size
	int filesize = ((numPackets - 1) * PACKET_SIZE) + lastPacketSize;
	char *fileBuffer = new char[filesize];

	// Receive packets
	int receivedPackets = 0;

	while(true) {

		// Receive packet to buffer
		FD_ZERO(&readfds);
		FD_SET(s, &readfds);

		int outfds = select (1 , &readfds, NULL, NULL, & timeouts);
		if(outfds==SOCKET_ERROR) {
			// socket error 
		} else if (outfds == 0) {
			// timeout

			// NAK!! 					
			char nak[128] = "";
			nak[0] |= 0x01 << 0;
			nak[1] &= ~(0x01 << 0);
			nak[2] = seq;

			ibufferlen = 127;
			ibytessent = sendto(s,nak,ibufferlen,0, (struct sockaddr *) &fromAddr, sizeof(fromAddr));
			if (ibytessent == SOCKET_ERROR)
				throw "Send failed\n";  
			else {
				cout << "Sending NAK for seq #" << seq <<  "\n";
				log("sent NAK for packet " + seq);
			}
		} else if (outfds > 0) {

			// received correctly
			memset(&szbuffer,0,sizeof(szbuffer));
			fromAddrSize = sizeof(fromAddr);
			ibytesrecv = recvfrom(s, (char*)& szbuffer, sizeof(szbuffer),0, (struct sockaddr*)&fromAddr, &fromAddrSize);

			// check that we didn't receive a header..
			string temp = string(szbuffer);
			if(temp.find("put;num_packet") != -1) {
				
				int startIndex = temp.find("SR:");

				if(startIndex >= 0) {
					int endIndex = temp.find(";", startIndex);
					int incoming_SR = stoi(temp.substr(startIndex + 3, endIndex - startIndex - 3));
					char sendConf[128] = "SR:";
					char SRChar[3] = "";
					sprintf(SRChar, "%d", incoming_SR);
					strcat(sendConf, SRChar);
					strcat(sendConf, ";");
					sendMessage(sendConf);
				}

			} else {

				// received a sequence (as opposed to a header)

				// get sequence number
				int packetSize = (receivedPackets == numPackets - 1 ? lastPacketSize : PACKET_SIZE - 1);
				int receivedSeq = szbuffer[packetSize];

				char packetSeqChar[128] = "";
				itoa(receivedSeq, packetSeqChar, 10); // <<-- C++ BULLSHIT.  
				string packetSeqString(packetSeqChar);
				log("received packet " + packetSeqString);


				if(receivedSeq < seq) {
					// do nothing 
				} else if(receivedSeq == seq) {
					// ack		
					// Print received packet
					printf("Received packet %ld, sequence number %ld expected size %ld, bytes received %ld\n", receivedPackets, receivedSeq, packetSize, ibytesrecv);

					// Copy to file buffer
					memcpy(&fileBuffer[receivedPackets * packetSize], szbuffer, packetSize);

					receivedPackets++;
					seq = (seq == maxSeqNum ? 0 : seq + 1);

					char ack[128] = "";
					ack[0] |= 0x01 << 0;
					ack[1] |= 0x01 << 0;
					ack[2] = receivedSeq;

					ibufferlen = strlen(ack);
					ibytessent = sendto(s,ack,ibufferlen,0, (struct sockaddr *) &fromAddr, sizeof(fromAddr));
					if (ibytessent == SOCKET_ERROR)
						throw "Send failed\n";  
					else {
						cout << "Sending ACK for seq #" << packetSeqString <<  "\n";
						log("sent ACK for packet " + packetSeqString);
					}

					if(receivedPackets == numPackets) {
						break;
					}


				} else {
					// nak
					// NAK!! 					
					char nak[128] = "";
					nak[0] |= 0x01 << 0;
					nak[1] &= ~(0x01 << 0);
					nak[2] = seq;

					ibytessent = sendto(s,nak,ibufferlen,0, (struct sockaddr *) &fromAddr, sizeof(fromAddr));
					if (ibytessent == SOCKET_ERROR)
						throw "Send failed\n";  
					else {
						cout << "Sending NAK for seq #" << seq <<  "\n";
						log("sent NAK for packet " + seq);
					}
				}
			}
		}
	}

	// Empty buffer to file stream
	int byteswritten = fwrite(fileBuffer, 1, filesize, stream);

	printf("Wrote %ld bytes \n", byteswritten);

	log("transfer completed");
	log("bytes received: " + to_string(byteswritten));
	log("");

	// FINAL ACKS IN CASE OF DROPPING

	cout << "Waiting for any extra messages (because of dropping)\n";

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

	cout << "Done.\n\n";
}

void Transfer::setCRSR(int CR, int SR) {
	this->CR = CR;
	this->SR = SR;
}