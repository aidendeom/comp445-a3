#pragma once
#pragma comment(lib,"wsock32.lib")
#include <winsock.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <windows.h>
#include <time.h>
#include  <io.h>
#include  <stdlib.h>


#define STIMER 0
#define UTIMER 300000

#define CLIENT_PORT 5000
#define REMOTE_PORT 7000

#define TRACECLI 1

#define MAX_RETRY 10

#define INPUT_LENGTH 40
#define HNAME_LENGTH 40
#define UNAME_LENGTH 40
#define FNAME_LENGTH 40
#define MAX_FSIZE 256
#define MAX_RAND 256;
#define SEQ_WIDTH 1

typedef enum 
{ 
	GET=1, 
	PUT 
} Direction;

//types of results
typedef enum
{ 
	TIMEOUT=1, 
	IN_PKT, 
	RECEIVE_ERROR
} RecRes;

//types of handshakes
typedef enum 
{ 
	CLIENT_REQ=1, 
	ACK_CNUM, 
	ACK_SNUM, 
	FILE_NE, 
	INVALID 
} HandshakeType;

//the type of data being sent
typedef enum
{ 
	INITIAL_DATA=1, 
	DATA, 
	FINAL_DATA 
} MsgHead;

//what type of packet is being received 
typedef enum 
{ 
	HANDSHAKE=1, 
	FRAME, 
	FRAME_ACK 
} PktType;

//structure for ACKS
typedef struct 
{ 
	PktType packet_type;
	int number;
} Ack;

//message frame for data
typedef struct 
{
	PktType packet_type;
	MsgHead header;
	unsigned int snwseq:SEQ_WIDTH;
	int buffer_length;
	char buffer[MAX_FSIZE];
} Msg;

//structure of the three way handshake
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

class Client
{
    int sock;
	struct sockaddr_in sa;
	struct sockaddr_in sa_in;
	int sa_in_size;
	struct timeval timeouts;
	TWHandshake hs;
	int random;
	WSADATA wsadata;

private:
	std::ofstream fout;

public:
	Client(char *fn="clientLog.txt");
	~Client();
    void run();	

	bool sendF(int, char *, char *, int);
	int sendReq(int, TWHandshake *, struct sockaddr_in *);
	int sendFrame(int, Msg *);
	int sendAck(int, Ack *);

	bool recFile(int, char *, char *, int);
	RecRes recResp(int, TWHandshake *);
	RecRes recFrame(int, Msg *);
	RecRes recAck(int, Ack *);

	unsigned long ResolveName(char name[]);
    void err_sys(char * fmt,...);

	void resetTimeout();
};
