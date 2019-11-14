这是客户端的服务器压力测试工具。

发现有个崩溃：
当点“开始测试”后，立即点“停止测试”。
由于Context列表会立即销毁掉，而线程还在使用，从而就导致了崩溃！
CClient::Stop()中的WaitForSingleObject/WaitForMultipleObjects并没卡住。

发现有个卡住不动：
连接失败的时，停止后，再连接，整个应用就卡住了。