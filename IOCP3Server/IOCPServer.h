//IOCPServer.h : Ӧ�ó������ͷ�ļ�
#pragma once
// �йش����ʵ�֣������ IOCPServer.cpp
class CMyServerApp : public CWinApp
{
public:
	CMyServerApp();

public:
	virtual BOOL InitInstance();
	// ʵ��
	DECLARE_MESSAGE_MAP()
};