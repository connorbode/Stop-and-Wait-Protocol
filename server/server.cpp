#include "server.h"

using namespace std;

Server::Server() {};

Server::Server(SOCKET s) {
	
	transfer = Transfer(s);
	srand((unsigned)time(NULL));
}


/**
 * Starts the server listening for incoming messages
 */
void Server::run(SOCKADDR_IN fromAddr) {

	transfer.fromAddr = fromAddr;

	// Set current directory
	if( ! _getcwd(directory, sizeof(directory)) ) {
		
		// If we can't set the current directory, quit
		cout << "ERROR: could not retrieve working directory. \n\r Quitting..";
		Sleep(1000);
		exit(0);
	}
	
	// Set directory to files folder
	strcat(directory, "\\files");

	// Loop until close
	while (true) {

		// Receive a message
		char message[128];
		strcpy(message, transfer.receiveMessage());
		stringstream ss;
		string messageString;
		ss << message;
		ss >> messageString;

		if( ! messageString.empty() ) {

			cout << "Received message: " << messageString << "\n";

			
			int startIndex = messageString.find("CR:");

			if(startIndex == -1) {}
			else {

				int endIndex = messageString.find(";", startIndex);
				int CR = stoi(messageString.substr(startIndex + 3, endIndex - startIndex - 3));

				cout << "Received CR " << CR << "\n";

				/* Decode message */

				// Client wants to list remote files
				if(messageString.substr(0,4).compare("list") == 0) { list(CR); }

				// Client wants to upload a file
				else if(messageString.substr(0, 3).compare("put") == 0) { put(messageString, CR); }

				// Client wants to download a file
				else if(messageString.substr(0, 3).compare("get") == 0) {get(messageString, CR); }

				// Client wants to delete a file
				else if(messageString.substr(0, 6).compare("delete") == 0) {deleteFile(messageString, CR); }
			}

		}
	}
}

/**
 * Lists files on the server
 */
void Server::list(int CR) {

	// Generate server random number
	int SR = rand() % 255;
	cout << "Generated SR " << SR << "\n";

	// Filelist to send
	string fileListString = "SR:" + to_string(SR) + ";CR:" + to_string(CR) + ";";
	char fileList[128] = "";
	strcat(fileList, fileListString.c_str());

	// Open directory
	if ((dir = opendir(directory)) != NULL) {
		  
		// Iterate through the directory
		while ((dirEntry = readdir(dir)) != NULL) {

			// If it is a file
			if( !S_ISDIR(dirEntry->d_type)) {

				// Append the filename to the list
				strcat(fileList, dirEntry->d_name);
				strcat(fileList, ";");
			}
		}

		// Close the directory
		closedir (dir);

		// Check to see if the filelist was empty
		if(strcmp(fileList, "") == 0) {
			strcat(fileList, ";");
		}

		char response[128] = "";

		// Send the fileList back to the client
		transfer.sendMessage(fileList);
	} 
		
	// Failed to open directory
	else {
		  
		// say bye and quit!
		cout << "Failed to open directory :( \n\r Quitting..";
		Sleep(1000);
		exit(0);
	}

	cout << "\n\r";
}

/**
 * Deals with a put request
 */
void Server::put(string request, int CR) {

	cout << "\nPUT Request\n";

	// Decode request
	int numPackets, lastPacketSize;
	string filename;

	// Find any occurrences of ; and extract the string
	string delimiter = ";";
	size_t pos = 0, pos2 = 0;
	string token, param, value;
	while((pos = request.find(delimiter)) != string::npos) {
		token = request.substr(0, pos);

		// Ignore put 
		if(token.compare("put") != 0) {

			// Divide at :
			pos2 = request.find(":");
			param = request.substr(0, pos2);
			value = request.substr(pos2+1, token.size()-pos2-1);

			// If numPackets
			if(param.compare("num_packets") == 0) {
				numPackets = atoi(value.c_str());
				printf("Number of packets: %ld. \n", numPackets);
			}

			// If lastPacketSize
			else if(param.compare("last_packet_size") == 0) { 
				lastPacketSize = atoi(value.c_str());
				printf("Last packet size: %ld. \n", lastPacketSize);
			}

			// If filename
			else if(param.compare("filename") == 0) {
				filename = value;
				cout << filename << "\n";
			}
		}

		request.erase(0, pos + 1);
	}

	// Establish connection to file
	FILE *stream;
	if((stream = fopen(filename.c_str(), "wb")) != NULL) {

		// Receive file
		transfer.receiveFile(stream, numPackets, lastPacketSize);

		// Close the stream
		fclose(stream);
	}

	// If we couldn't create file
	else {

		cout << "Couldn't create file\n\n";
	}
	
	
}

/**
 * Deals with a get request
 */
void Server::get(string request, int CR) {

	string filename;

	// Figure out filename
	string delimiter = ";";
	size_t pos = 0, pos2 = 0;
	string token, param, value;
	while((pos = request.find(delimiter)) != string::npos) {
		token = request.substr(0, pos);

		// Ignore put 
		if(token.compare("get") != 0) {

			// Divide at :
			pos2 = request.find(":");
			param = request.substr(0, pos2);
			value = request.substr(pos2+1, token.size()-pos2-1);

			// If filename
			if(param.compare("filename") == 0) {
				filename = value;
			}
		}

		request.erase(0, pos + 1);
	}

	cout << "GET request for \"" << filename << "\" \n";

	// Open stream to file 
	FILE* stream;
	filename = "files\\" + filename;

	// If we can open the file
	if((stream = fopen(filename.c_str(), "rb")) != NULL) {
		
		cout << "file opened\n";

		// Send the file
		transfer.sendFile(stream, filename.c_str());

		// Close the filestream
		fclose(stream);
	} 

	// If we couldn't open the file
	else {
		transfer.sendMessage("could not open file");
	}

}

/**
 * Deletes a local file
 */
void Server::deleteFile(string request, int CR) {

	string filename;

	// Figure out filename
	string delimiter = ";";
	size_t pos = 0, pos2 = 0;
	string token, param, value;
	while((pos = request.find(delimiter)) != string::npos) {
		token = request.substr(0, pos);

		// Ignore put 
		if(token.compare("delete") != 0) {

			// Divide at :
			pos2 = request.find(":");
			param = request.substr(0, pos2);
			value = request.substr(pos2+1, token.size()-pos2-1);

			// If filename
			if(param.compare("filename") == 0) {
				filename = value;
			}
		}

		request.erase(0, pos + 1);
	}

	// Convert filename to char
	filename = "files\\" + filename;
	char filenameChar[128];
	stringstream ss;
	ss << filename;
	ss >> filenameChar;

	// Delete file
	int result = remove(filenameChar);

	// Failed to delete file
	if(result != 0) {
		transfer.sendMessage("failed to delete file");
	}

	// File deleted
	else {
		transfer.sendMessage("ok");
	}
}