//#include <vld.h>
#include "ServerEngine/MyServerEngine.h"

using namespace MyServer;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,PSTR cmdLine, int showCmd){
	//初始化随机数
	srand(timeGetTime());

	//生成一个引擎实例并执行
	MyServerEngine app(hInstance);
	g_pServerEngine = &app;
	g_pServerEngine->Init();
	g_pServerEngine->Run();
	g_pServerEngine->ShutDown();

	return 0;

}