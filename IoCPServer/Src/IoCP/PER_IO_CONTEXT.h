#pragma once

#include <winsock2.h>
#include <MSWSock.h>
#include <vector>

using namespace std;

//缓冲区长度 (1024*8)
#define MAX_BUFFER_LEN        8192  

namespace MyServer{

	// 在完成端口上投递的I/O操作的类型
	typedef enum _OPERATION_TYPE  {  
		ACCEPT_POSTED,                     // 标志投递的Accept操作
		SEND_POSTED,                       // 标志投递的是发送操作
		RECV_POSTED,                       // 标志投递的是接收操作
		NULL_POSTED                        // 用于初始化，无意义
	}OPERATION_TYPE;


	//每次套接字操作(如：AcceptEx, WSARecv, WSASend等)对应的数据结构：OVERLAPPED结构(标识本次操作)，关联的套接字，缓冲区，操作类型；
	typedef struct _PER_IO_CONTEXT{
		OVERLAPPED     m_Overlapped;                               // 每一个重叠网络操作的重叠结构(针对每一个Socket的每一个操作，都要有一个)              
		SOCKET         m_sockAccept;                               // 这个网络操作所使用的Socket
		WSABUF         m_wsaBuf;                                   // WSA类型的缓冲区，用于给重叠操作传参数的
		char           m_szBuffer[MAX_BUFFER_LEN];                 // 这个是WSABUF里具体存字符的缓冲区
		OPERATION_TYPE m_OpType;                                   // 标识网络操作的类型(对应上面的枚举)
		DWORD			m_nTotalBytes;	//数据总的字节数
		DWORD			m_nSendBytes;	//已经发送的字节数，如未发送数据则设置为0

		//构造函数
		_PER_IO_CONTEXT(){
			ZeroMemory(&m_Overlapped, sizeof(m_Overlapped));  
			ZeroMemory( m_szBuffer,MAX_BUFFER_LEN );
			m_sockAccept = INVALID_SOCKET;
			m_wsaBuf.buf = m_szBuffer;
			m_wsaBuf.len = MAX_BUFFER_LEN;
			m_OpType     = NULL_POSTED;

			m_nTotalBytes	= 0;
			m_nSendBytes	= 0;
		}
		//析构函数
		~_PER_IO_CONTEXT(){
			if( m_sockAccept!=INVALID_SOCKET ){
				closesocket(m_sockAccept);
				m_sockAccept = INVALID_SOCKET;
			}
		}
		//重置缓冲区内容
		void ResetBuffer(){
			ZeroMemory( m_szBuffer,MAX_BUFFER_LEN );
			m_wsaBuf.buf = m_szBuffer;
			m_wsaBuf.len = MAX_BUFFER_LEN;
		}

	} PER_IO_CONTEXT, *PPER_IO_CONTEXT;

}