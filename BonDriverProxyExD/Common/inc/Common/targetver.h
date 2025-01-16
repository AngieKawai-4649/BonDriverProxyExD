#pragma once

// Windows ヘッダー ファイルで使用されるマクロ
// ---------------------------------------------------------------------------------|
// 必要な最小システム							| _WIN32_WINNT と WINVER の最小値	|
// ---------------------------------------------------------------------------------|
// Windows 10									| _WIN32_WINNT_WIN10	(0x0A00)	|
// Windows 8.1									| _WIN32_WINNT_WINBLUE	(0x0603)	|
// Windows 8									| _WIN32_WINNT_WIN8		(0x0602)	|
// Windows 7									| _WIN32_WINNT_WIN7		(0x0601)	|
// Windows Server 2008							| _WIN32_WINNT_WS08		(0x0600)	|
// Windows Vista								| _WIN32_WINNT_VISTA	(0x0600)	|	
// Windows Server 2003 SP1、Windows XP(SP2)		| _WIN32_WINNT_WS03		(0x0502)	|
// Windows Server 2003、Windows XP				| _WIN32_WINNT_WINXP	(0x0501)	|
// ---------------------------------------------------------------------------------|
// GetTickCount64() を使用できるのはVISA 以降

#define _WIN32_WINNT _WIN32_WINNT_VISTA
#include <SDKDDKVer.h>
