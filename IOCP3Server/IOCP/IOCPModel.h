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
#include "PerSocketContext.h"

#define WORKER_THREADS_PER_PROCESSOR 2 // 每一个处理器上产生多少个线程
#define MAX_POST_ACCEPT 10 // 同时投递的AcceptEx请求的数量
#define EXIT_CODE NULL // 传递给Worker线程的退出信号
#define RELEASE_POINTER(x) {if(x != NULL ){delete x;x=NULL;}} // 释放指针宏
#define RELEASE_HANDLE(x) {if(x != NULL && x!=INVALID_HANDLE_VALUE)\
	{ CloseHandle(x);x = NULL;}} // 释放句柄宏
#define RELEASE_SOCKET(x) {if(x != NULL && x !=INVALID_SOCKET) \
	{ closesocket(x);x=INVALID_SOCKET;}} // 释放Socket宏
#define DEFAULT_IP "127.0.0.1" //默认IP地址
#define DEFAULT_PORT 10240 //默认端口号

/****************************************************************
BOOL WINAPI GetQueuedCompletionStatus(
__in   HANDLE CompletionPort,
__out  LPDWORD lpNumberOfBytes,
__out  PULONG_PTR lpCompletionKey,
__out  LPOVERLAPPED *lpOverlapped,
__in   DWORD dwMilliseconds
);
lpCompletionKey [out] 对应于SocketContext结构，
调用CreateIoCompletionPort绑定套接字到完成端口时传入；
A pointer to a variable that receives the completion key value
associated with the file handle whose I/O operation has completed.
A completion key is a per-file key that is specified
in a call to CreateIoCompletionPort.

lpOverlapped [out] 对应于IoContext结构，
如：进行accept操作时，调用AcceptEx函数时传入；
A pointer to a variable that receives the address of
the OVERLAPPED structure that was specified
when the completed I/O operation was started.
****************************************************************/
//============================================================
//				CIocpModel类定义
//============================================================
// 工作者线程的线程参数
class CIocpModel;
struct WorkerThreadParam
{
	CIocpModel* pIocpModel; //类指针，用于调用类中的函数
	int nThreadNo; //线程编号
};

class CIocpModel
{
private:
	HANDLE m_hShutdownEvent; // 用来通知线程，为了能够更好的退出
	HANDLE m_hIOCompletionPort; // 完成端口的句柄
	HANDLE* m_phWorkerThreads; // 工作者线程的句柄指针
	int m_nThreads; // 生成的线程数量
	string m_strIP; // 服务器端的IP地址
	int m_nPort; // 服务器端的监听端口
	CRITICAL_SECTION m_csContextList; // 用于Worker线程同步的互斥量
	vector<SocketContext*> m_arrayClientContext; // 客户端Socket的Context信息 
	SocketContext* m_pListenContext; // 用于监听的Socket的Context信息
	// AcceptEx 和 GetAcceptExSockaddrs 的函数指针，用于调用这两个扩展函数
	LPFN_ACCEPTEX m_lpfnAcceptEx;
	LPFN_GETACCEPTEXSOCKADDRS m_lpfnGetAcceptExSockAddrs;

public:
	CIocpModel(void);
	~CIocpModel(void);

	// 加载Socket库
	bool LoadSocketLib();
	// 卸载Socket库，彻底完事
	void UnloadSocketLib() { WSACleanup(); }
	// 启动服务器
	bool Start();
	//	停止服务器
	void Stop();
	// 获得本机的IP地址
	string GetLocalIP();
	// 设置监听端口
	void SetPort(const int& nPort) { m_nPort = nPort; }
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
	//获得本机的处理器数量
	int _GetNoOfProcessors();
	//判断客户端Socket是否已经断开
	bool _IsSocketAlive(SOCKET s);
	//线程函数，为IOCP请求服务的工作者线程
	static DWORD WINAPI _WorkerThread(LPVOID lpParam);
	//在主界面中显示信息
	void _ShowMessage(const char* szFormat, ...) const;

public:
	typedef void (*fnAddInfo)(const string strInfo);
	void SetAddInfoFunc(fnAddInfo fn) { m_fnAddInfo = fn; }
protected:
	fnAddInfo m_fnAddInfo;
};
