#define _WIN32_WINNT _WIN32_WINNT_VISTA
#include "BonDriverProxyExD.h"
#include "StringUtil.h"
#include "IB25Decoder.h"

//#include <fstream>


#if _DEBUG
#define DETAILLOG	0
#define DETAILLOG2	1
#endif

#ifdef HAVE_UI
#define WM_TASKTRAY			(WM_USER + 1)
#define ID_TASKTRAY			0
#define ID_TASKTRAY_SHOW	1
#define ID_TASKTRAY_HIDE	2
#define ID_TASKTRAY_RELOAD	3
#define ID_TASKTRAY_EXIT	4
HINSTANCE g_hInstance;
HWND g_hWnd;
HMENU g_hMenu;
#endif

using namespace std;



//ofstream debugOut("BonDriverProxyExD_debug.txt");

inline static bool CheckInt(const tstring& str, int min, int max)
{
	int n;
	try {
		n = std::stoi(str);
	}
	catch (...) {
		return false;
	}
	return((min <= n) && (n <= max));
}



static int Init(HMODULE hModule)
{
	TCHAR szExePath[MAX_PATH + 16] = _T("");
	GetModuleFileName(hModule, szExePath, MAX_PATH);

	TCHAR szPath[_MAX_PATH];
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	TCHAR szFname[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];
	_tsplitpath_s(szExePath, szDrive, _MAX_DRIVE, szDir, _MAX_DIR, szFname, _MAX_FNAME, szExt, _MAX_EXT);
	_tmakepath_s(szPath, _MAX_PATH, szDrive, szDir, NULL, NULL);

	g_bonDriverPath.setPath(szPath);

	TCHAR iniFilePath[MAX_PATH];
	_tcscpy_s(iniFilePath, MAX_PATH, szPath);
	_tcsncat_s(iniFilePath, MAX_PATH - _tcslen(iniFilePath), szFname, _MAX_FNAME);
	_tcsncat_s(iniFilePath, MAX_PATH - _tcslen(iniFilePath), _T(".ini"), sizeof(_T(".ini")) / sizeof(TCHAR));

	struct _stat64 s;
	if (_tstat64(iniFilePath, &s) < 0) {
		return(-2);
	}


	TCHAR readBuf[1024];
	tstring tstrLeft, tstrRight;
	BOOL remain = FALSE;
	tstring tstrHost, tstrPort;

	// [OPTION]
	GetPrivateProfileString(_T("OPTION"), _T("ADDRESS"), _T("127.0.0.1"), readBuf, 1024, iniFilePath);
	cut_tab_space(tstrHost, readBuf);
	if (tstrHost.empty()) {
		return(-3);
	}

	for (int i = 0; ; i++) {
		remain = Separate(tstrHost, _T(","), tstrLeft, tstrRight);
		if (tstrLeft.empty()) {
			break;
		}
		vector<tstring>::iterator itr = find_if(g_Host.hostVec.begin(), g_Host.hostVec.end(), [tstrLeft](const tstring& s) {return(s.compare(tstrLeft)==0); });
		if (itr == g_Host.hostVec.end()) {
			g_Host.hostVec.emplace_back(tstrLeft);
		}
		else {
			g_Host.hostVec.clear();
			g_Host.hostVec.shrink_to_fit();
			return(-3);
		}
		if (!remain) {
			break;
		}
		else {
			tstrHost = tstrRight;
		}
	}
	if (g_Host.hostVec.size() > MAX_HOSTS) {
		g_Host.hostVec.clear();
		g_Host.hostVec.shrink_to_fit();
		return(-3);
	}
	
	GetPrivateProfileString(_T("OPTION"), _T("PORT"), _T("1192"), readBuf, 1024, iniFilePath);
	cut_tab_space(g_Host.port, readBuf);
	if (!CheckInt(g_Host.port, 1, 65535)) {
		return(-4);
	}
	g_OpenTunerRetDelay = GetPrivateProfileInt(_T("OPTION"), _T("OPENTUNER_RETURN_DELAY"), 0, iniFilePath);
	g_SandBoxedRelease = GetPrivateProfileInt(_T("OPTION"), _T("SANDBOXED_RELEASE"), 0, iniFilePath);
	g_DisableUnloadBonDriver = GetPrivateProfileInt(_T("OPTION"), _T("DISABLE_UNLOAD_BONDRIVER"), 0, iniFilePath);


	// [BONDRIVER_FOLDERS]
	GetPrivateProfileString(_T("BONDRIVER_FOLDERS"), _T("FOLDER"), NULL, readBuf, 1024, iniFilePath);
	g_bonDriverPath.setFolder(readBuf);


	// [DECODER]
	TCHAR seqNo[8];
	tstring decoderName;
	BOOL rtn = TRUE;
	int decoderMax;
	for (decoderMax = 0; ; decoderMax++) {
		_itot_s(decoderMax, seqNo, 8, 10);
		GetPrivateProfileString(_T("DECODER"), seqNo, NULL, readBuf, 1024, iniFilePath);
		cut_tab_space(decoderName, readBuf);
		if (decoderName.size() == 0) {
			break;
		}
		rtn = FALSE;
		// DecoderDLLファイル名
		tstring dllName, emm, nullPacket, round;
		if (Separate(decoderName, _T(","), dllName, tstrRight)) {				// DecoderDll名
			if (Separate(tstrRight, _T(","), emm, tstrRight)) {					// EMM
				if (CheckInt(emm, 0, 1)) {
					if (Separate(tstrRight, _T(","), nullPacket, tstrRight)) {	// Null Packet
						if (CheckInt(nullPacket, 0, 1)) {
							if (tstrRight.size() != 0) {
								round = tstrRight;							// ROUND
								if (CheckInt(round, 0, 100)) {
									stDecoder* dec = new stDecoder(dllName.c_str(), stoi(emm), stoi(nullPacket), stoi(round));
									DecoderVec.emplace_back(dec);
									rtn = TRUE;
								}
							}
						}
					}
				}
			}
		
		}
		if (!rtn) {
			break;
		}
	}
	if (!rtn) {
		return(-5);
	}

	rtn = TRUE;
	for (vector<stDecoder*>::iterator itr = DecoderVec.begin(); itr != DecoderVec.end(); itr++) {
		(*itr)->hModule = LoadLibrary((*itr)->strDecoder);
		if ((*itr)->hModule == NULL) {
			rtn = FALSE;
			break;
		}
		else {
			(*itr)->CreateInstance = (IB25Decoder2 * (*)())::GetProcAddress((*itr)->hModule, "CreateB25Decoder2");
			if ((*itr)->CreateInstance == NULL) {
				rtn = FALSE;
				break;
			}
		}
	}
	if (!rtn) {
		return(-5);
	}

	// [SYSTEM]
	g_PacketFifoSize = GetPrivateProfileInt(_T("SYSTEM"), _T("PACKET_FIFO_SIZE"), 64, iniFilePath);
	g_TsPacketBufSize = GetPrivateProfileInt(_T("SYSTEM"), _T("TSPACKET_BUFSIZE"), (188 * 1024), iniFilePath);

	GetPrivateProfileString(_T("SYSTEM"), _T("PROCESSPRIORITY"), _T("NORMAL"), readBuf, 1024, iniFilePath);
	if (_tcscmp(readBuf, _T("REALTIME")) == 0)
		g_ProcessPriority = REALTIME_PRIORITY_CLASS;
	else if (_tcscmp(readBuf, _T("HIGH")) == 0)
		g_ProcessPriority = HIGH_PRIORITY_CLASS;
	else if (_tcscmp(readBuf, _T("ABOVE_NORMAL")) == 0)
		g_ProcessPriority = ABOVE_NORMAL_PRIORITY_CLASS;
	else if (_tcscmp(readBuf, _T("BELOW_NORMAL")) == 0)
		g_ProcessPriority = BELOW_NORMAL_PRIORITY_CLASS;
	else if (_tcscmp(readBuf, _T("IDLE")) == 0)
		g_ProcessPriority = IDLE_PRIORITY_CLASS;
	else
		g_ProcessPriority = NORMAL_PRIORITY_CLASS;
	SetPriorityClass(GetCurrentProcess(), g_ProcessPriority);

	GetPrivateProfileString(_T("SYSTEM"), _T("THREADPRIORITY_TSREADER"), _T("NORMAL"), readBuf, 1024, iniFilePath);
	if (_tcscmp(readBuf, _T("CRITICAL")) == 0)
		g_ThreadPriorityTsReader = THREAD_PRIORITY_TIME_CRITICAL;
	else if (_tcscmp(readBuf, _T("HIGHEST")) == 0)
		g_ThreadPriorityTsReader = THREAD_PRIORITY_HIGHEST;
	else if (_tcscmp(readBuf, _T("ABOVE_NORMAL")) == 0)
		g_ThreadPriorityTsReader = THREAD_PRIORITY_ABOVE_NORMAL;
	else if (_tcscmp(readBuf, _T("BELOW_NORMAL")) == 0)
		g_ThreadPriorityTsReader = THREAD_PRIORITY_BELOW_NORMAL;
	else if (_tcscmp(readBuf, _T("LOWEST")) == 0)
		g_ThreadPriorityTsReader = THREAD_PRIORITY_LOWEST;
	else if (_tcscmp(readBuf, _T("IDLE")) == 0)
		g_ThreadPriorityTsReader = THREAD_PRIORITY_IDLE;
	else
		g_ThreadPriorityTsReader = THREAD_PRIORITY_NORMAL;

	GetPrivateProfileString(_T("SYSTEM"), _T("THREADPRIORITY_SENDER"), _T("NORMAL"), readBuf, 1024, iniFilePath);
	if (_tcscmp(readBuf, _T("CRITICAL")) == 0)
		g_ThreadPrioritySender = THREAD_PRIORITY_TIME_CRITICAL;
	else if (_tcscmp(readBuf, _T("HIGHEST")) == 0)
		g_ThreadPrioritySender = THREAD_PRIORITY_HIGHEST;
	else if (_tcscmp(readBuf, _T("ABOVE_NORMAL")) == 0)
		g_ThreadPrioritySender = THREAD_PRIORITY_ABOVE_NORMAL;
	else if (_tcscmp(readBuf, _T("BELOW_NORMAL")) == 0)
		g_ThreadPrioritySender = THREAD_PRIORITY_BELOW_NORMAL;
	else if (_tcscmp(readBuf, _T("LOWEST")) == 0)
		g_ThreadPrioritySender = THREAD_PRIORITY_LOWEST;
	else if (_tcscmp(readBuf, _T("IDLE")) == 0)
		g_ThreadPrioritySender = THREAD_PRIORITY_IDLE;
	else
		g_ThreadPrioritySender = THREAD_PRIORITY_NORMAL;

	// [BONDRIVER]
	tstring bonName;
	remain = FALSE;
	vector<tstring> doubleCheck;	// 重複チェック用
	for (int i = 0; ; i++) {
		_itot_s(i, seqNo, 8, 10);
		GetPrivateProfileString(_T("BONDRIVER"), seqNo, NULL, readBuf, 1024, iniFilePath);
		cut_tab_space(bonName, readBuf);
		if (bonName.size() > 0) {
			// 1つ目は BonDriver Group
			tstring tstrGroup;
			if ((rtn = Separate(bonName, _T(","), tstrGroup, tstrRight))) {
				//2つ目以降はBonDriver.dllファイル名:デコーダー番号
				vector<stDriver> vstDriver;
				while (1) {
					rtn = FALSE;
					tstring tstrDllName, tstrDecoderNo;
					remain = Separate(tstrRight, _T(","), tstrLeft, tstrRight);
					if ((rtn = Separate(tstrLeft, _T(":"), tstrDllName, tstrDecoderNo))) {
						if (!CheckInt(tstrDecoderNo, -1, decoderMax-1)) {
							rtn = FALSE;
							break;
						}

						vector<tstring>::iterator itr = find_if(doubleCheck.begin(), doubleCheck.end(), [tstrDllName](const tstring& s) {return(s.compare(tstrDllName) == 0); });
						if (itr == doubleCheck.end()) {
							doubleCheck.emplace_back(tstrDllName);
							
							TCHAR bonPath[1024];
							if (g_bonDriverPath.getBonDriverPath(tstrDllName.c_str(), bonPath)) {
								stDriver dri(bonPath);
								dri.decoderNo = stoi(tstrDecoderNo);
								vstDriver.emplace_back(dri);
							}
							// BonDriver.dllが存在しない時エラーとする
							else {
								rtn = FALSE;
								break;
							}
						// 同じ名前のBonDriver.dllが複数iniに設定されている時エラーとする
						}
						else {
							rtn = FALSE;
							break;
						}
					}
					else {
						break;
					}
					if (!remain) {	// 後続無し
						rtn = TRUE;
						break;
					}
				}
				if (!rtn) {
					break;
				}
#if defined(UNICODE) || defined(_UNICODE)
				string bonGroupName;
				WtoA(tstrGroup, bonGroupName);
#else
				string bonGroupName = strGroup;
#endif

				DriversMap.insert(make_pair(_strdup(bonGroupName.c_str()), vstDriver));
			}
			else {
				break;
			}
		}
		else {
			rtn = (i > 0) ? TRUE : FALSE;
			break;
		}
	}
	doubleCheck.clear();
	doubleCheck.shrink_to_fit();
	if (!rtn) {
		return(-6);
	}

#if _DEBUG && DETAILLOG2
	{
		int i = 0;
		for (map<char*, std::vector<stDriver>>::iterator itm = DriversMap.begin(); itm != DriversMap.end(); itm++, i++) {
			char* p = (*itm).first;
			_RPT2(_CRT_WARN, "%d: %s\n", i, p);
			vector<stDriver>& v = (*itm).second;
			for (vector<stDriver>::iterator itv = v.begin(); itv != v.end(); itv++) {
				_RPT1(_CRT_WARN, "  : %s\n", (*itv).strBonDriver);
			}
		}
	}
#endif

	OSVERSIONINFOEXA osvi = {};
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	osvi.dwMajorVersion = 6;	// >= Vista
	if (VerifyVersionInfoA(&osvi, VER_MAJORVERSION, VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL)))
		g_ThreadExecutionState = ES_SYSTEM_REQUIRED | ES_CONTINUOUS | ES_AWAYMODE_REQUIRED;
	else
		g_ThreadExecutionState = ES_SYSTEM_REQUIRED | ES_CONTINUOUS;

	return 0;
}

#if defined(HAVE_UI) || defined(BUILD_AS_SERVICE)
static void ShutdownInstances()
{
	// シャットダウンイベントトリガ
	if (!g_ShutdownEvent.IsSet())
		g_ShutdownEvent.Set();

	// まず待ち受けスレッドの終了を待つ
	g_Lock.Enter();
	if (g_hListenThread != NULL)
	{
		WaitForSingleObject(g_hListenThread, INFINITE);
		CloseHandle(g_hListenThread);
		g_hListenThread = NULL;
	}
	g_Lock.Leave();

	// 全クライアントインスタンスの終了を待つ
	for (;;)
	{
		// g_InstanceListの数確認でわざわざロックしてるのは、cProxyServerExインスタンスが
		// "リストからは削除されていてもデストラクタが終了していない"状態を排除する為
		g_Lock.Enter();
		size_t num = g_InstanceList.size();
		g_Lock.Leave();
		if (num == 0)
			break;
		Sleep(10);
	}

	// シャットダウンイベントクリア
	g_ShutdownEvent.Reset();
	
}
#endif

static void CleanUp()
{
	for (map<char*, std::vector<stDriver>>::iterator itm = DriversMap.begin(); itm != DriversMap.end(); itm++) {
		char* p = (*itm).first;
		vector<stDriver>& v = (*itm).second;
		for (vector<stDriver>::iterator itv = v.begin(); itv != v.end(); itv++) {
			if ((*itv).hModule != NULL) {
				FreeLibrary((*itv).hModule);
			}
			if ((*itv).strBonDriver) {
				free((*itv).strBonDriver);
			}
		}
		free(p);
		v.clear();
		v.shrink_to_fit();
	}
	DriversMap.clear();

	for (vector<stDecoder*>::iterator itr = DecoderVec.begin(); itr != DecoderVec.end(); itr++) {
		if ((*itr)->hModule) {
			FreeLibrary((*itr)->hModule);
		}
		if ((*itr)->strDecoder) {
			free((*itr)->strDecoder);
		}
		delete&** itr;
	}
	DecoderVec.clear();
	DecoderVec.shrink_to_fit();

	g_bonDriverPath.freeMemory();

	g_Host.hostVec.clear();
	g_Host.hostVec.shrink_to_fit();
	g_Host.port = _T("");

}

cProxyServerEx::cProxyServerEx() : m_Error(TRUE, FALSE)
{
	m_s = INVALID_SOCKET;
	m_hModule = NULL;
	m_pIBon = m_pIBon2 = m_pIBon3 = NULL;
	m_bTunerOpen = FALSE;
	m_bChannelLock = 0;
	m_hTsRead = NULL;
	m_pTsReaderArg = NULL;
	m_dwSpace = m_dwChannel = 0x7fffffff;	// INT_MAX
	m_pDriversMapKey = NULL;
	m_iDriverNo = -1;
	m_iDriverUseOrder = 0;
}

cProxyServerEx::~cProxyServerEx()
{
	LOCK(g_Lock);
	BOOL bRelease = TRUE;
	std::list<cProxyServerEx *>::iterator it = g_InstanceList.begin();
	while (it != g_InstanceList.end())
	{
		if (*it == this)
			g_InstanceList.erase(it++);
		else
		{
			if ((m_hModule != NULL) && (m_hModule == (*it)->m_hModule))
				bRelease = FALSE;
			++it;
		}
	}
	if (bRelease)
	{
		if (m_hTsRead)
		{
			m_pTsReaderArg->StopTsRead = TRUE;
			::WaitForSingleObject(m_hTsRead, INFINITE);
			::CloseHandle(m_hTsRead);
			delete m_pTsReaderArg;
		}

		Release();

		if (m_hModule)
		{
			std::vector<stDriver> &vstDriver = DriversMap[m_pDriversMapKey];
			vstDriver[m_iDriverNo].bUsed = FALSE;
			if (!g_DisableUnloadBonDriver)
			{
				::FreeLibrary(m_hModule);
				vstDriver[m_iDriverNo].hModule = NULL;
			}
		}
	}
	else
	{
		if (m_hTsRead)
			StopTsReceive();
	}
	if (m_s != INVALID_SOCKET)
		::closesocket(m_s);
}

DWORD WINAPI cProxyServerEx::Reception(LPVOID pv)
{
	cProxyServerEx *pProxy = static_cast<cProxyServerEx *>(pv);

	// 内部でCOMを使用しているBonDriverに対する対策
	HRESULT hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY);

	// 接続クライアントがいる間はスリープ抑止
	EXECUTION_STATE es = ::SetThreadExecutionState(g_ThreadExecutionState);

	DWORD ret = pProxy->Process();
	delete pProxy;

#ifdef HAVE_UI
	::InvalidateRect(g_hWnd, NULL, TRUE);
#endif

	if (es != NULL)
		::SetThreadExecutionState(es);

	if (SUCCEEDED(hr))
		::CoUninitialize();

	return ret;
}

DWORD cProxyServerEx::Process()
{
	HANDLE hThread[2]{};
	hThread[0] = ::CreateThread(NULL, 0, cProxyServerEx::Sender, this, 0, NULL);
	if (hThread[0] == NULL)
		return 1;
	::SetThreadPriority(hThread[0], g_ThreadPrioritySender);

	hThread[1] = ::CreateThread(NULL, 0, cProxyServerEx::Receiver, this, 0, NULL);
	if (hThread[1] == NULL)
	{
		m_Error.Set();
		::WaitForSingleObject(hThread[0], INFINITE);
		::CloseHandle(hThread[0]);
		return 2;
	}

	HANDLE h[3] = { m_Error, m_fifoRecv.GetEventHandle(), g_ShutdownEvent };
	for (;;)
	{
		DWORD dwRet = ::WaitForMultipleObjects(3, h, FALSE, INFINITE);
		switch (dwRet)
		{
		case WAIT_OBJECT_0:
			goto end;

		case WAIT_OBJECT_0 + 1:
		{
			// コマンド処理の全体をロックするので、BonDriver_Proxyをロードして自分自身に
			// 接続させるとデッドロックする
			// しかしそうしなければ困る状況と言うのは多分無いと思うので、これは仕様と言う事で
			LOCK(g_Lock);
			cPacketHolder *pPh;
			m_fifoRecv.Pop(&pPh);
#if _DEBUG && DETAILLOG2
			{
				char *CommandName[]={
					(char *)"eSelectBonDriver",
					(char *)"eCreateBonDriver",
					(char*)"eOpenTuner",
					(char*)"eCloseTuner",
					(char*)"eSetChannel1",
					(char*)"eGetSignalLevel",
					(char*)"eWaitTsStream",
					(char*)"eGetReadyCount",
					(char*)"eGetTsStream",
					(char*)"ePurgeTsStream",
					(char*)"eRelease",

					(char*)"eGetTunerName",
					(char*)"eIsTunerOpening",
					(char*)"eEnumTuningSpace",
					(char*)"eEnumChannelName",
					(char*)"eSetChannel2",
					(char*)"eGetCurSpace",
					(char*)"eGetCurChannel",

					(char*)"eGetTotalDeviceNum",
					(char*)"eGetActiveDeviceNum",
					(char*)"eSetLnbPower",

					(char*)"eGetClientInfo",
				};
				if (pPh->GetCommand() <= eGetClientInfo)
				{
					_RPT2(_CRT_WARN, "Recieve Command : [%s] / this[%p]\n", CommandName[pPh->GetCommand()], this);
				}
				else
				{
					_RPT2(_CRT_WARN, "Illegal Command : [%d] / this[%p]\n", (int)(pPh->GetCommand()), this);
				}
			}
#endif
			switch (pPh->GetCommand())
			{
			case eSelectBonDriver:
			{
				if (pPh->GetBodyLength() <= sizeof(char))
					makePacket(eSelectBonDriver, FALSE);
				else
				{
					char *p;
					if ((p = ::strrchr((char *)(pPh->m_pPacket->payload), ':')) != NULL)
					{
						if (::strcmp(p, ":desc") == 0)	// 降順
						{
							*p = '\0';
							m_iDriverUseOrder = 1;
						}
						else if (::strcmp(p, ":asc") == 0)	// 昇順
							*p = '\0';
					}
					BOOL b = SelectBonDriver((LPCSTR)(pPh->m_pPacket->payload), 0);
					if (b)
						g_InstanceList.push_back(this);
					makePacket(eSelectBonDriver, b);
				}
				break;
			}

			case eCreateBonDriver:
			{
				if (m_pIBon == NULL)
				{
					BOOL bFind = FALSE;
					for (std::list<cProxyServerEx *>::iterator it = g_InstanceList.begin(); it != g_InstanceList.end(); ++it)
					{
						if (*it == this)
							continue;
						if (m_hModule == (*it)->m_hModule)
						{
							if ((*it)->m_pIBon != NULL)
							{
								bFind = TRUE;	// ここに来るのはかなりのレアケースのハズ
								m_pIBon = (*it)->m_pIBon;
								m_pIBon2 = (*it)->m_pIBon2;
								m_pIBon3 = (*it)->m_pIBon3;
								break;
							}
							// ここに来るのは上より更にレアケース
							// 一応リストの最後まで検索してみて、それでも見つからなかったら
							// CreateBonDriver()をやらせてみる
						}
					}
					if (!bFind)
					{
						if ((CreateBonDriver() != NULL) && (m_pIBon2 != NULL))
							makePacket(eCreateBonDriver, TRUE);
						else
						{
							makePacket(eCreateBonDriver, FALSE);
							m_Error.Set();
						}
					}
					else
						makePacket(eCreateBonDriver, TRUE);
				}
				else
					makePacket(eCreateBonDriver, TRUE);
				break;
			}

			case eOpenTuner:
			{
				BOOL bFind = FALSE;
				{
					for (std::list<cProxyServerEx *>::iterator it = g_InstanceList.begin(); it != g_InstanceList.end(); ++it)
					{
						if (*it == this)
							continue;
						if ((m_pIBon != NULL) && (m_pIBon == (*it)->m_pIBon))
						{
							if ((*it)->m_bTunerOpen)
							{
								bFind = TRUE;
								m_bTunerOpen = TRUE;
								break;
							}
						}
					}
				}
				if (!bFind)
					m_bTunerOpen = OpenTuner();
				makePacket(eOpenTuner, m_bTunerOpen);
				break;
			}

			case eCloseTuner:
			{
				BOOL bFind = FALSE;
				std::list<cProxyServerEx*>::iterator it;
				for (it = g_InstanceList.begin(); it != g_InstanceList.end(); ++it)
				{
					if (*it == this)
						continue;
					if ((m_pIBon != NULL) && (m_pIBon == (*it)->m_pIBon))
					{
						if ((*it)->m_bTunerOpen)
						{
							bFind = TRUE;
							break;
						}
					}
				}
				if (!bFind)
				{
					if (m_hTsRead)
					{
						m_pTsReaderArg->StopTsRead = TRUE;
					
						::WaitForSingleObject(m_hTsRead, INFINITE);
						::CloseHandle(m_hTsRead);
						delete m_pTsReaderArg;
					}
					CloseTuner();
				}
				else
				{
					if (m_hTsRead)
						StopTsReceive();
				}
				m_bChannelLock = 0;
				m_dwSpace = m_dwChannel = 0x7fffffff;	// INT_MAX
				m_hTsRead = NULL;
				m_pTsReaderArg = NULL;
				m_bTunerOpen = FALSE;
				break;
			}

			case ePurgeTsStream:
			{
				if (m_hTsRead)
				{
					m_pTsReaderArg->TsLock.Enter();
					if (m_pTsReaderArg->TsReceiversList.size() <= 1)
					{
						PurgeTsStream();
						m_pTsReaderArg->pos = 0;
					}
#if _DEBUG && DETAILLOG2
					_RPT2(_CRT_WARN, "ePurgeTsStream : [%d] / size[%zu]\n", (m_pTsReaderArg->TsReceiversList.size() <= 1) ? 1 : 0, m_pTsReaderArg->TsReceiversList.size());
#endif
					m_pTsReaderArg->TsLock.Leave();
					makePacket(ePurgeTsStream, TRUE);
				}
				else
					makePacket(ePurgeTsStream, FALSE);
				break;
			}

			case eRelease:
				m_Error.Set();
				break;

			case eEnumTuningSpace:
			{
				if (pPh->GetBodyLength() != sizeof(DWORD))
					makePacket(eEnumTuningSpace, _T(""));
				else
				{
					LPCTSTR p = EnumTuningSpace(::ntohl(*(DWORD *)(pPh->m_pPacket->payload)));
					if (p)
						makePacket(eEnumTuningSpace, p);
					else
						makePacket(eEnumTuningSpace, _T(""));
				}
				break;
			}

			case eEnumChannelName:
			{
				if (pPh->GetBodyLength() != (sizeof(DWORD) * 2))
					makePacket(eEnumChannelName, _T(""));
				else
				{
					LPCTSTR p = EnumChannelName(::ntohl(*(DWORD *)(pPh->m_pPacket->payload)), ::ntohl(*(DWORD *)&(pPh->m_pPacket->payload[sizeof(DWORD)])));
					if (p)
						makePacket(eEnumChannelName, p);
					else
						makePacket(eEnumChannelName, _T(""));
				}
				break;
			}

			case eSetChannel2:
			{
				if (pPh->GetBodyLength() != ((sizeof(DWORD) * 2) + sizeof(BYTE)))
					makePacket(eSetChannel2, (DWORD)0xff);
				else
				{
					BYTE bChannelLock = pPh->m_pPacket->payload[sizeof(DWORD) * 2];
					DWORD dwReqSpace = ::ntohl(*(DWORD *)(pPh->m_pPacket->payload));
					DWORD dwReqChannel = ::ntohl(*(DWORD *)&(pPh->m_pPacket->payload[sizeof(DWORD)]));
					if ((dwReqSpace == m_dwSpace) && (dwReqChannel == m_dwChannel))
					{
						// 既にリクエストされたチャンネルを選局済み
#if _DEBUG && DETAILLOG2
						_RPT2(_CRT_WARN, "** already tuned! ** : m_dwSpace[%d] / m_dwChannel[%d]\n", dwReqSpace, dwReqChannel);
#endif
						// 必ずtrueのハズだけど、一応
						if (m_hTsRead)
						{
							// このインスタンスが要求している優先度が255であった場合に
							if (bChannelLock == 0xff)
							{
								// 現在の配信リストには優先度255のインスタンスが既にいるか？
								BOOL bFind = FALSE;
								m_pTsReaderArg->TsLock.Enter();
								std::list<cProxyServerEx *>::iterator it = m_pTsReaderArg->TsReceiversList.begin();
								while (it != m_pTsReaderArg->TsReceiversList.end())
								{
									if ((*it != this) && ((*it)->m_bChannelLock == 0xff))
									{
										bFind = TRUE;
										break;
									}
									++it;
								}
								m_pTsReaderArg->TsLock.Leave();
								if (bFind)
								{
									// いた場合は、このインスタンスの優先度を暫定的に254にする
									bChannelLock = 0xfe;
									// 排他権取得待ちリストにまだ自身が含まれていなければ追加
									bFind = FALSE;
									it = m_pTsReaderArg->WaitExclusivePrivList.begin();
									while (it != m_pTsReaderArg->WaitExclusivePrivList.end())
									{
										if (*it == this)
										{
											bFind = TRUE;
											break;
										}
										++it;
									}
									if (!bFind)
										m_pTsReaderArg->WaitExclusivePrivList.push_back(this);
#if _DEBUG && DETAILLOG2
									_RPT2(_CRT_WARN, "** exclusive tuner! ** : wait-exclusivepriv-list size[%zu] / added[%d]\n", m_pTsReaderArg->WaitExclusivePrivList.size(), bFind ? 0 : 1);
#endif
								}
							}
						}
						m_bChannelLock = bChannelLock;
						makePacket(eSetChannel2, (DWORD)0x00);
					}
					else
					{
						BOOL bSuccess;
						BOOL bLocked = FALSE;
						BOOL bShared = FALSE;
						BOOL bSetChannel = FALSE;
						cProxyServerEx *pHavePriv = NULL;
						
						for (std::list<cProxyServerEx*>::iterator it = g_InstanceList.begin(); it != g_InstanceList.end(); ++it)
						{
							if (*it == this)
								continue;
							// ひとまず現在のインスタンスが共有されているかどうかを確認しておく
							if ((m_pIBon != NULL) && (m_pIBon == (*it)->m_pIBon))
								bShared = TRUE;

							// 対象BonDriver群の中でチューナをオープンしているもの
							if (m_pDriversMapKey == (*it)->m_pDriversMapKey && (*it)->m_pIBon != NULL && (*it)->m_bTunerOpen)
							{
								// かつクライアントからの要求と同一チャンネルを選択しているもの
								if ((*it)->m_dwSpace == dwReqSpace && (*it)->m_dwChannel == dwReqChannel)
								{
									// 今クライアントがオープンしているチューナに関して
									if (m_pIBon != NULL)
									{
										BOOL bModule = FALSE;
										BOOL bIBon = FALSE;
										BOOL bTuner = FALSE;
										for (std::list<cProxyServerEx*>::iterator it2 = g_InstanceList.begin(); it2 != g_InstanceList.end(); ++it2)
										{
											if (*it2 == this)
												continue;
											if (m_hModule == (*it2)->m_hModule)
											{
												bModule = TRUE;	// モジュール使用者有り
												if (m_pIBon == (*it2)->m_pIBon)
												{
													bIBon = TRUE;	// インスタンス使用者有り
													if ((*it2)->m_bTunerOpen)
													{
														bTuner = TRUE;	// チューナ使用者有り
														break;
													}
												}
											}
										}

										// チューナ使用者無しならクローズ
										if (!bTuner)
										{
											if (m_hTsRead)
											{
												m_pTsReaderArg->StopTsRead = TRUE;
												::WaitForSingleObject(m_hTsRead, INFINITE);
												::CloseHandle(m_hTsRead);
												//m_hTsRead = NULL;
												/*
												if (g_b25_enable && ((*it2)->m_pTsReaderArg->b25decoder != NULL)) {
													(*it2)->m_pTsReaderArg->b25decoder->Release();
												}
												*/

												delete m_pTsReaderArg;
												//m_pTsReaderArg = NULL;
											}
											CloseTuner();
											//m_bTunerOpen = FALSE;
											// かつインスタンス使用者も無しならインスタンスリリース
											if (!bIBon)
											{
												Release();
												// m_pIBon = NULL;
												// かつモジュール使用者も無しならモジュールリリース
												if (!bModule)
												{
													std::vector<stDriver> &vstDriver = DriversMap[m_pDriversMapKey];
													vstDriver[m_iDriverNo].bUsed = FALSE;
													if (!g_DisableUnloadBonDriver)
													{
														::FreeLibrary(m_hModule);
														vstDriver[m_iDriverNo].hModule = NULL;
													}
													// m_hModule = NULL;
												}
											}
										}
										else	// 他にチューナ使用者有りの場合
										{
											// 現在TSストリーム配信中ならその配信対象リストから自身を削除
											if (m_hTsRead)
												StopTsReceive();
										}
									}

									// このインスタンスが要求している優先度が255であった場合に
									if (bChannelLock == 0xff)
									{
										// 切り替え先チューナに対して優先度255のインスタンスが既にいるか？
										for (std::list<cProxyServerEx *>::iterator it2 = g_InstanceList.begin(); it2 != g_InstanceList.end(); ++it2)
										{
											if (*it2 == this)
												continue;
											if ((*it2)->m_pIBon == (*it)->m_pIBon)
											{
												if ((*it2)->m_bChannelLock == 0xff)
												{
													// いた場合は、このインスタンスの優先度を暫定的に254にする
													// (そうしないと、優先度255のインスタンスもチャンネル変更できなくなる為)
													bChannelLock = 0xfe;
													pHavePriv = *it2;
													break;
												}
											}
										}
									}

									// インスタンス切り替え
									m_hModule = (*it)->m_hModule;
									m_iDriverNo = (*it)->m_iDriverNo;
									m_pIBon = (*it)->m_pIBon;
									m_pIBon2 = (*it)->m_pIBon2;
									m_pIBon3 = (*it)->m_pIBon3;
									m_bTunerOpen = TRUE;
									m_hTsRead = (*it)->m_hTsRead;	// この時点でもNULLの可能性はゼロではない
									m_pTsReaderArg = (*it)->m_pTsReaderArg;
									if (m_hTsRead)
									{
										m_pTsReaderArg->TsLock.Enter();
										m_pTsReaderArg->TsReceiversList.push_back(this);
										m_pTsReaderArg->TsLock.Leave();
									}
#if _DEBUG && DETAILLOG2
									_RPT3(_CRT_WARN, "** found! ** : m_hModule[%p] / m_iDriverNo[%d] / m_pIBon[%p]\n", m_hModule, m_iDriverNo, m_pIBon);
									_RPT3(_CRT_WARN, "             : m_dwSpace[%d] / m_dwChannel[%d] / m_bChannelLock[%d]\n", dwReqSpace, dwReqChannel, bChannelLock);
#endif
									// このインスタンスの優先度が下げられた場合
									if (pHavePriv != NULL)
									{
										if (m_hTsRead)
										{
											// 排他権取得待ちリストにまだ自身が含まれていなければ追加
											BOOL bFind = FALSE;
											std::list<cProxyServerEx *>::iterator it2 = m_pTsReaderArg->WaitExclusivePrivList.begin();
											while (it2 != m_pTsReaderArg->WaitExclusivePrivList.end())
											{
												if (*it2 == this)
												{
													bFind = TRUE;
													break;
												}
												++it2;
											}
											if (!bFind)
												m_pTsReaderArg->WaitExclusivePrivList.push_back(this);
#if _DEBUG && DETAILLOG2
											_RPT2(_CRT_WARN, "** exclusive tuner! ** : wait-exclusivepriv-list size[%zu] / added[%d]\n", m_pTsReaderArg->WaitExclusivePrivList.size(), bFind ? 0 : 1);
#endif
										}
										else
										{
											// この処理の意図は少し下の同じ処理のコメント参照
											pHavePriv->m_bChannelLock = 0;
											bChannelLock = 0xff;
										}
									}
									goto ok;	// これは酷い
								}
							}
						}

						// 同一チャンネルを使用中のチューナは見つからず、現在のチューナは共有されていたら
						if (bShared)
						{
							// 出来れば未使用、無理ならなるべくロックされてないチューナを選択して、
							// 一気にチューナオープン状態にまで持って行く
							if (SelectBonDriver(m_pDriversMapKey, bChannelLock))
							{
								if (m_pIBon == NULL)
								{
									// 未使用チューナがあった
									if ((CreateBonDriver() == NULL) || (m_pIBon2 == NULL))
									{
										makePacket(eSetChannel2, (DWORD)0xff);
										m_Error.Set();
										break;
									}
								}
								if (!m_bTunerOpen)
								{
									m_bTunerOpen = OpenTuner();
									if (!m_bTunerOpen)
									{
										makePacket(eSetChannel2, (DWORD)0xff);
										m_Error.Set();
										break;
									}
								}
							}
							else
							{
								makePacket(eSetChannel2, (DWORD)0xff);
								m_Error.Set();
								break;
							}

							// 使用チューナのチャンネルロック状態確認
							
							for (std::list<cProxyServerEx *>::iterator it = g_InstanceList.begin(); it != g_InstanceList.end(); ++it)
							{
								if (*it == this)
									continue;
								if (m_pIBon == (*it)->m_pIBon)
								{
									if ((*it)->m_bChannelLock > bChannelLock)
										bLocked = TRUE;
									else if ((*it)->m_bChannelLock == 0xff)
									{
										// 対象チューナに対して優先度255のインスタンスが既にいる状態で、このインスタンスが
										// 要求している優先度も255の場合、このインスタンスの優先度を暫定的に254にする
										// (そうしないと、優先度255のインスタンスもチャンネル変更できなくなる為)
										bChannelLock = 0xfe;
										bLocked = TRUE;
										pHavePriv = *it;
									}
									if (bLocked)
										break;
								}
							}

							// このインスタンスの優先度が下げられた場合
							if (pHavePriv != NULL)
							{
								if (m_hTsRead)
								{
									// 排他権取得待ちリストにまだ自身が含まれていなければ追加
									BOOL bFind = FALSE;
									std::list<cProxyServerEx *>::iterator it = m_pTsReaderArg->WaitExclusivePrivList.begin();
									while (it != m_pTsReaderArg->WaitExclusivePrivList.end())
									{
										if (*it == this)
										{
											bFind = TRUE;
											break;
										}
										++it;
									}
									if (!bFind)
										m_pTsReaderArg->WaitExclusivePrivList.push_back(this);
#if _DEBUG && DETAILLOG2
									_RPT2(_CRT_WARN, "** exclusive tuner! ** : wait-exclusivepriv-list size[%zu] / added[%d]\n", m_pTsReaderArg->WaitExclusivePrivList.size(), bFind ? 0 : 1);
#endif
								}
								else
								{
									// このインスタンスの優先度が下げられたが、排他権を持っているインスタンスへの配信が
									// 開始されていない場合は、そのインスタンスから排他権を奪う
									// こうする事が挙動として望ましいのかどうかは微妙だが、そもそもここに来るのは、
									// 当該インスタンスでのSetChannel()の失敗後、何もせずに接続だけ続けている状態であり、
									// 可能性としてはゼロではないものの、かなりのレアケースに限られるはず
									pHavePriv->m_bChannelLock = 0;
									bChannelLock = 0xff;
								}
							}
						}

#if _DEBUG && DETAILLOG2
						_RPT2(_CRT_WARN, "eSetChannel2 : bShared[%d] / bLocked[%d]\n", bShared, bLocked);
						_RPT3(_CRT_WARN, "             : dwReqSpace[%d] / dwReqChannel[%d] / m_bChannelLock[%d]\n", dwReqSpace, dwReqChannel, bChannelLock);
#endif

						if (bLocked)
						{
							// ロックされてる時は単純にロックされてる事を通知
							// この場合クライアントアプリへのSetChannel()の戻り値は成功になる
							// (おそらく致命的な問題にはならない)
							makePacket(eSetChannel2, (DWORD)0x01);
						}
						else
						{
							if (m_hTsRead)
								m_pTsReaderArg->TsLock.Enter();
							bSuccess = SetChannel(dwReqSpace, dwReqChannel);
							if (m_hTsRead)
							{
								// 一旦ロックを外すとチャンネル変更前のデータが送信されない事を保証できなくなる為、
								// チャンネル変更前のデータの破棄とCNRの更新指示はここで行う
								if (bSuccess)
								{
									// 同一チャンネルを使用中のチューナが見つからなかった場合は、このリクエストで
									// インスタンスの切り替えが発生していたとしても、この時点ではどうせチャンネルが
									// 変更されているので、未送信バッファを破棄しても別に問題にはならないハズ
									m_pTsReaderArg->pos = 0;
									m_pTsReaderArg->ChannelChanged = TRUE;
								}
								m_pTsReaderArg->TsLock.Leave();
							}
							if (bSuccess)
							{
								bSetChannel = TRUE;
							ok:
								m_dwSpace = dwReqSpace;
								m_dwChannel = dwReqChannel;
								m_bChannelLock = bChannelLock;
								makePacket(eSetChannel2, (DWORD)0x00);
								if (m_hTsRead == NULL)
								{
									//m_pTsReaderArg = new stTsReaderArg();
									m_pTsReaderArg->TsReceiversList.push_back(this);
									m_pTsReaderArg->pIBon = m_pIBon;
									/*
									if (g_b25_enable) {
										m_pTsReaderArg->b25decoder = B25DecoderCreateInstance();
										if (m_pTsReaderArg->b25decoder->Initialize()) {
											IB25Decoder2* dec2 = dynamic_cast <IB25Decoder2*>(m_pTsReaderArg->b25decoder);
											dec2->DiscardNullPacket(g_b25_strip);
											dec2->EnableEmmProcess(g_b25_emm);
											dec2->SetMulti2Round(g_b25_round);
											dec2->DiscardScramblePacket(FALSE);
										}
									}
									*/
									m_hTsRead = ::CreateThread(NULL, 0, cProxyServerEx::TsReader, m_pTsReaderArg, 0, NULL);
									if (m_hTsRead == NULL)
									{
										delete m_pTsReaderArg;
										m_pTsReaderArg = NULL;
										m_Error.Set();
									}
									else
										::SetThreadPriority(m_hTsRead, g_ThreadPriorityTsReader);
								}
								if (bSetChannel)
								{
									// SetChannel()が行われた場合は、同一BonDriverインスタンスを使用しているインスタンスの保持チャンネルを変更
									for (std::list<cProxyServerEx *>::iterator it = g_InstanceList.begin(); it != g_InstanceList.end(); ++it)
									{
										if (*it == this)
											continue;
										if (m_pIBon == (*it)->m_pIBon)
										{
											(*it)->m_dwSpace = dwReqSpace;
											(*it)->m_dwChannel = dwReqChannel;
											// 対象インスタンスがまだ一度もSetChannel()を行っていなかった場合
											if ((*it)->m_hTsRead == NULL)
											{
												// 強制的に配信開始
												(*it)->m_bTunerOpen = TRUE;
												(*it)->m_hTsRead = m_hTsRead;
												(*it)->m_pTsReaderArg = m_pTsReaderArg;
												if (m_hTsRead)
												{
													m_pTsReaderArg->TsLock.Enter();
													m_pTsReaderArg->TsReceiversList.push_back(*it);
													m_pTsReaderArg->TsLock.Leave();
												}
											}
										}
									}
								}
							}
							else
								makePacket(eSetChannel2, (DWORD)0xff);
						}
					}
#ifdef HAVE_UI
					::InvalidateRect(g_hWnd, NULL, TRUE);
#endif
				}
				break;
			}

			case eGetTotalDeviceNum:
				makePacket(eGetTotalDeviceNum, GetTotalDeviceNum());
				break;

			case eGetActiveDeviceNum:
				makePacket(eGetActiveDeviceNum, GetActiveDeviceNum());
				break;

			case eSetLnbPower:
			{
				if (pPh->GetBodyLength() != sizeof(BYTE))
					makePacket(eSetLnbPower, FALSE);
				else
					makePacket(eSetLnbPower, SetLnbPower((BOOL)(pPh->m_pPacket->payload[0])));
				break;
			}

			case eGetClientInfo:
			{
				/* クライアント側でこのコマンドを使用していないのでコメント化
				union {
					SOCKADDR_STORAGE ss;
					SOCKADDR_IN si4;
					SOCKADDR_IN6 si6;
				};
				char addr[INET6_ADDRSTRLEN]{},  info[4096]{}, buf[2048]{}, * p, * exinfo;
				int port, len, num = 0;
				size_t left, size;
				p = info;
				p[0] = '\0';
				left = size = sizeof(info);
				exinfo = NULL;
				for(list<cProxyServerEx*>::iterator it = g_InstanceList.begin(); it != g_InstanceList.end(); it++)
				{
					len = sizeof(ss);
					if (::getpeername((*it)->m_s, (SOCKADDR *)&ss, &len) == 0)
					{
						if (ss.ss_family == AF_INET)
						{
							// IPv4
							::inet_ntop(AF_INET, &(si4.sin_addr), addr, sizeof(addr));
							port = ::ntohs(si4.sin_port);
						}
						else
						{
							// IPv6
							::inet_ntop(AF_INET6, &(si6.sin6_addr), addr, sizeof(addr));
							port = ::ntohs(si6.sin6_port);
						}
					}
					else
					{
						::lstrcpyA(addr, "unknown host...");
						port = 0;
					}
					//TCHAR buf[2048]{};
					std::vector<stDriver> &vstDriver = DriversMap[(*it)->m_pDriversMapKey];
					len = ::wsprintf(buf, _T("%02d: [%s]:[%d] / [%s][%s] / space[%u] ch[%u]\n"), num, addr, port, (*it)->m_pDriversMapKey, vstDriver[(*it)->m_iDriverNo].strBonDriver, (*it)->m_dwSpace, (*it)->m_dwChannel);
					if ((size_t)len >= left)
					{
						left += size;
						size *= 2;
						if (exinfo != NULL)
						{
							char *bp = exinfo;
							exinfo = new char[size];
							::lstrcpyA(exinfo, bp);
							delete[] bp;
						}
						else
						{
							exinfo = new char[size];
							::lstrcpyA(exinfo, info);
						}
						p = exinfo + ::lstrlenA(exinfo);
					}
					::lstrcpyA(p, buf);
					p += len;
					left -= len;
					num++;
				}
				if (exinfo != NULL)
				{
					size = (p - exinfo) + 1;
					p = exinfo;
				}
				else
				{
					size = (p - info) + 1;
					p = info;
				}
				cPacketHolder *ph = new cPacketHolder(eGetClientInfo, size);
				::memcpy(ph->m_pPacket->payload, p, size);
				m_fifoSend.Push(ph);
				if (exinfo != NULL)
					delete[] exinfo;
				*/

				break;
			}

			default:
				break;
			}
			delete pPh;
			break;
		}
		break;

		case WAIT_OBJECT_0 + 2:
			// 終了要求
			// fall-through
		default:
			// 何かのエラー
			m_Error.Set();
			goto end;
		}
	}
end:
	::WaitForMultipleObjects(2, hThread, TRUE, INFINITE);
	::CloseHandle(hThread[0]);
	::CloseHandle(hThread[1]);
	return 0;
}

int cProxyServerEx::ReceiverHelper(char *pDst, DWORD left)
{
	int len, ret;
	fd_set rd{};
	timeval tv{};

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	while (left > 0)
	{
		if (m_Error.IsSet())
			return -1;

		FD_ZERO(&rd);
		FD_SET(m_s, &rd);
		if ((len = ::select((int)(m_s + 1), &rd, NULL, NULL, &tv)) == SOCKET_ERROR)
		{
			ret = -2;
			goto err;
		}

		if (len == 0)
			continue;

		// MSDNのrecv()のソース例とか見る限り、"SOCKET_ERROR"が負の値なのは保証されてるっぽい
		if ((len = ::recv(m_s, pDst, left, 0)) <= 0)
		{
			ret = -3;
			goto err;
		}
		left -= len;
		pDst += len;
	}
	return 0;
err:
	m_Error.Set();
	return ret;
}

DWORD WINAPI cProxyServerEx::Receiver(LPVOID pv)
{
	cProxyServerEx *pProxy = static_cast<cProxyServerEx *>(pv);
	char *p;
	DWORD left, ret;
	cPacketHolder *pPh = NULL;

	for (;;)
	{
		pPh = new cPacketHolder(16);
		left = sizeof(stPacketHead);
		p = (char *)&(pPh->m_pPacket->head);
		if (pProxy->ReceiverHelper(p, left) != 0)
		{
			ret = 201;
			goto end;
		}

		if (!pPh->IsValid())
		{
			pProxy->m_Error.Set();
			ret = 202;
			goto end;
		}

		left = pPh->GetBodyLength();
		if (left == 0)
		{
			pProxy->m_fifoRecv.Push(pPh);
			continue;
		}

		if (left > 16)
		{
			if (left > 512)
			{
				pProxy->m_Error.Set();
				ret = 203;
				goto end;
			}
			cPacketHolder *pTmp = new cPacketHolder(left);
			pTmp->m_pPacket->head = pPh->m_pPacket->head;
			delete pPh;
			pPh = pTmp;
		}

		p = (char *)(pPh->m_pPacket->payload);
		if (pProxy->ReceiverHelper(p, left) != 0)
		{
			ret = 204;
			goto end;
		}

		pProxy->m_fifoRecv.Push(pPh);
	}
end:
	delete pPh;
	return ret;
}

void cProxyServerEx::makePacket(enumCommand eCmd, BOOL b)
{
	cPacketHolder *p = new cPacketHolder(eCmd, sizeof(BYTE));
	p->m_pPacket->payload[0] = (BYTE)b;
	m_fifoSend.Push(p);
}

void cProxyServerEx::makePacket(enumCommand eCmd, DWORD dw)
{
	cPacketHolder *p = new cPacketHolder(eCmd, sizeof(DWORD));
	DWORD *pos = (DWORD *)(p->m_pPacket->payload);
	*pos = ::htonl(dw);
	m_fifoSend.Push(p);
}

void cProxyServerEx::makePacket(enumCommand eCmd, LPCTSTR str)
{
	register size_t size = (::_tcslen(str) + 1) * sizeof(TCHAR);
	cPacketHolder *p = new cPacketHolder(eCmd, size);
	::memcpy(p->m_pPacket->payload, str, size);
	m_fifoSend.Push(p);
}

void cProxyServerEx::makePacket(enumCommand eCmd, BYTE *pSrc, DWORD dwSize, float fSignalLevel)
{
	register size_t size = (sizeof(DWORD) * 2) + dwSize;
	cPacketHolder *p = new cPacketHolder(eCmd, size);
	union {
		DWORD dw;
		float f;
	} u{};
	u.f = fSignalLevel;
	DWORD *pos = (DWORD *)(p->m_pPacket->payload);
	*pos++ = ::htonl(dwSize);
	*pos++ = ::htonl(u.dw);
	if (dwSize > 0)
		::memcpy(pos, pSrc, dwSize);
	m_fifoSend.Push(p);
}

DWORD WINAPI cProxyServerEx::Sender(LPVOID pv)
{
	cProxyServerEx *pProxy = static_cast<cProxyServerEx *>(pv);
	DWORD ret;
	HANDLE h[2] = { pProxy->m_Error, pProxy->m_fifoSend.GetEventHandle() };
	for (;;)
	{
		DWORD dwRet = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
		switch (dwRet)
		{
		case WAIT_OBJECT_0:
			ret = 101;
			goto end;

		case WAIT_OBJECT_0 + 1:
		{
			cPacketHolder *pPh;
			pProxy->m_fifoSend.Pop(&pPh);
			int left = (int)pPh->m_Size;
			char *p = (char *)(pPh->m_pPacket);
			while (left > 0)
			{
				int len = ::send(pProxy->m_s, p, left, 0);
				if (len == SOCKET_ERROR)
				{
					pProxy->m_Error.Set();
					break;
				}
				left -= len;
				p += len;
			}
			delete pPh;
			break;
		}

		default:
			// 何かのエラー
			pProxy->m_Error.Set();
			ret = 102;
			goto end;
		}
	}
end:
	return ret;
}

DWORD WINAPI cProxyServerEx::TsReader(LPVOID pv)
{
	stTsReaderArg *pArg = static_cast<stTsReaderArg *>(pv);
	IBonDriver *pIBon = pArg->pIBon;
	volatile BOOL &StopTsRead = pArg->StopTsRead;
	volatile BOOL &ChannelChanged = pArg->ChannelChanged;
	DWORD &pos = pArg->pos;
	std::list<cProxyServerEx *> &TsReceiversList = pArg->TsReceiversList;
	cCriticalSection &TsLock = pArg->TsLock;
	DWORD dwSize, dwRemain;
	ULONGLONG now, before = 0;
	float fSignalLevel = 0;
	DWORD ret = 300;
	const DWORD TsPacketBufSize = g_TsPacketBufSize;
	BYTE *pBuf, *pTsBuf = new BYTE[TsPacketBufSize];
	IB25Decoder2* b25decoderIF = NULL;
#if _DEBUG && DETAILLOG
	DWORD Counter = 0;
#endif


	if (pArg->decoder) {
		b25decoderIF = pArg->decoder->CreateInstance();
		if (b25decoderIF) {
			if (b25decoderIF->Initialize()) {
				b25decoderIF->DiscardNullPacket(pArg->decoder->nullPacket);
				b25decoderIF->EnableEmmProcess(pArg->decoder->emm);
				b25decoderIF->SetMulti2Round(pArg->decoder->round);
				b25decoderIF->DiscardScramblePacket(FALSE);
			}
		}
	}

	// 内部でCOMを使用しているBonDriverに対する対策
	HRESULT hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY);
	// TS読み込みループ
	while (!StopTsRead)
	{
		dwSize = dwRemain = 0;
		{
			LOCK(TsLock);
			if ((((now = ::GetTickCount64()) - before) >= 1000) || ChannelChanged)
			{
				if (ChannelChanged) {
					if (b25decoderIF) {
						b25decoderIF->Reset();
					}
				}
				fSignalLevel = pIBon->GetSignalLevel();
				before = now;
				ChannelChanged = FALSE;
			}
			if (pIBon->GetTsStream(&pBuf, &dwSize, &dwRemain) && (dwSize != 0))
			{
				if  (b25decoderIF) {
					b25decoderIF->Decode(pBuf, dwSize, &pBuf, &dwSize);
					if (dwSize == 0) {
						goto next;
					}
				}
				if ((pos + dwSize) < TsPacketBufSize)
				{
					::memcpy(&pTsBuf[pos], pBuf, dwSize);
					pos += dwSize;
					if (dwRemain == 0)
					{
						for (std::list<cProxyServerEx *>::iterator it = TsReceiversList.begin(); it != TsReceiversList.end(); ++it)
							(*it)->makePacket(eGetTsStream, pTsBuf, pos, fSignalLevel);
#if _DEBUG && DETAILLOG
						_RPT3(_CRT_WARN, "makePacket0() : %u : size[%x] / dwRemain[%d]\n", Counter++, pos, dwRemain);
#endif
						pos = 0;
					}
				}
				else
				{
					DWORD left, dwLen = TsPacketBufSize - pos;
					::memcpy(&pTsBuf[pos], pBuf, dwLen);
					for (std::list<cProxyServerEx *>::iterator it = TsReceiversList.begin(); it != TsReceiversList.end(); ++it)
						(*it)->makePacket(eGetTsStream, pTsBuf, TsPacketBufSize, fSignalLevel);
#if _DEBUG && DETAILLOG
					_RPT3(_CRT_WARN, "makePacket1() : %u : size[%x] / dwRemain[%d]\n", Counter++, TsPacketBufSize, dwRemain);
#endif
					left = dwSize - dwLen;
					pBuf += dwLen;
					while (left >= TsPacketBufSize)
					{
						for (std::list<cProxyServerEx *>::iterator it = TsReceiversList.begin(); it != TsReceiversList.end(); ++it)
							(*it)->makePacket(eGetTsStream, pBuf, TsPacketBufSize, fSignalLevel);
#if _DEBUG && DETAILLOG
						_RPT2(_CRT_WARN, "makePacket2() : %u : size[%x]\n", Counter++, TsPacketBufSize);
#endif
						left -= TsPacketBufSize;
						pBuf += TsPacketBufSize;
					}
					if (left != 0)
					{
						if (dwRemain == 0)
						{
							for (std::list<cProxyServerEx *>::iterator it = TsReceiversList.begin(); it != TsReceiversList.end(); ++it)
								(*it)->makePacket(eGetTsStream, pBuf, left, fSignalLevel);
#if _DEBUG && DETAILLOG
							_RPT3(_CRT_WARN, "makePacket3() : %u : size[%x] / dwRemain[%d]\n", Counter++, left, dwRemain);
#endif
							left = 0;
						}
						else
							::memcpy(pTsBuf, pBuf, left);
					}
					pos = left;
				}
			}
		}
	next:
		if (dwRemain == 0)
			::Sleep(WAIT_TIME);
	}

	if (SUCCEEDED(hr))
		::CoUninitialize();
	delete[] pTsBuf;

	if (b25decoderIF) {
		b25decoderIF->Release();
	}

	return ret;
}

void cProxyServerEx::StopTsReceive()
{
	// このメソッドは必ず、
	// 1. グローバルなインスタンスロック中
	// 2. かつ、TS受信中(m_hTsRead != NULL)
	// の2つを満たす状態で呼び出す事
	m_pTsReaderArg->TsLock.Enter();

	std::list<cProxyServerEx *>::iterator it = m_pTsReaderArg->TsReceiversList.begin();
	while (it != m_pTsReaderArg->TsReceiversList.end())
	{
		if (*it == this)
		{
			m_pTsReaderArg->TsReceiversList.erase(it);
			break;
		}
		++it;
	}

	m_pTsReaderArg->TsLock.Leave();

	// このインスタンスはチャンネル排他権を持っているか？
	if (m_bChannelLock == 0xff)
	{
		// 持っていた場合は、排他権取得待ちのインスタンスは存在しているか？
		if (m_pTsReaderArg->WaitExclusivePrivList.size() > 0)
		{
			// 存在する場合は、リスト先頭のインスタンスに排他権を引き継ぎ、リストから削除
			cProxyServerEx *p = m_pTsReaderArg->WaitExclusivePrivList.front();
			m_pTsReaderArg->WaitExclusivePrivList.pop_front();
			p->m_bChannelLock = 0xff;
		}
	}
	else
	{
		// 持っていない場合は、排他権取得待ちリストに自身が含まれているかもしれないので削除
		m_pTsReaderArg->WaitExclusivePrivList.remove(this);
	}

	// 自分が最後の受信者だった場合は、TS配信スレッドも停止
	if (m_pTsReaderArg->TsReceiversList.empty())
	{
		m_pTsReaderArg->StopTsRead = TRUE;
		::WaitForSingleObject(m_hTsRead, INFINITE);
		::CloseHandle(m_hTsRead);
		m_hTsRead = NULL;
		delete m_pTsReaderArg;
		m_pTsReaderArg = NULL;
	}
}

BOOL cProxyServerEx::SelectBonDriver(LPCSTR p, BYTE bChannelLock)
{
	char *pKey = NULL;
	std::vector<stDriver> *pvstDriver = NULL;
	for (std::map<char *, std::vector<stDriver> >::iterator it = DriversMap.begin(); it != DriversMap.end(); ++it)
	{
		if (::strcmp(p, it->first) == 0)
		{
			pKey = it->first;
			pvstDriver = &(it->second);
			break;
		}
	}
	if (pvstDriver == NULL)
	{
		m_hModule = NULL;
		return FALSE;
	}

	// 現在時刻を取得しておく
	SYSTEMTIME stNow;
	FILETIME ftNow;
	::GetLocalTime(&stNow);
	::SystemTimeToFileTime(&stNow, &ftNow);

	// まず使われてないのを探す
	std::vector<stDriver> &vstDriver = *pvstDriver;
	int i;
	if (m_iDriverUseOrder == 0)
		i = 0;
	else
		i = (int)(vstDriver.size() - 1);
	for (;;)
	{
		if (vstDriver[i].bUsed)
			goto next;
		HMODULE hModule;
		if (vstDriver[i].hModule != NULL)
			hModule = vstDriver[i].hModule;
		else
		{
			hModule = ::LoadLibrary(vstDriver[i].strBonDriver);
			if (hModule == NULL)
				goto next;
			vstDriver[i].hModule = hModule;
		}
		m_hModule = hModule;
		vstDriver[i].bUsed = TRUE;
		vstDriver[i].ftLoad = ftNow;
		m_pDriversMapKey = (char *)pKey;
		m_iDriverNo = i;

		// 各種項目再初期化の前に、現在TSストリーム配信中ならその配信対象リストから自身を削除
		if (m_hTsRead)
			StopTsReceive();

		// eSetChannel2からも呼ばれるので、各種項目再初期化
		m_pIBon = m_pIBon2 = m_pIBon3 = NULL;
		m_bTunerOpen = FALSE;
		m_hTsRead = NULL;

		m_pTsReaderArg = new stTsReaderArg();

		if (vstDriver[i].decoderNo == -1) {
			m_pTsReaderArg->decoder = NULL;
		}
		else {
			m_pTsReaderArg->decoder = DecoderVec[vstDriver[i].decoderNo];
		}

		return TRUE;
	next:
		if (m_iDriverUseOrder == 0)
		{
			if (i >= (int)(vstDriver.size() - 1))
				break;
			i++;
		}
		else
		{
			if (i <= 0)
				break;
			i--;
		}
	}

	// 全部使われてたら(あるいはLoadLibrary()出来なければ)、あるインスタンスを使用している
	// クライアント群のチャンネル優先度の最大値が最も低いインスタンスを選択する
	// 同値の物が複数あった場合はBonDriverのロード時刻(もしくは使用要求時刻)が古い物を優先
	cProxyServerEx *pCandidate = NULL;
	std::vector<cProxyServerEx *> vpCandidate;
	for (std::list<cProxyServerEx *>::iterator it = g_InstanceList.begin(); it != g_InstanceList.end(); ++it)
	{
		if (*it == this)
			continue;
		if (pKey == (*it)->m_pDriversMapKey)	// この段階では文字列比較である必要は無い
		{
			// 候補リストに既に入れているなら以後のチェックは不要
			for (i = 0; i < (int)vpCandidate.size(); i++)
			{
				if (vpCandidate[i]->m_hModule == (*it)->m_hModule)
					break;
			}
			if (i != (int)vpCandidate.size())
				continue;
			// 暫定候補
			pCandidate = *it;
			// この暫定候補が使用しているインスタンスはロックされているか？
			BOOL bLocked = FALSE;
			for (std::list<cProxyServerEx *>::iterator it2 = g_InstanceList.begin(); it2 != g_InstanceList.end(); ++it2)
			{
				if (*it2 == this)
					continue;
				if (pCandidate->m_hModule == (*it2)->m_hModule)
				{
					if (((*it2)->m_bChannelLock > bChannelLock) || ((*it2)->m_bChannelLock == 0xff))	// ロックされてた
					{
						bLocked = TRUE;
						break;
					}
				}
			}
			if (!bLocked)	// ロックされていなければ候補リストに追加
				vpCandidate.push_back(pCandidate);
		}
	}

#if _DEBUG && DETAILLOG2
	_RPT1(_CRT_WARN, "** SelectBonDriver ** : vpCandidate.size[%zd]\n", vpCandidate.size());
#endif

	// 候補リストが空でなければ(==ロックされていないインスタンスがあったなら)
	if (vpCandidate.size() != 0)
	{
		pCandidate = vpCandidate[0];
		// 候補に選択の余地はあるか？
		if (vpCandidate.size() > 1)
		{
			// 接続クライアント群のチャンネル優先度の最大値が最も低いインスタンスを探す
			// なお若干ややこしいが、自身が排他権を持っており、かつ排他権取得待ちがいた場合も、
			// 元のインスタンスはこの時点での候補リストに含まれている
			BYTE bGroupMaxPriv, bMinPriv;
			std::vector<BYTE> vbGroupMaxPriv(vpCandidate.size());
			bMinPriv = 0xff;
			for (i = 0; i < (int)vpCandidate.size(); i++)
			{
				bGroupMaxPriv = 0;
				for (std::list<cProxyServerEx *>::iterator it = g_InstanceList.begin(); it != g_InstanceList.end(); ++it)
				{
					if (*it == this)
						continue;
					if (vpCandidate[i]->m_hModule == (*it)->m_hModule)
					{
						if ((*it)->m_bChannelLock > bGroupMaxPriv)
							bGroupMaxPriv = (*it)->m_bChannelLock;
					}
				}
				vbGroupMaxPriv[i] = bGroupMaxPriv;
				if (bMinPriv > bGroupMaxPriv)
					bMinPriv = bGroupMaxPriv;
			}
			std::vector<cProxyServerEx *> vpCandidate2;
			for (i = 0; i < (int)vpCandidate.size(); i++)
			{
#if _DEBUG && DETAILLOG2
				_RPT2(_CRT_WARN, "                      : vbGroupMaxPriv[%d] : [%d]\n", i, vbGroupMaxPriv[i]);
#endif
				if (vbGroupMaxPriv[i] == bMinPriv)
					vpCandidate2.push_back(vpCandidate[i]);
			}
#if _DEBUG && DETAILLOG2
			_RPT1(_CRT_WARN, "                      : vpCandidate2.size[%zd]\n", vpCandidate2.size());
#endif
			// eSetChannel2からの呼び出しの場合
			if (m_pIBon)
			{
				for (i = 0; i < (int)vpCandidate2.size(); i++)
				{
					// この時点での候補リストに現在のインスタンスが含まれていた場合は
					if (m_hModule == vpCandidate2[i]->m_hModule)
					{
						// 現在のインスタンスを継続使用
						// 「同値の物が複数あった場合はBonDriverのロード時刻(もしくは使用要求時刻)が古い物を優先」
						// が守られない事になる場合もあるが、効率優先
						vstDriver[m_iDriverNo].ftLoad = ftNow;
						return TRUE;
					}
				}
			}

			// vpCandidate2.size()が1以上なのは保証されている
			pCandidate = vpCandidate2[0];
			// インスタンス毎の最大優先度の最小値が複数インスタンスで同値であった場合
			if (vpCandidate2.size() > 1)
			{
				// BonDriverのロード時刻が一番古いのを探す
				FILETIME ft = vstDriver[vpCandidate2[0]->m_iDriverNo].ftLoad;
				for (i = 1; i < (int)vpCandidate2.size(); i++)
				{
					if (::CompareFileTime(&ft, &(vstDriver[vpCandidate2[i]->m_iDriverNo].ftLoad)) > 0)
					{
						ft = vstDriver[vpCandidate2[i]->m_iDriverNo].ftLoad;
						pCandidate = vpCandidate2[i];
					}
				}
			}
		}
		else
		{
			// eSetChannel2からの呼び出しの場合
			if (m_pIBon)
			{
				// 唯一の候補が現在のインスタンスと同じだった場合は
				if (m_hModule == pCandidate->m_hModule)
				{
					// 現在のインスタンスを継続使用
					vstDriver[m_iDriverNo].ftLoad = ftNow;
					return TRUE;
				}
			}
		}

		// eSetChannel2からの呼び出しの場合かつ現在TSストリーム配信中だったなら、
		// インスタンスが切り替わるので、現在の配信対象リストから自身を削除
		if (m_pIBon && m_hTsRead)
			StopTsReceive();
	}
	else
	{
		// eSetChannel2からの呼び出しの場合
		if (m_pIBon)
		{
			// ロックされていないインスタンスが無かったので現在のインスタンスを継続使用
			vstDriver[m_iDriverNo].ftLoad = ftNow;
			return TRUE;
		}
	}

	// NULLである事は無いハズだけど
	if (pCandidate != NULL)
	{
		m_hModule = pCandidate->m_hModule;
		m_pDriversMapKey = pCandidate->m_pDriversMapKey;
		m_iDriverNo = pCandidate->m_iDriverNo;
		m_pIBon = pCandidate->m_pIBon;	// pCandidate->m_pIBonがNULLの可能性はゼロではない
		m_pIBon2 = pCandidate->m_pIBon2;
		m_pIBon3 = pCandidate->m_pIBon3;
		m_bTunerOpen = pCandidate->m_bTunerOpen;
		m_hTsRead = pCandidate->m_hTsRead;
		m_pTsReaderArg = pCandidate->m_pTsReaderArg;
		m_dwSpace = pCandidate->m_dwSpace;
		m_dwChannel = pCandidate->m_dwChannel;
		// 使用するBonDriverのロード時刻(使用要求時刻)を現在時刻で更新
		vstDriver[m_iDriverNo].ftLoad = ftNow;
	}

	// 選択したインスタンスが既にTSストリーム配信中なら、その配信対象リストに自身を追加
	if (m_hTsRead)
	{
		m_pTsReaderArg->TsLock.Enter();
		m_pTsReaderArg->TsReceiversList.push_back(this);
		m_pTsReaderArg->TsLock.Leave();
	}

	return (m_hModule != NULL);
}

IBonDriver *cProxyServerEx::CreateBonDriver()
{
	if (m_hModule)
	{
		IBonDriver *(*f)() = (IBonDriver *(*)())::GetProcAddress(m_hModule, "CreateBonDriver");
		if (f)
		{
			try { m_pIBon = f(); }
			catch (...) {}
			if (m_pIBon)
			{
				m_pIBon2 = dynamic_cast<IBonDriver2 *>(m_pIBon);
				m_pIBon3 = dynamic_cast<IBonDriver3 *>(m_pIBon);
			}
		}
	}
	return m_pIBon;
}

const BOOL cProxyServerEx::OpenTuner(void)
{
	BOOL b = FALSE;
	if (m_pIBon)
		b = m_pIBon->OpenTuner();
	if (g_OpenTunerRetDelay != 0)
		::Sleep(g_OpenTunerRetDelay);
	return b;
}

void cProxyServerEx::CloseTuner(void)
{
	if (m_pIBon) {
		m_pIBon->CloseTuner();
	}
}

void cProxyServerEx::PurgeTsStream(void)
{
	if (m_pIBon)
		m_pIBon->PurgeTsStream();
}

void cProxyServerEx::Release(void)
{
	if (m_pIBon)
	{
		if (g_SandBoxedRelease)
		{
			__try { m_pIBon->Release(); }
			__except (EXCEPTION_EXECUTE_HANDLER){}
		}
		else
			m_pIBon->Release();
	}
}

LPCTSTR cProxyServerEx::EnumTuningSpace(const DWORD dwSpace)
{
	LPCTSTR pStr = NULL;
	if (m_pIBon2)
		pStr = m_pIBon2->EnumTuningSpace(dwSpace);
	return pStr;
}

LPCTSTR cProxyServerEx::EnumChannelName(const DWORD dwSpace, const DWORD dwChannel)
{
	LPCTSTR pStr = NULL;
	if (m_pIBon2)
		pStr = m_pIBon2->EnumChannelName(dwSpace, dwChannel);
	return pStr;
}

const BOOL cProxyServerEx::SetChannel(const DWORD dwSpace, const DWORD dwChannel)
{
	BOOL b = FALSE;
	if (m_pIBon2)
		b = m_pIBon2->SetChannel(dwSpace, dwChannel);
	return b;
}

const DWORD cProxyServerEx::GetTotalDeviceNum(void)
{
	DWORD d = 0;
	if (m_pIBon3)
		d = m_pIBon3->GetTotalDeviceNum();
	return d;
}

const DWORD cProxyServerEx::GetActiveDeviceNum(void)
{
	DWORD d = 0;
	if (m_pIBon3)
		d = m_pIBon3->GetActiveDeviceNum();
	return d;
}

const BOOL cProxyServerEx::SetLnbPower(const BOOL bEnable)
{
	BOOL b = FALSE;
	if (m_pIBon3)
		b = m_pIBon3->SetLnbPower(bEnable);
	return b;
}

#if defined(HAVE_UI) || defined(BUILD_AS_SERVICE)

static DWORD WINAPI Listen(LPVOID pv)
{
	stHost *phwork = static_cast<stHost *>(pv);
	stHost phi = *phwork;
	delete phwork;

#else
static int Listen()
{
	stHost phi = g_Host;

#endif
	addrinfo hints, *results, *rp;
	SOCKET lsock[MAX_HOSTS]{} ;
	int hostCnt = 0;
	fd_set rd{};
	timeval tv{};
	string strPort, strHost;

#if defined (UNICODE) || defined (_UNICODE)
	WtoA(phi.port, strPort);
#else
	strPort = phi.port;
#endif

	for (vector<tstring>::iterator itr = phi.hostVec.begin(); itr != phi.hostVec.end(); itr++, hostCnt++) {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;

#if defined (UNICODE) || defined (_UNICODE)
		WtoA(*itr, strHost);
#else
		strHost = *itr;
#endif

		if (getaddrinfo(strHost.c_str(), strPort.c_str(), &hints, &results) != 0) {
			hints.ai_flags = AI_PASSIVE;
			if (getaddrinfo(strHost.c_str(), strPort.c_str(), &hints, &results) != 0) {
				for (int j = 0; j < hostCnt; j++) {
					closesocket(lsock[j]);
				}
				return 1;
			}
		}

		for (rp = results; rp != NULL; rp = rp->ai_next) {
			lsock[hostCnt] = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			if (lsock[hostCnt] == INVALID_SOCKET) {
				continue;
			}

			BOOL exclusive = TRUE;
			setsockopt(lsock[hostCnt], SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&exclusive, sizeof(exclusive));

			if (bind(lsock[hostCnt], rp->ai_addr, (int)(rp->ai_addrlen)) != SOCKET_ERROR) {
				break;
			}

			closesocket(lsock[hostCnt]);
		}
		freeaddrinfo(results);
		if (rp == NULL) {
			for (int j = 0; j < hostCnt; j++) {
				closesocket(lsock[j]);
			}
			return 2;
		}

		if (listen(lsock[hostCnt], 4) == SOCKET_ERROR) {
			for (int j = 0; j <= hostCnt; j++) {
				closesocket(lsock[j]);
			}
			return 3;
		}
	}

	SOCKET csock;
	int len;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	while (!g_ShutdownEvent.IsSet())
	{
		FD_ZERO(&rd);
		for (int i = 0; i < hostCnt; i++) {
			FD_SET(lsock[i], &rd);
		}
		if ((len = select(0/*(int)(max(lsock) + 1)*/, &rd, NULL, NULL, &tv)) == SOCKET_ERROR)
		{
			for (int i = 0; i < hostCnt; i++) {
				closesocket(lsock[i]);
			}
			return 4;
		}
		if (len > 0)
		{
			for (int i = 0; i < hostCnt; i++)
			{
				if (FD_ISSET(lsock[i], &rd))
				{
					len--;
					if ((csock = accept(lsock[i], NULL, NULL)) != INVALID_SOCKET)
					{
						cProxyServerEx *pProxy = new cProxyServerEx();
						pProxy->setSocket(csock);
						HANDLE hThread = CreateThread(NULL, 0, cProxyServerEx::Reception, pProxy, 0, NULL);
						if (hThread)
							CloseHandle(hThread);
						else
							delete pProxy;
					}
				}
				if (len == 0)
					break;
			}
		}
	}

	for (int i = 0; i < hostCnt; i++)
		closesocket(lsock[i]);
	return 0;
}

#ifndef BUILD_AS_SERVICE
#ifdef HAVE_UI
void NotifyIcon(int mode)
{
	NOTIFYICONDATA nid = {};
	nid.cbSize = sizeof(nid);
	nid.hWnd = g_hWnd;
	nid.uID = ID_TASKTRAY;
	if (mode == 0)
	{
		// ADD
		nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		nid.uCallbackMessage = WM_TASKTRAY;
		nid.hIcon = LoadIcon(g_hInstance, _T("BDPEX_ICON"));
		wcscpy_s(nid.szTip, sizeof(nid.szTip)/sizeof(TCHAR), _T("BonDriverProxyExD"));
		
		for (int i=0;i<1000;i++)
		{
			if (Shell_NotifyIcon(NIM_ADD, &nid))
				break;	// 登録成功
			Sleep(500);	// ちょっと待ってから確認
			if (Shell_NotifyIcon(NIM_MODIFY, &nid))
				break;	// 登録成功してた	
		}
	}
	else
	{
		// DEL
		nid.uFlags = 0;
		Shell_NotifyIcon(NIM_DELETE, &nid);
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	static UINT s_iTaskbarRestart;

	switch (iMsg)
	{
	case WM_CREATE:
		s_iTaskbarRestart = RegisterWindowMessage(_T("TaskbarCreated"));
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_CLOSE:
		ModifyMenu(g_hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TASKTRAY_SHOW, _T("情報ウィンドウ表示"));
		ShowWindow(hWnd, SW_HIDE);
		return 0;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hDc = BeginPaint(hWnd, &ps);
		TEXTMETRIC tm;
		GetTextMetrics(hDc, &tm);
		union {
			SOCKADDR_STORAGE ss;
			SOCKADDR_IN si4;
			SOCKADDR_IN6 si6;
		};
		char addr[INET6_ADDRSTRLEN];
		int port, len, num = 0;
		TCHAR buf[2048];
		string strAddr, strDriverMapKey;
		tstring tstrAddr, tstrDriverMapKey;;
		g_Lock.Enter();
		std::list<cProxyServerEx *>::iterator it = g_InstanceList.begin();
		while (it != g_InstanceList.end())
		{
			len = sizeof(ss);
			if (getpeername((*it)->m_s, (SOCKADDR *)&ss, &len) == 0)
			{
				if (ss.ss_family == AF_INET)
				{
					// IPv4
					inet_ntop(AF_INET, &(si4.sin_addr), addr, sizeof(addr));
					port = ntohs(si4.sin_port);
				}
				else
				{
					// IPv6
					inet_ntop(AF_INET6, &(si6.sin6_addr), addr, sizeof(addr));
					port = ntohs(si6.sin6_port);
				}
			}
			else
			{
				strcpy_s(addr, INET6_ADDRSTRLEN, "unknown host...");
				port = 0;
			}
			std::vector<stDriver> &vstDriver = DriversMap[(*it)->m_pDriversMapKey];

#if defined(UNICODE) || defined(_UNICODE)
			strAddr = addr;
			strDriverMapKey = (*it)->m_pDriversMapKey;
			AtoW(strAddr, tstrAddr);
			AtoW(strDriverMapKey, tstrDriverMapKey);
#else
			tstrAddr = addr;
			tstrDriverMapKey = (*it)->m_pDriversMapKey;
#endif
			TCHAR decName[256];
			if (vstDriver[(*it)->m_iDriverNo].decoderNo == -1) {
				_tcscpy_s(decName, 256, _T("No Decoder"));
			}
			else {
				_tcscpy_s(decName, 256, DecoderVec[vstDriver[(*it)->m_iDriverNo].decoderNo]->strDecoder);
			}

			_stprintf_s(buf, 2048, _T("%2d: [%s]:[%d] / [%s][%s] / space[%u] ch[%u] / decoder[%s]"), num, tstrAddr.c_str(), port, tstrDriverMapKey.c_str(), vstDriver[(*it)->m_iDriverNo].strBonDriver, (*it)->m_dwSpace, (*it)->m_dwChannel, decName);
			TextOut(hDc, 5, 5 + (num * tm.tmHeight), buf, (int)_tcslen(buf));

			num++;
			++it;
		}
		g_Lock.Leave();
		EndPaint(hWnd, &ps);
		return 0;
	}

	case WM_TASKTRAY:
	{
		switch (LOWORD(lParam))
		{
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			POINT pt;
			GetCursorPos(&pt);
			SetForegroundWindow(hWnd);
			TrackPopupMenu(g_hMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
			PostMessage(hWnd, WM_NULL, 0, 0);
			return 0;
		}
		break;
	}

	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case ID_TASKTRAY_SHOW:
		{
			ModifyMenu(g_hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TASKTRAY_HIDE, _T("情報ウィンドウ非表示"));
			ShowWindow(hWnd, SW_SHOW);
			return 0;
		}

		case ID_TASKTRAY_HIDE:
		{
			ModifyMenu(g_hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TASKTRAY_SHOW, _T("情報ウィンドウ表示"));
			ShowWindow(hWnd, SW_HIDE);
			return 0;
		}

		case ID_TASKTRAY_RELOAD:
		{
			int ret;
			if (g_InstanceList.size() != 0)
			{
				if (MessageBox(hWnd, _T("接続中のクライアントが存在しています。切断されますがよろしいですか？"), _T("Caution"), MB_YESNO) != IDYES)
					return 0;
			}
			ShutdownInstances();
			CleanUp();
			ret = Init(g_hInstance);
			switch (ret) {
			case -1:
			case -2:
				MessageBox(NULL, _T("iniファイルが見つかりません。正しく設置したのち再読み込みして下さい。"), _T("Error"), MB_OK);
				break;
			case -3:
				MessageBox(NULL, _T("iniファイル[OPTION] ADDRESS 読み込み失敗。正しく設置したのち再読み込みして下さい。"), _T("Error"), MB_OK);
				break;
			case -4:
				MessageBox(NULL, _T("iniファイル[OPTION] PORT 読み込み失敗。正しく設置したのち再読み込みして下さい。"), _T("Error"), MB_OK);
				break;
			case -5:
				MessageBox(NULL, _T("iniファイル[DECODER]読み込み失敗。正しく設置したのち再読み込みして下さい。"), _T("Error"), MB_OK);
				break;
			case -6:
				MessageBox(NULL, _T("iniファイル[BONDRIVER]読み込み失敗。正しく設置したのち再読み込みして下さい。"), _T("Error"), MB_OK);
				break;
			}
			if (ret < 0) {
				return 0;
			}

			stHost *phi = new stHost;
			*phi = g_Host;
			g_hListenThread = CreateThread(NULL, 0, Listen, phi, 0, NULL);
			if (g_hListenThread == NULL)
			{
				delete phi;
				MessageBox(NULL, _T("待ち受けスレッドの作成に失敗しました。終了します。"), _T("Error"), MB_OK);
				PostQuitMessage(0);
			}
			else
				MessageBox(hWnd, _T("再読み込みしました。"), _T("Info"), MB_OK);
			return 0;
		}

		case ID_TASKTRAY_EXIT:
		{
			if (g_InstanceList.size() != 0)
			{
				if (MessageBox(hWnd, _T("接続中のクライアントが存在していますが、よろしいですか？"), _T("Caution"), MB_YESNO) != IDYES)
					return 0;
			}
			PostQuitMessage(0);
			return 0;
		}
		}
		break;
	}

	default:
		if (iMsg == s_iTaskbarRestart)
			NotifyIcon(0);
		break;
	}
	return DefWindowProc(hWnd, iMsg, wParam, lParam);
}

 int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int nShowCmd)
{
#if _DEBUG
	HANDLE hLogFile = CreateFile(_T("dbglog.txt"), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	_CrtMemState ostate, nstate, dstate;
	_CrtMemCheckpoint(&ostate);
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_WARN, hLogFile);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_ERROR, hLogFile);
	_RPT0(_CRT_WARN, "--- PROCESS_START ---\n");
//	int *p = new int[2];	// リーク検出テスト用
#endif

	int ret;

	ret = Init(hInstance);
	switch (ret) {
	case -1:
	case -2:
		MessageBox(NULL, _T("iniファイルが見つかりません。"), _T("Error"), MB_OK);
		break;
	case -3:
		MessageBox(NULL, _T("iniファイル[OPTION] ADDRESS 読み込み失敗"), _T("Error"), MB_OK);
		break;
	case -4:
		MessageBox(NULL, _T("iniファイル[OPTION] PORT 読み込み失敗"), _T("Error"), MB_OK);
		break;
	case -5:
		MessageBox(NULL, _T("iniファイル[DECODER]読み込み失敗"), _T("Error"), MB_OK);
		break;
	case -6:
		MessageBox(NULL, _T("iniファイル[BONDRIVER]読み込み失敗"), _T("Error"), MB_OK);
		break;
	}
	if (ret < 0) {
		return -1;
	}

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		MessageBox(NULL, _T("winsockの初期化に失敗しました。"), _T("Error"), MB_OK);
		return -2;
	}

	stHost *phi = new stHost;
	*phi = g_Host;
	
	g_hListenThread = CreateThread(NULL, 0, Listen, phi, 0, NULL);
	if (g_hListenThread == NULL)
	{
		delete phi;
		MessageBox(NULL, _T("待ち受けスレッドの作成に失敗しました。"), _T("Error"), MB_OK);
		return -3;
	}

	MSG msg;
	WNDCLASSEX wndclass{};

	wndclass.cbSize = sizeof(wndclass);
	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = hInstance;
	wndclass.hIcon = LoadIcon(hInstance, _T("BDPEX_ICON"));
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = _T("bdpex");
	wndclass.hIconSm = LoadIcon(hInstance, _T("BDPEX_ICON"));

	RegisterClassEx(&wndclass);

	g_hWnd = CreateWindow(_T("bdpex"), _T("Information"), WS_OVERLAPPED | WS_SYSMENU | WS_THICKFRAME, CW_USEDEFAULT, 0, 1300, 640, NULL, NULL, hInstance, NULL);

//	ShowWindow(g_hWnd, nCmdShow);
//	UpdateWindow(g_hWnd);

	g_hInstance = hInstance;
	g_hMenu = CreatePopupMenu();
	InsertMenu(g_hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TASKTRAY_SHOW, _T("情報ウィンドウ表示"));
	InsertMenu(g_hMenu, 1, MF_BYPOSITION | MF_STRING, ID_TASKTRAY_RELOAD, _T("ini再読み込み"));
	InsertMenu(g_hMenu, 2, MF_BYPOSITION | MF_STRING, ID_TASKTRAY_EXIT, _T("終了"));
	NotifyIcon(0);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	ShutdownInstances();	// g_hListenThreadはこの中でCloseHandle()される
	CleanUp();				// ShutdownInstances()でDriversMapにアクセスするスレッドは無くなっているはず

	NotifyIcon(1);
	DestroyMenu(g_hMenu);

	WSACleanup();

#if _DEBUG
	_CrtMemCheckpoint(&nstate);
	if (_CrtMemDifference(&dstate, &ostate, &nstate))
	{
		_CrtMemDumpStatistics(&dstate);
		_CrtMemDumpAllObjectsSince(&ostate);
	}
	_RPT0(_CRT_WARN, "--- PROCESS_END ---\n");
	CloseHandle(hLogFile);
#endif

	return (int)msg.wParam;
}
#else
int WINAPI WinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE/*hPrevInstance*/, _In_ LPSTR/*lpCmdLine*/, _In_ int/*nCmdShow*/)
{
	int ret;
	ret = Init(hInstance);
	switch (ret) {
	case -1:
	case -2:
		fprintf(stderr, "Init() ret : %d iniファイルが見つかりません。\n", ret);
		break;
	case -3:
		fprintf(stderr, "Init() ret : %d iniファイル[OPTION] ADDRESS 読み込み失敗\n", ret);
		break;
	case -4:
		fprintf(stderr, "Init() ret : %d iniファイル[OPTION] PORT 読み込み失敗\n", ret);
		break;
	case -5:
		fprintf(stderr, "Init() ret : %d iniファイル[DECODER]読み込み失敗\n", ret);
		break;
	case -6:
		fprintf(stderr, "Init() ret : %d iniファイル[BONDRIVER]読み込み失敗\n", ret);
		break;
	}
	if (ret < 0) {
		return -1;
	}

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return -2;

	ret = Listen();

	{
		// 来ないけど一応
		LOCK(g_Lock);
		CleanUp();
	}

	WSACleanup();
	return ret;
}
#endif
#else
#include "ServiceMain.cpp"
#endif
