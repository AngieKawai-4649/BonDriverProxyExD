#ifndef __BONDRIVER_PROXYEX_H__
#define __BONDRIVER_PROXYEX_H__
#include <winsock2.h>
#include <ws2tcpip.h>
#include <tchar.h>
#include <process.h>
#include <sys/stat.h>
#include <list>
#include <queue>
#include <map>

#include "Common.h"
#include "IBonDriver3.h"
#include "IB25Decoder.h"
#include "StringUtil.h"



#define HAVE_UI
#ifdef BUILD_AS_SERVICE
#undef HAVE_UI
#endif

#if _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#define WAIT_TIME	10	// GetTsStream()の後で、dwRemainが0だった場合に待つ時間(ms)

typedef std::basic_string<_TCHAR> tstring;

#define cut_tab_space(tstr, tchr ) { tstr = tchr; Replace(tstr, _T("\t"), _T("")); Replace(tstr, _T(" "), _T("")); }

const tstring SPACE = _T(" \n\r\t\f\v");
//const tstring SPACE = _T("\s");

tstring ltrim(const tstring& s)
{
	size_t start = s.find_first_not_of(SPACE);
	return (start == tstring::npos) ? _T("") : s.substr(start);
}
tstring rtrim(const tstring& s)
{
	size_t end = s.find_last_not_of(SPACE);
	return (end == tstring::npos) ? _T("") : s.substr(0, end + 1);
}
tstring trim(const tstring& s) {
	return rtrim(ltrim(s));
}



////////////////////////////////////////////////////////////////////////////////

#define MAX_HOSTS	8	// listen()できるソケットの最大数

static size_t g_PacketFifoSize;
static DWORD g_TsPacketBufSize;
static DWORD g_OpenTunerRetDelay;
static BOOL g_SandBoxedRelease;
static BOOL g_DisableUnloadBonDriver;
static DWORD g_ProcessPriority;		// 不要だと思うけど保持しておく
static int g_ThreadPriorityTsReader;
static int g_ThreadPrioritySender;
static EXECUTION_STATE g_ThreadExecutionState;

#include "BdpPacket.h"

#define MAX_DRIVERS	64		// ドライバのグループ数とグループ内の数の両方
//static char **g_ppDriver[MAX_DRIVERS];
struct stDriver {
	LPTSTR strBonDriver;	// BonDriver.dll フルパス＋ファイル名
	HMODULE hModule;
	BOOL bUsed;
	FILETIME ftLoad;
	int decoderNo;

	stDriver() {
		strBonDriver = NULL;
		hModule = NULL;
		bUsed = FALSE;
		ftLoad = {};
		decoderNo = 0;
	}

	stDriver(LPCTSTR dri) {
		strBonDriver = _tcsdup(dri);
		hModule = NULL;
		bUsed = FALSE;
		ftLoad = {};
		decoderNo = 0;
	}

	~stDriver() {
	}

	stDriver operator= (const stDriver o) {
		strBonDriver = o.strBonDriver;
		hModule = o.hModule;
		bUsed = o.bUsed;
		ftLoad = o.ftLoad;
		return(*this);
	}

};
static std::map<char *, std::vector<stDriver>> DriversMap;

struct stDecoder {
	LPTSTR strDecoder;
	HMODULE hModule;
	IB25Decoder2* (*CreateInstance)();
	bool emm;
	bool nullPacket;
	int round;
	stDecoder() {
		strDecoder = NULL;
		hModule = NULL;
		CreateInstance = NULL;
		emm = FALSE;
		nullPacket = FALSE;
		round = 4;
	}
	stDecoder(LPCTSTR dec, const BOOL e, const BOOL n, const int r) {
		strDecoder = _tcsdup(dec);
		hModule = NULL;
		CreateInstance = NULL;
		emm = e;
		nullPacket = n;
		round = r;
	}
	~stDecoder() {
	}

};
static std::vector<stDecoder*> DecoderVec;

struct stHost {
	std::vector<tstring> hostVec;
	tstring port;
	stHost() {
		port = _T("");
	}
	stHost operator= (const stHost o) {
		hostVec = o.hostVec;
		port = o.port;
		return *this;
	}
};
static stHost g_Host;

////////////////////////////////////////////////////////////////////////////////

struct stTsReaderArg {
	IBonDriver *pIBon;
	volatile BOOL StopTsRead;
	volatile BOOL ChannelChanged;
	DWORD pos;
	std::list<cProxyServerEx *> TsReceiversList;
	std::list<cProxyServerEx *> WaitExclusivePrivList;
	cCriticalSection TsLock;
	stDecoder* decoder;
	stTsReaderArg()
	{
		pIBon = NULL;
		StopTsRead = FALSE;
		ChannelChanged = TRUE;
		pos = 0;
		decoder = NULL;
	}
	~stTsReaderArg() {
	}
	stTsReaderArg& operator= (const stTsReaderArg& o) {
		pIBon = o.pIBon;
		StopTsRead = o.StopTsRead;
		ChannelChanged = o.ChannelChanged;
		pos = o.pos;
		TsReceiversList = o.TsReceiversList;
		WaitExclusivePrivList = o.WaitExclusivePrivList;
		TsLock = o.TsLock;
		decoder = o.decoder;
		return *this;
	}
};

class cProxyServerEx {
#ifdef HAVE_UI
public:
#endif
	SOCKET m_s;
	DWORD m_dwSpace;
	DWORD m_dwChannel;
	char *m_pDriversMapKey;
	int m_iDriverNo;
#ifdef HAVE_UI
private:
#endif
	int m_iDriverUseOrder;
	IBonDriver *m_pIBon;
	IBonDriver2 *m_pIBon2;
	IBonDriver3 *m_pIBon3;
	HMODULE m_hModule;
	cEvent m_Error;
	BOOL m_bTunerOpen;
	HANDLE m_hTsRead;
	BYTE m_bChannelLock;
	stTsReaderArg *m_pTsReaderArg;
	cPacketFifo m_fifoSend;
	cPacketFifo m_fifoRecv;

	DWORD Process();
	int ReceiverHelper(char *pDst, DWORD left);
	static DWORD WINAPI Receiver(LPVOID pv);
	void makePacket(enumCommand eCmd, BOOL b);
	void makePacket(enumCommand eCmd, DWORD dw);
	void makePacket(enumCommand eCmd, LPCTSTR str);
	void makePacket(enumCommand eCmd, BYTE *pSrc, DWORD dwSize, float fSignalLevel);
	static DWORD WINAPI Sender(LPVOID pv);
	static DWORD WINAPI TsReader(LPVOID pv);
	void StopTsReceive();

	BOOL SelectBonDriver(LPCSTR p, BYTE bChannelLock);
	IBonDriver *CreateBonDriver();

	// IBonDriver
	const BOOL OpenTuner(void);
	void CloseTuner(void);
	void PurgeTsStream(void);
	void Release(void);

	// IBonDriver2
	LPCTSTR EnumTuningSpace(const DWORD dwSpace);
	LPCTSTR EnumChannelName(const DWORD dwSpace, const DWORD dwChannel);
	const BOOL SetChannel(const DWORD dwSpace, const DWORD dwChannel);

	// IBonDriver3
	const DWORD GetTotalDeviceNum(void);
	const DWORD GetActiveDeviceNum(void);
	const BOOL SetLnbPower(const BOOL bEnable);

public:
	cProxyServerEx();
	~cProxyServerEx();
	void setSocket(SOCKET s){ m_s = s; }
	static DWORD WINAPI Reception(LPVOID pv);

	cProxyServerEx& operator= ( cProxyServerEx& ) { return *this; }
	cProxyServerEx* operator= (cProxyServerEx *) { return this; }
};


class cBonDriverPath {
	LPTSTR curpath;
	std::vector<LPTSTR> bonfolder;

public:
	cBonDriverPath() {
		curpath = NULL;
	}

	void setPath(LPCTSTR cur) {
		curpath = _tcsdup(cur);
	}

	void setFolder(LPCTSTR folders) {
		tstring tstfolders = folders;
		for (std::vector<LPTSTR>::iterator itr = bonfolder.begin(); itr != bonfolder.end(); itr++) {
			free(*itr);
		}
		bonfolder.clear();
		bonfolder.shrink_to_fit();
		while (tstfolders.size() > 0) {
			tstring folder;
			BOOL rtn;
			rtn = Separate(tstfolders, _T(","), folder, tstfolders);
			if (folder != _T("")) {
				folder = trim(folder);
				if (folder != _T("")) {
					bonfolder.emplace_back(_tcsdup(folder.c_str()));
				}
			}
			if (!rtn) {
				break;
			}
		}
	}

	BOOL getBonDriverPath(LPCTSTR bonDriver, LPTSTR bonPath) {
		struct _stat64 s;
		TCHAR bonFilePath[MAX_PATH];
		_tcscpy_s(bonFilePath, MAX_PATH, curpath);
		_tcsncat_s(bonFilePath, MAX_PATH - _tcslen(bonFilePath), bonDriver, _MAX_FNAME);
		if (_tstat64(bonFilePath, &s) == 0) {
			_tcscpy_s(bonPath, MAX_PATH, bonFilePath);
			return(TRUE);
		}
		else {
			BOOL rtn = FALSE;
			for (std::vector<LPTSTR>::iterator itr = bonfolder.begin(); itr != bonfolder.end(); itr++) {
				_tcscpy_s(bonFilePath, MAX_PATH, curpath);
				_tcsncat_s(bonFilePath, MAX_PATH - _tcslen(bonFilePath), (*itr), _MAX_FNAME);
				_tcsncat_s(bonFilePath, MAX_PATH - _tcslen(bonFilePath), _T("\\"), _MAX_FNAME);
				_tcsncat_s(bonFilePath, MAX_PATH - _tcslen(bonFilePath), bonDriver, _MAX_FNAME);
				if (_tstat64(bonFilePath, &s) == 0) {
					rtn = TRUE;
					_tcscpy_s(bonPath, MAX_PATH, bonFilePath);
					break;
				}
			}
			return(rtn);
		}
	}

	void freeMemory() {
		if (curpath) {
			free(curpath);
		}
		curpath = NULL;
		for (vector<LPTSTR>::iterator itr = bonfolder.begin(); itr != bonfolder.end(); itr++) {
			free(*itr);
		}
		bonfolder.clear();
		bonfolder.shrink_to_fit();
	}
};

static std::list<cProxyServerEx *> g_InstanceList;
static cCriticalSection g_Lock;
static cEvent g_ShutdownEvent(TRUE, FALSE);
#if defined(HAVE_UI) || defined(BUILD_AS_SERVICE)
static HANDLE g_hListenThread;

static cBonDriverPath g_bonDriverPath;

#endif

#endif	// __BONDRIVER_PROXYEX_H__
