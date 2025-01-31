/*!
 * \file ParserAdapter.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 
 */
#include "ParserAdapter.h"
#include "WtEngine.h"
#include "WtCtaTicker.h"
#include "WtHelper.h"

#include "../Share/DLLHelper.hpp"
#include "../Share/CodeHelper.hpp"
#include "../Share/TimeUtils.hpp"
#include "../Includes/WTSContractInfo.hpp"
#include "../Includes/WTSDataDef.hpp"
#include "../Includes/WTSVariant.hpp"
#include "../Includes/IBaseDataMgr.h"
#include "../Includes/IHotMgr.h"
#include "../Share/StdUtils.hpp"

#include "../WTSTools/WTSLogger.h"

USING_NS_OTP;

//////////////////////////////////////////////////////////////////////////
//ParserAdapter
ParserAdapter::ParserAdapter()
	: _parser_api(NULL)
	, _remover(NULL)
	, _stopped(false)
	, _bd_mgr(NULL)
	, _stub(NULL)
	, _cfg(NULL)
{
}


ParserAdapter::~ParserAdapter()
{
}

bool ParserAdapter::init(const char* id, WTSVariant* cfg, IParserStub* stub, IBaseDataMgr* bgMgr, IHotMgr* hotMgr/* = NULL*/)
{
	if (cfg == NULL)
		return false;

	_stub = stub;
	_bd_mgr = bgMgr;
	_hot_mgr = hotMgr;
	_id = id;

	if (_cfg != NULL)
		return false;

	_cfg = cfg;
	_cfg->retain();

	{
		//加载模块
		if (cfg->getString("module").empty())
			return false;

		const char* module = cfg->getCString("module");

		//先看工作目录下是否有行情模块
		std::string dllpath = WtHelper::getModulePath(module, "parsers", true);
		//如果没有,则再看模块目录,即dll同目录下
		if(!StdFile::exists(dllpath.c_str()))
			dllpath = WtHelper::getModulePath(module, "parsers", false);

		DllHandle hInst = DLLHelper::load_library(dllpath.c_str());
		if (hInst == NULL)
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[%s] Parser module %s loading failed", _id.c_str(), dllpath.c_str());
			return false;
		}

		FuncCreateParser pFuncCreateParser = (FuncCreateParser)DLLHelper::get_symbol(hInst, "createParser");
		if (NULL == pFuncCreateParser)
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_FATAL, "[%s] Entrance function createParser not found", _id.c_str());
			return false;
		}

		_parser_api = pFuncCreateParser();
		if (NULL == _parser_api)
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_FATAL, "[%s] Creating parser api failed", _id.c_str());
			return false;
		}

		_remover = (FuncDeleteParser)DLLHelper::get_symbol(hInst, "deleteParser");
	}
	

	const std::string& strFilter = cfg->getString("filter");
	if (!strFilter.empty())
	{
		const StringVector &ayFilter = StrUtil::split(strFilter, ",");
		auto it = ayFilter.begin();
		for (; it != ayFilter.end(); it++)
		{
			_exchg_filter.insert(*it);
		}
	}

	std::string strCodes = cfg->getString("code");
	if (!strCodes.empty())
	{
		const StringVector &ayCodes = StrUtil::split(strCodes, ",");
		auto it = ayCodes.begin();
		for (; it != ayCodes.end(); it++)
		{
			_code_filter.insert(*it);
		}
	}

	if (_parser_api)
	{
		_parser_api->registerSpi(this);

		WTSParams* params = cfg->toParams();
		if (_parser_api->init(params))
		{
			ContractSet contractSet;
			if (!_code_filter.empty())//优先判断合约过滤器
			{
				ExchgFilter::iterator it = _code_filter.begin();
				for (; it != _code_filter.end(); it++)
				{
					//全代码,形式如SSE.600000,期货代码为CFFEX.IF2005
					std::string code, exchg;
					auto ay = StrUtil::split((*it).c_str(), ".");
					if (ay.size() == 1)
						code = ay[0];
					else
					{
						exchg = ay[0];
						code = ay[1];
					}
					WTSContractInfo* contract = _bd_mgr->getContract(code.c_str(), exchg.c_str());
					WTSCommodityInfo* pCommInfo = _bd_mgr->getCommodity(contract);
					contractSet.insert(contract->getFullCode());
				}
			}
			else if (!_exchg_filter.empty())
			{
				ExchgFilter::iterator it = _exchg_filter.begin();
				for (; it != _exchg_filter.end(); it++)
				{
					WTSArray* ayContract =_bd_mgr->getContracts((*it).c_str());
					WTSArray::Iterator it = ayContract->begin();
					for (; it != ayContract->end(); it++)
					{
						WTSContractInfo* contract = STATIC_CONVERT(*it, WTSContractInfo*);
						WTSCommodityInfo* pCommInfo = _bd_mgr->getCommodity(contract);
						contractSet.insert(contract->getFullCode());
					}

					ayContract->release();
				}
			}
			else
			{
				WTSArray* ayContract =_bd_mgr->getContracts();
				WTSArray::Iterator it = ayContract->begin();
				for (; it != ayContract->end(); it++)
				{
					WTSContractInfo* contract = STATIC_CONVERT(*it, WTSContractInfo*);
					WTSCommodityInfo* pCommInfo =_bd_mgr->getCommodity(contract);
					contractSet.insert(contract->getFullCode());
				}

				ayContract->release();
			}

			_parser_api->subscribe(contractSet);
			contractSet.clear();
		}
		else
		{
			WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[%s] Parser initializing failed: api initializing failed...", _id.c_str());
		}

		params->release();
	}
	else
	{
		WTSLogger::log_dyn("parser", _id.c_str(), LL_ERROR, "[%s] Parser initializing failed: creating api failed...", _id.c_str());
	}

	return true;
}

void ParserAdapter::release()
{
	_stopped = true;
	if (_parser_api)
	{
		_parser_api->release();
	}

	_remover(_parser_api);
}

bool ParserAdapter::run()
{
	if (_parser_api == NULL)
		return false;

	_parser_api->connect();
	return true;
}

void ParserAdapter::handleQuote(WTSTickData *quote, bool bNeedSlice)
{
	if (_stopped)
		return;

	if (!_exchg_filter.empty() && (_exchg_filter.find(quote->exchg()) == _exchg_filter.end()))
		return;

	if (quote->actiondate() == 0 || quote->tradingdate() == 0)
		return;

	bool isHot = false;

	WTSContractInfo* cInfo = _bd_mgr->getContract(quote->code(), quote->exchg());
	WTSCommodityInfo* commInfo = _bd_mgr->getCommodity(cInfo);
	std::string stdCode;
	if (commInfo->getCategoty() == CC_Future)
	{
		stdCode = CodeHelper::bscFutCodeToStdCode(cInfo->getCode(), cInfo->getExchg());
		std::string hotCode = _hot_mgr->getHotCode(quote->exchg(), quote->code(), quote->tradingdate());
		isHot = !hotCode.empty();
	}
	else if(commInfo->getCategoty() == CC_Stock)
	{
		stdCode = CodeHelper::bscStkCodeToStdCode(cInfo->getCode(), cInfo->getExchg());
	}
	else if (commInfo->getCategoty() == CC_ETFOption || commInfo->getCategoty() == CC_SpotOption)
	{
		stdCode = CodeHelper::bscStkCodeToStdCode(cInfo->getCode(), cInfo->getExchg(), commInfo->getProduct());
	}
	else if (commInfo->getCategoty() == CC_FutOption)
	{
		stdCode = CodeHelper::bscFutOptCodeToStdCode(cInfo->getCode(), cInfo->getExchg());
	}
	quote->setCode(stdCode.c_str());

	_stub->handle_push_quote(quote, isHot);
}

void ParserAdapter::handleOrderQueue(WTSOrdQueData* ordQueData)
{
	if (_stopped)
		return;

	if (!_exchg_filter.empty() && (_exchg_filter.find(ordQueData->exchg()) == _exchg_filter.end()))
		return;

	if (ordQueData->actiondate() == 0 || ordQueData->tradingdate() == 0)
		return;

	WTSContractInfo* cInfo = _bd_mgr->getContract(ordQueData->code(), ordQueData->exchg());
	if (cInfo == NULL)
		return;

	WTSCommodityInfo* commInfo = _bd_mgr->getCommodity(cInfo);
	std::string stdCode = CodeHelper::bscStkCodeToStdCode(cInfo->getCode(), cInfo->getExchg());
	ordQueData->setCode(stdCode.c_str());

	if (_stub)
		_stub->handle_push_order_queue(ordQueData);
}

void ParserAdapter::handleOrderDetail(WTSOrdDtlData* ordDtlData)
{
	if (_stopped)
		return;

	if (!_exchg_filter.empty() && (_exchg_filter.find(ordDtlData->exchg()) == _exchg_filter.end()))
		return;

	if (ordDtlData->actiondate() == 0 || ordDtlData->tradingdate() == 0)
		return;

	WTSContractInfo* cInfo = _bd_mgr->getContract(ordDtlData->code(), ordDtlData->exchg());
	if (cInfo == NULL)
		return;

	WTSCommodityInfo* commInfo = _bd_mgr->getCommodity(cInfo);
	std::string stdCode = CodeHelper::bscStkCodeToStdCode(cInfo->getCode(), cInfo->getExchg());
	ordDtlData->setCode(stdCode.c_str());

	if (_stub)
		_stub->handle_push_order_detail(ordDtlData);
}

void ParserAdapter::handleTransaction(WTSTransData* transData)
{
	if (_stopped)
		return;

	if (!_exchg_filter.empty() && (_exchg_filter.find(transData->exchg()) == _exchg_filter.end()))
		return;

	if (transData->actiondate() == 0 || transData->tradingdate() == 0)
		return;

	WTSContractInfo* cInfo = _bd_mgr->getContract(transData->code(), transData->exchg());
	if (cInfo == NULL)
		return;

	WTSCommodityInfo* commInfo = _bd_mgr->getCommodity(cInfo);
	std::string stdCode = CodeHelper::bscStkCodeToStdCode(cInfo->getCode(), cInfo->getExchg());
	transData->setCode(stdCode.c_str());

	if (_stub)
		_stub->handle_push_transaction(transData);
}


void ParserAdapter::handleParserLog(WTSLogLevel ll, const char* format, ...)
{
	if (_stopped)
		return;

	va_list args;
	va_start(args, format);
	WTSLogger::vlog_dyn("parser", _id.c_str(), ll, format, args);
	va_end(args);
}


//////////////////////////////////////////////////////////////////////////
//ParserAdapterMgr
void ParserAdapterMgr::release()
{
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)
	{
		it->second->release();
	}

	_adapters.clear();
}

bool ParserAdapterMgr::addAdapter(const char* id, ParserAdapterPtr& adapter)
{
	if (adapter == NULL || strlen(id) == 0)
		return false;

	auto it = _adapters.find(id);
	if (it != _adapters.end())
	{
		WTSLogger::error(" Same name of parsers: %s", id);
		return false;
	}

	_adapters[id] = adapter;

	return true;
}


ParserAdapterPtr ParserAdapterMgr::getAdapter(const char* tname)
{
	auto it = _adapters.find(tname);
	if (it != _adapters.end())
	{
		return it->second;
	}

	return ParserAdapterPtr();
}

void ParserAdapterMgr::run()
{
	for (auto it = _adapters.begin(); it != _adapters.end(); it++)
	{
		it->second->run();
	}

	WTSLogger::info("%u parsers started", _adapters.size());
}