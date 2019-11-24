#pragma once
#include "iocp\IocpModel.h"
#include <string>
#include <vector>
using namespace std;

#define	IDC_hLB_Output				40002
#define	IDC_hEB_InputServerIP		40003
#define	IDC_hEB_InputServerPort		40004
#define	IDC_hST_TextServerIP		40006
#define	IDC_hST_TextServerPort		40007
#define	IDC_hBtn_Start				50000
#define	IDC_hBtn_Stop				50001
#define	IDC_hBtn_Exit				50002

class CMyServer
{
protected:
	HINSTANCE mhAppInst; //ʵ��
	HWND mhMainWnd; //�����ھ��
	LPCTSTR lpszAppName; //������
	LPCTSTR lpszTitle; //����
	int mServerPort; //����˿�
	CIocpModel m_IOCP; // ��Ҫ������ɶ˿�ģ��	
	HWND hLB_Output;
	HWND hST_TextServerIP;
	HWND hEB_InputServerIP;
	HWND hST_TextServerPort;
	HWND hEB_InputServerPort;
	HWND hBtnStart; //��ʼ����
	HWND hBtnStop; //ֹͣ����
	HWND hBtnExit; //�˳���ť
	static HANDLE m_hMutex; //�������
	static vector<string> m_vtServerMsgs; //Ҫ��ʾ����Ϣ

public:
	CMyServer(HINSTANCE hInstance);
	virtual ~CMyServer();
	HINSTANCE getAppInst();	 //����ʵ��
	HWND getMainWnd();	 //���������ھ��
	virtual void Init(); //��ʼ��
	virtual void InitMainWindow(); //��ʼ��������
	virtual void InitControls(HWND hWnd); //��ʼ���������еĿؼ�
	virtual int Run(); //����
	virtual void ShutDown(); //�ر�
	virtual void SettleServerMsgs(); //�����������Ϣ
	//�����ڴ�����
	virtual LRESULT msgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	static void AddServerMsgs(const string& msg); //��ӷ�������Ϣ

protected:
	virtual void ShowText(string& msg); //��ʾ��Ϣ
};