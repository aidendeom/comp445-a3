#include "server.h"
#include <dirent.h>
#include <utils.h>
#include <sstream>
using namespace std;

//return the size of the file
long GetFileSize(char * filename)
{
	int result;
	struct _stat stat_buf;
	result = _stat(filename, &stat_buf);
	if (result != 0) return 0; 
	return stat_buf.st_size;
}

//sned the file to client during a GET request 
bool Server::sendF(int sock, char * filename, char * sendHost, int cliNum)
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
	int seqNum = cliNum % 2;
	
	if (TRACESER)
		fout << "The sender has started on " << sendHost << endl;
	
	FILE * stream;
	fopen_s(&stream, filename, "rb");

	if (stream != NULL)
	{
		bytes_counter = GetFileSize(filename);
		while (1)
		{ 
			if ( bytes_counter > MAX_FSIZE)
			{
				frame.header = ( isFirstPacket ? INITIAL_DATA : DATA) ;
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
			frame.snwseq = seqNum;
			
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

				cout << endl << "Sent the frame " << seqNum << endl;
				if (TRACESER)
					fout << "Sent frame " << seqNum << endl;

				if (isFinished && (nTries > MAX_RETRY))
				{ 
					isMaxAttempts = true;
					break;
				}

			} while (recAck(sock, &ack) != IN_PKT || ack.number != seqNum );
	
			if (isMaxAttempts)
			{
				cout << "The sender has not received ACK " << seqNum << " after " << MAX_RETRY << " tries. The transfer has finished." << endl;
				if (TRACESER) 
					fout << "Sender has not received ACK " << seqNum << " after " << MAX_RETRY << " tries. The transfer has finished." << endl;
			}
			else
			{
				cout << "The sender has received ACK " << ack.number << endl;
				if (TRACESER)
					fout << "Sender received ACK " << ack.number << endl;
			}
			isFirstPacket = false;
			seqNum = (seqNum == 0 ? 1 : 0);
			if (isFinished) 
				break;
		}
		
		fclose( stream );

		cout << endl << "The file transfer is complete." << endl;
		cout << "The number of packets sent: " << packetsSent << endl;
		cout << "The number of packets sent with no duplicates: " << packetsSentNeeded << endl;
		cout << "The number of bytes that have been sent: " << bytes_sent << endl;
		cout << "The number of bytes that have been read: " << bytes_read_total << endl << endl;
		
		if (TRACESER) 
		{ 
			fout << endl << "The file transfer is complete." << endl;
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
        if (TRACESER) { fout << "Problem with opening the requested file" << endl; }
		return false;		
	}
}

int Server::sendDir(SOCKET sock, int server_number)
{
	string directory(getDirectoryItems());
	stringstream ss(directory);

	Msg frame;
	Ack ack;
	ack.packet_type = FRAME_ACK;

	long bytes_count = 0;

	size_t numberOfFrames = directory.length() / MAX_FSIZE + 1;
	size_t remainder = directory.length() % MAX_FSIZE;
	
	frame.snwseq = server_number;

	for (size_t i = 1; i <= numberOfFrames; i++)
	{
		size_t length = i == numberOfFrames ? remainder : MAX_FSIZE;
		ss.read(frame.buffer, length);
		frame.buffer_length = length;
		
		if (i == 1)
			frame.header = INITIAL_DATA;
		else if (i == numberOfFrames)
			frame.header = FINAL_DATA;
		else
			frame.header = DATA;

		frame.packet_type = FRAME;
		frame.snwseq++;

		RecResult result = RecResult::TIMEOUT;

		while (result == RecResult::TIMEOUT)
		{
			bytes_count += sendFrame(sock, &frame);
			result = recAck(sock, &ack);
		}

		if (result == RecResult::REC_ERR)
		{
			err_sys("Failure sending directory to client");
			return 0;
		}
	}

	return bytes_count;
}

//receives the file during a client PUT
bool Server::recFile(int sock, char * filename, char * recHost, int servNum)
{
	Msg frame;
	Ack ack; 
	ack.packet_type = FRAME_ACK;

	long byte_count = 0;
	int packetsSent = 0, packetsSentNeeded = 0;
	int bytes_received = 0, bytes_written = 0, bytes_written_total = 0;
	int seqNum = servNum % 2;

	if (TRACESER) { fout << "Receiver started on " << recHost << endl; }
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
				cout << "The server has received handshake C" << hs.client_number << " and S" << hs.server_number << endl;
				if (TRACESER)
					fout << "Server received handshake C" << hs.client_number << " and S" << hs.server_number << endl; 
			}
			else if (frame.packet_type == FRAME)
			{
				cout << "The server has received frame " << (int)frame.snwseq << endl;
				if (TRACESER) 
					fout << "Server received frame " << (int)frame.snwseq << endl; 
				if ( (int)frame.snwseq != seqNum )
				{
					ack.number = (int)frame.snwseq;
					if ( sendAck(sock, &ack) != sizeof(ack) )
						return false;
					cout << "The server has sent ACK " << ack.number << " again" << endl;
					if (TRACESER)
						fout << "Server sent ACK " << ack.number << " again" << endl; 
					packetsSent++;
				}
				else
				{
					ack.number = (int)frame.snwseq;
					if ( sendAck(sock, &ack) != sizeof(ack) )
						return false;
					
					cout << "The server has sent ack " << ack.number << endl;
					if (TRACESER)
						fout << "Server sent ack " << ack.number << endl;
					
					packetsSent++;
					packetsSentNeeded++;

					byte_count = frame.buffer_length;
					bytes_written = fwrite(frame.buffer, sizeof(char), byte_count, stream );
					bytes_written_total += bytes_written;
					
					seqNum = (seqNum == 0 ? 1 : 0);
					
					if (frame.header == FINAL_DATA)
						break;
				}
			}
		}
		
		fclose( stream );

		cout << "The file transfer is complete" << endl;
		cout << "The number of packets sent: " << packetsSent << endl;
		cout << "The number of packets sent with no duplicates: " << packetsSentNeeded << endl;
		cout << "The number of bytes that have been received: " << bytes_received << endl;
		cout << "The number of bytes that have been written: " << bytes_written_total << endl << endl;

		if (TRACESER) 
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
		cout << "There was an issue with opening the file..." << endl;
        if (TRACESER) { fout << "Issue with opening the file." << endl; }
		return false;
	}
}

//receives the responses from a client
RecResult Server::recResp(int sock, TWHandshake * ptr_handshake)
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
			bytes_recvd = recvfrom(sock, (char *)ptr_handshake, sizeof(*ptr_handshake),0, (struct sockaddr*)&sa_in, &sa_in_size);
			return IN_PKT; 
			break;
		default:
			return REC_ERR; 
			break;
	}
}

//receives frames of data from client during PUT
RecResult Server::recFrame(int sock, Msg * ptr_message_frame)
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
			return REC_ERR; 
			break;
	}
}

//send back an ack for frame being received 
RecResult Server::recAck(int sock, Ack * ack)
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
			return REC_ERR; 
			break;
	}
}

//send a handshake to the client
int Server::sendReq(int sock, TWHandshake * ptr_handshake, struct sockaddr_in * sa_in)
{
	int bytes_sent = sendto(sock, (const char *)ptr_handshake, sizeof(*ptr_handshake), 0, (struct sockaddr *)sa_in, sizeof(*sa_in));
	return bytes_sent;
}

//send the file to the client during a GET
int Server::sendFrame(int sock, Msg * ptr_message_frame)
{
	int bytes_sent = sendto(sock, (const char*)ptr_message_frame, sizeof(*ptr_message_frame), 0, (struct sockaddr*)&sa_in, sizeof(sa_in));
	return bytes_sent;
}

//send an ACK during a client PUT 
int Server::sendAck(int sock, Ack * ack)
{
	int bytes_sent = sendto(sock, (const char*)ack, sizeof(*ack), 0, (struct sockaddr*)&sa_in, sizeof(sa_in));
	return bytes_sent;
}

void Server::run()
{
	if (WSAStartup(0x0202,&wsadata) != 0)
	{  
		WSACleanup();  
	    err_sys("Error in starting WSAStartup()\n");
	}
	
	//get the name of the server 
	if(gethostname(server_name, HNAME_LENGTH)!=0)
		err_sys("Server gethostname() error.");
	

	printf("Server Program\n");
	printf("========================================================\n");
	printf("The server is running on host [%s].\n", server_name);
	printf("Waiting for connections and commands...\n");
	printf("========================================================\n");

	//create the socket
	if ( (sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 )
		err_sys("socket() failed");
	
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(SERVER_PORT);
	sa_in_size = sizeof(sa_in);

	//bind to the server socket 
	if (bind(sock, (LPSOCKADDR)&sa, sizeof(sa) ) < 0)
		err_sys("Socket binding error");
	
	while (true)
	{
		resetTimeout();
			
		//loop until the client responds to it 
		while ( recResp(sock, &hs) != IN_PKT || hs.type != CLIENT_REQ ) {;}

		//confirm that the server has received a handshake
		cout << "The server has received handshake C" << hs.client_number << endl;
		if (TRACESER)
			fout << "Server received handshake C" << hs.client_number << endl;

		//check to see if it's a GET/PUT or INVALID handshake
		if ( hs.direction == GET )
		{
			cout << "The user \"" << hs.username << "\" from hostname \"" << hs.hostname << "\" is requesting the file: \"" << hs.filename << "\" as a GET command" << endl;
			if (TRACESER)
				fout << "User \"" << hs.username << "\" from hostname \"" << hs.hostname << "\" needs file \"" << hs.filename << "\" through GET request" << endl;

			//if the file does exist
			if( _access(hs.filename, 0) != -1)
				hs.type = ACK_CNUM;//send server handshake back to client as an ACK
			else
			{
				hs.type = FILE_NE;
				cout << "The file that has been requested does not exist!" << endl;	
				if (TRACESER) { fout << "File does not exist" << endl; }
			}
		}
		
		else if ( hs.direction == PUT )
		{
			cout << "The user \"" << hs.username << "\" from hostname \"" << hs.hostname << "\" is requesting the file: \"" << hs.filename << "\" as a PUT command" << endl;
			if (TRACESER)
				fout << "The user \"" << hs.username << "\" from hostname \"" << hs.hostname << "\" needs file \"" << hs.filename << "\" through PUT request" << endl;
			hs.type = ACK_CNUM;
		}
		else
		{
			hs.type = INVALID;

			cout << "The request is invalid!" << endl;	
			if (TRACESER) { fout << "Request is invalid." << endl; }
		}

		if (hs.type != ACK_CNUM) //if the handshake is an error 
		{
			if ( sendReq(sock, &hs, &sa_in) != sizeof(hs) )
				err_sys("Error in sending packet.");

			cout << "There was an error with communicating with client."<< endl;
			if (TRACESER) { fout << "Error with sending msgs to client"<< endl; }
		}
		
		else if (hs.type == ACK_CNUM) // if the handshake is ACK from client
		{
			//generate a random number for the server 
			srand((unsigned)time(NULL));
			random = rand() % MAX_RAND;
			hs.server_number = random;
			
			//send the handshake response and expect a valid response 
			if ( sendReq(sock, &hs, &sa_in) != sizeof(hs) )
				err_sys("Error in sending packet.");

			cout << "The server has sent handshake C" << hs.client_number << " and S" << hs.server_number << endl;
			if (TRACESER) { fout << "Server sent handshake C" << hs.client_number << " and S" << hs.server_number << endl; }

			resetTimeout();
			 
			RecResult result = recResp(sock, &hs);
			
			if(result == REC_ERR)
			{
				err_sys("An error has occured");
				
				if (TRACESER) 
				{ 
					fout << "----------------------------------------" << endl;
					fout << "Select() returned an error." << endl; 
				}
			}
			else if(result == TIMEOUT)
			{
				err_sys("Select() timed out.");
				
				if (TRACESER) 
				{ 
					fout << "----------------------------------------" << endl;
					fout << "Select() timed out." << endl; 
				}
			}
			else //incoming packet
			{
				//confirm that the server has 3-way with client 
				cout << "The server has received handshake C" << hs.client_number << " and S" << hs.server_number << endl;
				if (TRACESER)
					fout << "Server received handshake C" << hs.client_number << " and S" << hs.server_number << endl;

				//if an ACK to this server 
				if (hs.type == ACK_SNUM)
				{
					//check if PUT or GET 
					switch (hs.direction)
					{
						case GET:
							if ( ! sendF(sock, hs.filename, server_name, hs.client_number) )
								err_sys("An error occurred while sending the file.");	
							break;

						case PUT:
							if ( ! recFile(sock, hs.filename, server_name, hs.server_number) )
								err_sys("An error occurred while receiving the file.");	
							break;

						default:
							break;
					}
				}
				else
				{
					cout << "Handshake error!" << endl;
					if (TRACESER) { fout << "handshake error!" << endl; }
				}
			}
		}

	}
}

//output error messages to the screen 
void Server::err_sys(char * fmt,...)
{
	va_list args;
	va_start(args,fmt);
	fprintf(stderr,"\nERROR: ");
	vfprintf(stderr,fmt,args);
	fprintf(stderr,"\n\n");
	va_end(args);
}

//resolve the host name 
unsigned long Server::ResolveName(char name[])
{
	struct hostent *host;
	
	if ((host = gethostbyname(name)) == NULL)
		err_sys("gethostbyname() failed");

	return *((unsigned long *) host->h_addr_list[0]);
}

//set the timeout used by select() (300 ms)
Server::Server(char * fn)
{
	timeouts.tv_sec = STIMER;
	timeouts.tv_usec = UTIMER;
	
	fout.open(fn);
} 

//destructor
Server::~Server()
{
	fout.close();
	
	WSACleanup();
}

void Server::resetTimeout()
{
	timeouts.tv_sec = STIMER;
	timeouts.tv_usec = UTIMER;
}