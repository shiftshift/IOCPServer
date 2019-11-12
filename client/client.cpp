#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
#include <iostream>
using namespace std;

#pragma comment(lib,"ws2_32.lib")

char g_buffer[1024];

int main()
{
	WSADATA wsaData = { 0 };
	int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
	cout << "WSAStartup() nRet=" << nRet << endl;
	if (nRet != NO_ERROR)
	{
		int nErr = WSAGetLastError();
		return 1;
	}
	sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(6000);
	server.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	SOCKET hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	cout << "socket() hSocket=" << hSocket << endl;
	nRet = connect(hSocket, (sockaddr*)&server, sizeof(server));
	cout << "connect() nRet=" << nRet << endl;
	if (nRet < 0) 
	{
		int nErr = WSAGetLastError();
		cout << "error:" << nErr << endl;
		WSACleanup();
		return 2;
	}
	while (true) 
	{
		cout << "sent hello!!!" << endl;
		memset(g_buffer, NULL, sizeof(g_buffer));
		strcpy(g_buffer, "hello");
		nRet = send(hSocket, g_buffer, strlen(g_buffer) + 1, 0);
		cout << "send() nRet=" << nRet << endl;
		memset(g_buffer, NULL, sizeof(g_buffer));
		int nRet = recv(hSocket, g_buffer, 1024, 0);
		cout << "recv() nRet=" << nRet << endl;
		if (nRet == 0)
		{
			cout << "recv nothing!" << endl;
		}
		cout << g_buffer << endl;
		Sleep(10000);
	}

	nRet = closesocket(hSocket);
	cout << "closesocket() nRet=" << nRet << endl;
	nRet = WSACleanup();
	cout << "WSACleanup() nRet=" << nRet << endl;
	return 0;
}