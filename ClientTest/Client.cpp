#include "StdAfx.h"
#include "Client.h"
#include "MainDlg.h"
#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib")

#define RELEASE_HANDLE(x) {if(x != NULL && x!=INVALID_HANDLE_VALUE)\
		{ CloseHandle(x);x = NULL;}}
#define RELEASE_POINTER(x) {if(x != NULL ){delete x;x=NULL;}}

CClient::CClient(void) :
	m_strServerIP(DEFAULT_IP),
	m_strLocalIP(DEFAULT_IP),
	m_nThreads(DEFAULT_THREADS),
	m_pMain(NULL),
	m_nPort(DEFAULT_PORT),
	m_strMessage(DEFAULT_MESSAGE),
	m_phWorkerThreads(NULL),
	m_hConnectionThread(NULL),
	m_hShutdownEvent(NULL)
{
}

CClient::~CClient(void)
{
	this->Stop();
}

//////////////////////////////////////////////////////////////////////////////////
//	建立连接的线程
DWORD WINAPI CClient::_ConnectionThread(LPVOID lpParam)
{
	ConnectThreadParam* pParams = (ConnectThreadParam*)lpParam;
	CClient* pClient = (CClient*)pParams->pClient;
	TRACE("_AccpetThread启动，系统监听中...\n");
	pClient->EstablishConnections();
	TRACE(_T("_ConnectionThread线程结束.\n"));
	RELEASE_POINTER(pParams);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////
// 用于发送信息的线程
DWORD WINAPI CClient::_WorkerThread(LPVOID lpParam)
{
	WorkerThreadParam* pParams = (WorkerThreadParam*)lpParam;
	CClient* pClient = (CClient*)pParams->pClient;
	char szTemp[MAX_BUFFER_LEN];
	memset(szTemp, 0, sizeof(szTemp));
	char szRecv[MAX_BUFFER_LEN];
	memset(szRecv, 0, MAX_BUFFER_LEN);
	int nBytesSent = 0;
	int nBytesRecv = 0;
	// 向服务器发送信息
	sprintf(szTemp, ("第1条信息：%s"), pParams->szBuffer);
	nBytesSent = send(pParams->sock, szTemp, strlen(szTemp), 0);
	if (SOCKET_ERROR == nBytesSent)
	{
		TRACE("错误：发送1次信息失败，错误代码：%ld\n", WSAGetLastError());
		return 1;
	}
	TRACE("向服务器发送信息成功: %s\n", szTemp);
	pClient->ShowMessage("SEND1 向服务器发送信息成功: %s", szTemp);

	memset(pParams->szBuffer1, 0, 8196);
	memset(szTemp, 0, sizeof(szTemp));
	nBytesRecv = recv(pParams->sock, pParams->szBuffer1, 8196, 0);
	if (SOCKET_ERROR == nBytesRecv)
	{
		TRACE("错误：接收1次信息失败，错误代码：%ld\n", WSAGetLastError());
		return 1;
	}
	pParams->szBuffer[nBytesRecv] = 0;
	sprintf(szTemp, ("RECV1 第%d号线程，接收第1条信息：\"%s\""),
		pParams->nThreadNo, pParams->szBuffer1);
	pClient->ShowMessage(szTemp);
	Sleep(3000);

	// 再发送一条信息
	memset(szTemp, 0, sizeof(szTemp));
	sprintf(szTemp, ("第2条信息：%s"), pParams->szBuffer);
	nBytesSent = send(pParams->sock, szTemp, strlen(szTemp), 0);
	if (SOCKET_ERROR == nBytesSent)
	{
		TRACE("错误：发送第2次信息失败，错误代码：%ld\n", WSAGetLastError());
		return 1;
	}
	TRACE("向服务器发送信息成功: %s\n", szTemp);
	pClient->ShowMessage("SEND2 向服务器发送信息成功: %s", szTemp);
	memset(pParams->szBuffer1, 0, 8196);
	memset(szTemp, 0, sizeof(szTemp));
	nBytesRecv = recv(pParams->sock, pParams->szBuffer1, 8196, 0);
	if (SOCKET_ERROR == nBytesRecv)
	{
		TRACE("错误：接收2次信息失败，错误代码：%ld\n", WSAGetLastError());
		return 1;
	}
	pParams->szBuffer[nBytesRecv] = 0;
	sprintf(szTemp, ("RECV2 第%d号线程，接收第2条信息：\"%s\""),
		pParams->nThreadNo, pParams->szBuffer1);
	pClient->ShowMessage(szTemp);
	Sleep(3000);

	// 发第3条信息
	memset(szTemp, 0, sizeof(szTemp));
	sprintf(szTemp, ("第3条信息：%s"), pParams->szBuffer);
	nBytesSent = send(pParams->sock, szTemp, strlen(szTemp), 0);
	if (SOCKET_ERROR == nBytesSent)
	{
		TRACE("错误：发送第3次信息失败，错误代码：%ld\n", WSAGetLastError());
		return 1;
	}
	pClient->ShowMessage("SEND3 向服务器发送信息成功: %s", szTemp);
	TRACE("向服务器发送信息成功: %s\n", szTemp);
	memset(pParams->szBuffer1, 0, 8196);
	memset(szTemp, 0, sizeof(szTemp));
	nBytesRecv = recv(pParams->sock, pParams->szBuffer1, 8196, 0);
	if (SOCKET_ERROR == nBytesRecv)
	{
		TRACE("错误：接收3次信息失败，错误代码：%ld\n", WSAGetLastError());
		return 1;
	}
	pParams->szBuffer[nBytesRecv] = 0;
	sprintf(szTemp, ("RECV3 第%d号线程，接收第3条信息：\"%s\""),
		pParams->nThreadNo, pParams->szBuffer1);
	pClient->ShowMessage(szTemp);
	if (pParams->nThreadNo == pClient->m_nThreads)
	{
		pClient->ShowMessage(_T("测试并发 %d 个线程完毕."), pClient->m_nThreads);
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////////////
// 建立连接
bool  CClient::EstablishConnections()
{
	DWORD nThreadID;
	m_phWorkerThreads = new HANDLE[m_nThreads];
	m_pParamsWorker = new WorkerThreadParam[m_nThreads];
	// 根据用户设置的线程数量，生成每一个线程连接至服务器，并生成线程发送数据
	for (int i = 0; i < m_nThreads; i++)
	{
		// 监听用户的停止事件
		if (WAIT_OBJECT_0 == WaitForSingleObject(m_hShutdownEvent, 0))
		{
			TRACE(_T("接收到用户停止命令.\n"));
			return true;
		}
		// 向服务器进行连接
		if (!this->ConnetToServer(&m_pParamsWorker[i].sock,
			m_strServerIP, m_nPort))
		{
			ShowMessage(_T("连接服务器失败！"));
			CleanUp();
			return false;
		}
		m_pParamsWorker[i].nThreadNo = i + 1;
		sprintf(m_pParamsWorker[i].szBuffer, "%d号线程 发送数据 %s",
			i + 1, m_strMessage.GetString());
		Sleep(10);
		// 如果连接服务器成功，就开始建立工作者线程，向服务器发送指定数据
		m_pParamsWorker[i].pClient = this;
		m_phWorkerThreads[i] = CreateThread(0, 0, _WorkerThread,
			(void*)(&m_pParamsWorker[i]), 0, &nThreadID);
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////////
//	向服务器进行Socket连接
bool CClient::ConnetToServer(SOCKET* pSocket,
	CString strServer, int nPort)
{
	struct sockaddr_in ServerAddress;
	struct hostent* Server;
	// 生成SOCKET
	*pSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == *pSocket)
	{
		TRACE("错误：初始化Socket失败，错误信息：%d\n",
			WSAGetLastError());
		return false;
	}
	// 生成地址信息
	Server = gethostbyname(strServer.GetString());
	if (Server == NULL)
	{
		closesocket(*pSocket);
		TRACE("错误：无效的服务器地址.\n");
		return false;
	}
	ZeroMemory((char*)&ServerAddress, sizeof(ServerAddress));
	ServerAddress.sin_family = AF_INET;
	CopyMemory((char*)&ServerAddress.sin_addr.s_addr,
		(char*)Server->h_addr,
		Server->h_length);
	ServerAddress.sin_port = htons(m_nPort);
	// 开始连接服务器
	if (SOCKET_ERROR == connect(*pSocket,
		reinterpret_cast<const struct sockaddr*>(&ServerAddress),
		sizeof(ServerAddress)))
	{
		closesocket(*pSocket);
		TRACE("错误：连接至服务器失败！\n");
		return false;
	}
	return true;
}

////////////////////////////////////////////////////////////////////
// 初始化WinSock 2.2
bool CClient::LoadSocketLib()
{
	WSADATA wsaData;
	int nResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (NO_ERROR != nResult)
	{
		ShowMessage(_T("初始化WinSock 2.2失败！\n"));
		return false; // 错误
	}
	return true;
}

///////////////////////////////////////////////////////////////////
// 开始监听
bool CClient::Start()
{
	// 建立系统退出的事件通知
	m_hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	// 启动连接线程
	DWORD nThreadID;
	ConnectThreadParam* pThreadParams = new ConnectThreadParam;
	pThreadParams->pClient = this;
	m_hConnectionThread = ::CreateThread(0, 0, _ConnectionThread,
		(void*)pThreadParams, 0, &nThreadID);
	return true;
}

///////////////////////////////////////////////////////////////////////
//	停止监听
void CClient::Stop()
{
	if (m_hShutdownEvent == NULL) return;
	SetEvent(m_hShutdownEvent);
	// 等待Connection线程退出
	WaitForSingleObject(m_hConnectionThread, INFINITE);
	// 关闭所有的Socket
	for (int i = 0; i < m_nThreads; i++)
	{
		closesocket(m_pParamsWorker[i].sock);
	}
	// 等待所有的工作者线程退出
	WaitForMultipleObjects(m_nThreads, m_phWorkerThreads, TRUE, INFINITE);
	// 清空资源
	CleanUp();
	TRACE("测试停止.\n");
}

//////////////////////////////////////////////////////////////////////
//	清空资源
void CClient::CleanUp()
{
	if (m_hShutdownEvent == NULL) return;
	RELEASE_POINTER(m_phWorkerThreads);
	RELEASE_HANDLE(m_hConnectionThread);
	RELEASE_POINTER(m_pParamsWorker);
	RELEASE_HANDLE(m_hShutdownEvent);
}

////////////////////////////////////////////////////////////////////
// 获得本机的IP地址
CString CClient::GetLocalIP()
{
	char hostname[MAX_PATH];
	gethostname(hostname, MAX_PATH); // 获得本机主机名
	struct hostent FAR* lpHostEnt = gethostbyname(hostname);
	if (lpHostEnt == NULL)
	{
		return DEFAULT_IP;
	}
	// 取得IP地址列表中的第一个为返回的IP
	LPSTR lpAddr = lpHostEnt->h_addr_list[0];
	struct in_addr inAddr;
	memmove(&inAddr, lpAddr, 4);
	// 转化成标准的IP地址形式
	m_strLocalIP = CString(inet_ntoa(inAddr));
	return m_strLocalIP;
}

/////////////////////////////////////////////////////////////////////
// 在主界面中显示信息
void CClient::ShowMessage(const CString strInfo, ...)
{
	// 根据传入的参数格式化字符串
	CString   strMessage;
	va_list   arglist;

	va_start(arglist, strInfo);
	strMessage.FormatV(strInfo, arglist);
	va_end(arglist);

	// 在主界面中显示
	CMainDlg* pMain = (CMainDlg*)m_pMain;
	if (m_pMain != NULL)
	{
		pMain->AddInformation(strMessage);
	}
}
