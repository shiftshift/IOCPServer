#include <time.h>
#include "MyServer.h"

int WINAPI WinMain(HINSTANCE hInstance,
	HINSTANCE prevInstance, PSTR cmdLine, int showCmd) 
{
	//��ʼ�������
	srand((long)time(0));
	//����һ������ʵ����ִ��
	CMyServer app(hInstance);
	app.Init();
	app.Run();
	app.ShutDown();
	return 0;
}