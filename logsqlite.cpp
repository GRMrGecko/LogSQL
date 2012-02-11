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
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <time.h>

#include <sqlite3.h>

class CLogSQLite : public CModule {
public:
    MODCONSTRUCTOR(CLogSQLite) {
        AddHelpCommand();
        AddCommand("Replay", static_cast<CModCommand::ModCmdFunc>(&CLogSQLite::ReplayCommand),
            "", "Play back the messages received.");
        AddCommand("ReplayAll", static_cast<CModCommand::ModCmdFunc>(&CLogSQLite::ReplayAllCommand),
            "[1|0]", "Set LogSQLite to replay all messages stored (default is off).");
        AddCommand("LogLimit",  static_cast<CModCommand::ModCmdFunc>(&CLogSQLite::LogLimitCommand),
            "[1|0]", "Set LogSQLite to limit the amount of items to store into the log (0 means to keep everything, 1 means clear after replay, anything else would limit to the count, default is 1).");
        AddCommand("LogLevel",  static_cast<CModCommand::ModCmdFunc>(&CLogSQLite::LogLevelCommand),
            "[0-4]", "Set LogSQLite log level (0 messages mentioning you and/or to you only. 1 include all messages sent in an channel. 2 include all actions, join/part, and notices sent in channel and to you. 3 include all server wide messages. 4 include messages sent by you. Default is 1).");
    }

    void ReplayCommand(const CString &sLine) {
        Replay();
        PutModule("Replayed");
    }

    void ReplayAllCommand(const CString &sLine) {
        CString sArgs = sLine.Token(1, true);

        if (!sArgs.empty()) {
            replayAll = sArgs.ToBool();
            SetSetting("replayAll", replayAll ? "1" : "0");
        }

        CString status = (replayAll ? "On" : "Off");
        CString sNow = (sArgs.empty() ? "" : "now ");
        PutModule("ReplayAll is " + sNow + "set to: "+ status);
    }

    void LogLimitCommand(const CString &sLine) {
        CString sArgs = sLine.Token(1, true);

        if (sArgs.empty()) {
            CString result;
            char limitStr[20];
            snprintf(limitStr, sizeof(limitStr), "%lu", logLimit);
            result = limitStr;
            PutModule("LogLimit is set to: "+result);
        } else {
            logLimit = strtoul(sArgs.c_str(), NULL, 10);
            CString result;
            char limitStr[20];
            snprintf(limitStr, sizeof(limitStr), "%lu", logLimit);
            result = limitStr;
            SetSetting("logLimit", result);
            PutModule("LogLimit is now set to: "+result);
        }            
    }

    void LogLevelCommand(const CString &sLine) {
        CString sArgs = sLine.Token(1, true);

        if (sArgs.empty()) {
            CString result;
            char limitStr[20];
            snprintf(limitStr, sizeof(limitStr), "%lu", logLimit);
            result = limitStr;
            PutModule("LogLimit is set to: "+result);
        } else {
            logLimit = strtoul(sArgs.c_str(), NULL, 10);
            CString result;
            char limitStr[20];
            snprintf(limitStr, sizeof(limitStr), "%lu", logLimit);
            result = limitStr;
            SetSetting("logLimit", result);
            PutModule("LogLimit is now set to: "+result);
        }
    }
    
    virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
        connected = true;
        
        CString savePath = GetSavePath();
        savePath += "/log.sqlite";
        
        bool found = false;
        FILE *fp = fopen(savePath.c_str(), "rb");
        if (fp!=NULL) {
            found = true;
            fclose(fp);
        }
        
        sqlite3_open(savePath.c_str(), &database);
        
        if (!found) {
            sqlite3_stmt *result;
            int status = sqlite3_prepare(database, "CREATE TABLE `settings` (`name` text, `value` text)", -1, &result, NULL);
            if (status==SQLITE_OK) {
                sqlite3_step(result);
                sqlite3_finalize(result);
            }
            status = sqlite3_prepare(database, "CREATE TABLE `messages` (`target` text, `nick` text, `type` text, `message` text, `time` real(20,5))", -1, &result, NULL);
            if (status==SQLITE_OK) {
                sqlite3_step(result);
                sqlite3_finalize(result);
            }
            SetSetting("replayAll","0");
            SetSetting("logLimit","1");
            SetSetting("logLevel","1");
        }
        replayAll = atoi(GetSetting("replayAll").c_str());
        logLimit = strtoul(GetSetting("logLimit").c_str(), NULL, 10);
        logLevel = atoi(GetSetting("logLevel").c_str());
        
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
        if (!m_pNetwork->IsUserAttached()) {
            SetSetting("clientDisconnected",GetUNIXTime());
        }
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
            
            time_t now = time(NULL);
            time_t unixTime = (time_t)strtol(data["time"].c_str(),NULL,10);
            struct tm *timeinfo = localtime(&unixTime);
            
            char timeStr[20];
            if (((long)now-86000)<(long)time)
                strftime(timeStr, sizeof(timeStr), "%I:%M:%S %p", timeinfo);
            else
                strftime(timeStr, sizeof(timeStr), "%m/%d/%y %I:%M:%S %p", timeinfo);
            
            if (data["type"].Equals("DISCONNECT")) {
                PutUser(":*LogSQLite!LogSQLite@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :["+timeStr+"] Server Disconnected");
            } else if (data["type"].Equals("CONNECT")) {
                PutUser(":*LogSQLite!LogSQLite@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :["+timeStr+"] Server Connected");
            } else if (data["type"].Equals("JOIN")) {
                PutUser(":"+data["nick"]+" NOTICE "+data["target"]+" :["+timeStr+"] joined");
            } else if (data["type"].Equals("PART")) {
                if (data["message"].Equals(""))
                    PutUser(":"+data["nick"]+" NOTICE "+data["target"]+" :["+timeStr+"] parted");
                else
                    PutUser(":"+data["nick"]+" NOTICE "+data["target"]+" :["+timeStr+"] parted: "+data["message"]);
            } else if (data["type"].Equals("TOPIC")) {
                PutUser(":"+data["nick"]+" NOTICE "+data["target"]+" :["+timeStr+"] changed topic: "+data["message"]);
            } else if (data["type"].Equals("QUIT")) {
                if (data["message"].Equals(""))
                    PutUser(":"+data["nick"]+" NOTICE "+data["target"]+" :["+timeStr+"] quit");
                else
                    PutUser(":"+data["nick"]+" NOTICE "+data["target"]+" :["+timeStr+"] quit: "+data["message"]);
            } else if (data["type"].Equals("MODE")) {
                PutUser(":"+data["nick"]+" NOTICE "+data["target"]+" :["+timeStr+"] changed mode: "+data["message"]);
            } else if (data["type"].Equals("ACTION")) {
                PutUser(":"+data["nick"]+" PRIVMSG "+data["target"]+" :\001ACTION ["+timeStr+"] "+data["message"]+"\001");
            } else {
                PutUser(":"+data["nick"]+" "+data["type"]+" "+data["target"]+" :["+timeStr+"] "+data["message"]);
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
};

template<> void TModInfo<CLogSQLite>(CModInfo& Info) {
    Info.SetWikiPage("logsqlite");
}

NETWORKMODULEDEFS(CLogSQLite, "Add logging to SQLite")
