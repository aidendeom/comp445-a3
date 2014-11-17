#include "client.h"
#include <vector>
#include <dirent.h>
#include <utils.h>
#include <sstream>

using namespace std;

//get the size of the file
long GetFileSize(char * filename)
{
	int result;
	struct _stat stat_buf;
	result = _stat(filename, &stat_buf);
	if (result != 0) 
		return 0; 

	return stat_buf.st_size;
}

//PUTS file from client onto the server
//returns true or false
bool Client::sendF(int sock, char * filename, char * sendHost, int server_number)
{
	Msg frame; 
	frame.packet_type = FRAME;
	Ack ack; 
	ack.number = -1;
	
	long bytes_counter = 0, byte_count = 0;
	int bytes_sent = 0, bytes_read = 0, bytes_read_total = 0;
	int packetsSent = 0, packetsSentNeeded = 0;
	bool isFirstPacket = true, isFinished = false;
	int nTries; 
	bool isMaxAttempts = false;

	int sequence_number = server_number % 2;
	
	if(TRACECLI) 
		fout << "The sender has started on " << sendHost << endl;

	//open the file
	FILE * stream;
	fopen_s(&stream, filename, "rb");

	if (stream != NULL)
	{
		bytes_counter = GetFileSize(filename);
		while (1)
		{ 
			if ( bytes_counter > MAX_FSIZE)
			{
				frame.header = ( isFirstPacket ? INITIAL_DATA : DATA); 
				byte_count = MAX_FSIZE;
			}
			else
			{
				byte_count = bytes_counter;
				isFinished = true;
				frame.header = FINAL_DATA;
			}

			bytes_counter -= MAX_FSIZE;

			bytes_read = fread(frame.buffer, sizeof(char), byte_count, stream); 
			bytes_read_total += bytes_read;
			frame.buffer_length = byte_count;
			frame.snwseq = sequence_number;

			nTries = 0;
			do 
			{
				nTries++;
				if ( sendFrame(sock, &frame) != sizeof(frame) )
					return false;
				
				packetsSent++;

				if (nTries == 1)
					packetsSentNeeded++;

				bytes_sent += sizeof(frame);

				cout << "Sent the frame " << sequence_number << endl;
				if (TRACECLI) 
					fout << "Sent frame " << sequence_number << endl;

				if (isFinished && (nTries > MAX_RETRY))
				{ 
					isMaxAttempts = true;
					break;
				}
			} while ( recAck(sock, &ack) != IN_PKT || ack.number != sequence_number );

			
			if (isMaxAttempts)
			{
				cout << "The sender has not received ACK " << sequence_number << " after " << MAX_RETRY << " tries. The transfer has finished." << endl;
				if (TRACECLI)
					fout << "Sender has not received ACK " << sequence_number << " after " << MAX_RETRY << " tries. The transfer has finished." << endl;
			}
			else
			{
				cout << "The sender has received ACK " << ack.number << endl;
				if (TRACECLI) { fout << "The sender received ACK " << ack.number << endl; }
			}

			isFirstPacket = false;
			sequence_number = (sequence_number == 0 ? 1 : 0);
			if (isFinished) 
				break;
		}
		fclose( stream );

		cout << "The file transfer is complete." << endl << endl;
		cout << "The number of packets sent: " << packetsSent << endl;
		cout << "The number of packets sent with no duplicates: " << packetsSentNeeded << endl;
		cout << "The number of bytes that have been sent: " << bytes_sent << endl;
		cout << "The number of bytes that have been read: " << bytes_read_total << endl << endl;
		
		if (TRACECLI) 
		{
			fout << "The file transfer is complete." << endl << endl;
			fout << "The number of packets sent: " << packetsSent << endl;
			fout << "The number of packets sent with no duplicates: " << packetsSentNeeded << endl;
			fout << "The number of bytes that have been sent: " << bytes_sent << endl;
			fout << "The number of bytes that have been read: " << bytes_read_total << endl << endl;
		}
		return true;
	}
	else
	{
		cout << "There was an issue with opening the file..." << endl;
        if (TRACECLI) { fout << "Problem with opening the requested file" << endl; }
		return false;		
	}
}

//GET file from the server
//returns true or false
bool Client::recFile(int sock, char * filename, char * recHost, int cliNum)
{
	Msg frame;
	Ack ack; 
	ack.packet_type = FRAME_ACK;
	
	long byte_count = 0;
	int packetsSent = 0, packetsSentNeeded = 0;
	int bytes_received = 0, bytes_written = 0, bytes_written_total = 0;
	int seqNum = cliNum % 2;

	if (TRACECLI) { fout << "Receiver started on " << recHost << endl; }

	FILE * stream;
	fopen_s(&stream, filename, "w+b");
	
	if (stream != NULL)
	{
		while (1)
		{ 
			while( recFrame(sock, &frame) != IN_PKT ) {;}
			
			bytes_received += sizeof(frame);

			if (frame.packet_type == HANDSHAKE)
			{
				cout << "The client has received handshake C" << hs.client_number << " and S" << hs.server_number << endl;
				if (TRACECLI)
					fout << "The client has received handshake C" << hs.client_number << " and S" << hs.server_number << endl;
			
				if ( sendReq(sock, &hs, &sa_in) != sizeof(hs) )
					err_sys("Error in sending packet.");

				cout << "The client has sent handshake C" << hs.client_number << " and S" << hs.server_number << endl;
				if (TRACECLI) { fout << "Client sent handshake C" << hs.client_number << " and S" << hs.server_number << endl; }
			}
			else if (frame.packet_type == FRAME)
			{
				cout << endl << "The client has received frame " << (int)frame.snwseq << endl;
				if (TRACECLI) { fout << "The client received frame " << (int)frame.snwseq << endl; }
				
				if ( (int)frame.snwseq != seqNum )
				{
					ack.number = (int)frame.snwseq;

					if ( sendAck(sock, &ack) != sizeof(ack) )
						return false;
					
					cout << "The client has sent ACK " << ack.number << " again" << endl;
					packetsSent++;
					if (TRACECLI) 
						fout << "The client sent ACK " << ack.number << " again" << endl;
				}
				else
				{
					ack.number = (int)frame.snwseq;	
					
					if ( sendAck(sock, &ack) != sizeof(ack) )
						return false;
					
					cout << "The client has sent ACK " << ack.number << endl;
					if (TRACECLI) { fout << "The client sent ACK " << ack.number << endl; }
					
					packetsSent++;
					packetsSentNeeded++;

					byte_count = frame.buffer_length;
					bytes_written = fwrite(frame.buffer, sizeof(char), byte_count, stream );
					bytes_written_total += bytes_written;
					
					seqNum = (seqNum == 0 ? 1 : 0);

					if (frame.header == FINAL_DATA) //stop loop if last frame of file
						break;
				}
			}
		}
		
		fclose( stream );

		cout << endl << "The file transfer is complete" << endl;
		cout << "The number of packets sent: " << packetsSent << endl;
		cout << "The number of packets sent with no duplicates: " << packetsSentNeeded << endl;
		cout << "The number of bytes that have been received: " << bytes_received << endl;
		cout << "The number of bytes that have been written: " << bytes_written_total << endl << endl;

		if (TRACECLI) 
		{ 
			fout << "The file transfer is complete" << endl;
			fout << "The number of packets sent: " << packetsSent << endl;
			fout << "The number of packets sent with no duplicates: " << packetsSentNeeded << endl;
			fout << "The number of bytes that have been received: " << bytes_received << endl;
			fout << "The number of bytes that have been written: " << bytes_written_total << endl << endl;
		}
		return true;
	}
	else
	{
		cout << "There was a problem with opening the file." << endl;
        if (TRACECLI) 
			fout << "Problem with opening the file." << endl; 
		return false;
	}
}

bool Client::recDir(SOCKET sock, std::string& outstring)
{

	printf("got into the function");

	Msg frame;
	Ack ack;
	ack.packet_type = PktType::FRAME_ACK;

	int seqNo = 0;

	stringstream ss;

	RecRes result = RecRes::TIMEOUT;

	while (result == RecRes::TIMEOUT)
		result = recFrame(sock, &frame);

	if (result == RecRes::RECEIVE_ERROR)
	{
		err_sys("Failure receiving frame from server");
		return false;
	}
	if (frame.header != INITIAL_DATA)
	{
		err_sys("First packet wasn't INITIAL_DATA");
		return false;
	}

	ack.number = frame.snwseq;
	if (!sendAck(sock, &ack))
		return false;

	ss.write(frame.buffer, frame.buffer_length);

	while (frame.header != FINAL_DATA)
	{
		result = RecRes::TIMEOUT;

		while (result == RecRes::TIMEOUT)
		{
			result = recFrame(sock, &frame);
		}
		if (result == RecRes::RECEIVE_ERROR)
		{
			err_sys("Failure receiving frame from server");
			return false;
		}

		if (frame.snwseq == seqNo)
		{
			ss.write(frame.buffer, frame.buffer_length);
			seqNo ^= 1;
		}

		ack.number = frame.snwseq;
		if (!sendAck(sock, &ack))
			return false;
	}

	outstring = ss.str();
	return true;
}

//RECEIVE RESPONSE
//takes all responses from server
//returns the result (timeout, incoming packet, error)
RecRes Client::recResp(int sock, TWHandshake * ptr_handshake)
{
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);
	int bytes_recvd;
		
	resetTimeout();

	
	int outfds = select(1, &readfds, NULL, NULL, &timeouts);
	switch (outfds)
	{
		case 0:
			return TIMEOUT; 
			break;
		case 1:
			bytes_recvd = recvfrom(sock, (char *)ptr_handshake, sizeof(*ptr_handshake),0, (struct sockaddr*)&sa_in, &sa_in_size);
			return IN_PKT; 
			break;
		default:
			return RECEIVE_ERROR; 
			break;
	}
}

//receives a frame of the incoming data
//returns the result (timeout, incoming packet or error 
RecRes Client::recFrame(int sock, Msg * ptr_message_frame)
{
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);
	int bytes_recvd;

	resetTimeout();
	
	int outfds = select(1 , &readfds, NULL, NULL, &timeouts);
	switch (outfds)
	{
		case 0:
			return TIMEOUT; 
			break;
		case 1:
			bytes_recvd = recvfrom(sock, (char *)ptr_message_frame, sizeof(*ptr_message_frame),0, (struct sockaddr*)&sa_in, &sa_in_size);
			return IN_PKT; 
			break;
		default:
			return RECEIVE_ERROR; 
			break;
	}
}

//receive an ACK from ther server during a put
RecRes Client::recAck(int sock, Ack * ack)
{
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);
	int bytes_recvd;

	resetTimeout();

	int outfds = select(1 , &readfds, NULL, NULL, &timeouts);
	switch (outfds)
	{
		case 0:
			return TIMEOUT; 
			break;
		case 1:
			bytes_recvd = recvfrom(sock, (char *)ack, sizeof(*ack),0, (struct sockaddr*)&sa_in, &sa_in_size);
			return IN_PKT; 
			break;
		default:
			return RECEIVE_ERROR; 
			break;
	}
}

//handshake with the server
int Client::sendReq(int sock, TWHandshake * ptr_handshake, struct sockaddr_in * sa_in)
{
	return sendto(sock, (const char *)ptr_handshake, sizeof(*ptr_handshake), 0, (struct sockaddr *)sa_in, sizeof(*sa_in));
}

//send frames to server during PUT
int Client::sendFrame(int sock, Msg * ptr_message_frame)
{
	return sendto(sock, (const char*)ptr_message_frame, sizeof(*ptr_message_frame), 0, (struct sockaddr*)&sa_in, sizeof(sa_in));
}

//send an ACK when received from server during server GET
int Client::sendAck(int sock, Ack * ack)
{
	return sendto(sock, (const char*)ack, sizeof(*ack), 0, (struct sockaddr*)&sa_in, sizeof(sa_in));
}

//NOT IN USE
string ExePath()
{
    char buffer[MAX_PATH];
   // GetModuleFileName( NULL, buffer, MAX_PATH );
    string::size_type pos = string( buffer ).find_last_of( "\\/" );

	return string( buffer ).substr( 0, pos);
}
void list(char* direction, string path)
{
	string folderOption;
	WIN32_FIND_DATAA ffd;
	string pathIs;
	if(strcmp(direction, "get") == 0)
		folderOption = "Server";
	else
		folderOption = "Client";

	
	vector<string> fileTypes;
	fileTypes.push_back("/*.txt");
	fileTypes.push_back("/*.pdf");
	fileTypes.push_back("/*.jpg");
	
	for (int i = 0; i < fileTypes.size(); i++)
	{
		pathIs = "";
		pathIs.append(path);
		pathIs.append("\\../");
		pathIs.append(folderOption);
		pathIs.append(fileTypes[i]);
			HANDLE hfile=FindFirstFileA(string(pathIs).c_str(), &ffd);
			do
			{
				cout << ffd.cFileName << endl;
			}
			while(FindNextFileA(hfile,&ffd));

			FindClose(&ffd);
	}
}
//END NOT IN USE


void Client::run()
{
	char server[INPUT_LENGTH]; char filename[INPUT_LENGTH]; char direction[INPUT_LENGTH];
	char hostname[HNAME_LENGTH]; char username[UNAME_LENGTH]; char remotehost[HNAME_LENGTH];
	unsigned long fileName = (unsigned long)	FNAME_LENGTH;
	bool runContinue = true;

	if (WSAStartup(0x0202,&wsadata) != 0)
	{  
		WSACleanup();  
	    err_sys("Error in starting WSAStartup()\n");
	}

	//get client username
	if ( !GetUserNameA((LPSTR)username, &fileName) )
		err_sys("Cannot get the user name");

	//get client hostname
	if ( gethostname(hostname, (int)HNAME_LENGTH) != 0 ) 
		err_sys("Cannot get the host name");

	printf("Client Program\n");
	printf("========================================================\n");
	printf("The client is running on host [%s].\n", hostname);
	printf("The user of this program is [%s]. \n", username);
	printf("To exit, type \"exit\" as the command.\n");
	printf("========================================================\n");

	printf("========================================================\n");
	printf("How I work\n");
	printf("========================================================\n");
	printf("First, type in the command you want to do [GET|PUT|EXIT]\n");
	printf("If GET or PUT, type in the file that you want\n");
	printf("After, enter the server that you want to connect to\n");
	printf("========================================================\n\n");

	while ( true )
	{
		//create the socket
		if ( (sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 )
			err_sys("socket() failed");		

		memset(&sa, 0, sizeof(sa));
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = htonl(INADDR_ANY);
		sa.sin_port = htons(CLIENT_PORT);
		
		//bind to it
		if (bind( sock, (LPSOCKADDR)&sa, sizeof(sa) ) < 0)
			err_sys("Socket binding error");		

		//generate rand num for handshake
		srand((unsigned)time(NULL));
		random = rand() % MAX_RAND;

		//validate the client's input command. 
		bool invalidCommand = false;
		do
		{
			cout << "Enter command: "; 
			cin >> direction;
						
			if ( strcmp(direction, "get") == 0 || strcmp(direction, "put") == 0 || strcmp(direction, "list") == 0 || strcmp(direction, "exit")==0 )
			{
				invalidCommand = false;
			}
			else
			{
				invalidCommand = true;
				err_sys("Invalid direction. Use \"get\" or \"put\" or \"list\" or \"exit\".");
			}

		} while (invalidCommand);
		
		//check for exit command
		if (strncmp(direction, "exit", 4) == 0)
			break ;

		//if the command is PUT or GET, check for the filename
		bool invalidFilename = false;
		if (strcmp(direction, "get") == 0 || strcmp(direction, "put") == 0)
		{
			do
			{
				if (strcmp(direction, "get") == 0)
				{
					cout << getDirectoryItems();
				}

				cout << "Enter filename: ";
				cin >> filename;

				if (strcmp(direction, "put") == 0)
				{
					if (_access(filename, 0) == -1)
					{
						invalidFilename = true;
						err_sys("The file does not exist!");
					}
					else
						invalidFilename = false;
				}
				else
					invalidFilename = false;

			} while (invalidFilename);
		}
		
		//check the hostname again
		bool invalidHost = false;
		do
		{
			cout << "Enter the server name: "; 
			cin >> remotehost;
			struct hostent *rp;
			rp = gethostbyname(remotehost);
			if (rp == NULL)
			{
				invalidHost = true;
				err_sys("Unable to connect to the server...");
			}			
			else
			{
				invalidHost = false;
			}
		} while (invalidHost);

		cout << endl << "========================================" << endl;

		//take user input and copy to the handshake struct
		strcpy(hs.hostname, hostname);
		strcpy(hs.username, username);
		strcpy(hs.filename, filename);
		
		if ( runContinue )
		{
			struct hostent *rp;
			rp = gethostbyname(remotehost);
			memset(&sa_in, 0, sizeof(sa_in) );
			memcpy(&sa_in.sin_addr, rp->h_addr, rp->h_length);
			sa_in.sin_family = rp->h_addrtype;
			sa_in.sin_port = htons(REMOTE_PORT);
			sa_in_size = sizeof(sa_in);

			//start the handshake with random num
			hs.client_number = random;
			hs.type = CLIENT_REQ;
			hs.packet_type = HANDSHAKE;

			//set what direction you will be using
			if ( strcmp(direction, "get") == 0 )
			{
				hs.direction = GET;
			}
			else if ( strcmp(direction, "put") == 0 )
			{
				hs.direction = PUT;
			}
			else if (strcmp(direction, "list") == 0)
			{
				hs.direction = LIST;
			}
			
			if ( sendReq(sock, &hs, &sa_in) != sizeof(hs) )
				err_sys("Error in sending packet.");
				
			cout << "The client has sent handshake C" << hs.client_number << endl;
				
			if (TRACECLI) 
			{ 					         
				fout << "----------------------------------------" << endl;
				fout << "The client has sent handshake C" << hs.client_number << endl; 
			}

			RecRes result = recResp(sock, &hs);

			if(result == RECEIVE_ERROR)
			{
				err_sys("An error has been returned.");
				if (TRACECLI) 
				{ 
					fout << "----------------------------------------" << endl;
					fout << "Select() has returned an error." << endl; 
				}
			}
			else if(result == TIMEOUT)
			{
				err_sys("Select() timed out.");
				if (TRACECLI) 
				{ 
					fout << "----------------------------------------" << endl;
					fout << "Select() has timed out." << endl; 
				}
			}
			else //INCOMING PACKET
			{
				//if the file does not exist on server
				if (hs.type == FILE_NE)
				{
					cout << "The files does not exist on the server!" << endl;
					if (TRACECLI) 
					{ 
						fout << "---------------------------------------------" << endl;
						fout << "The server says that the file does not exist!" << endl; 
					}
				}
				else if (hs.type == INVALID) //if the handshake fails
				{
					cout << "The request is not valid." << endl;
					if (TRACECLI) 
					{ 
						fout << "----------------------------------------" << endl;
						fout << "The request is invalid." << endl; 
					}
				}
				if (hs.type == ACK_CNUM) //response from server is ACK
				{
					cout << "The client has received handshake C" << hs.client_number << " and S" << hs.server_number << endl;
					if (TRACECLI) 
					{ 
						fout << "----------------------------------------" << endl;
						fout << "Received handshake C" << hs.client_number << " and S" << hs.server_number << endl; 
					}
					
					//set handshake to ACK for server
					hs.type = ACK_SNUM;
					
					//calculate the sequence number of last correct packet
					int sequence_number = hs.server_number % 2;
					
					//send client response to server (3-way)
					if ( sendReq(sock, &hs, &sa_in) != sizeof(hs) )
						err_sys("Error in sending packet.");

					cout << "The client has sent handshake C" << hs.client_number << " and S" << hs.server_number << endl;
				
					if (TRACECLI) 
					{ 
						fout << "----------------------------------------" << endl;
						fout << "Send handshake C" << hs.client_number << " and S" << hs.server_number << endl; 
					}
				
					//check that the file is to be sent or received (GET or PUT)
					std::string directory;
					switch (hs.direction)
					{
						case GET:
							if ( ! recFile(sock, hs.filename, hostname, hs.client_number) )
								err_sys("An error occurred while receiving the file.");
							break;
						case PUT:
							if ( ! sendF(sock, hs.filename, hostname, hs.server_number) )
								err_sys("An error occurred while sending the file.");
							break;
						case LIST:
							if (!recDir(sock, directory))
								err_sys("Error receiving directory");
							cout << directory;
							break;
						default:
							break;
					}
				}
				else
				{
					err_sys("An error has occured while doing the handshake!");
					fout << "----------------------------------------" << endl;
					fout << "Error occured during handshake" << endl; 
				}
			}
		}
		
		//close the client socket connection
		cout << "Closing the connection between client and server." << endl << endl;		
		cout << "=================================================" << endl << endl;
		
		if (TRACECLI) 
		{ 
			fout << "----------------------------------------" << endl;
			fout << "Closing socket connection." << endl; 
			fout << "----------------------------------------" << endl;
		}		

		closesocket(sock);
		
	}
}

//print the error messages to screen 
void Client::err_sys(char * fmt,...)
{
	va_list args;
	va_start(args,fmt);
	fprintf(stderr,"\nERROR: ");
	vfprintf(stderr,fmt,args);
	fprintf(stderr,"\n\n");
	va_end(args);
}

//resolve the hostname
unsigned long Client::ResolveName(char name[])
{
	struct hostent *host;

	if ((host = gethostbyname(name)) == NULL)
		err_sys("gethostbyname() failed");
	
	return *((unsigned long *) host->h_addr_list[0]);
}

//constructor builds the timeouts
Client::Client(char * fn)
{
	timeouts.tv_sec = STIMER;
	timeouts.tv_usec = UTIMER;
	
	fout.open(fn);
} 

//descrutctor
Client::~Client()
{
	fout.close();

	WSACleanup();
}
void Client::resetTimeout()
{
	timeouts.tv_sec = STIMER;
	timeouts.tv_usec = UTIMER;
}