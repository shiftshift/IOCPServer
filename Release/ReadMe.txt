今天测试，开启无数个客户端，可以将socket数量，达到10K多。


查看链接数：netstat -ano | find /c "192.168.0.13:10240"
杀进程：taskkill /f /im ClientTest.exe
查端口是否畅通：telnet 127.0.0.1 10240