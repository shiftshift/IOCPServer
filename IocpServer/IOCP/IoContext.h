#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <vector>
using namespace std;

//缓冲区长度 (1024*8)
#define MAX_BUFFER_LEN (1024*8)

namespace MyServer 
{
	// 在完成端口上投递的I/O操作的类型
	enum class OPERATION_TYPE
	{
		UNKNOWN, // 用于初始化，无意义
		ACCEPT, // 标志投递的Accept操作
		SEND, // 标志投递的是发送操作
		RECV, // 标志投递的是接收操作
	};
	
	//每次套接字操作(如：AcceptEx, WSARecv, WSASend等)对应的数据结构：
	//	OVERLAPPED结构(标识本次操作)，关联的套接字，缓冲区，操作类型；
	struct IoContext
	{
		OVERLAPPED m_Overlapped; // 每一个重叠网络操作的重叠结构
		SOCKET m_sockAccept; // 这个网络操作所使用的Socket
		WSABUF m_wsaBuf; // WSA类型的缓冲区，用于给重叠操作传参数的
		char m_szBuffer[MAX_BUFFER_LEN]; // 这个是WSABUF里具体存字符的缓冲区
		OPERATION_TYPE m_OpType; // 标识网络操作的类型(对应上面的枚举)
		DWORD m_nTotalBytes; //数据总的字节数
		DWORD m_nSendBytes;	//已经发送的字节数，如未发送数据则设置为0

		//构造函数
		IoContext()
		{
			this->m_OpType = OPERATION_TYPE::UNKNOWN;
			ZeroMemory(&m_Overlapped, sizeof(m_Overlapped));
			ZeroMemory(m_szBuffer, MAX_BUFFER_LEN);
			this->m_sockAccept = INVALID_SOCKET;
			this->m_wsaBuf.len = MAX_BUFFER_LEN;
			this->m_wsaBuf.buf = m_szBuffer;

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
			this->m_wsaBuf.len = MAX_BUFFER_LEN;
			this->m_wsaBuf.buf = m_szBuffer;
		}
	};
}