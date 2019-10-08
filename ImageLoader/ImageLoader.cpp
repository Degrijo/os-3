#define _CRT_SECURE_NO_WARNINGS   
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_NO_VA_START_VALIDATION
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
SOCKET socketArr[BUFF_SIZE];
string imageNameArr[BUFF_SIZE];
fstream image[BUFF_SIZE];
fd_set readfds;
int socketAddressNumber = -1;
char logFileName[FILE_NAME_LENGTH];
CRITICAL_SECTION console;
CRITICAL_SECTION file;
Queue queue(6);

SOCKET establishConnection(string, string);
unsigned __stdcall downloadImg(void* pArg);
string format(const string& format, ...);
unsigned __stdcall runThreadPool(void* pArg);
void syncLog(CRITICAL_SECTION* section, string message, ostream* target);
void runTests();

int main(int argc, char* argv[]) {
	time_t t = time(0);
	struct tm* now = localtime(&t);
	strftime(logFileName, 80, LOG_FILE_PATTERN, now);
	CreateDirectoryA(IMAGE_DIRECTORY, 0);
	InitializeCriticalSection(&console);
	InitializeCriticalSection(&file);
	unsigned int qThreadID=0;
	string url = "";
	unsigned int numLink = -1;
	if (strcmp(argv[1], "test") == 0) {
		runTests();
		return 0;
	}
	HANDLE threadsHandler = (HANDLE)_beginthreadex(NULL, 0, runThreadPool, NULL, 0, &qThreadID);
	while (true) {
		cin >> url;
		ImageLink* link = convertToImageLink(url, &socketAddressNumber, imageNameArr);
		socketArr[socketAddressNumber] = establishConnection(link->hostName, link->imagePath);
		FD_SET(socketArr[socketAddressNumber], &readfds);
		numLink += 1;
		Queue::Element element = {qThreadID, numLink};
		bool rez = queue.put(&element, 5000);
		if (!rez) {
			syncLog(&console, ADD_ELEMENT_IN_QUEUE_EXCEPTION_MESSAGE, &cout);
		}
	}
	system("pause");
	return 0;
}

void testLinks() {
	unsigned int qThreadID = 0;
	HANDLE threadsHandler = (HANDLE)_beginthreadex(NULL, 0, runThreadPool, NULL, 0, &qThreadID);
	
	string url = "";
	unsigned int numLink = -1;
	vector<string> testCase{
"http://localhost/1.png",
"http://localhost/2.png",
"http://localhost/3.png",
"http://localhost/4.png",
"http://localhost/5.png",
"http://localhost/6.png" };
	for (std::vector<string>::iterator it = testCase.begin(); it != testCase.end(); it++) {
		ImageLink* link = convertToImageLink(url, &socketAddressNumber, imageNameArr);
		socketArr[socketAddressNumber] = establishConnection(link->hostName, link->imagePath);
		FD_SET(socketArr[socketAddressNumber], &readfds);
		numLink += 1;
		Queue::Element element = { qThreadID, numLink };
		bool rez = queue.put(&element, 5000);
		if (!rez) {
			syncLog(&console, ADD_ELEMENT_IN_QUEUE_EXCEPTION_MESSAGE, &cout);
		}
	}
}

void runTests() {
	testLinks();
}


unsigned __stdcall runThreadPool(void* pArg)
{
	while (true)
	{
		Queue::Element element = {};
		if (queue.pull(&element, INFINITE)) {
			unsigned int* arg;
			arg = (unsigned int*)malloc(sizeof(unsigned int));
			*arg = element.nLink;
			HANDLE h = (HANDLE)_beginthreadex(NULL, 0, downloadImg, (void*)arg, 0, &element.nLink);
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
		syncLog(&console, WSDATA_LOAD_EXCEPTION_MESSAGE, &cout);
		return result;
	}

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) {
		syncLog(&console, SOCKET_INITIALIZATION_EXCEPTION_MESSAGE, &cout);
		return sock;
	}

	sockaddr_in sai = { AF_INET };
	sai.sin_family = AF_INET;

	hostInfo = gethostbyname(host.c_str());
	if (hostInfo == NULL) {
		syncLog(&console, INCORRECT_LINK_EXCEPTION_MESSAGE, &cout);
		return sock;
	}
	sai.sin_port = htons(80);
	memcpy(&sai.sin_addr, hostInfo->h_addr, 4);

	int check = connect(sock, (sockaddr*)&sai, sizeof(sai));
	if (check == SOCKET_ERROR) {
		syncLog(&console, CONNECTION_EXCEPTION_MESSAGE, &cout);
		return sock;
	}

	char* requestMessage = new char[sizeof(imagePath) + sizeof(host) + REQUEST_MESSAGE_LENGTH];
	sprintf(requestMessage, REQUEST_MESSAGE, imagePath.c_str(), host.c_str());
	int sendInfo = send(sock, requestMessage, strlen(requestMessage), 0);
	if (SOCKET_ERROR == sendInfo) {
		syncLog(&console, SEND_REQUEST_EXCEPTION_MESSAGE, &cout);
		closesocket(sock);
		return sock;
	}
	delete(requestMessage);
	return sock;
}

string generateMessageOfImageDownoloading(string image, int bites) {
	time_t seconds = time(NULL);
	tm* timeinfo = localtime(&seconds);
	char timeForAnswer[100];
	strftime(timeForAnswer, 80, TIME_TEMPLATE_IN_FILE, timeinfo);

	char answer_char[200];
	string answer;

	if (bites > 0) {
		char message[1000];
		sprintf(message, IMAGE_STILL_DOWNLOADING_MESSAGE, image.c_str(), bites, ((string)timeForAnswer).c_str());
		answer = message;
	}
	else if (bites < 0) {
		char message[1000];
		sprintf(message, IMAGE_NOT_DOWNLOADING_MESSAGE, image.c_str(), ((string)timeForAnswer).c_str());
		answer = message;
	}
	else {
		char message[1000];
		sprintf(message, IMAGE_DOWNLOAD_SUCCESSFULLY_MESSAGE, image.c_str(), ((string)timeForAnswer).c_str());
		answer = message;
	}

	for (int j = 0; j < answer.length(); j++) {
		answer_char[j] = answer[j];
	}

	answer_char[answer.length()] = '\0';
	return answer_char;
}

void writeToLogFile(string stringToWrite) //формат лог файла переделать
{
	EnterCriticalSection(&file);
	FILE* fileToSave = fopen(logFileName, "a");
	fprintf(fileToSave, "%s\n", stringToWrite.c_str());
	fflush(fileToSave);
	fclose(fileToSave);
	LeaveCriticalSection(&file);
}

unsigned __stdcall downloadImg(void* pArg)
{
	unsigned int numPtr = *((int*)pArg);
	unsigned int numLink = numPtr;

	int threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
	string message = format(START_DOWNOLOAD_IMAGE_MESSAGE, threadId);
	writeToLogFile(message);

	char buf[10240];
	image[numLink].open(IMAGE_DIRECTORY + imageNameArr[numLink], ios::out | ios::binary); 
	memset(buf, 0, sizeof(buf));
	int n = recv(socketArr[numLink], buf, sizeof(buf) - 1, 0);
	if (n == -1) {
		syncLog(&console, START_DOWNOLOAD_IMAGE_MESSAGE, &cout);
		_endthreadex(0);
		return 0;
	}
	else {
		char* cpos = strstr(buf, RESPONSE_DELIMETER);
		image[numLink].write(cpos + strlen(RESPONSE_DELIMETER), n - (cpos - buf) - strlen(RESPONSE_DELIMETER));
		string message = generateMessageOfImageDownoloading(imageNameArr[numLink], n);
		writeToLogFile(message);
	}

	while (true) {
		if (FD_ISSET(socketArr[numLink], &readfds)) {
			memset(buf, 0, sizeof(buf));
			n = recv(socketArr[numLink], buf, sizeof(buf) - 1, 0);
			if (n > 0) {
				image[numLink].write(buf, n);
				string message = generateMessageOfImageDownoloading(imageNameArr[numLink], n);
				writeToLogFile(message);
			}
			else if (n == 0) {
				FD_CLR(socketArr[numLink], &readfds);
				image[numLink].close();
				string message = generateMessageOfImageDownoloading(imageNameArr[numLink], n);
				writeToLogFile(message);
				syncLog(&console, message, &cout);
				break;
			}
			else if (n == -1) {
				syncLog(&console, SOCKER_DATA_READ_EXCEPTION_MESSAGE, &cout);
			}
		}
	}
	_endthreadex(0);
	return(0);
}

void syncLog(CRITICAL_SECTION* section, string message, ostream* target) {
	EnterCriticalSection(section);
	*target << message << endl;
	LeaveCriticalSection(section);
}

string format(const string& format, ...)
{
	va_list args;
	va_start(args, format);
	size_t len = std::vsnprintf(NULL, 0, format.c_str(), args);
	va_end(args);
	std::vector<char> vec(len + 1);
	va_start(args, format);
	std::vsnprintf(&vec[0], len + 1, format.c_str(), args);
	va_end(args);
	return &vec[0];
}
