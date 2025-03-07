#ifndef __STRING_UTIL_H__
#define __STRING_UTIL_H__

// MFC‚ÅŽg‚¤Žž—p
//#define _MFC
#ifdef _MFC
#ifdef _DEBUG
#undef new
#endif
#include <string>
#include <map>
#include <vector>
#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#else
#include <string>
#include <map>
#include <vector>
#include <deque>
#endif
using namespace std;
#include <windows.h>
#include <TCHAR.h>
#include <WinDef.h>


#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }



void Format(string& strBuff, const char* format, ...);

void Format(wstring& strBuff, const WCHAR* format, ...);

void Replace(string& strBuff, const string strOld, const string strNew);

void Replace(wstring& strBuff, const wstring strOld, const wstring strNew);

void WtoA(wstring strIn, string& strOut);

void WtoUTF8(wstring strIn, string& strOut);

void AtoW(string strIn, wstring& strOut);

void UTF8toW(string strIn, wstring& strOut);

BOOL Separate(string strIn, string strSep, string& strLeft, string& strRight);

BOOL Separate(wstring strIn, wstring strSep, wstring& strLeft, wstring& strRight);

void ChkFolderPath(string& strPath);

void ChkFolderPath(wstring& strPath);

void ChkFileName(string& strPath);

void ChkFileName(wstring& strPath);

int CompareNoCase(string str1, string str2);

int CompareNoCase(wstring str1, wstring str2);

#endif
