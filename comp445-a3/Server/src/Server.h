#pragma once
#pragma comment(lib,"wsock32.lib")
#include <winsock.h>
#include <iostream>
#include <fstream>
#include <windows.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <process.h>

#include  <io.h>
#include  <stdlib.h>

#define STIMER 0
#define UTIMER 300000

#define SERVER_PORT 5001
#define REMOTE_PORT 7001

#define TRACESER 1

#define MAX_RETRY 10

#define INPUT_LENGTH    40
#define HNAME_LENGTH 40
#define UNAME_LENGTH 40
#define FNAME_LENGTH 40
#define MAX_FSIZE 256
#define MAX_RAND 256;
#define SEQ_WIDTH 1

typedef enum 
{ 
	GET=1, 
	PUT,
	LIST
} Direction;

//result of the client
typedef enum 
{ 
	TIMEOUT=1, 
	IN_PKT, 
	REC_ERR
} RecResult;

//what type of handshake
typedef enum 
{ 
	CLIENT_REQ=1, 
	ACK_CNUM, 
	ACK_SNUM, 
	FILE_NE, 
	INVALID 
} HandshakeType;

//what type of data
typedef enum 
{ 
	INITIAL_DATA=1, 
	DATA, 
	FINAL_DATA 
} MsgHead;

//struct for the packet
typedef enum 
{ 
	HANDSHAKE=1, 
	FRAME, 
	FRAME_ACK 
} PktType;

//ack struct 
typedef struct 
{ 
	PktType packet_type;
	int number;
} Ack;

//structure of msg with data 
typedef struct 
{
	PktType packet_type;
	MsgHead header;
	unsigned int snwseq:SEQ_WIDTH;
	int buffer_length;
	char buffer[MAX_FSIZE];
} Msg;

//struct of the three way handshake
typedef struct 
{
	PktType packet_type;
	HandshakeType type;
	Direction direction;
	int client_number;
	int server_number;
	char hostname[HNAME_LENGTH];
	char username[UNAME_LENGTH];
	char filename[FNAME_LENGTH];
} TWHandshake;

class Server
{
	int sock;
	struct sockaddr_in sa;
	struct sockaddr_in sa_in;
	int sa_in_size;
	char server_name[HNAME_LENGTH];
	struct timeval timeouts;
	int random;
	WSADATA wsadata;

	TWHandshake hs;

private:
	std::ofstream fout;

public: 
	Server(char *fn="serverLog.txt");
	~Server();
	void run();

	bool sendF(int, char *, char *, int);
	int sendDir(SOCKET sock);
	int sendReq(int, TWHandshake *, struct sockaddr_in *);
	int sendFrame(int, Msg *);
	int sendAck(int, Ack *);

	bool recFile(int, char *, char *, int);
	RecResult recResp(int, TWHandshake *);
	RecResult recFrame(int, Msg *);
	RecResult recAck(int, Ack *);

	unsigned long ResolveName(char name[]);
    void err_sys(char * fmt,...);

	void resetTimeout();
};
