#pragma once

#include <string>

using namespace std;

/************************************************************************/
/* Desc : 工具类 
/* Author : thjie
/* Date : 2013-02-28
/************************************************************************/
namespace MyServer{
	class pub{
	public:
		//string 转换为 float
		static float str2float(string str);

		//string 转换为 int
		static int str2int( string str);

		//int转字符串
		static string Int2Str(int num);

		//float转字符串
		static string Float2Str(float num,int round = 0);
	};

}
