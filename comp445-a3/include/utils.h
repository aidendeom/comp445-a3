#pragma once

#include "dirent.h"
#include <string>
#include <sstream>
#include <fstream>
#include <WinBase.h>
#include <winsock.h>
#include <iostream>

void intToChars(unsigned char bytes[], unsigned int n)
{
	bytes[0] = (n >> 24) & 0xFF;
	bytes[1] = (n >> 16) & 0xFF;
	bytes[2] = (n >> 8) & 0xFF;
	bytes[3] = n & 0xFF;
}

int charsToInt(unsigned char bytes[])
{
	int result = 0;
	for (unsigned int n = 0; n < sizeof(result); n++)
	{
		result = (result << 8) + bytes[n];
	}
	return result;
}

char * getExePath()
{
	char *buffer = new char[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, buffer);

	return buffer;
}

std::string getDirectoryItems()
{
	DIR *dir;
	struct dirent *ent;
	std::stringstream ss;

	char *dirStr = getExePath();

	if ((dir = opendir(dirStr)) != NULL)
	{
		/* print all the files and directories within directory */
		while ((ent = readdir(dir)) != NULL)
		{
			ss << " " << ent->d_name << "\n";
		}
		closedir(dir);
	}
	else
	{
		delete[] dirStr;
		/* could not open directory */
		perror("");
		return "";
	}
	delete[] dirStr;
	return ss.str();
}

std::string appendCopy(const std::string &str)
{
	int dotloc = str.find_last_of(".");

	std::string sub = str.substr(0, dotloc);
	std::string ext = str.substr(dotloc);

	return sub.append("_copy").append(ext);
}

std::string appendCopy(const char filename[])
{
	std::string str(filename);
	return appendCopy(str);
}

bool fileExists(const std::string& name)
{
	std::ifstream test(name.c_str());
	if (test.good())
	{
		test.close();
		return true;
	}
	test.close();
	return false;
}

int getFileSize(SOCKET sock)
{
	unsigned char directorySize[4];

	// Wait for file size
	if ((recv(sock, (char*)directorySize, 4, 0)) == SOCKET_ERROR)
		throw "Recieve file size failed\n";

	return charsToInt(directorySize);
}

void sendFileSize(SOCKET sock, unsigned int size)
{
	unsigned char filesize[4];
	intToChars(filesize, size);

	if (send(sock, (const char*)filesize, 4, 0) == SOCKET_ERROR)
		throw "Send file size failed\n";
}

void recvFile(SOCKET sock, const char filename[], unsigned int filesize, char buffer[], size_t bufferSize)
{
	std::ofstream file(filename, std::ios::binary | std::ios::trunc);

	int numBuffs = (filesize / bufferSize) + 1;
	int remainder = (filesize % bufferSize);

	for (int i = 0; i < numBuffs; i++)
	{
		int length = i == numBuffs - 1 ? remainder : bufferSize;

		if (recv(sock, buffer, length, 0) == SOCKET_ERROR)
			throw "Receive error in server program\n";

		file.write(buffer, length);
	}

	file.close();
}

void prompt(const char *str, char *buffer, size_t bufferlen)
{
	std::cout << str << std::flush;
	std::cin.getline(buffer, bufferlen);
	std::cout << std::endl << std::flush;
}

int readFileSize(std::string filename)
{
	std::ifstream file(filename, std::ios::ate);
	return (int)file.tellg();
}

struct Packet
{
	static const size_t DATA_LENGTH{ 1024 };

	bool syn;
	bool control;
	int seqNo;
	int ackNo;
	size_t length;
	char data[DATA_LENGTH];

	Packet() :
		syn{ false },
		control{ false },
		seqNo{ 0 },
		ackNo{ 0 },
		length{ 0 }
	{
		memset(data, '\0', DATA_LENGTH);
	}

	~Packet() {};
};