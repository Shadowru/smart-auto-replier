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

#pragma once

#include "RuleItem.h"
#include "RulesStorage.h"
#include "interfaces\ISettings.h"
#include "comfunc.h"
#include "scriptsreader.h"

/// settings defs...
#define SETTINGS_DB						"sar.cs"
#define READMEFILE						"sar.html"

#define SETTINGS_HEADER_MAXVALENGTH		1024 * 1		/// 1 kb by def
#define SETTINGS_MESSAGE_MAXVALENGTH	1024 * 3		/// 3 kb by def

#define SETTINGS_DEF_COMMON_DWMION			1
#define SETTINGS_DEF_COMMVALUE				0
#define SETTINGS_DEF_HEADER					"%currdate%, Hey! %username% user has left auto reply message:"
#define SETTINGS_DEF_MESSAGE				"Why do you write this:\r\n'%incomingmessage%' ?\r\nI am not near pc, read my mode msg:\r\n'%modemessage%'\r\n(%ownnickname%)"
#define SETTINGS_DEF_COMMON_VALREPDELAY		2
#define SETTINGS_DEF_COMMON_EN_ONMODEON		0
#define SETTINGS_DEF_COMMON_EN_DISONMODEON	0

#define SETTINGS_DEF_COMMON_EN_ONMODEDISVAL	0
#define SETTINGS_DEF_COMMON_EN_ONMODEONVAL	0
#define SETTINGS_DEF_COMMON_SHOWAURDLGWMC	0

/// settings that are stored in file...
/// serialized
typedef struct _REPLYER_SETTINGS : ISettingsStream
{
	bool			bEnabled;
	bool			bDisableWhenMirandaIsOn;
	int				ReplayDelay;
	bool			bEnableWhenModeIsSet;
	bool			bDisableWhenModeIsSet;
	int				ModeValue;
	int				ModeDisValue;
	bool			bShowAURDlgWhenModeChanges;
	bool			bFirstRun;
	bool			bSaveToHistory;
}REPLYER_SETTINGS, *PREPLYER_SETTINGS;

#ifndef MAX_PATH
	#define MAX_PATH 260
#endif

/// manager that handles settings of a plugin...
class CSettingsHandler : ISettings
{
public:
	CSettingsHandler(void);
	~CSettingsHandler(void);
public: /// ISettings interface...
	bool Serialize(ISettingsStream *pSettings, DWORD & ObjectSize);
	bool Deserialize(DWORD & ObjectSize);
public:	
	void Init(void);	/// deserialize...
	void DeInit(void);	/// serialize...
	REPLYER_SETTINGS & getSettings(void);
	CScriptsReader & getScriptsReader(void);
	CRulesStorage & getStorage(void);
	void setSettings(REPLYER_SETTINGS & settings, COMMON_RULE_ITEM * pcommrule = NULL);
	DWORD AddReplyAction(RULE_ITEM item, bool & bExists);
private:
	REPLYER_SETTINGS m_Settings;
	TCHAR			 m_szSettFileName[MAX_PATH];
	bool			 m_bResettings;
	CRulesStorage	 m_rulesStorage;
	CScriptsReader	 m_scriptsReader;
	DWORD			 m_replSize;
};