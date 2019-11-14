//#include "StdAfx.h"
#include "IOCPModel.h"
#include "..\ServerEngine\MyServerEngine.h"


namespace MyServer{


	HANDLE CIOCPModel::m_hMutexServerEngine = CreateMutex(NULL,FALSE,"m_hMutexServerEngine");

	CIOCPModel::CIOCPModel(void):
								m_nThreads(0),
								m_hShutdownEvent(NULL),
								m_hIOCompletionPort(NULL),
								m_phWorkerThreads(NULL),
								m_strIP(DEFAULT_IP),
								m_lpfnAcceptEx( NULL ),
								m_pListenContext( NULL ){

		m_nPort = 12345;	//端口

	}


	CIOCPModel::~CIOCPModel(void){
		// 确保资源彻底释放
		this->Stop();
	}


	/*********************************************************************
	*函数功能：线程函数，根据GetQueuedCompletionStatus返回情况进行处理；
	*函数参数：lpParam是THREADPARAMS_WORKER类型指针；
	*函数说明：GetQueuedCompletionStatus正确返回时表示某操作已经完成，第二个参数lpNumberOfBytes表示本次套接字传输的字节数，
	参数lpCompletionKey和lpOverlapped包含重要的信息，请查询MSDN文档；
	*********************************************************************/
	DWORD WINAPI CIOCPModel::_WorkerThread(LPVOID lpParam){    

		THREADPARAMS_WORKER* pParam = (THREADPARAMS_WORKER*)lpParam;
		CIOCPModel* pIOCPModel = (CIOCPModel*)pParam->pIOCPModel;
		int nThreadNo = (int)pParam->nThreadNo;

		char aa[256];
		sprintf_s(aa,"工作者线程启动，ID: %d.",nThreadNo);
		g_pServerEngine->AddServerMsgs(string(aa));

		OVERLAPPED           *pOverlapped = NULL;
		PER_SOCKET_CONTEXT   *pSocketContext = NULL;
		DWORD                dwBytesTransfered = 0;

		//循环处理请求，直到接收到Shutdown信息为止
		while (WaitForSingleObject(pIOCPModel->m_hShutdownEvent, 0) != WAIT_OBJECT_0 ){
			//检查完成端口状态
			BOOL bReturn = GetQueuedCompletionStatus(
				pIOCPModel->m_hIOCompletionPort,
				&dwBytesTransfered,
				(PULONG_PTR)&pSocketContext,
				&pOverlapped,
				INFINITE);

			//接收EXIT_CODE退出标志，则直接退出
			if ( (DWORD)pSocketContext == EXIT_CODE){
				break;
			}

			//返回值为0，表示出错
			if( bReturn == 0 )  {
				DWORD dwErr = GetLastError();
				// 不可以恢复的错误,退出
				if( pIOCPModel->HandleError( pSocketContext,dwErr ) == false){
					break;
				}
				
				//可以恢复的错误,继续运行
				continue;  

			}  else  {  	
				// 读取传入的参数
				PER_IO_CONTEXT* pIoContext = CONTAINING_RECORD(pOverlapped, PER_IO_CONTEXT, m_Overlapped);  

				// 判断是否有客户端断开了
				if((dwBytesTransfered == 0) && ( pIoContext->m_OpType == RECV_POSTED || pIoContext->m_OpType == SEND_POSTED))  {  
					sprintf_s(aa,"客户端 %s:%d 断开连接.",inet_ntoa(pSocketContext->m_ClientAddr.sin_addr), ntohs(pSocketContext->m_ClientAddr.sin_port));
					g_pServerEngine->AddServerMsgs(string(aa));

					// 释放掉对应的资源
					pIOCPModel->_RemoveContext( pSocketContext );

 					continue;  

				}  else{
					switch( pIoContext->m_OpType )  {  
						//客户端连接
					case ACCEPT_POSTED:
						{
							pIoContext->m_nTotalBytes = dwBytesTransfered;
							pIOCPModel->_DoAccpet( pSocketContext, pIoContext);						
						}
						break;

						//接收数据
					case RECV_POSTED:
						{
							pIoContext->m_nTotalBytes	= dwBytesTransfered;
							pIOCPModel->_DoRecv( pSocketContext,pIoContext );
						}
						break;

						//发送数据
					case SEND_POSTED:
						{
							pIoContext->m_nSendBytes += dwBytesTransfered;
							if (pIoContext->m_nSendBytes < pIoContext->m_nTotalBytes){
								//数据未能发送完，继续发送数据
								pIoContext->m_wsaBuf.buf = pIoContext->m_szBuffer + pIoContext->m_nSendBytes;
								pIoContext->m_wsaBuf.len = pIoContext->m_nTotalBytes - pIoContext->m_nSendBytes;
								pIOCPModel->PostWrite(pIoContext);
							}else{
								pIOCPModel->PostRecv(pIoContext);
							}
						}
						break;
					default:
						// 不应该执行到这里
						throw ("_WorkThread中的 pIoContext->m_OpType 参数异常.\n");
						break;
					}
				}
			}

		}

		sprintf_s(aa,"工作者线程 %d 号退出.\n",nThreadNo);
		g_pServerEngine->AddServerMsgs(string(aa));

		// 释放线程参数
		RELEASE(lpParam);	

		return 0;
	}

	//函数功能：初始化套接字
	bool CIOCPModel::LoadSocketLib()
	{    
		WSADATA wsaData;
		int nResult;
		nResult = WSAStartup(MAKEWORD(2,2), &wsaData);
		// 错误(一般都不可能出现)
		if (nResult != NO_ERROR ){
			g_pServerEngine->AddServerMsgs(string("初始化WinSock 2.2失败！\n"));
			return false; 
		}

		return true;
	}


	//函数功能：启动服务器
	bool CIOCPModel::Start(){
		// 初始化线程互斥量
		InitializeCriticalSection(&m_csContextList);

		// 建立系统退出的事件通知
		m_hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		// 初始化IOCP
		if (_InitializeIOCP() == false){
			g_pServerEngine->AddServerMsgs(string("初始化IOCP失败！\n"));
			return false;
		}else{
			g_pServerEngine->AddServerMsgs(string("\nIOCP初始化完毕\n."));
		}

		// 初始化Socket
		if( _InitializeListenSocket() == false ){
			g_pServerEngine->AddServerMsgs(string("Listen Socket初始化失败！\n"));
			this->_DeInitialize();
			return false;
		}else{
			g_pServerEngine->AddServerMsgs(string("Listen Socket初始化完毕."));
		}

		g_pServerEngine->AddServerMsgs(string("系统准备就绪，等候连接....\n"));

		return true;
	}


	////////////////////////////////////////////////////////////////////
	//	开始发送系统退出消息，退出完成端口和线程资源
	void CIOCPModel::Stop(){
		if( m_pListenContext!=NULL && m_pListenContext->m_Socket!=INVALID_SOCKET ){
			// 激活关闭消息通知
			SetEvent(m_hShutdownEvent);

			for (int i = 0; i < m_nThreads; i++){
				// 通知所有的完成端口操作退出
				PostQueuedCompletionStatus(m_hIOCompletionPort, 0, (DWORD)EXIT_CODE, NULL);
			}

			// 等待所有的客户端资源退出
			WaitForMultipleObjects(m_nThreads, m_phWorkerThreads, TRUE, INFINITE);

			// 清除客户端列表信息
			this->_ClearContextList();

			// 释放其他资源
			this->_DeInitialize();

			g_pServerEngine->AddServerMsgs(string("停止监听\n"));
		}	
	}


	/*************************************************************
	*函数功能：投递WSARecv请求；
	*函数参数：
	PER_IO_CONTEXT* pIoContext:	用于进行IO的套接字上的结构，主要为WSARecv参数和WSASend参数；
	**************************************************************/
	bool CIOCPModel::PostRecv( PER_IO_CONTEXT* pIoContext )
	{
		// 初始化变量
		DWORD dwFlags = 0;
		DWORD dwBytes = 0;
		WSABUF *p_wbuf   = &pIoContext->m_wsaBuf;
		OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

		pIoContext->ResetBuffer();
		pIoContext->m_OpType = RECV_POSTED;
		pIoContext->m_nSendBytes = 0;
		pIoContext->m_nTotalBytes= 0;

		// 初始化完成后，，投递WSARecv请求
		int nBytesRecv = WSARecv( pIoContext->m_sockAccept, p_wbuf, 1, &dwBytes, &dwFlags, p_ol, NULL );

		// 如果返回值错误，并且错误的代码并非是Pending的话，那就说明这个重叠请求失败了
		if ((nBytesRecv == SOCKET_ERROR ) && ( WSAGetLastError() != WSA_IO_PENDING)){
			g_pServerEngine->AddServerMsgs(string("投递一个WSARecv失败！"));
			return false;
		}

		return true;
	}

	/*************************************************************
	*函数功能：投递WSASend请求
	*函数参数：
	PER_IO_CONTEXT* pIoContext:	用于进行IO的套接字上的结构，主要为WSARecv参数和WSASend参数
	*函数说明：调用PostWrite之前需要设置pIoContext中m_wsaBuf, m_nTotalBytes, m_nSendBytes；
	**************************************************************/
	bool CIOCPModel::PostWrite(PER_IO_CONTEXT* pIoContext)
	{
		// 初始化变量
		DWORD dwFlags = 0;
		DWORD dwSendNumBytes = 0;
		WSABUF *p_wbuf   = &pIoContext->m_wsaBuf;
		OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

		pIoContext->m_OpType = SEND_POSTED;

		//投递WSASend请求 -- 需要修改
		int nRet = WSASend(pIoContext->m_sockAccept, &pIoContext->m_wsaBuf, 1, &dwSendNumBytes, dwFlags,
			&pIoContext->m_Overlapped, NULL);

		// 如果返回值错误，并且错误的代码并非是Pending的话，那就说明这个重叠请求失败了
		if (( nRet == SOCKET_ERROR) && ( WSAGetLastError() != WSA_IO_PENDING)){
			g_pServerEngine->AddServerMsgs(string("投递WSASend失败！"));
			return false;
		}
		return true;
	}


	////////////////////////////////
	// 初始化完成端口
	bool CIOCPModel::_InitializeIOCP()
	{
		// 建立第一个完成端口
		m_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );
		if ( m_hIOCompletionPort == NULL ){
			char aa[256];
			sprintf_s(aa,"建立完成端口失败！错误代码: %d!\n", WSAGetLastError());
			g_pServerEngine->AddServerMsgs(string(aa));
			return false;
		}

		// 根据本机中的处理器数量，建立对应的线程数
		m_nThreads = WORKER_THREADS_PER_PROCESSOR * _GetNoOfProcessors();
		
		// 为工作者线程初始化句柄
		m_phWorkerThreads = new HANDLE[m_nThreads];
		
		// 根据计算出来的数量建立工作者线程
		DWORD nThreadID;
		for (int i = 0; i < m_nThreads; i++){
			THREADPARAMS_WORKER* pThreadParams = new THREADPARAMS_WORKER;
			pThreadParams->pIOCPModel = this;
			pThreadParams->nThreadNo  = i+1;
			m_phWorkerThreads[i] = ::CreateThread(0, 0, _WorkerThread, (void *)pThreadParams, 0, &nThreadID);
		}

		//TRACE(" 建立 _WorkerThread %d 个.\n", m_nThreads );
		char aa[256];
		sprintf_s(aa," 建立 _WorkerThread %d 个.\n", m_nThreads);
		g_pServerEngine->AddServerMsgs(string(aa));

		return true;
	}


	/////////////////////////////////////////////////////////////////
	// 初始化Socket
	bool CIOCPModel::_InitializeListenSocket()
	{
		// AcceptEx 和 GetAcceptExSockaddrs 的GUID，用于导出函数指针
		GUID GuidAcceptEx = WSAID_ACCEPTEX;  
		GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS; 

		// 服务器地址信息，用于绑定Socket
		struct sockaddr_in ServerAddress;

		// 生成用于监听的Socket的信息
		m_pListenContext = new PER_SOCKET_CONTEXT;

		// 需要使用重叠IO，必须得使用WSASocket来建立Socket，才可以支持重叠IO操作
		m_pListenContext->m_Socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (INVALID_SOCKET == m_pListenContext->m_Socket) {
			char aa[256];
			sprintf_s(aa,"初始化Socket失败！错误代码: %d!\n", WSAGetLastError());
			g_pServerEngine->AddServerMsgs(string(aa));
			return false;
		}else{
			g_pServerEngine->AddServerMsgs(string("WSASocket() 完成.\n"));
		}

		// 将Listen Socket绑定至完成端口中
		if( NULL== CreateIoCompletionPort( (HANDLE)m_pListenContext->m_Socket, m_hIOCompletionPort,(DWORD)m_pListenContext, 0))  {  
			//this->_ShowMessage("绑定 Listen Socket至完成端口失败！错误代码: %d/n", WSAGetLastError());  
			char aa[256];
			sprintf_s(aa,"绑定 Listen Socket至完成端口失败！错误代码: %d!\n", WSAGetLastError());
			g_pServerEngine->AddServerMsgs(string(aa));

			RELEASE_SOCKET( m_pListenContext->m_Socket );
			return false;
		}else{
			//TRACE("Listen Socket绑定完成端口 完成.\n");
			g_pServerEngine->AddServerMsgs(string("Listen Socket绑定完成端口 完成.\n"));
		}

		// 填充地址信息
		ZeroMemory((char *)&ServerAddress, sizeof(ServerAddress));
		ServerAddress.sin_family = AF_INET;
		// 这里可以绑定任何可用的IP地址，或者绑定一个指定的IP地址 
		//ServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);                      
		ServerAddress.sin_addr.s_addr = inet_addr(m_strIP.c_str());         
		ServerAddress.sin_port = htons(m_nPort);                          

		// 绑定地址和端口
		if (SOCKET_ERROR == bind(m_pListenContext->m_Socket, (struct sockaddr *) &ServerAddress, sizeof(ServerAddress))) {
			g_pServerEngine->AddServerMsgs(string("bind()函数执行错误.\n"));
			return false;
		}else{
			g_pServerEngine->AddServerMsgs(string("bind() 完成.\n"));
		}

		// 开始进行监听
		if (SOCKET_ERROR == listen(m_pListenContext->m_Socket,SOMAXCONN)){
			g_pServerEngine->AddServerMsgs(string("Listen()函数执行出现错误.\n"));
			return false;
		}else{
			g_pServerEngine->AddServerMsgs(string("Listen() 完成.\n"));
		}

		// 使用AcceptEx函数，因为这个是属于WinSock2规范之外的微软另外提供的扩展函数
		// 所以需要额外获取一下函数的指针，
		// 获取AcceptEx函数指针
		DWORD dwBytes = 0;  
		if(SOCKET_ERROR == WSAIoctl(
			m_pListenContext->m_Socket, 
			SIO_GET_EXTENSION_FUNCTION_POINTER, 
			&GuidAcceptEx, 
			sizeof(GuidAcceptEx), 
			&m_lpfnAcceptEx, 
			sizeof(m_lpfnAcceptEx), 
			&dwBytes, 
			NULL, 
			NULL))  
		{  
			char aa[256];
			sprintf_s(aa,"WSAIoctl 未能获取AcceptEx函数指针。错误代码: %d\n", WSAGetLastError());
			g_pServerEngine->AddServerMsgs(string(aa));
			this->_DeInitialize();
			return false;  
		}  

		// 获取GetAcceptExSockAddrs函数指针，也是同理
		if(SOCKET_ERROR == WSAIoctl(
			m_pListenContext->m_Socket, 
			SIO_GET_EXTENSION_FUNCTION_POINTER, 
			&GuidGetAcceptExSockAddrs,
			sizeof(GuidGetAcceptExSockAddrs), 
			&m_lpfnGetAcceptExSockAddrs, 
			sizeof(m_lpfnGetAcceptExSockAddrs),   
			&dwBytes, 
			NULL, 
			NULL))  
		{  
			char aa[256];
			sprintf_s(aa,"WSAIoctl 未能获取GuidGetAcceptExSockAddrs函数指针。错误代码: %d\n", WSAGetLastError());
			g_pServerEngine->AddServerMsgs(string(aa));
			this->_DeInitialize();
			return false; 
		}  


		// 为AcceptEx 准备参数，然后投递AcceptEx I/O请求
		//创建10个套接字，投递AcceptEx请求，即共有10个套接字进行accept操作；
		for( int i=0;i<MAX_POST_ACCEPT;i++ ){
			// 新建一个IO_CONTEXT
			PER_IO_CONTEXT* pAcceptIoContext = m_pListenContext->GetNewIoContext();
			if( false==this->_PostAccept( pAcceptIoContext ) ){
				m_pListenContext->RemoveContext(pAcceptIoContext);
				return false;
			}
		}

		//this->_ShowMessage( "投递 %d 个AcceptEx请求完毕",MAX_POST_ACCEPT );
		char aa[256];
		sprintf_s(aa,"投递 %d 个AcceptEx请求完毕",MAX_POST_ACCEPT);
		g_pServerEngine->AddServerMsgs(string(aa));

		return true;
	}

	////////////////////////////////////////////////////////////
	//	最后释放掉所有资源
	void CIOCPModel::_DeInitialize(){
		// 删除客户端列表的互斥量
		DeleteCriticalSection(&m_csContextList);

		// 关闭系统退出事件句柄
		RELEASE_HANDLE(m_hShutdownEvent);

		// 释放工作者线程句柄指针
		for( int i=0;i<m_nThreads;i++ ){
			RELEASE_HANDLE(m_phWorkerThreads[i]);
		}
		
		RELEASE(m_phWorkerThreads);

		// 关闭IOCP句柄
		RELEASE_HANDLE(m_hIOCompletionPort);

		// 关闭监听Socket
		RELEASE(m_pListenContext);

		g_pServerEngine->AddServerMsgs(string("释放资源完毕.\n"));
	}


	//====================================================================================
	//
	//				    投递完成端口请求
	//
	//====================================================================================


	//////////////////////////////////////////////////////////////////
	// 投递Accept请求
	bool CIOCPModel::_PostAccept( PER_IO_CONTEXT* pAcceptIoContext )
	{
		//ASSERT( INVALID_SOCKET!=m_pListenContext->m_Socket );
		if(m_pListenContext->m_Socket == INVALID_SOCKET) {
			throw "_PostAccept,m_pListenContext->m_Socket != INVALID_SOCKET";
		}

		// 准备参数
		DWORD dwBytes = 0;  
		pAcceptIoContext->m_OpType = ACCEPT_POSTED;  
		WSABUF *p_wbuf   = &pAcceptIoContext->m_wsaBuf;
		OVERLAPPED *p_ol = &pAcceptIoContext->m_Overlapped;
		
		// 为以后新连入的客户端先准备好Socket( 这个是与传统accept最大的区别 ) 
		pAcceptIoContext->m_sockAccept  = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);  
		if( pAcceptIoContext->m_sockAccept == INVALID_SOCKET)  {  
			char aa[256];
			sprintf_s(aa,"创建用于Accept的Socket失败。错误代码: %d\n", WSAGetLastError());
			g_pServerEngine->AddServerMsgs(string(aa));
			return false;  
		} 

		// 投递AcceptEx
		if(FALSE == m_lpfnAcceptEx( m_pListenContext->m_Socket, pAcceptIoContext->m_sockAccept, p_wbuf->buf, p_wbuf->len - ((sizeof(SOCKADDR_IN)+16)*2),   
									sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, &dwBytes, p_ol))  {  
			if(WSAGetLastError() != WSA_IO_PENDING)  {  
				char aa[256];
				sprintf_s(aa,"投递 AcceptEx 请求失败。错误代码: %d\n", WSAGetLastError());
				g_pServerEngine->AddServerMsgs(string(aa));
				return false;  
			}  
		} 

		return true;
	}


	/********************************************************************
	*函数功能：函数进行客户端接入处理；
	*参数说明：
	PER_SOCKET_CONTEXT* pSocketContext:	本次accept操作对应的套接字，该套接字所对应的数据结构；
	PER_IO_CONTEXT* pIoContext:			本次accept操作对应的数据结构；
	DWORD		dwIOSize:				本次操作数据实际传输的字节数
	********************************************************************/
	bool CIOCPModel::_DoAccpet( PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext ){
		
		if (pIoContext->m_nTotalBytes > 0){
			//客户接入时，第一次接收dwIOSize字节数据
			_DoFirstRecvWithData(pIoContext);
		}else{
			//客户端接入时，没有发送数据，则投递WSARecv请求，接收数据
			_DoFirstRecvWithoutData(pIoContext);

		}

		// 5. 使用完毕之后，把Listen Socket的那个IoContext重置，然后准备投递新的AcceptEx
		pIoContext->ResetBuffer();
		return this->_PostAccept( pIoContext ); 	
	}


	/////////////////////////////////////////////////////////////////
	//函数功能：在有接收的数据到达的时候，进行处理
	bool CIOCPModel::_DoRecv( PER_SOCKET_CONTEXT* pSocketContext, PER_IO_CONTEXT* pIoContext ){
		//输出接收的数据
		SOCKADDR_IN* ClientAddr = &pSocketContext->m_ClientAddr;
		char aa[256];
		sprintf_s(aa,"收到  %s:%d 信息：%s",inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port),pIoContext->m_wsaBuf.buf );
		g_pServerEngine->AddServerMsgs(string(aa));

		//发送数据
		pIoContext->m_nSendBytes = 100;
		pIoContext->m_nTotalBytes= pIoContext->m_nTotalBytes;
		pIoContext->m_wsaBuf.len = pIoContext->m_nTotalBytes;
		//pIoContext->m_wsaBuf.buf = pIoContext->m_szBuffer;
		pIoContext->m_wsaBuf.buf = "abababab";
		return PostWrite( pIoContext );
	}

	/*************************************************************
	*函数功能：AcceptEx接收客户连接成功，接收客户第一次发送的数据，故投递WSASend请求
	*函数参数：
	PER_IO_CONTEXT* pIoContext:	用于监听套接字上的操作
	**************************************************************/
	bool CIOCPModel::_DoFirstRecvWithData(PER_IO_CONTEXT* pIoContext){
		SOCKADDR_IN* ClientAddr = NULL;
		SOCKADDR_IN* LocalAddr = NULL;  
		int remoteLen = sizeof(SOCKADDR_IN), localLen = sizeof(SOCKADDR_IN);  

		//1. 首先取得连入客户端的地址信息
		this->m_lpfnGetAcceptExSockAddrs(pIoContext->m_wsaBuf.buf, pIoContext->m_wsaBuf.len - ((sizeof(SOCKADDR_IN)+16)*2),  
			sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, (LPSOCKADDR*)&LocalAddr, &localLen, (LPSOCKADDR*)&ClientAddr, &remoteLen);  

		//显示客户端信息
		char aa[256];
		sprintf_s(aa,"客户端 %s:%d 信息：%s.",inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port),pIoContext->m_wsaBuf.buf);
		g_pServerEngine->AddServerMsgs(string(aa));


		//2.为新接入的套接创建PER_SOCKET_CONTEXT，并将该套接字绑定到完成端口
		PER_SOCKET_CONTEXT* pNewSocketContext = new PER_SOCKET_CONTEXT;
		pNewSocketContext->m_Socket           = pIoContext->m_sockAccept;
		memcpy(&(pNewSocketContext->m_ClientAddr), ClientAddr, sizeof(SOCKADDR_IN));

		// 参数设置完毕，将这个Socket和完成端口绑定(这也是一个关键步骤)
		if( this->_AssociateWithIOCP( pNewSocketContext ) == false ){
			RELEASE( pNewSocketContext );
			return false;
		}  

		//3. 继续，建立其下的IoContext，用于在这个Socket上投递第一个Recv数据请求
		PER_IO_CONTEXT* pNewIoContext = pNewSocketContext->GetNewIoContext();
		pNewIoContext->m_OpType       = SEND_POSTED;
		pNewIoContext->m_sockAccept   = pNewSocketContext->m_Socket;
		pNewIoContext->m_nTotalBytes  = pIoContext->m_nTotalBytes;
		pNewIoContext->m_nSendBytes   = 0;
		pNewIoContext->m_wsaBuf.len	  = pIoContext->m_nTotalBytes;
		memcpy(pNewIoContext->m_wsaBuf.buf, pIoContext->m_wsaBuf.buf, pIoContext->m_nTotalBytes);	//复制数据到WSASend函数的参数缓冲区

		//此时是第一次接收数据成功，所以这里投递PostWrite，向客户端发送数据
		if( this->PostWrite( pNewIoContext) == false ){
			pNewSocketContext->RemoveContext( pNewIoContext );
			return false;
		}

		//4. 如果投递成功，那么就把这个有效的客户端信息，加入到ContextList中去(需要统一管理，方便释放资源)
		this->_AddToContextList( pNewSocketContext ); 

		return true;
	}

	/*************************************************************
	*函数功能：AcceptEx接收客户连接成功，此时并未接收到数据，故投递WSARecv请求
	*函数参数：
	PER_IO_CONTEXT* pIoContext:	用于监听套接字上的操作
	**************************************************************/
	bool CIOCPModel::_DoFirstRecvWithoutData(PER_IO_CONTEXT* pIoContext )
	{
		//为新接入的套接字创建PER_SOCKET_CONTEXT结构，并绑定到完成端口
		PER_SOCKET_CONTEXT* pNewSocketContext = new PER_SOCKET_CONTEXT;
		SOCKADDR_IN ClientAddr;
		int Len = sizeof(ClientAddr);

		getpeername(pIoContext->m_sockAccept, (sockaddr*)&ClientAddr, &Len);

		pNewSocketContext->m_Socket           = pIoContext->m_sockAccept;
		memcpy(&(pNewSocketContext->m_ClientAddr), &ClientAddr, sizeof(SOCKADDR_IN));
		
		//将该套接字绑定到完成端口
		if( this->_AssociateWithIOCP( pNewSocketContext ) == false ){
			RELEASE( pNewSocketContext );
			return false;
		} 

		//投递WSARecv请求，接收数据
		PER_IO_CONTEXT* pNewIoContext = pNewSocketContext->GetNewIoContext();

		//此时是AcceptEx未接收到客户端第一次发送的数据，所以这里调用PostRecv，接收来自客户端的数据
		if( this->PostRecv( pNewIoContext) == false){
			pNewSocketContext->RemoveContext( pNewIoContext );
			return false;
		}

		//如果投递成功，那么就把这个有效的客户端信息，加入到ContextList中去(需要统一管理，方便释放资源)
		this->_AddToContextList( pNewSocketContext ); 

		return true;
	}


	/////////////////////////////////////////////////////
	// 将句柄(Socket)绑定到完成端口中
	bool CIOCPModel::_AssociateWithIOCP( PER_SOCKET_CONTEXT *pContext ){
		// 将用于和客户端通信的SOCKET绑定到完成端口中
		HANDLE hTemp = CreateIoCompletionPort((HANDLE)pContext->m_Socket, m_hIOCompletionPort, (DWORD)pContext, 0);

		if ( hTemp == NULL){
			char aa[256];
			sprintf_s(aa,"执行CreateIoCompletionPort()出现错误.错误代码。错误代码: %d\n", GetLastError());
			g_pServerEngine->AddServerMsgs(string(aa));
			return false;
		}

		return true;
	}



	//////////////////////////////////////////////////////////////
	// 将客户端的相关信息存储到数组中
	void CIOCPModel::_AddToContextList( PER_SOCKET_CONTEXT *pHandleData ){
		EnterCriticalSection(&m_csContextList);
		m_arrayClientContext.push_back(pHandleData);	
		LeaveCriticalSection(&m_csContextList);
	}

	////////////////////////////////////////////////////////////////
	//	移除某个特定的Context
	void CIOCPModel::_RemoveContext( PER_SOCKET_CONTEXT *pSocketContext ){

		EnterCriticalSection(&m_csContextList);

		vector<PER_SOCKET_CONTEXT*>::iterator it  = m_arrayClientContext.begin();
		while(it != m_arrayClientContext.end()){
			PER_SOCKET_CONTEXT* p_obj = *it;
			if(pSocketContext == p_obj){
				delete pSocketContext;
				pSocketContext = NULL;
				it = m_arrayClientContext.erase(it);
				break;
			}

			it ++;
		}

		LeaveCriticalSection(&m_csContextList);
	}

	////////////////////////////////////////////////////////////////
	// 清空客户端信息
	void CIOCPModel::_ClearContextList(){

		EnterCriticalSection(&m_csContextList);

		for( DWORD i=0;i<m_arrayClientContext.size();i++ ){
			delete m_arrayClientContext.at(i);
		}
		m_arrayClientContext.clear();

		LeaveCriticalSection(&m_csContextList);
	}



	////////////////////////////////////////////////////////////////////
	// 获得本机的IP地址
	string CIOCPModel::GetLocalIP(){
		// 获得本机主机名
		char hostname[MAX_PATH] = {0};
		gethostname(hostname,MAX_PATH);                
		struct hostent FAR* lpHostEnt = gethostbyname(hostname);
		if(lpHostEnt == NULL){
			return DEFAULT_IP;
		}

		// 取得IP地址列表中的第一个为返回的IP(因为一台主机可能会绑定多个IP)
		LPSTR lpAddr = lpHostEnt->h_addr_list[0];      

		// 将IP地址转化成字符串形式
		struct in_addr inAddr;
		memmove(&inAddr,lpAddr,4);
		m_strIP = string( inet_ntoa(inAddr) );        

		return m_strIP;
	}

	///////////////////////////////////////////////////////////////////
	// 获得本机中处理器的数量
	int CIOCPModel::_GetNoOfProcessors(){
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		return si.dwNumberOfProcessors;
	}


	/////////////////////////////////////////////////////////////////////
	// 判断客户端Socket是否已经断开，否则在一个无效的Socket上投递WSARecv操作会出现异常
	// 使用的方法是尝试向这个socket发送数据，判断这个socket调用的返回值
	// 因为如果客户端网络异常断开(例如客户端崩溃或者拔掉网线等)的时候，服务器端是无法收到客户端断开的通知的
	bool CIOCPModel::_IsSocketAlive(SOCKET s){
		int nByteSent=send(s,"",0,0);
		if (-1 == nByteSent) {
			return false;
		}else{
			return true;
		}
	}

	///////////////////////////////////////////////////////////////////
	//函数功能：显示并处理完成端口上的错误
	bool CIOCPModel::HandleError( PER_SOCKET_CONTEXT *pContext,const DWORD& dwErr ){
		// 如果是超时了，就再继续等吧  
		if(dwErr == WAIT_TIMEOUT)  {  	
			// 确认客户端是否还活着...
			if( _IsSocketAlive( pContext->m_Socket) == 0 ){
				g_pServerEngine->AddServerMsgs( string("检测到客户端异常退出！" ));
				this->_RemoveContext( pContext );
				return true;
			}else{
				g_pServerEngine->AddServerMsgs(string("网络操作超时！重试中..." ));
				return true;
			}

		}  else if( dwErr == ERROR_NETNAME_DELETED ){
			// 可能是客户端异常退出了
			g_pServerEngine->AddServerMsgs(string("检测到客户端异常退出！" ));
			this->_RemoveContext( pContext );
			return true;
		}else{
			char aa[256];
			sprintf_s(aa,"完成端口操作出现错误，线程退出。错误代码：%d",dwErr);
			g_pServerEngine->AddServerMsgs(string(aa));
			return false;
		}
	}

}




