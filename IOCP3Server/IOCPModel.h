/*==========================================================================
Purpose:
	* 这个类CIocpModel是本代码的核心类，用于说明WinSock服务器端编程模型中的
	 完成端口(IOCP)的使用方法，并使用MFC对话框程序来调用这个类实现了基本的
	 服务器网络通信的功能。
	* 其中的IoContext结构体是封装了用于每一个重叠操作的参数
	 SocketContext 是封装了用于每一个Socket的参数，也就是用于每一个完成端口的参数
	* 详细的文档说明请参考 http://blog.csdn.net/PiggyXP
Notes:
	* 具体讲明了服务器端建立完成端口、建立工作者线程、投递Recv请求、
	 投递Accept请求的方法，所有的客户端连入的Socket都需要绑定到IOCP上，
	 所有从客户端发来的数据，都会实时显示到主界面中去。
==========================================================================*/
#pragma once
#include <MSWSock.h>
#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib")

// 缓冲区长度 (1024*8) 之所以为什么设置8K，也是一个江湖上的经验值
// 如果确实客户端发来的每组数据都比较少，那么就设置得小一些，省内存
#define MAX_BUFFER_LEN 8192 
// 默认IP地址
#define DEFAULT_IP _T("127.0.0.1")
// 默认端口
#define DEFAULT_PORT 10240 

/****************************************************************
BOOL WINAPI GetQueuedCompletionStatus(
__in HANDLE CompletionPort,
__out LPDWORD lpNumberOfBytes,
__out PULONG_PTR lpCompletionKey,
__out LPOVERLAPPED *lpOverlapped,
__in DWORD dwMilliseconds
);
lpCompletionKey [out] 对应于SocketContext结构，
调用CreateIoCompletionPort绑定套接字到完成端口时传入；
A pointer to a variable that receives the completion key value
associated with the file handle whose I/O operation has completed.
A completion key is a per-file key that is specified in a call
to CreateIoCompletionPort.

lpOverlapped [out] 对应于IoContext结构，
如：进行accept操作时，调用AcceptEx函数时传入；
A pointer to a variable that receives the address of the OVERLAPPED structure
that was specified when the completed I/O operation was started.

****************************************************************/

// 在完成端口上投递的I/O操作的类型
typedef enum _OPERATION_TYPE
{
	UNKNOWN, // 用于初始化，无意义
	ACCEPT, // 标志投递的Accept操作
	SEND, // 标志投递的是发送操作
	RECV, // 标志投递的是接收操作
}OPERATION_TYPE;

//===============================================================================
//
//				单IO数据结构体定义(用于每一个重叠操作的参数)
//
//===============================================================================
//每次套接字操作(如：AcceptEx, WSARecv, WSASend等)对应的数据结构：
//OVERLAPPED结构(标识本次操作)，关联的套接字，缓冲区，操作类型；
struct IoContext
{
	// 每一个重叠网络操作的重叠结构
	OVERLAPPED m_Overlapped; //(针对每一个Socket的每一个操作，都要有一个) 
	SOCKET m_sockAccept; // 这个网络操作所使用的Socket
	WSABUF m_wsaBuf; // WSA类型的缓冲区，用于给重叠操作传参数的
	char m_szBuffer[MAX_BUFFER_LEN]; // 这个是WSABUF里具体存字符的缓冲区
	OPERATION_TYPE m_OpType; // 标识网络操作的类型(对应上面的枚举)

	DWORD m_nTotalBytes; //数据总的字节数
	DWORD m_nSendBytes;	//已经发送的字节数，如未发送数据则设置为0

	//构造函数
	IoContext()
	{
		ZeroMemory(&m_Overlapped, sizeof(m_Overlapped));
		ZeroMemory(m_szBuffer, MAX_BUFFER_LEN);
		m_sockAccept = INVALID_SOCKET;
		m_wsaBuf.buf = m_szBuffer;
		m_wsaBuf.len = MAX_BUFFER_LEN;
		m_OpType = UNKNOWN;

		m_nTotalBytes = 0;
		m_nSendBytes = 0;
	}
	//析构函数
	~IoContext()
	{
		if (m_sockAccept != INVALID_SOCKET)
		{
			closesocket(m_sockAccept);
			m_sockAccept = INVALID_SOCKET;
		}
	}
	//重置缓冲区内容
	void ResetBuffer()
	{
		ZeroMemory(m_szBuffer, MAX_BUFFER_LEN);
		m_wsaBuf.buf = m_szBuffer;
		m_wsaBuf.len = MAX_BUFFER_LEN;
	}
};

//=================================================================================
//
//				单句柄数据结构体定义(用于每一个完成端口，也就是每一个Socket的参数)
//
//=================================================================================
//每个SOCKET对应的数据结构(调用GetQueuedCompletionStatus传入)：-
//SOCKET，该SOCKET对应的客户端地址，作用在该SOCKET操作集合(对应结构IoContext)；
struct SocketContext
{
	SOCKET m_Socket; // 每一个客户端连接的Socket
	SOCKADDR_IN m_ClientAddr; // 客户端的地址
	// 客户端网络操作的上下文数据，
	// 也就是说对于每一个客户端Socket，是可以在上面同时投递多个IO请求的
	//套接字操作，本例是WSARecv和WSASend共用一个IoContext
	CArray<IoContext*> m_arrayIoContext;

	//构造函数
	SocketContext()
	{
		m_Socket = INVALID_SOCKET;
		memset(&m_ClientAddr, 0, sizeof(m_ClientAddr));
	}

	//析构函数
	~SocketContext()
	{
		if (m_Socket != INVALID_SOCKET)
		{
			closesocket(m_Socket);
			m_Socket = INVALID_SOCKET;
		}
		// 释放掉所有的IO上下文数据
		for (int i = 0; i < m_arrayIoContext.GetCount(); i++)
		{
			delete m_arrayIoContext.GetAt(i);
		}
		m_arrayIoContext.RemoveAll();
	}

	//进行套接字操作时，调用此函数返回PER_IO_CONTEX结构
	IoContext* GetNewIoContext()
	{
		IoContext* p = new IoContext;
		m_arrayIoContext.Add(p);
		return p;
	}

	// 从数组中移除一个指定的IoContext
	void RemoveContext(IoContext* pContext)
	{
		ASSERT(pContext != NULL);
		for (int i = 0; i < m_arrayIoContext.GetCount(); i++)
		{
			if (pContext == m_arrayIoContext.GetAt(i))
			{
				delete pContext;
				pContext = NULL;
				m_arrayIoContext.RemoveAt(i);
				break;
			}
		}
	}
};

//============================================================
//				CIocpModel类定义
//============================================================
// 工作者线程的线程参数
class CIocpModel;
struct WorkerThreadParam
{
	CIocpModel* pIocpModel; //类指针，用于调用类中的函数
	int nThreadNo; //线程编号
} ;

class CIocpModel
{
public:
	CIocpModel(void);
	~CIocpModel(void);

public:
	// 加载Socket库
	bool LoadSocketLib();
	// 卸载Socket库，彻底完事
	void UnloadSocketLib() { WSACleanup(); }
	// 启动服务器
	bool Start();
	//	停止服务器
	void Stop();
	// 获得本机的IP地址
	CString GetLocalIP();
	// 设置监听端口
	void SetPort(const int& nPort) { m_nPort = nPort; }
	// 设置主界面的指针，用于调用显示信息到界面中
	void SetMainDlg(CDialog* p) { m_pMain = p; }
	//投递WSASend，用于发送数据
	bool PostWrite(IoContext* pAcceptIoContext);
	//投递WSARecv用于接收数据
	bool PostRecv(IoContext* pIoContext);

protected:
	// 初始化IOCP
	bool _InitializeIOCP();
	// 初始化Socket
	bool _InitializeListenSocket();
	// 最后释放资源
	void _DeInitialize();
	//投递AcceptEx请求
	bool _PostAccept(IoContext* pAcceptIoContext);
	//在有客户端连入的时候，进行处理
	bool _DoAccpet(SocketContext* pSocketContext, IoContext* pIoContext);
	//连接成功时，根据第一次是否接收到来自客户端的数据进行调用
	bool _DoFirstRecvWithData(IoContext* pIoContext);
	bool _DoFirstRecvWithoutData(IoContext* pIoContext);
	//数据到达，数组存放在pIoContext参数中
	bool _DoRecv(SocketContext* pSocketContext, IoContext* pIoContext);
	//将客户端socket的相关信息存储到数组中
	void _AddToContextList(SocketContext* pSocketContext);
	//将客户端socket的信息从数组中移除
	void _RemoveContext(SocketContext* pSocketContext);
	// 清空客户端socket信息
	void _ClearContextList();
	// 将句柄绑定到完成端口中
	bool _AssociateWithIOCP(SocketContext* pContext);
	// 处理完成端口上的错误
	bool HandleError(SocketContext* pContext, const DWORD& dwErr);
	//线程函数，为IOCP请求服务的工作者线程
	static DWORD WINAPI _WorkerThread(LPVOID lpParam);
	//获得本机的处理器数量
	int _GetNoOfProcessors();
	//判断客户端Socket是否已经断开
	bool _IsSocketAlive(SOCKET s);
	//在主界面中显示信息
	void _ShowMessage(const CString szFormat, ...) const;

private:
	HANDLE m_hShutdownEvent; // 用来通知线程系统退出的事件，为了能够更好的退出线程
	HANDLE m_hIOCompletionPort; // 完成端口的句柄
	HANDLE* m_phWorkerThreads; // 工作者线程的句柄指针
	int m_nThreads; // 生成的线程数量
	CString m_strIP; // 服务器端的IP地址
	int m_nPort; // 服务器端的监听端口
	CDialog* m_pMain; // 主界面的界面指针，用于在主界面中显示消息
	CRITICAL_SECTION m_csContextList; // 用于Worker线程同步的互斥量
	CArray<SocketContext*> m_arrayClientContext; // 客户端Socket的Context信息 
	SocketContext* m_pListenContext; // 用于监听的Socket的Context信息
	// AcceptEx 和 GetAcceptExSockaddrs 的函数指针，用于调用这两个扩展函数
	LPFN_ACCEPTEX m_lpfnAcceptEx; 
	LPFN_GETACCEPTEXSOCKADDRS m_lpfnGetAcceptExSockAddrs;
};
