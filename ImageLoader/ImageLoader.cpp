#define _CRT_SECURE_NO_WARNINGS   
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")

#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <Windows.h>
#include <regex>
#include "Constants.h"
#include "Queue.h"
#include "ImageLink.h"

using namespace std;
SOCKET socketArr[bufSize];
string imageNameArr[bufSize];
fstream image[bufSize];
fd_set readfds;
int socketAddressNumber = -1;
char logFileName[fileNameLen];
CRITICAL_SECTION console;
CRITICAL_SECTION file;
Queue queue(8);

SOCKET establishConnection(string, string);
void download(int);
void WriteLogFile(string, int);
unsigned __stdcall downloadImg(void* pArg);
unsigned __stdcall runThreadPool(void* pArg);
void syncLog(CRITICAL_SECTION* section, string message, ostream* target);



int main()
{
	CreateDirectoryA(imgDir, 0);
	InitializeCriticalSection(&console);
	unsigned int qThreadID=0;
	string url = "";
	unsigned int numLink = 0;
	HANDLE threadsHandler = (HANDLE)_beginthreadex(NULL, 0, runThreadPool, NULL, 0, &qThreadID);
	while (true) {
		cin >> url;
		ImageLink* link = convertToImageLink(url, &socketAddressNumber, imageNameArr);
		socketArr[socketAddressNumber] = establishConnection(link->hostName, link->imagePath);
		FD_SET(socketArr[socketAddressNumber], &readfds);
		Queue::Element element = {qThreadID, numLink};
		bool rez = queue.append(&element, 5000);
		numLink++;
		if (!rez) {
			syncLog(&console, "failed to add new element to sync queue", &cout);
		}
	}
	system("pause");
	return 0;
}

unsigned __stdcall runThreadPool(void* pArg)
{
	while (true)
	{
		Queue::Element element = {};
		if (queue.remove(&element, INFINITE)) {
			HANDLE h = (HANDLE)_beginthreadex(NULL, 0, downloadImg, &element.nLink, 0, &element.nLink);
		}
	}
	return 0;
}

SOCKET establishConnection(string host, string imagePath)
{
	WSADATA wd;
	SOCKET sock;
	struct hostent* hostInfo;

	int result = WSAStartup(MAKEWORD(2, 2), &wd);
	if (result != 0) {
		EnterCriticalSection(&console);
		cerr << "Can not load WSDATA library: " << result << "\n";
		LeaveCriticalSection(&console);
		return result;
	}

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) {
		EnterCriticalSection(&console);
		cout << "Socket error : " << WSAGetLastError() << endl;
		LeaveCriticalSection(&console);
		return sock;
	}

	sockaddr_in sai = { AF_INET };
	sai.sin_family = AF_INET;

	hostInfo = gethostbyname(host.c_str());
	if (hostInfo == NULL) {
		EnterCriticalSection(&console);
		cout << "\nHostName： " << WSAGetLastError() << endl << "Incorrect link" << endl;
		LeaveCriticalSection(&console);
		return sock;
	}
	sai.sin_port = htons(80);
	memcpy(&sai.sin_addr, hostInfo->h_addr, 4);

	int check = connect(sock, (sockaddr*)&sai, sizeof(sai));
	if (check == SOCKET_ERROR) {
		EnterCriticalSection(&console);//вынести эти действия с критическими секциями в отдельный метод
		cout << "\nConnect error： " << WSAGetLastError() << endl;
		LeaveCriticalSection(&console);
		return sock;
	}

	string  reqInfo = "GET " + imagePath + " HTTP/1.1\r\nHost: " + host + "\r\nConnection:Close\r\n\r\n";

	int sendInfo = send(sock, reqInfo.c_str(), reqInfo.size(), 0);
	if (SOCKET_ERROR == sendInfo) {
		EnterCriticalSection(&console);
		cout << "\nSend error: " << WSAGetLastError() << endl;
		LeaveCriticalSection(&console);
		closesocket(sock);
		return sock;
	}
	return sock;
}

void download(int numLink) {
	char buf[1024];
	image[numLink].open(imgDirIn + imageNameArr[numLink], ios::out | ios::binary); //вынесим константы
	memset(buf, 0, sizeof(buf));
	int n = recv(socketArr[numLink], buf, sizeof(buf) - 1, 0);
	if (n == -1) {
		EnterCriticalSection(&console);
		cout << "\nError with socket data\n";
		LeaveCriticalSection(&console);
		return;
	}
	else {
		char* cpos = strstr(buf, "\r\n\r\n");
		image[numLink].write(cpos + strlen("\r\n\r\n"), n - (cpos - buf) - strlen("\r\n\r\n"));
		EnterCriticalSection(&file);
		WriteLogFile(imageNameArr[numLink], n);
		LeaveCriticalSection(&file);
	}

	while (true) {
		if (FD_ISSET(socketArr[numLink], &readfds)) {
			memset(buf, 0, sizeof(buf));
			n = recv(socketArr[numLink], buf, sizeof(buf) - 1, 0);
			if (n > 0) {
				image[numLink].write(buf, n);
				EnterCriticalSection(&file);
				WriteLogFile(imageNameArr[numLink], n);
				LeaveCriticalSection(&file);
			}
			else if (n == 0) {
				FD_CLR(socketArr[numLink], &readfds);
				image[numLink].close();
				EnterCriticalSection(&file);
				WriteLogFile(imageNameArr[numLink], n);
				LeaveCriticalSection(&file);
				EnterCriticalSection(&console);
				cout << "<------------file \"" + imageNameArr[numLink] + "\" is download------------>" << endl;
				LeaveCriticalSection(&console);
				break;
			}
			else if (n == -1) {
				EnterCriticalSection(&console);
				cout << "\nError with socket data\n";
				LeaveCriticalSection(&console);
			}
		}
	}
}

void WriteLogFile(string file, int bites)
{
	time_t seconds = time(NULL);
	tm* timeinfo = localtime(&seconds);
	char buffTime[] = "%H.%M.%S";
	char timeForAnswer[100];
	strftime(timeForAnswer, 80, buffTime, timeinfo);

	char answer_char[200];
	string answer;

	FILE* fileToSave = fopen(logFileName, "a");

	if (bites > 0) {
		answer = "File: " + file + ", Bites: " + to_string(bites) + ", Time: " + (string)timeForAnswer;
	}
	else if (bites < 0) {
		answer = "File: " + file + ", ERROR!" + ", Time: " + (string)timeForAnswer;
	}
	else {
		answer = "<---" + file + " is download" + "--->" + " Time: " + (string)timeForAnswer;
	}

	fclose(fileToSave);
}

unsigned __stdcall downloadImg(void* pArg)
{
	unsigned int* numPtr = (unsigned int*)pArg;
	unsigned int numLink = *numPtr;
	char buf[10240];
	image[numLink].open(imgDirIn + imageNameArr[numLink], ios::out | ios::binary); //вынесим константы
	memset(buf, 0, sizeof(buf));
	int n = recv(socketArr[numLink], buf, sizeof(buf) - 1, 0);
	if (n == -1) {
		EnterCriticalSection(&console);
		cout << "\nError with socket data\n";
		LeaveCriticalSection(&console);
		_endthreadex(0);
		return 0;
	}
	else {
		char* cpos = strstr(buf, "\r\n\r\n");
		image[numLink].write(cpos + strlen("\r\n\r\n"), n - (cpos - buf) - strlen("\r\n\r\n"));
		EnterCriticalSection(&file);
		WriteLogFile(imageNameArr[numLink], n);
		LeaveCriticalSection(&file);
	}

	while (true) {
		if (FD_ISSET(socketArr[numLink], &readfds)) {
			memset(buf, 0, sizeof(buf));
			n = recv(socketArr[numLink], buf, sizeof(buf) - 1, 0);
			if (n > 0) {
				image[numLink].write(buf, n);
				EnterCriticalSection(&file);
				WriteLogFile(imageNameArr[numLink], n);
				LeaveCriticalSection(&file);
			}
			else if (n == 0) {
				FD_CLR(socketArr[numLink], &readfds);
				image[numLink].close();
				EnterCriticalSection(&file);
				WriteLogFile(imageNameArr[numLink], n);
				LeaveCriticalSection(&file);
				EnterCriticalSection(&console);
				cout << "<------------file \"" + imageNameArr[numLink] + "\" is download------------>" << endl;
				LeaveCriticalSection(&console);
				break;
			}
			else if (n == -1) {
				EnterCriticalSection(&console);
				cout << "\nError with socket data\n";
				LeaveCriticalSection(&console);
			}
		}
	}
}

void syncLog(CRITICAL_SECTION* section, string message, ostream* target) {
	EnterCriticalSection(section);
	*target << message;
	LeaveCriticalSection(section);
}
