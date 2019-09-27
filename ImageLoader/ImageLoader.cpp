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

using namespace std;
SOCKET socketArr[bufSize];
string imageNameArr[bufSize];
fstream image[bufSize];
fd_set readfds;
int socketAddressNumber = -1;
char logFileName[fileNameLen];
CRITICAL_SECTION console;
CRITICAL_SECTION file;

SOCKET establishConnection(string, string);
void download(int);
void WriteLogFile(string, int);

class ImageLink {
public: string hostName;
public: string imagePath;

	  ImageLink(string hostName, string imagePath)
	  {
		  this->hostName = hostName;
		  this->imagePath = imagePath;
	  }
};

ImageLink* convertToImageLink(string url);

int main()
{
	time_t seconds = time(NULL);
	tm* timeinfo = localtime(&seconds);
	char format[] = "%d %H %M %S.txt";
	strftime(logFileName, 80, format, timeinfo);
	CreateDirectoryA(imgDir, 0);
	string url;
	int numLink = 0;

	InitializeCriticalSection(&console);
	InitializeCriticalSection(&file);

	vector<thread> threads;//переделать, использовать не гоотовый объект(Джефри Лихтер Windows для проффесионалов)

	/*cout << "Choose one:" << endl
		 << "1)Input image url" << endl
		 << "2)Run test on pre-prepared urls" << endl
		 << "3)Exit" << endl;
	while (true) {
		cin >> numberOfChoose;
		if (numberOfChoose == 1) {
			cin >> url;
			ImageLink* imageLink = convertToImageLink(url);
			socketArr[socketAddressNumber] = establishConnection(imageLink -> hostName, imageLink->imagePath);
			FD_SET(socketArr[socketAddressNumber], &readfds);
			threads.push_back(thread(download, numLink));
			numLink++;
		}
		else if (numberOfChoose == 2) {
			ifstream link("test.txt");
			while (getline(link, url)) {
				ImageLink* imageLink = convertToImageLink(url);
				socketArr[socketAddressNumber] = establishConnection(imageLink->hostName, imageLink->imagePath);
				FD_SET(socketArr[socketAddressNumber], &readfds);
				threads.push_back(thread(download, numLink));
				numLink++;
			}
			link.close();
		}
		else if (numberOfChoose == 3) {
			break;
		}
	}*/
	while (true) {
		cin >> url;
		ImageLink* imageLink = convertToImageLink(url);
		socketArr[socketAddressNumber] = establishConnection(imageLink->hostName, imageLink->imagePath);
		FD_SET(socketArr[socketAddressNumber], &readfds);
		threads.push_back(thread(download, numLink));
		numLink++;
	}
	for (int numLink = 0; numLink <= threads.size() - 1; numLink++) {
		threads[numLink].join();
	}


	DeleteCriticalSection(&file);
	DeleteCriticalSection(&console);
	system("pause");
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
	char buf[10240];

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

ImageLink* convertToImageLink(string url) {

	char temp[100];

	regex pathPattern(pathRegex);
	string imagePath = regex_replace(url, pathPattern, "$3");

	regex hostPattern(hostRegex);
	string hostName = regex_replace(url, hostPattern, "$2");

	regex namePattern(pathRegex);
	string addressUrl = regex_replace(url, namePattern, "$2");

	url = addressUrl;

	socketAddressNumber++;
	imageNameArr[socketAddressNumber] = url;
	int lenght = url.length();

	for (int j = 0; j < lenght; j++) {
		temp[j] = url[j];
		if (temp[j] == '/') {
			imageNameArr[socketAddressNumber][j] = '.';
		}
		else {
			imageNameArr[socketAddressNumber][j] = url[j];
		}
	}
	return new ImageLink(hostName, imagePath);
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