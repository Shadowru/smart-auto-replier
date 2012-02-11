/*
 *  Smart Auto Replier (SAR) - auto replier plugin for Miranda IM
 *
 *  Copyright (C) 2005 - 2012 by Volodymyr M. Shcherbyna <volodymyr@shcherbyna.com>
 *
 *      This file is part of SAR.
 *
 *  SAR is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  SAR is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with SAR.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"
#include "messageshandler.h"
#include <time.h>

extern CCrushLog CRUSHLOGOBJ;

/// init static members...
CMessagesHandler * CMessagesHandler::m_pThis = NULL;

/// thread that is checking
/// users that sent messages,
/// analize sent time and if 
/// reply delay is greater then
/// difference between current time and 
/// sent time thread deletes item.
INT WINAPI ContactsFetcherThread(LPVOID lp)
{
BEGIN_PROTECT_AND_LOG_CODE
#define SLEEP_TIME	50	
	CMessagesHandler *pHolder = CMessagesHandler::GetObject();
	if (pHolder)
	{
		while (pHolder->m_bShouldWork)
		{
			WaitForSingleObject(pHolder->m_hEnableEvent, INFINITE);
			if (!pHolder->m_bShouldWork)
				break;

			EnterCriticalSection(&pHolder->m_critSect);

			if (pHolder->m_contacts.size() == 0)
			{
				LeaveCriticalSection(&pHolder->m_critSect);
				Sleep (SLEEP_TIME);
				continue;
			}

			SYSTEMTIME sysTime = {0};
			FILETIME   ft = {0};
			LARGE_INTEGER liNow = {0};

			GetLocalTime (&sysTime);
			SystemTimeToFileTime(&sysTime, &ft);
			memcpy(&liNow, &ft, sizeof(liNow));

			ContactsMap::iterator it;

			CONTACT_METAINFO rawm = {0};
			LARGE_INTEGER liDiff = {0};

			for (it = pHolder->m_contacts.begin(); it != pHolder->m_contacts.end(); it++)
			{
				if (pHolder->m_contacts.size() == 0)
				{
					break;
				}
				rawm = it->second;
				liDiff.QuadPart = liNow.QuadPart - rawm.StartTime.QuadPart;
				if (liDiff.QuadPart >= rawm.DiffTime.QuadPart)
				{
					if (pHolder->m_contacts.size() == 1)
					{
						pHolder->m_contacts.clear();
						break;
					}
					else
						pHolder->m_contacts.erase(it);
				}
			}

			LeaveCriticalSection(&pHolder->m_critSect);

			Sleep (SLEEP_TIME);
		}
	}
END_PROTECT_AND_LOG_CODE
	return FALSE;
}

/// ctor of main plugin manager
CMessagesHandler::CMessagesHandler(void) : IMSingeltone<CMessagesHandler>(this),
m_hFetcherThread(NULL), 
m_hEnableEvent(NULL), 
m_bShouldWork(true),
m_bInited(false)
{
BEGIN_PROTECT_AND_LOG_CODE
	m_settings.Init();
END_PROTECT_AND_LOG_CODE
}

/// dtor of main plugin manager
CMessagesHandler::~CMessagesHandler(void)
{
BEGIN_PROTECT_AND_LOG_CODE
	m_settings.DeInit();
	m_bShouldWork = false;
	CMessagesHandler::m_pThis = NULL;
END_PROTECT_AND_LOG_CODE
}

/// enables or disables feature
void CMessagesHandler::MakeAction(bool bEnable)
{
BEGIN_PROTECT_AND_LOG_CODE	
	REPLYER_SETTINGS & s = m_settings.getSettings();
	s.bEnabled = bEnable;
	m_settings.setSettings(s);

	if (bEnable)
	{
		HookEvents();
		SetEvent (m_hEnableEvent);
	}
	else
		UnHookEvents();
END_PROTECT_AND_LOG_CODE
}

bool CMessagesHandler::AllowReply(HANDLE hContact)
{	
BEGIN_PROTECT_AND_LOG_CODE
	if (!m_settings.getSettings().bEnabled)
		return FALSE;
	ContactsMap::iterator it;
	bool bRetVal = false;

	EnterCriticalSection(&m_critSect);

	it = m_contacts.find(hContact);

	SYSTEMTIME sysTime = {0};
	FILETIME   ft = {0};
	GetLocalTime (&sysTime);
	SystemTimeToFileTime(&sysTime, &ft);

	if (it == m_contacts.end())
	{	/// no such contact...
		CONTACT_METAINFO cm = {0};		
		
		memcpy(&cm.StartTime, &ft, sizeof(cm.StartTime));
		cm.DiffTime.QuadPart = m_settings.getSettings().ReplayDelay * nano100SecInSec;
		ContactsPair p(hContact, cm);		

		m_contacts.insert(p);
		bRetVal = true;		
	}
	else
	{ /// there is one...
		memcpy(&it->second.StartTime, &ft, sizeof(it->second.StartTime));
	}

	LeaveCriticalSection(&m_critSect);

	return bRetVal;
END_PROTECT_AND_LOG_CODE
	return false;
}

void CMessagesHandler::HookEvents(void)
{
BEGIN_PROTECT_AND_LOG_CODE
#ifdef _DEBUG
#endif

	InitializeCriticalSection(&m_critSect);
	m_settings.Init();
#ifdef _DEBUG
	bool bEnabled = m_settings.getSettings().bEnabled;
#endif
	m_hEnableEvent = CreateEvent(NULL, TRUE, m_settings.getSettings().bEnabled, NULL);	

	DWORD dw = 0;
	m_bShouldWork = true;
	m_hFetcherThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ContactsFetcherThread, NULL, NULL, &dw);
	m_hEvents[0] = HookEvent(ME_DB_EVENT_ADDED, CMessagesHandler::EventAddHook);
	m_bInited = true;
END_PROTECT_AND_LOG_CODE
}

void CMessagesHandler::UnHookEvents(void)
{
BEGIN_PROTECT_AND_LOG_CODE	
	m_bShouldWork = false;
	if (!m_bInited)
		return;
	SetEvent(m_hEnableEvent);

	for (size_t i = 0; i < HOOKS_NUM; i++)
		UnhookEvent(m_hEvents[i]);

	WaitForSingleObject(m_hFetcherThread, INFINITE);
	DeleteCriticalSection(&m_critSect);
	CloseHandle(m_hEnableEvent);
	//m_settings.DeInit();

	m_hEnableEvent = NULL;
	m_hFetcherThread = NULL;

END_PROTECT_AND_LOG_CODE
}

LPTSTR CMessagesHandler::GetContactName(HANDLE hContact)
{
BEGIN_PROTECT_AND_LOG_CODE
	return reinterpret_cast<LPTSTR>(CallService(MS_CLIST_GETCONTACTDISPLAYNAME, (WPARAM)hContact, GCDNF_NOMYHANDLE));
END_PROTECT_AND_LOG_CODE
	return NULL;
}

int CMessagesHandler::SendProtoMess(HANDLE hContact, LPCSTR szMess)
{	
BEGIN_PROTECT_AND_LOG_CODE
	return CallContactService(hContact, PSS_MESSAGE, 0, reinterpret_cast<LPARAM>(szMess));
END_PROTECT_AND_LOG_CODE
	return 0;
}

/// this handle is invoked wnen
/// event is added to db
int CMessagesHandler::EventAddHook(WPARAM wp, LPARAM lp)
{
BEGIN_PROTECT_AND_LOG_CODE
	DWORD dwOffset  = static_cast<DWORD> (lp);
	HANDLE hContact = reinterpret_cast<HANDLE> (wp);
	DBEVENTINFO dbei;	

	if (!hContact || !dwOffset)
		return FALSE;	/// unspecifyed error 
	
	ZeroMemory(&dbei, sizeof(dbei));
	dbei.cbSize = sizeof(dbei);
	dbei.cbBlob = 0;
	
	CallService(MS_DB_EVENT_GET, lp, (LPARAM)&dbei); /// detect size of msg

	if ((dbei.eventType != EVENTTYPE_MESSAGE) || (dbei.flags == DBEF_READ) || (dbei.flags == DBEF_SENT) ) 
		return FALSE; /// we need EVENTTYPE_MESSAGE event..
	else
	{	/// needed event has occured..
		if (!dbei.cbBlob)	/// invalid size
			return FALSE;
		dbei.pBlob = reinterpret_cast<LPBYTE>(VirtualAlloc(NULL, dbei.cbBlob, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE));
		if (dbei.pBlob)
		{
			CallService(MS_DB_EVENT_GET, lp, reinterpret_cast<LPARAM>(&dbei));

			char* szContactName = reinterpret_cast<char*>(CallService(MS_CLIST_GETCONTACTDISPLAYNAME, wp, GCDNF_NOMYHANDLE));

			CMessagesHandler *ptrHolder = NULL;
			ptrHolder = CMessagesHandler::GetObject();

			if (!szContactName || !ptrHolder)
				return FALSE;

			if (!ptrHolder->AllowReply(hContact))
				return FALSE;

			LPSTR lpMessage = NULL;

			{
				RULE_METAINFO inf = {0};
				inf.ContactName = szContactName;

				/// analizes all rules and formes autoreply
				/// message - it's stored in lpMessage
				/// then - just sent it to user
				CSettingsHandler & settingsManager = ptrHolder->getSettings();
				settingsManager.getStorage().IsRuleMatch(inf, lpMessage, reinterpret_cast<LPTSTR&>(dbei.pBlob));
				int nRetVal = SendProtoMess(hContact, lpMessage);
				if (settingsManager.getSettings().bSaveToHistory)
				{
					ptrHolder->WriteToHistory(lpMessage, hContact);
				}
				VirtualFree (lpMessage, NULL, MEM_RELEASE);
				VirtualFree (dbei.pBlob, NULL, MEM_RELEASE);
				return FALSE;
			}
		}
	}

END_PROTECT_AND_LOG_CODE
	return FALSE;
}

void CMessagesHandler::WriteToHistory(LPTSTR lpMsg, HANDLE hContact)
{
	LPTSTR lp1 = Translate("Generated autoreply:\r\n");

	LPTSTR lpszMess = reinterpret_cast<LPTSTR>(VirtualAlloc (NULL, _tcslen(lpMsg) + _tcslen(lp1), MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE));
	if (lpszMess == NULL)
		return;
	
	_tcscpy(lpszMess, lp1);
	_tcscat(lpszMess, lpMsg);

	DBEVENTINFO dbei = {0};
	dbei.cbSize = sizeof(dbei);
	dbei.eventType = EVENTTYPE_MESSAGE;
	dbei.flags = DBEF_READ;
	dbei.szModule = (char*)CallService(MS_PROTO_GETCONTACTBASEPROTO,(WPARAM)hContact, 0);
	dbei.timestamp = time(NULL);
	dbei.cbBlob = _tcslen(lpszMess) + 1;
	dbei.pBlob = (PBYTE)lpszMess;
	CallService(MS_DB_EVENT_ADD, (WPARAM)hContact, (LPARAM)&dbei);
	VirtualFree (lpszMess, NULL, MEM_RELEASE);
}

/// getting settings..
CSettingsHandler & CMessagesHandler::getSettings(void)
{
BEGIN_PROTECT_AND_LOG_CODE
	return m_settings;
END_PROTECT_AND_LOG_CODE
}