#include "pub.h"

namespace MyServer{

	//string 转换为 float
	float pub::str2float(string str){
		float f = (float)atof(str.c_str());
		return f;
	}

	//string 转换为 int
	int pub::str2int( string str){
		int rr = atoi(str.c_str());
		return rr;
	}

	//int转字符串
	string pub::Int2Str(int num){
		char aa[20];
		sprintf_s(aa,"%d",num);
		string ss = aa;
		return ss;
	}

	//float转字符串
	string pub::Float2Str(float num,int round){
		char aa[20];
		string format = "%."+Int2Str(round)+"f";
		sprintf_s(aa,format.c_str(),num);
		string ss = aa;
		return ss;
	}

}

