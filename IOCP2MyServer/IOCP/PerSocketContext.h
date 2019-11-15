#pragma once
#include "PerIoContext.h"

namespace MyServer
{
	//每个SOCKET对应的数据结构(调用GetQueuedCompletionStatus传入)：
	//	-SOCKET，该SOCKET对应的客户端地址，
	//	作用在该SOCKET操作集合(对应结构IoContext)；
	struct SocketContext
	{
		SOCKET m_Socket; //连接客户端的socket
		SOCKADDR_IN m_ClientAddr; //客户端地址
		//套接字操作，本例是WSARecv和WSASend共用一个IoContext
		vector<IoContext*> m_arrayIoContext;

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
			for (DWORD i = 0; i < m_arrayIoContext.size(); i++)
			{
				delete m_arrayIoContext.at(i);
			}
			m_arrayIoContext.clear();
		}

		//进行套接字操作时，调用此函数返回PER_IO_CONTEX结构
		IoContext* GetNewIoContext()
		{
			IoContext* p = new IoContext;
			m_arrayIoContext.push_back(p);
			return p;
		}

		// 从数组中移除一个指定的IoContext
		void RemoveContext(IoContext* pContext)
		{
			if (pContext == nullptr)
			{
				return;
			}
			vector<IoContext*>::iterator it;
			it = m_arrayIoContext.begin();
			while (it != m_arrayIoContext.end())
			{
				IoContext* pObj = *it;
				if (pContext == pObj)
				{
					delete pContext;
					pContext = NULL;
					it = m_arrayIoContext.erase(it);
					break;
				}
				it++;
			}
		}
	};
}
