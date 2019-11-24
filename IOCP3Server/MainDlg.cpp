#include "StdAfx.h"
#include "IOCPServer.h"
#include "MainDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

HWND g_hWnd = NULL;

// ����Ӧ�ó��򡰹��ڡ��˵���� CAboutDlg �Ի���
class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

	// �Ի�������
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

// ʵ��
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg(): CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

// CMainDlg �Ի���
CMainDlg::CMainDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CMainDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMainDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CMainDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDOK, &CMainDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDC_STOP, &CMainDlg::OnBnClickedStop)
	ON_BN_CLICKED(IDCANCEL, &CMainDlg::OnBnClickedCancel)
	ON_MESSAGE(WM_ADD_LIST_ITEM, OnAddListItem)
	ON_WM_DESTROY()
	ON_WM_CLOSE()
END_MESSAGE_MAP()

// CMainDlg ��Ϣ�������
BOOL CMainDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	// ��������...���˵�����ӵ�ϵͳ�˵��С�
	// IDM_ABOUTBOX ������ϵͳ���Χ�ڡ�
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);
	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}
	// ���ô˶Ի����ͼ�ꡣ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��
	// ��ʼ��������Ϣ
	this->Init();
	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
}

void CMainDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�
void CMainDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������
		SendMessage(WM_ICONERASEBKGND, 
			reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);
		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;
		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CMainDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

//////////////////////////////////////////////////////////////////////
// ��ʼ��Socket���Լ�������Ϣ
void CMainDlg::Init()
{
	// ��ʼ��Socket��
	if (!m_IOCP.LoadSocketLib())
	{
		AfxMessageBox(_T("����Winsock 2.2ʧ�ܣ����������޷����У�"));
		PostQuitMessage(0);
	}

	// ���ñ���IP��ַ
	SetDlgItemText(IDC_STATIC_SERVERIP, m_IOCP.GetLocalIP().c_str());
	// ����Ĭ�϶˿�
	SetDlgItemInt(IDC_EDIT_PORT, DEFAULT_PORT);
	// ��ʼ���б�
	this->InitListCtrl();
	g_hWnd = this->m_hWnd;
	LPVOID pfn = (LPVOID)AddInformation;
	m_IOCP.SetLogFunc((LOG_FUNC)pfn);
}

///////////////////////////////////////////////////////////////////////
//	��ʼ����
void CMainDlg::OnBnClickedOk()
{
	if (!m_IOCP.Start())
	{
		AfxMessageBox(_T("����������ʧ�ܣ�"));
		return;
	}

	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_INFO);
	pList->DeleteAllItems();
	GetDlgItem(IDOK)->EnableWindow(FALSE);
	GetDlgItem(IDC_STOP)->EnableWindow(TRUE);
}

//////////////////////////////////////////////////////////////////////
//	��������
void CMainDlg::OnBnClickedStop()
{
	m_IOCP.Stop();
	GetDlgItem(IDC_STOP)->EnableWindow(FALSE);
	GetDlgItem(IDOK)->EnableWindow(TRUE);
}

///////////////////////////////////////////////////////////////////////
//	��ʼ��List Control
void CMainDlg::InitListCtrl()
{
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_INFO);
	pList->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	pList->InsertColumn(0, "INFORMATION", LVCFMT_LEFT, 500);
}

///////////////////////////////////////////////////////////////////////
//	������˳�����ʱ��ֹͣ���������Socket���
void CMainDlg::OnBnClickedCancel()
{
	// ֹͣ����
	m_IOCP.Stop();
	m_IOCP.UnloadSocketLib();
	CDialog::OnCancel();
}

///////////////////////////////////////////////////////////////////////
//	ϵͳ�˳���ʱ��Ϊȷ����Դ�ͷţ�ֹͣ���������Socket���
void CMainDlg::OnDestroy()
{
	OnBnClickedCancel();
	CDialog::OnDestroy();
}

LRESULT CMainDlg::OnAddListItem(WPARAM wParam, LPARAM lParam)
{
	string* pStr = ((string*)lParam);
	CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST_INFO);
	pList->InsertItem(0, (*pStr).c_str());
	delete pStr;
	return 0;
}

void CMainDlg::AddInformation(const string& strInfo)
{
	if (g_hWnd)
	{
		string* pStr = new string(strInfo);
		::PostMessage(g_hWnd, WM_ADD_LIST_ITEM, 0,
			(LPARAM)pStr);
	}
}