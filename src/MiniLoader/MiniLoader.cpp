#include <string>
#include <map>
//v6.3.15
#include "./ThostTraderApi/ThostFtdcTraderApi.h"
#include "TraderSpi.h"

#include "../Share/IniHelper.hpp"
#include "../Share/StrUtil.hpp"
#include "../Share/StdUtils.hpp"
#include "../Share/DLLHelper.hpp"
#include <boost/filesystem.hpp>


// UserApi对象
CThostFtdcTraderApi* pUserApi;

// 配置参数
std::string	FRONT_ADDR;	// 前置地址
std::string	BROKER_ID;	// 经纪公司代码
std::string	INVESTOR_ID;// 投资者代码
std::string	PASSWORD;	// 用户密码
std::string SAVEPATH;	//保存位置
std::string APPID;
std::string AUTHCODE;
uint32_t	CLASSMASK;	//期权

std::string COMM_FILE;		//输出的品种文件名
std::string CONT_FILE;		//输出的合约文件名

std::string MODULE_NAME;	//外部模块名

typedef std::map<std::string, std::string>	SymbolMap;
SymbolMap	MAP_NAME;
SymbolMap	MAP_SESSION;

typedef CThostFtdcTraderApi* (*CTPCreator)(const char *);
CTPCreator		g_ctpCreator = NULL;

// 请求编号
int iRequestID = 0;

std::string getBaseFolder()
{
	static std::string basePath;
	if(basePath.empty())
	{
#ifdef _WIN32
		char strPath[MAX_PATH];
		GetModuleFileName(NULL, strPath, MAX_PATH);

		basePath = StrUtil::standardisePath(strPath, false);

		uint32_t nPos = basePath.find_last_of('/');
		basePath = basePath.substr(0, nPos + 1);
#else

		char path[1024];
		int cnt = readlink("/proc/self/exe", path, 1024);
		//最后一个'/' 后面是可执行程序名,去掉可执行程序的名字,只保留路径
		for (int i = cnt; i >= 0; --i)
		{
			if (path[i] == '/')
			{
				path[i + 1] = '\0';
				break;
			}
		}
		basePath = path;
		basePath = StrUtil::standardisePath(basePath, false);
#endif
	}

	return basePath;
}

int main()
{
	std::string cfg = "config.ini";
	IniHelper ini;
	ini.load(cfg.c_str());

	FRONT_ADDR = ini.readString("ctp", "front", "");
	BROKER_ID	= ini.readString("ctp", "broker", "");
	INVESTOR_ID = ini.readString("ctp", "user", "");
	PASSWORD	= ini.readString("ctp", "pass", "");
	APPID = ini.readString("ctp", "appid", "");
	AUTHCODE = ini.readString("ctp", "authcode", "");

	SAVEPATH	= ini.readString("config", "path", "");
	CLASSMASK = ini.readUInt("config", "mask", 1|2|4); //1-期货,2-期权,4-股票

	COMM_FILE = ini.readString("config", "commfile", "commodities.json");
	CONT_FILE = ini.readString("config", "contfile", "contracts.json");

#ifdef _WIN32
	MODULE_NAME = ini.readString("config", "module", "thosttraderapi.dll");
#else
	MODULE_NAME = ini.readString("config", "module", "thosttraderapi.so");
#endif
	if(!boost::filesystem::exists(MODULE_NAME.c_str()))
	{
#ifdef _WIN32
		MODULE_NAME = getBaseFolder() + "traders/thosttraderapi.dll";
#else
		MODULE_NAME = getBaseFolder() + "traders/thosttraderapi.so";
#endif
	}

	if(FRONT_ADDR.empty() || BROKER_ID.empty() || INVESTOR_ID.empty() || PASSWORD.empty() || SAVEPATH.empty())
	{
		return 0;
	}

	SAVEPATH = StrUtil::standardisePath(SAVEPATH);
	std::string map_files = ini.readString("config", "mapfiles", "");
	if (!map_files.empty())
	{
		StringVector ayFiles = StrUtil::split(map_files, ",");
		for (const std::string& fName : ayFiles)
		{
			printf("开始读取映射文件%s...", fName.c_str());
			IniHelper iniMap;
			if (!StdFile::exists(fName.c_str()))
				continue;

			iniMap.load(fName.c_str());
			FieldArray ayKeys, ayVals;
			int cout = iniMap.readSecKeyValArray("Name", ayKeys, ayVals);
			for (int i = 0; i < cout; i++)
			{
				MAP_NAME[ayKeys[i]] = ayVals[i];
				printf("品种名称映射: %s - %s\r\n", ayKeys[i].c_str(), ayVals[i].c_str());
			}

			ayKeys.clear();
			ayVals.clear();
			cout = iniMap.readSecKeyValArray("Session", ayKeys, ayVals);
			for (int i = 0; i < cout; i++)
			{
				MAP_SESSION[ayKeys[i]] = ayVals[i];
				printf("交易时间映射: %s - %s\r\n", ayKeys[i].c_str(), ayVals[i].c_str());
			}
		}
	}

	// 初始化UserApi
	DllHandle dllInst = DLLHelper::load_library(MODULE_NAME.c_str());
#ifdef _WIN32
#	ifdef _WIN64
	const char* creatorName = "?CreateFtdcTraderApi@CThostFtdcTraderApi@@SAPEAV1@PEBD@Z";
#	else
	const char* creatorName = "?CreateFtdcTraderApi@CThostFtdcTraderApi@@SAPAV1@PBD@Z";
#	endif
#else
	const char* creatorName = "_ZN19CThostFtdcTraderApi19CreateFtdcTraderApiEPKc";
#endif
	g_ctpCreator = (CTPCreator)DLLHelper::get_symbol(dllInst, creatorName);
	pUserApi = g_ctpCreator("");			// 创建UserApi
	CTraderSpi* pUserSpi = new CTraderSpi();
	pUserApi->RegisterSpi((CThostFtdcTraderSpi*)pUserSpi);			// 注册事件类
	pUserApi->SubscribePublicTopic(THOST_TERT_QUICK);					// 注册公有流
	pUserApi->SubscribePrivateTopic(THOST_TERT_QUICK);					// 注册私有流
	pUserApi->RegisterFront((char*)FRONT_ADDR.c_str());				// connect
	pUserApi->Init();

	pUserApi->Join();
	return 0;
}