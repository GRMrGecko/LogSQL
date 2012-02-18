//
//  logsqlite.cpp
//  LogSQL
//
//  Created by Mr. Gecko on 2/3/12.
//  Copyright (c) 2012 Mr. Gecko's Media (James Coleman). http://mrgeckosmedia.com/
//
//  Permission to use, copy, modify, and/or distribute this software for any purpose
//  with or without fee is hereby granted, provided that the above copyright notice
//  and this permission notice appear in all copies.
//
//  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
//  REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
//  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
//  OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
//  DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
//  ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//

#include <znc/Chan.h>
#include <znc/User.h>
#include <znc/Client.h>
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <time.h>

#include <sqlite3.h>

class CLogSQLite : public CModule {
public:
    MODCONSTRUCTOR(CLogSQLite) {
        AddHelpCommand();
        AddCommand("Replay", static_cast<CModCommand::ModCmdFunc>(&CLogSQLite::ReplayCommand), "", "Play back the messages received.");
        AddCommand("ReplayAll", static_cast<CModCommand::ModCmdFunc>(&CLogSQLite::ReplayAllCommand), "[1|0]", "Replay all messages stored.");
        AddCommand("LogLimit", static_cast<CModCommand::ModCmdFunc>(&CLogSQLite::LogLimitCommand), "[0-9]+", "Limit the amount of items to store into the log.");
        AddCommand("LogLevel", static_cast<CModCommand::ModCmdFunc>(&CLogSQLite::LogLevelCommand), "[0-4]", "Log level.");
        
        AddCommand("AddIgnore", static_cast<CModCommand::ModCmdFunc>(&CLogSQLite::AddIgnoreCommand), "Type[nick|chan] Target", "Add to ignore list.");
        AddCommand("RemoveIgnore", static_cast<CModCommand::ModCmdFunc>(&CLogSQLite::RemoveIgnoreCommand), "Type[nick|chan] Target", "Remove from ignore list.");
        AddCommand("IgnoreList", static_cast<CModCommand::ModCmdFunc>(&CLogSQLite::IgnoreListCommand), "", "View what is currently ignored.");
        
        AddCommand("DoNotTrack", static_cast<CModCommand::ModCmdFunc>(&CLogSQLite::DoNotTrack), "", "Adds the current session to the do not track disconnect list.");
    }
    
    void ReplayCommand(const CString &sLine) {
        Replay();
        PutModule("Replayed");
    }
    
    void ReplayAllCommand(const CString &sLine) {
        CString sArgs = sLine.Token(1, true);
        bool help = sArgs.Equals("HELP");
        
        if (help) {
            PutModule("On: All logs stored will be replayed.");
            PutModule("Off: Only logs since the last time you connected will be replayed.");
        } else if (!sArgs.empty()) {
            replayAll = sArgs.ToBool();
            SetSetting("replayAll", (replayAll ? "1" : "0"));
        }
        
        CString status = (replayAll ? "On" : "Off");
        CString now = (sArgs.empty() || help ? "" : "now ");
        PutModule("ReplayAll is "+now+"set to: "+=status);
    }
    
    void LogLimitCommand(const CString &sLine) {
        CString sArgs = sLine.Token(1, true);
        bool help = sArgs.Equals("HELP");
        
        CString setting;
        if (help) {
            PutModule("0: Everything will be kept in database.");
            PutModule("1: Everything will be kept in database until replayed.");
            PutModule("2+: Limit logs stored in database to limit set.");
        } else if (!sArgs.empty()) {
            logLimit = strtoul(sArgs.c_str(), NULL, 10);
            char limitStr[20];
            snprintf(limitStr, sizeof(limitStr), "%lu", logLimit);
            setting = limitStr;
            SetSetting("logLimit", setting);
        }
        if (setting.empty()) {
            char limitStr[20];
            snprintf(limitStr, sizeof(limitStr), "%lu", logLimit);
            setting = limitStr;
        }
        CString now = (sArgs.empty() || help ? "" : "now ");
        PutModule("LogLimit is "+now+"set to: "+setting);
    }
    
    void LogLevelCommand(const CString &sLine) {
        CString sArgs = sLine.Token(1, true);
        bool help = sArgs.Equals("HELP");
        
        CString setting;
        if (help) {
            PutModule("0: Mentions and messages to you.");
            PutModule("1: All messages.");
            PutModule("2: Actions, Joins/Parts, and Notices.");
            PutModule("3: Server wide messages.");
            PutModule("4: All messages, actions, joins/parts, and noticies sent by you.");
        } else if (!sArgs.empty()) {
            logLevel = atoi(sArgs.c_str());
            char levelStr[20];
            snprintf(levelStr, sizeof(levelStr), "%d", logLevel);
            setting = levelStr;
            SetSetting("logLevel", setting);
        }
        if (setting.empty()) {
            char levelStr[20];
            snprintf(levelStr, sizeof(levelStr), "%d", logLevel);
            setting = levelStr;
        }
        CString now = (sArgs.empty() || help ? "" : "now ");
        PutModule("LogLevel is "+now+"set to: "+setting);
    }
    
    void AddIgnoreCommand(const CString &sLine) {
        CString type = sLine.Token(1);
        CString target = sLine.Token(2);
        bool help = sLine.Equals("HELP");
        
        if (help) {
            PutModule("Inorder to add an ignore, you must choose what type of ignore it is which can ether be a nick or a chan.");
            PutModule("Nicks are matched with wildcards against the full mask.");
            PutModule("Channels are matched by #channel which can contain wildcards.");
        } else if (!type.empty() && !target.empty()) {
            if (type.Equals("nick"))
                type = "nick";
            else if (type.Equals("chan") || type.Equals("channel"))
                type = "chan";
            else
                type = "";
            
            if (type.empty()) {
                PutModule("Unknown type. If you need help, type \"AddIgnore help\".");
                return;
            }
            
            if (AddIgnore(type, target))
                PutModule("Successfully added \""+target+"\" to the ignore list.");
            else
                PutModule("Failed, maybe it already existed?");
        } else {
            PutModule("If you need help, type \"AddIgnore help\".");
        }
    }
    
    void RemoveIgnoreCommand(const CString &sLine) {
        CString type = sLine.Token(1);
        CString target = sLine.Token(2);
        bool help = sLine.Equals("HELP");
        
        if (help) {
            PutModule("Inorder to remove an ignore, you must specify the type and the exact pattren used to add it. If you need to find what currently exists, type \"IgnoreList\".");
        } else if (!type.empty() && !target.empty()) {
            if (type.Equals("nick"))
                type = "nick";
            else if (type.Equals("chan") || type.Equals("channel"))
                type = "chan";
            else
                type = "";
            
            if (type.empty()) {
                PutModule("Unknown type. If you need help, type \"RemoveIgnore help\".");
                return;
            }
            
            if (RemoveIgnore(type, target))
                PutModule("Successfully removed \""+target+"\" from the ignore list.");
            else
                PutModule("Failed, maybe it does not exist?");
        } else {
            PutModule("If you need help, type \"RemoveIgnore help\".");
        }
    }
    
    void IgnoreListCommand(const CString &sLine) {
        if (nickIgnoreList.size()==0) {
            PutModule("The nick ignore list is currently empty.");
        } else {
            PutModule("Nick ignore list contains:");
            
            for (vector<CString>::iterator it=nickIgnoreList.begin(); it<nickIgnoreList.end(); it++) {
                PutModule(*it);
            }
        }
        PutModule("---");
        if (chanIgnoreList.size()==0) {
            PutModule("The channel ignore list is currently empty.");
        } else {
            PutModule("Channel ignore list contains:");
            
            for (vector<CString>::iterator it=chanIgnoreList.begin(); it<chanIgnoreList.end(); it++) {
                PutModule(*it);
            }
        }
    }
    
    void DoNotTrack(const CString &sLine) {
        bool tracking = true;
        for (vector<CClient *>::iterator it=doNotTrackClient.begin(); it<doNotTrackClient.end(); it++) {
            if (*it==m_pClient) {
                tracking = false;
                break;
            }
        }
        if (tracking)
            doNotTrackClient.push_back(m_pClient);
    }
    
    virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
        connected = true;
        
        CString savePath = GetSavePath();
        savePath += "/log.sqlite";
        
        sqlite3_open(savePath.c_str(), &database);
        
        sqlite3_stmt *result;
        int status = sqlite3_prepare(database, "SELECT `name` FROM `sqlite_master` WHERE `name`='settings'", -1, &result, NULL);
        if (status==SQLITE_OK) {
            status = SQLITE_BUSY;
            while (status==SQLITE_BUSY) {
                status = sqlite3_step(result);
            }
            sqlite3_finalize(result);
            if (status!=SQLITE_ROW) {
                status = sqlite3_prepare(database, "CREATE TABLE `settings` (`name` text, `value` text)", -1, &result, NULL);
                if (status==SQLITE_OK) {
                    sqlite3_step(result);
                    sqlite3_finalize(result);
                    
                    SetSetting("replayAll","0");
                    SetSetting("logLimit","1");
                    SetSetting("logLevel","1");
                    SetSetting("version","1");
                }
            }
        }
        status = sqlite3_prepare(database, "SELECT `name` FROM `sqlite_master` WHERE `name`='messages'", -1, &result, NULL);
        if (status==SQLITE_OK) {
            status = SQLITE_BUSY;
            while (status==SQLITE_BUSY) {
                status = sqlite3_step(result);
            }
            sqlite3_finalize(result);
            if (status!=SQLITE_ROW) {
                status = sqlite3_prepare(database, "CREATE TABLE `messages` (`target` text, `nick` text, `type` text, `message` text, `time` real(20,5))", -1, &result, NULL);
                if (status==SQLITE_OK) {
                    sqlite3_step(result);
                    sqlite3_finalize(result);
                }
            }
        }
        status = sqlite3_prepare(database, "SELECT `name` FROM `sqlite_master` WHERE `name`='ignorelist'", -1, &result, NULL);
        if (status==SQLITE_OK) {
            status = SQLITE_BUSY;
            while (status==SQLITE_BUSY) {
                status = sqlite3_step(result);
            }
            sqlite3_finalize(result);
            if (status!=SQLITE_ROW) {
                status = sqlite3_prepare(database, "CREATE TABLE `ignorelist` (`type` text, `target` text)", -1, &result, NULL);
                if (status==SQLITE_OK) {
                    sqlite3_step(result);
                    sqlite3_finalize(result);
                }
            }
        }
        replayAll = atoi(GetSetting("replayAll").c_str());
        logLimit = strtoul(GetSetting("logLimit").c_str(), NULL, 10);
        logLevel = atoi(GetSetting("logLevel").c_str());
        
        unsigned long version = strtoul(GetSetting("version").c_str(), NULL, 10);
        if (version==0)
            SetSetting("version","1");
        
        UpdateIgnoreLists();
        
        return true;
    }

    virtual ~CLogSQLite() {
        int result = sqlite3_close(database);
        if (result==SQLITE_BUSY) {
            sqlite3_stmt *stmt;
            while ((stmt = sqlite3_next_stmt(database, NULL))!=NULL) {
                sqlite3_finalize(stmt);
            }
            result = sqlite3_close(database);
            if (result!=SQLITE_OK)
                printf("Unable to close the SQLite Database\n");
        }
    }
    
    CString GetUNIXTime() {
        struct timeval time;
        gettimeofday(&time, NULL);
        double microtime = (double)(time.tv_sec + (time.tv_usec/1000000.00));
        char timeStr[25];
        snprintf(timeStr, sizeof(timeStr), "%f", microtime);
        return timeStr;
    }
    
    void AddMessage(const CString& target, const CString& nick, const CString& type, const CString& message) {
        if (IsIgnored("nick",nick) || (target.Left(1).Equals("#") && IsIgnored("chan",target)))
            return;
        sqlite3_stmt *result;
        int status = sqlite3_prepare(database, "INSERT INTO `messages` (`target`, `nick`, `type`, `message`, `time`) VALUES (?,?,?,?,?)", -1, &result, NULL);
        if (status!=SQLITE_OK)
            return;
        status = sqlite3_bind_text(result, 1, target.c_str(), target.length(), SQLITE_STATIC);
        if (status!=SQLITE_OK) {
            sqlite3_finalize(result);
            return;
        }
        status = sqlite3_bind_text(result, 2, nick.c_str(), nick.length(), SQLITE_STATIC);
        if (status!=SQLITE_OK) {
            sqlite3_finalize(result);
            return;
        }
        status = sqlite3_bind_text(result, 3, type.c_str(), type.length(), SQLITE_STATIC);
        if (status!=SQLITE_OK) {
            sqlite3_finalize(result);
            return;
        }
        status = sqlite3_bind_text(result, 4, message.c_str(), message.length(), SQLITE_STATIC);
        if (status!=SQLITE_OK) {
            sqlite3_finalize(result);
            return;
        }
        CString time = GetUNIXTime();
        status = sqlite3_bind_text(result, 5, time.c_str(), time.length(), SQLITE_STATIC);
        if (status!=SQLITE_OK) {
            sqlite3_finalize(result);
            return;
        }
        sqlite3_step(result);
        sqlite3_finalize(result);
        
        if (logLimit>=2) {
            status = sqlite3_prepare(database, "SELECT COUNT(*) FROM `messages`", -1, &result, NULL);
            if (status!=SQLITE_OK)
                return;
            status = SQLITE_BUSY;
            while (status==SQLITE_BUSY) {
                status = sqlite3_step(result);
            }
            if (status!=SQLITE_ROW)
                return;
            
            int dataCount = sqlite3_data_count(result);
            if (dataCount!=1) {
                sqlite3_finalize(result);
                cout << "LogSQLite: We are only suppose to receive 1 field. We received the count of " << dataCount << " fields.\n";
                return;
            }
            unsigned long count = strtoul((const char *)sqlite3_column_text(result, 0), NULL, 10);
            if (count<=logLimit)
                count = 0;
            else
                count = count-logLimit;
            sqlite3_finalize(result);
            
            if (count!=0) {
                char *queryStr = (char *)malloc(73);
                snprintf(queryStr, 73, "SELECT `rowid` FROM `messages` ORDER BY `time` LIMIT %lu", count);
                status = sqlite3_prepare(database, queryStr, -1, &result, NULL);
                free(queryStr);
                if (status!=SQLITE_OK)
                    return;
                
                while (true) {
                    status = SQLITE_BUSY;
                    while (status==SQLITE_BUSY) {
                        status = sqlite3_step(result);
                    }
                    if (status!=SQLITE_ROW)
                        break;
                    
                    dataCount = sqlite3_data_count(result);
                    if (dataCount!=1) {
                        sqlite3_finalize(result);
                        cout << "LogSQLite: We are only suppose to receive 1 field. We received the count of " << dataCount << " fields.\n";
                        break;
                    }
                    
                    sqlite3_stmt *result2;
                    const char *rowid = (const char *)sqlite3_column_text(result, 0);
                    
                    status = sqlite3_prepare(database, "DELETE FROM `messages` WHERE `rowid`=?", -1, &result2, NULL);
                    if (status!=SQLITE_OK)
                        continue;
                    status = sqlite3_bind_text(result2, 1, rowid, strlen(rowid), SQLITE_STATIC);
                    if (status!=SQLITE_OK) {
                        sqlite3_finalize(result2);
                        continue;
                    }
                    sqlite3_step(result2);
                    sqlite3_finalize(result2);
                }
                sqlite3_finalize(result);
            }
        }
    }
    
    void SetSetting(const CString& name, const CString& value) {
        bool exists = false;
        sqlite3_stmt *result;
        int status = sqlite3_prepare(database, "SELECT `value` FROM `settings` WHERE `name`=?", -1, &result, NULL);
        if (status==SQLITE_OK) {
            status = sqlite3_bind_text(result, 1, name.c_str(), name.length(), SQLITE_STATIC);
            if (status==SQLITE_OK) {
                status = SQLITE_BUSY;
                while (status==SQLITE_BUSY) {
                    status = sqlite3_step(result);
                }
                if (status==SQLITE_ROW) {
                    exists = true;
                }
            }
            sqlite3_finalize(result);
        }
        
        if (exists) {
            status = sqlite3_prepare(database, "UPDATE `settings` SET `value`=? WHERE `name`=?", -1, &result, NULL);
            if (status!=SQLITE_OK) {
                return;
            }
            status = sqlite3_bind_text(result, 1, value.c_str(), value.length(), SQLITE_STATIC);
            if (status!=SQLITE_OK) {
                sqlite3_finalize(result);
                return;
            }
            status = sqlite3_bind_text(result, 2, name.c_str(), name.length(), SQLITE_STATIC);
            if (status!=SQLITE_OK) {

                sqlite3_finalize(result);
                return;
            }
            sqlite3_step(result);
            sqlite3_finalize(result);
        } else {
            status = sqlite3_prepare(database, "INSERT INTO `settings` (`name`,`value`) VALUES (?,?)", -1, &result, NULL);
            if (status!=SQLITE_OK) {
                return;
            }
            status = sqlite3_bind_text(result, 1, name.c_str(), name.length(), SQLITE_STATIC);
            if (status!=SQLITE_OK) {
                sqlite3_finalize(result);
                return;
            }
            status = sqlite3_bind_text(result, 2, value.c_str(), value.length(), SQLITE_STATIC);
            if (status!=SQLITE_OK) {
                sqlite3_finalize(result);
                return;
            }
            sqlite3_step(result);
            sqlite3_finalize(result);
        }
    }
    CString GetSetting(const CString& name) {
        CString stringValue;
        sqlite3_stmt *result;
        int status = sqlite3_prepare(database, "SELECT `value` FROM `settings` WHERE `name`=?", -1, &result, NULL);
        if (status!=SQLITE_OK)
            return stringValue;
        status = sqlite3_bind_text(result, 1, name.c_str(), name.length(), SQLITE_STATIC);
        if (status!=SQLITE_OK) {
            sqlite3_finalize(result);
            return stringValue;
        }
        while (true) {
            status = SQLITE_BUSY;
            while (status==SQLITE_BUSY) {
                status = sqlite3_step(result);
            }
            if (status!=SQLITE_ROW)
                break;
            
            int dataCount = sqlite3_data_count(result);
            if (dataCount!=1) {
                sqlite3_finalize(result);
                cout << "LogSQLite: Settings are only suppose to return 1 as the field count. We received the count of " << dataCount << " fields.\n";
                return stringValue;
            }
            stringValue = CString((const char *)sqlite3_column_text(result, 0));
            break;
        }
        sqlite3_finalize(result);
        return stringValue;
    }
    
    
    void UpdateIgnoreLists() {
        nickIgnoreList.clear();
        sqlite3_stmt *result;
        int status = sqlite3_prepare(database, "SELECT `target` FROM `ignorelist` WHERE `type`='nick'", -1, &result, NULL);
        if (status==SQLITE_OK) {
            while (true) {
                status = SQLITE_BUSY;
                while (status==SQLITE_BUSY) {
                    status = sqlite3_step(result);
                }
                if (status!=SQLITE_ROW)
                    break;
                
                int dataCount = sqlite3_data_count(result);
                if (dataCount!=1)
                    break;
                
                CString ignore = CString((const char *)sqlite3_column_text(result, 0));
                nickIgnoreList.push_back(ignore);
            }
            sqlite3_finalize(result);
        }
        chanIgnoreList.clear();
        status = sqlite3_prepare(database, "SELECT `target` FROM `ignorelist` WHERE `type`='chan'", -1, &result, NULL);
        if (status==SQLITE_OK) {
            while (true) {
                status = SQLITE_BUSY;
                while (status==SQLITE_BUSY) {
                    status = sqlite3_step(result);
                }
                if (status!=SQLITE_ROW)
                    break;
                
                int dataCount = sqlite3_data_count(result);
                if (dataCount!=1)
                    break;
                
                CString ignore = CString((const char *)sqlite3_column_text(result, 0));
                chanIgnoreList.push_back(ignore);
            }
            sqlite3_finalize(result);
        }
    }
    bool AddIgnore(const CString& type, const CString& target) {
        if (!IgnoreExists(type, target)) {
            sqlite3_stmt *result;
            int status = sqlite3_prepare(database, "INSERT INTO `ignorelist` (`type`, `target`) VALUES (?,?)", -1, &result, NULL);
            if (status!=SQLITE_OK)
                return false;
            status = sqlite3_bind_text(result, 1, type.c_str(), type.length(), SQLITE_STATIC);
            if (status!=SQLITE_OK) {
                sqlite3_finalize(result);
                return false;
            }
            status = sqlite3_bind_text(result, 2, target.c_str(), target.length(), SQLITE_STATIC);
            if (status!=SQLITE_OK) {
                sqlite3_finalize(result);
                return false;
            }
            sqlite3_step(result);
            sqlite3_finalize(result);
            
            if (type.Equals("nick"))
                nickIgnoreList.push_back(target);
            else if (type.Equals("chan"))
                chanIgnoreList.push_back(target);
            return true;
        }
        return false;
    }
    bool RemoveIgnore(const CString& type, const CString& target) {
        if (IgnoreExists(type, target)) {
            sqlite3_stmt *result;
            int status = sqlite3_prepare(database, "DELETE FROM `ignorelist` WHERE `type`=? AND `target`=?", -1, &result, NULL);
            if (status!=SQLITE_OK)
                return false;
            status = sqlite3_bind_text(result, 1, type.c_str(), type.length(), SQLITE_STATIC);
            if (status!=SQLITE_OK) {
                sqlite3_finalize(result);
                return false;
            }
            status = sqlite3_bind_text(result, 2, target.c_str(), target.length(), SQLITE_STATIC);
            if (status!=SQLITE_OK) {
                sqlite3_finalize(result);
                return false;
            }
            sqlite3_step(result);
            sqlite3_finalize(result);
            
            UpdateIgnoreLists();
            return true;
        }
        return false;
    }
    bool IgnoreExists(const CString& type, const CString& target) {
        if (type.Equals("nick")) {
            for (vector<CString>::iterator it=nickIgnoreList.begin(); it<nickIgnoreList.end(); it++) {
                if (target.Equals(*it))
                    return true;
            }
        } else if (type.Equals("chan")) {
            for (vector<CString>::iterator it=chanIgnoreList.begin(); it<chanIgnoreList.end(); it++) {
                if (target.Equals(*it))
                    return true;
            }
        }
        return false;
    }
    bool IsIgnored(const CString& type, const CString& target) {
        if (type.Equals("nick")) {
            for (vector<CString>::iterator it=nickIgnoreList.begin(); it<nickIgnoreList.end(); it++) {
                if (target.WildCmp(*it))
                    return true;
            }
        } else if (type.Equals("chan")) {
            for (vector<CString>::iterator it=chanIgnoreList.begin(); it<chanIgnoreList.end(); it++) {
                if (target.WildCmp(*it))
                    return true;
            }
        }
        return false;
    }
    
    //Server stuff
    virtual void OnIRCDisconnected() {
        if (connected) {
            connected = false;
            if (logLevel>=3) {
                AddMessage("","","DISCONNECT","");
            }
        }
    }
    
    virtual void OnIRCConnected() {
        if (!connected) {
            connected = true;
            if (logLevel>=3) {
                AddMessage("","","CONNECT","");
            }
        }
    }
    
    //User stuff
    virtual EModRet OnUserAction(CString& sTarget, CString& sMessage) {
        if (logLevel>=4) {
            AddMessage(sTarget,m_pNetwork->GetIRCNick().GetNickMask(),"ACTION",sMessage);
        }
        return CONTINUE;
    }
    
    virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage) {
        if (logLevel>=4) {
            AddMessage(sTarget,m_pNetwork->GetIRCNick().GetNickMask(),"PRIVMSG",sMessage);
        }
        return CONTINUE;
    }
    
    virtual EModRet OnUserNotice(CString& sTarget, CString& sMessage) {
        if (logLevel>=4) {
            AddMessage(sTarget,m_pNetwork->GetIRCNick().GetNickMask(),"NOTICE",sMessage);
        }
        return CONTINUE;
    }
    
    virtual EModRet OnUserJoin(CString& sChannel, CString& sKey) {
        if (logLevel>=4) {
            AddMessage(sChannel,m_pNetwork->GetIRCNick().GetNickMask(),"JOIN","");
        }
        return CONTINUE;
    }
    
    virtual EModRet OnUserPart(CString& sChannel, CString& sMessage) {
        if (logLevel>=4) {
            AddMessage(sChannel,m_pNetwork->GetIRCNick().GetNickMask(),"PART",sMessage);
        }
        return CONTINUE;
    }
    
    virtual EModRet OnUserTopic(CString& sChannel, CString& sTopic) {
        if (logLevel>=4) {
            AddMessage(sChannel,m_pNetwork->GetIRCNick().GetNickMask(),"TOPIC",sTopic);
        }
        return CONTINUE;
    }
    
    
    //Other stuff
       virtual void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs) {
        if (logLevel>=3) {
            AddMessage(Channel.GetName(),OpNick.GetNickMask(),"MODE",sModes+" "+sArgs);
        }
    }
    
    virtual void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) {
        if (logLevel>=2) {
            AddMessage(Channel.GetName(),OpNick.GetNickMask(),"KICK",sKickedNick+" "+sMessage);
        }
    }
    
    virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {
        if (logLevel>=2) {
            vector<CChan*>::const_iterator it;
            for (it=vChans.begin(); it!=vChans.end(); it++) {
                CChan& channel = **it;
                AddMessage(channel.GetName(),Nick.GetNickMask(),"QUIT",sMessage);
            }
        }
    }
    
    virtual void OnJoin(const CNick& Nick, CChan& Channel) {
        if (logLevel>=2) {
            AddMessage(Channel.GetName(),Nick.GetNickMask(),"JOIN","");
        }
    }
    
    virtual void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) {
        if (logLevel>=2) {
            AddMessage(Channel.GetName(),Nick.GetNickMask(),"JOIN",sMessage);
        }
    }
    
    virtual void OnNick(const CNick& OldNick, const CString& sNewNick, const vector<CChan*>& vChans) {
        if (logLevel>=2) {
            vector<CChan*>::const_iterator it;
            for (it=vChans.begin(); it!=vChans.end(); it++) {
                CChan& channel = **it;
                AddMessage(channel.GetName(),OldNick.GetNickMask(),"NICK",sNewNick);
            }
        }
    }
    
    virtual EModRet OnPrivAction(CNick& Nick, CString& sMessage) {
        if (logLevel>=0) {
            AddMessage(m_pNetwork->GetCurNick(),Nick.GetNickMask(),"ACTION",sMessage);
        }
        return CONTINUE;
    }
    
    virtual EModRet OnChanAction(CNick& Nick, CChan& Channel, CString& sMessage) {
        if (logLevel==0) {
            if (strcasestr(sMessage.c_str(),m_pNetwork->GetCurNick().c_str()))
                AddMessage(Channel.GetName(),Nick.GetNickMask(),"ACTION",sMessage);
        } else if (logLevel>=1) {
            AddMessage(Channel.GetName(),Nick.GetNickMask(),"ACTION",sMessage);
        }
        return CONTINUE;
    }
    
    virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage) {
        if (logLevel>=0) {
            AddMessage(m_pNetwork->GetCurNick(),Nick.GetNickMask(),"PRIVMSG",sMessage);
        }
        return CONTINUE;
    }
    
    virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) {
        if (logLevel==0) {
            if (strcasestr(sMessage.c_str(),m_pNetwork->GetCurNick().c_str()))
                AddMessage(Channel.GetName(),Nick.GetNickMask(),"PRIVMSG",sMessage);
        } else if (logLevel>=1) {
            AddMessage(Channel.GetName(),Nick.GetNickMask(),"PRIVMSG",sMessage);
        }
        return CONTINUE;
    }
    
    virtual EModRet OnPrivNotice(CNick& Nick, CString& sMessage) {
        if (logLevel>=2) {
            AddMessage(m_pNetwork->GetCurNick(),Nick.GetNickMask(),"NOTICE",sMessage);
        }
        return CONTINUE;
    }
    
    virtual EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) {
        if (logLevel>=2) {
            AddMessage(Channel.GetName(),Nick.GetNickMask(),"NOTICE",sMessage);
        }
        return CONTINUE;
    }
    
    virtual EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) {
        if (logLevel>=2) {
            AddMessage(Channel.GetName(),Nick.GetNickMask(),"TOPIC",sTopic);
        }
        return CONTINUE;
    }
    
    virtual EModRet OnRaw(CString& sLine) {
        if (logLevel>=3) {
            CString sCmd = sLine.Token(1);
            if ((sCmd.length() == 3) && (isdigit(sCmd[0])) && (isdigit(sCmd[1])) && (isdigit(sCmd[2]))) {
                unsigned int uRaw = sCmd.ToUInt();
                if (uRaw!=10 && uRaw!=305 && uRaw!=306 && uRaw!=324 && uRaw!=329 && uRaw<331 && uRaw>333 && uRaw!=352 && uRaw!=353 && uRaw!=366 && uRaw!=432 && uRaw!=433 && uRaw!=437 && uRaw!=451 && uRaw!=670) {
                    AddMessage("",sLine.Token(0),sCmd,sLine.Token(2, true));
                }
            }
        }
        return CONTINUE;
    }
    
    //Client Connection
    virtual void OnClientLogin() {
        SetSetting("clientConnected",GetUNIXTime());
        Replay();
    }
    
    virtual void OnClientDisconnect() {
        bool track = true;
        for (vector<CClient *>::iterator it=doNotTrackClient.begin(); it<doNotTrackClient.end(); it++) {
            if (*it==m_pClient) {
                doNotTrackClient.erase(it);
                track = false;
                break;
            }
        }
        if (track)
            SetSetting("clientDisconnected",GetUNIXTime());
    }
    
    void Replay() {
        PutUser(":*LogSQLite!LogSQLite@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :Buffer Playback...");
        
        CString lastOnline = GetSetting("clientDisconnected");
        
        sqlite3_stmt *result;
        int status = SQLITE_OK;
        if (!replayAll && !lastOnline.empty()) {
            status = sqlite3_prepare(database, "SELECT * FROM `messages` WHERE `time`>? ORDER BY `time`", -1, &result, NULL);
            if (status!=SQLITE_OK) {
                PutUser(":*LogSQLite!LogSQLite@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :Playback failed due to sql problem.");
                return;
            }
            status = sqlite3_bind_text(result, 1, lastOnline.c_str(), lastOnline.length(), SQLITE_STATIC);
            if (status!=SQLITE_OK) {
                PutUser(":*LogSQLite!LogSQLite@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :Playback failed due to sql problem.");
                sqlite3_finalize(result);
                return;
            }
        } else {
            status = sqlite3_prepare(database, "SELECT * FROM `messages` ORDER BY `time`", -1, &result, NULL);
            if (status!=SQLITE_OK) {
                PutUser(":*LogSQLite!LogSQLite@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :Playback failed due to sql problem.");
                return;
            }
        }
        
        int columnCount = sqlite3_column_count(result);
        map<int,CString> columns;
        for (int i=0; i<columnCount; i++) {
            columns[i] = CString(sqlite3_column_name(result, i));
        }
        
        time_t now = time(NULL);
        while (true) {
            status = SQLITE_BUSY;
            while (status==SQLITE_BUSY) {
                status = sqlite3_step(result);
            }
            if (status!=SQLITE_ROW)
                break;
            
            map<CString,CString> data;
            int dataCount = sqlite3_data_count(result);
            for (int i=0; i<dataCount; i++) {
                data[columns[i]] = CString((const char *)sqlite3_column_text(result, i));
            }
            
            time_t unixTime = (time_t)strtol(data["time"].c_str(),NULL,10);
            
            CString timeString;
            CString prefixString;
            if (m_pClient->HasServerTime()) {
                char timeStr[20];
                snprintf(timeStr, sizeof(timeStr), "%lu", unixTime);
                prefixString = "@t=";
                prefixString += timeStr;
                prefixString += " ";
            } else {
                struct tm *timeinfo = localtime(&unixTime);
                char timeStr[20];
                if (((long)now-86000)<(long)time)
                    strftime(timeStr, sizeof(timeStr), "%I:%M:%S %p", timeinfo);
                else
                    strftime(timeStr, sizeof(timeStr), "%m/%d/%y %I:%M:%S %p", timeinfo);
                timeString = "[";
                timeString += timeStr;
                timeString += "] ";
            }
            
            if (data["type"].Equals("DISCONNECT")) {
                PutUser(prefixString+":*LogSQLite!LogSQLite@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :"+timeString+"Server Disconnected");
            } else if (data["type"].Equals("CONNECT")) {
                PutUser(prefixString+":*LogSQLite!LogSQLite@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :"+timeString+"Server Connected");
            } else if (data["type"].Equals("JOIN")) {
                PutUser(prefixString+":"+data["nick"]+" NOTICE "+data["target"]+" :"+timeString+"joined");
            } else if (data["type"].Equals("PART")) {
                if (data["message"].Equals(""))
                    PutUser(prefixString+":"+data["nick"]+" NOTICE "+data["target"]+" :"+timeString+"parted");
                else
                    PutUser(prefixString+":"+data["nick"]+" NOTICE "+data["target"]+" :"+timeString+"parted: "+data["message"]);
            } else if (data["type"].Equals("TOPIC")) {
                PutUser(prefixString+":"+data["nick"]+" NOTICE "+data["target"]+" :"+timeString+"changed topic: "+data["message"]);
            } else if (data["type"].Equals("QUIT")) {
                if (data["message"].Equals(""))
                    PutUser(prefixString+":"+data["nick"]+" NOTICE "+data["target"]+" :"+timeString+"quit");
                else
                    PutUser(prefixString+":"+data["nick"]+" NOTICE "+data["target"]+" :"+timeString+"quit: "+data["message"]);
            } else if (data["type"].Equals("MODE")) {
                PutUser(prefixString+":"+data["nick"]+" NOTICE "+data["target"]+" :"+timeString+"changed mode: "+data["message"]);
            } else if (data["type"].Equals("ACTION")) {
                PutUser(prefixString+":"+data["nick"]+" PRIVMSG "+data["target"]+" :\001ACTION "+timeString+data["message"]+"\001");
            } else {
                PutUser(prefixString+":"+data["nick"]+" "+data["type"]+" "+data["target"]+" :"+timeString+data["message"]);
            }
        }
        
        sqlite3_finalize(result);
        
        if (logLimit==1) {
            status = sqlite3_prepare(database, "DELETE FROM `messages`", -1, &result, NULL);
            if (status==SQLITE_OK) {
                sqlite3_step(result);
                sqlite3_finalize(result);
            }
        }
        
        PutUser(":*LogSQLite!LogSQLite@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :Playback Complete.");
    }
    
private:
    sqlite3 *database;
    bool connected;
    bool replayAll;
    unsigned long logLimit;
    int logLevel;
    
    vector<CString> nickIgnoreList;
    vector<CString> chanIgnoreList;
    
    vector<CClient *> doNotTrackClient;
};

template<> void TModInfo<CLogSQLite>(CModInfo& Info) {
    Info.SetWikiPage("logsqlite");
}

NETWORKMODULEDEFS(CLogSQLite, "Add logging to SQLite")