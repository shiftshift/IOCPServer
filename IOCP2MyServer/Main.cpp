#include <time.h>
#include "MyServer.h"
using namespace MyServer;

int WINAPI WinMain(HINSTANCE hInstance,
	HINSTANCE prevInstance, PSTR cmdLine, int showCmd) 
{
	//初始化随机数
	srand((long)time(0));
	//生成一个引擎实例并执行
	CMyServer app(hInstance);
	g_pServer = &app;
	g_pServer->Init();
	g_pServer->Run();
	g_pServer->ShutDown();
	return 0;
}