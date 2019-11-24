/*==========================================================================
Purpose:
* �����CClient�Ǳ�����ĺ����࣬���ڲ�������ָ���Ĳ����߳���ָ������������
 ��Ϣ�����Է���������Ӧ����Դռ�����������ʹ����MFC�Ի������������˵��
Notes:
* �ͻ���ʹ�õ�����򵥵Ķ��߳�����ʽSocket������ÿ���߳�ֻ����һ������
 �����Ҫ�����޸ĳɷ��Ͷ�����ݵ����
==========================================================================*/
#pragma once
#include <string>
using std::string;
// ����deprecation����
#pragma warning(disable: 4996)
// ����������(8*1024�ֽ�)
#define MAX_BUFFER_LEN (4*1024)//(8*1024)
#define DEFAULT_PORT 10240 // Ĭ�϶˿�
#define DEFAULT_IP _T("127.0.0.1") // Ĭ��IP��ַ
#define DEFAULT_THREADS 10000 // Ĭ�ϲ����߳���
#define DEFAULT_TIMES 10000 // Ĭ�Ϸ��ʹ���
#define DEFAULT_MESSAGE _T("Hello!") // Ĭ�ϵķ�����Ϣ
typedef void (*LOG_FUNC)(const string& strInfo);

class CClient;

// ���ڷ������ݵ��̲߳���
struct WorkerThreadParam
{
	CClient* pClient; // ��ָ�룬���ڵ������еĺ���
	SOCKET sock; // ÿ���߳�ʹ�õ�Socket
	int nThreadNo; // �̱߳��
	int nSendTimes; // ���ʹ���
	char szSendBuffer[MAX_BUFFER_LEN];
	char szRecvBuffer[MAX_BUFFER_LEN];
};

// ����Socket���ӵ��߳�
struct ConnectionThreadParam
{
	CClient* pClient; // ��ָ�룬���ڵ������еĺ���
};

class CClient
{
public:
	CClient(void);
	~CClient(void);

public:
	// ����Socket��
	bool LoadSocketLib();
	// ж��Socket�⣬��������
	void UnloadSocketLib() { WSACleanup(); }
	// ��ʼ����
	bool Start();
	//	ֹͣ����
	void Stop();
	// ��ñ�����IP��ַ
	CString GetLocalIP();
	// ��������IP��ַ
	void SetIP(const CString& strIP) { m_strServerIP = strIP; }
	// ���ü����˿�
	void SetPort(const int& nPort) { m_nPort = nPort; }
	// ���ò����̷߳��ʹ���
	void SetTimes(const int& n) { m_nTimes = n; }
	// ���ò����߳�����
	void SetThreads(const int& n) { m_nThreads = n; }
	// ����Ҫ�����͵���Ϣ
	void SetMessage(const CString& strMessage) { m_strMessage = strMessage; }
	// �����������ָ�룬���ڵ����亯��
	void SetMainDlg(CDialog* p) { m_pMain = p; }

private:
	// ��������
	bool EstablishConnections();
	// ���������������
	bool ConnectToServer(SOCKET& pSocket, CString strServer, int nPort);
	// ���ڽ������ӵ��߳�
	static DWORD WINAPI _ConnectionThread(LPVOID lpParam);
	// ���ڷ�����Ϣ���߳�
	static DWORD WINAPI _WorkerThread(LPVOID lpParam);
	// �ͷ���Դ
	void CleanUp();

private:
	//LOG_FUNC m_LogFunc;
	CDialog* m_pMain; // ����ָ��
	CString m_strServerIP; // �������˵�IP��ַ
	CString m_strLocalIP; // ����IP��ַ
	CString m_strMessage; // ��������������Ϣ
	int m_nPort; // �����˿�
	int m_nTimes; // �����̷߳��ʹ���
	int m_nThreads; // �����߳�����
	//HANDLE* m_phWorkerThreads; //���й����߳�
	//DWORD* m_pWorkerThreadIds; //���й����̵߳�ID
	WorkerThreadParam* m_pWorkerParams; // �̲߳���
	HANDLE m_hConnectionThread; // �������ӵ��߳̾��
	HANDLE m_hShutdownEvent; // ֪ͨ�̣߳�Ϊ�˸��õ��˳��߳�
	LONG m_nRunningWorkerThreads; // �������еĲ����߳�����
	//ʹ���̳߳أ����в���
	TP_CALLBACK_ENVIRON te;
	PTP_POOL threadPool;
	PTP_CLEANUP_GROUP cleanupGroup;
	PTP_WORK* pWorks;

	static void NTAPI CClient::poolThreadWork(
		_Inout_ PTP_CALLBACK_INSTANCE Instance,
		_Inout_opt_ PVOID Context,
		_Inout_ PTP_WORK Work);

public:
	// ������������ʾ��Ϣ
	void ShowMessage(const char* szFormat, ...);
	//void SetLogFunc(LOG_FUNC fn) { m_LogFunc = fn; }
};
