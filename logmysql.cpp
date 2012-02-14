//
//  logmysql.cpp
//  LogSQL
//
//  Created by Mr. Gecko on 2/10/12.
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

#import <mysql.h>

class CLogMySQL : public CModule {
public:
    MODCONSTRUCTOR(CLogMySQL) {
        AddHelpCommand();
        AddCommand("Host", static_cast<CModCommand::ModCmdFunc>(&CLogMySQL::HostCommand), "[a-z0-9.]+", "MySQL host.");
        AddCommand("Port", static_cast<CModCommand::ModCmdFunc>(&CLogMySQL::PortCommand), "[0-9]+", "MySQL port.");
        AddCommand("Username", static_cast<CModCommand::ModCmdFunc>(&CLogMySQL::UsernameCommand), "[A-Za-z0-9]+", "MySQL username.");
        AddCommand("Password", static_cast<CModCommand::ModCmdFunc>(&CLogMySQL::PasswordCommand), "[A-Za-z0-9]+", "MySQL password.");
        AddCommand("DatabaseName", static_cast<CModCommand::ModCmdFunc>(&CLogMySQL::DatabaseNameCommand), "[a-z0-9]+", "MySQL database name.");
        AddCommand("Connect", static_cast<CModCommand::ModCmdFunc>(&CLogMySQL::ConnectCommand), "", "Reconnect to the MySQL database.");
        
        AddCommand("Replay", static_cast<CModCommand::ModCmdFunc>(&CLogMySQL::ReplayCommand), "", "Play back the messages received.");
        AddCommand("ReplayAll", static_cast<CModCommand::ModCmdFunc>(&CLogMySQL::ReplayAllCommand), "[1|0]", "Replay all messages stored.");
        AddCommand("LogLimit", static_cast<CModCommand::ModCmdFunc>(&CLogMySQL::LogLimitCommand), "[0-9]+", "Limit the amount of items to store into the log.");
        AddCommand("LogLevel", static_cast<CModCommand::ModCmdFunc>(&CLogMySQL::LogLevelCommand), "[0-4]", "Log level.");
        AddCommand("AddIgnore", static_cast<CModCommand::ModCmdFunc>(&CLogMySQL::AddIgnoreCommand), "Type[nick|chan] Target", "Add to ignore list.");
        AddCommand("RemoveIgnore", static_cast<CModCommand::ModCmdFunc>(&CLogMySQL::RemoveIgnoreCommand), "Type[nick|chan] Target", "Remove from ignore list.");
        AddCommand("IgnoreList", static_cast<CModCommand::ModCmdFunc>(&CLogMySQL::IgnoreListCommand), "", "View what is currently ignored.");
    }
    
    void HostCommand(const CString &sLine) {
        CString sArgs = sLine.Token(1, true);
        bool help = sArgs.Equals("HELP");
        
        if (help) {
            PutModule("The MySQL host. Can be an IP Address or Hostname. 127.0.0.1 or localhost is for the current server.");
        } else if (!sArgs.empty()) {
            SetNV("host",sArgs);
        }
        CString now = (sArgs.empty() || help ? "" : "now ");
        PutModule("Host is "+now+"set to: "+GetNV("host"));
    }
    
    void PortCommand(const CString &sLine) {
        CString sArgs = sLine.Token(1, true);
        bool help = sArgs.Equals("HELP");
        
        if (help) {
            PutModule("The MySQL port. Usually will be 3306.");
        } else if (!sArgs.empty()) {
            CString result;
            unsigned int port = strtoul(sArgs.c_str(), NULL, 10);
            char portStr[20];
            snprintf(portStr, sizeof(portStr), "%u", port);
            result = portStr;
            SetNV("port",result);
        }
        CString now = (sArgs.empty() || help ? "" : "now ");
        PutModule("Port is "+now+"set to: "+GetNV("port"));
    }
    
    void UsernameCommand(const CString &sLine) {
        CString sArgs = sLine.Token(1, true);
        bool help = sArgs.Equals("HELP");
        
        if (help) {
            PutModule("Your MySQL user username.");
        } else if (!sArgs.empty()) {
            SetNV("username",sArgs);
        }
        CString now = (sArgs.empty() || help ? "" : "now ");
        PutModule("Username is "+now+"set to: "+GetNV("username"));
    }
    
    void PasswordCommand(const CString &sLine) {
        CString sArgs = sLine.Token(1, true);
        bool help = sArgs.Equals("HELP");
        
        if (help) {
            PutModule("Your MySQL user password.");
        } else if (!sArgs.empty()) {
            SetNV("password",sArgs);
        }
        CString status = (GetNV("password").empty() ? "blank" : "set");
        CString now = (sArgs.empty() || help ? "" : "now ");
        PutModule("Password is "+now+status);
    }
    
    void DatabaseNameCommand(const CString &sLine) {
        CString sArgs = sLine.Token(1, true);
        bool help = sArgs.Equals("HELP");
        
        if (help) {
            PutModule("The MySQL database name which contains the messages and settings tables.");
        } else if (!sArgs.empty()) {
            SetNV("databaseName",sArgs);
        }
        CString now = (sArgs.empty() || help ? "" : "now ");
        PutModule("Database Name is "+now+"set to: "+GetNV("databaseName"));
    }
    
    void ConnectCommand(const CString &sLine) {
        MySQLConnect();
        if (databaseConnected)
            PutModule("Database is now connected");
        else
            PutModule("Unable to connect to database. Check configuration.");
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
        PutModule("ReplayAll is "+now+"set to: "+status);
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
    
    virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
        connected = true;
        databaseConnected = false;
        
        MySQLConnect();
        return true;
    }
    
    virtual ~CLogMySQL() {
        if (databaseConnected) {
            databaseConnected = false;
            mysql_close(database);
            database = NULL;
        }
    }
    
    void MySQLConnect() {
        CString host = GetNV("host");
        CString port = GetNV("port");
        if (port.empty()) {
            port = "3306";
            SetNV("port",port);
        }
        CString username = GetNV("username");
        CString password = GetNV("password");
        CString databaseName = GetNV("databaseName");
        if (!host.empty() && !username.empty() && !databaseName.empty()) {
            if (databaseConnected) {
                databaseConnected = false;
                mysql_close(database);
                database = NULL;
            }
            
            database = mysql_init(NULL);
            if (database!=NULL) {
                void *theRet = mysql_real_connect(database, host.c_str(), username.c_str(), password.c_str(), databaseName.c_str(), (unsigned int)strtoul(port.c_str(), NULL, 10), MYSQL_UNIX_ADDR, CLIENT_COMPRESS);
                databaseConnected = (theRet==database);
                if (databaseConnected) {
                    cout << "LogMySQL: Database connected.\n";
                    
                    MYSQL_RES *settings = mysql_list_tables(database, "settings");
                    if (mysql_num_rows(settings)==0) {
                        MYSQL_STMT *statement = mysql_stmt_init(database);
                        int status = mysql_stmt_prepare(statement, "CREATE TABLE `settings` (`name` text,`value` text)", 50);
                        if (status==0) {
                            mysql_stmt_execute(statement);
                            mysql_stmt_close(statement);
                            
                            SetSetting("replayAll","0");
                            SetSetting("logLimit","1");
                            SetSetting("logLevel","1");
                            SetSetting("version","1");
                        }
                    }
                    if (settings!=NULL)
                        mysql_free_result(settings);
                    MYSQL_RES *messages = mysql_list_tables(database, "messages");
                    if (mysql_num_rows(messages)==0) {
                        MYSQL_STMT *statement = mysql_stmt_init(database);
                        int status = mysql_stmt_prepare(statement, "CREATE TABLE `messages` (`rowid` int UNSIGNED AUTO_INCREMENT,`target` text,`nick` text,`type` text,`message` longblob,`time` decimal(20,5) UNSIGNED,PRIMARY KEY (`rowid`))", 170);
                        if (status==0) {
                            mysql_stmt_execute(statement);
                            mysql_stmt_close(statement);
                        }
                    }
                    if (messages!=NULL)
                        mysql_free_result(messages);
                    MYSQL_RES *ignorelist = mysql_list_tables(database, "ignorelist");
                    if (mysql_num_rows(ignorelist)==0) {
                        MYSQL_STMT *statement = mysql_stmt_init(database);
                        int status = mysql_stmt_prepare(statement, "CREATE TABLE `ignorelist` (`rowid` int UNSIGNED AUTO_INCREMENT,`type` text,`target` text,PRIMARY KEY (`rowid`))", 111);
                        if (status==0) {
                            mysql_stmt_execute(statement);
                            mysql_stmt_close(statement);
                        }
                    }
                    if (ignorelist!=NULL)
                        mysql_free_result(ignorelist);
                    
                    replayAll = atoi(GetSetting("replayAll").c_str());
                    logLimit = strtoul(GetSetting("logLimit").c_str(), NULL, 10);
                    logLevel = atoi(GetSetting("logLevel").c_str());
                    
                    unsigned long version = strtoul(GetSetting("version").c_str(), NULL, 10);
                    if (version==0)
                        SetSetting("version","1");
                } else {
                    cout << "LogMySQL: Database unable to connect.\n";
                }
            }
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
        if (!databaseConnected)
            return;
        if (IsIgnored("nick",nick) || (target.Left(1).Equals("#") && IsIgnored("chan",target)))
            return;
        MYSQL_STMT *statement = mysql_stmt_init(database);
        int status = mysql_stmt_prepare(statement, "INSERT INTO `messages` (`target`, `nick`, `type`, `message`, `time`) VALUES (?,?,?,?,?)", 87);
        if (status!=0)
            return;
        
        MYSQL_BIND bind[5];
        memset(bind, 0, sizeof(bind));
        
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (void*)target.c_str();
        bind[0].buffer_length = target.length();
        bind[0].is_null = false;
        
        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = (void*)nick.c_str();
        bind[1].buffer_length = nick.length();
        bind[1].is_null = false;
        
        bind[2].buffer_type = MYSQL_TYPE_STRING;
        bind[2].buffer = (void*)type.c_str();
        bind[2].buffer_length = type.length();
        bind[2].is_null = false;
        
        bind[3].buffer_type = MYSQL_TYPE_STRING;
        bind[3].buffer = (void*)message.c_str();
        bind[3].buffer_length = message.length();
        bind[3].is_null = false;
        
        CString time = GetUNIXTime();
        bind[4].buffer_type = MYSQL_TYPE_STRING;
        bind[4].buffer = (void*)time.c_str();
        bind[4].buffer_length = time.length();
        bind[4].is_null = false;
        
        status = mysql_stmt_bind_param(statement, bind);
        if (status!=0) {
            mysql_stmt_close(statement);
            return;
        }
        
        mysql_stmt_execute(statement);
        mysql_stmt_close(statement);
        
        if (logLimit>=2) {
            statement = mysql_stmt_init(database);
            status = mysql_stmt_prepare(statement, "SELECT COUNT(*) FROM `messages`", 31);
            if (status!=0)
                return;
            
            status = mysql_stmt_execute(statement);
            if (status!=0) {
                mysql_stmt_close(statement);
                return;
            }
            
            MYSQL_RES *result = mysql_stmt_result_metadata(statement);
            unsigned int dataCount = mysql_num_fields(result);
            if (dataCount!=1) {
                mysql_free_result(result);
                mysql_stmt_close(statement);
                cout << "LogMySQL: We are only suppose to receive 1 field. We received the count of " << dataCount << " fields.\n";
                return;
            }
            MYSQL_FIELD *fields = mysql_fetch_fields(result);
            
            unsigned long length;
            char countData[fields[0].length];
            MYSQL_BIND results[1];
            memset(results, 0, sizeof(results));
            results[0].buffer_type = MYSQL_TYPE_STRING;
            results[0].buffer = (void *)countData;
            results[0].buffer_length = fields[0].length;
            results[0].length = &length;
            
            status = mysql_stmt_bind_result(statement, results);
            if (status!=0) {
                mysql_free_result(result);
                mysql_stmt_close(statement);
                return;
            }
            
            status = mysql_stmt_fetch(statement);
            if (status!=0) {
                mysql_free_result(result);
                mysql_stmt_close(statement);
                return;
            }
            
            CString countString = CString(countData, length);
            
            unsigned long count = strtoul(countString.c_str(), NULL, 10);
            if (count<=logLimit)
                count = 0;
            else
                count = count-logLimit;
            mysql_free_result(result);
            mysql_stmt_close(statement);
            
            if (count!=0) {
                statement = mysql_stmt_init(database);
                char *queryStr = (char *)malloc(73);
                snprintf(queryStr, 73, "SELECT `rowid` FROM `messages` ORDER BY `time` LIMIT %lu", count);
                status = mysql_stmt_prepare(statement, queryStr, strlen(queryStr));
                free(queryStr);
                if (status!=0)
                    return;
                
                status = mysql_stmt_execute(statement);
                if (status!=0) {
                    mysql_stmt_close(statement);
                    return;
                }
                
                result = mysql_stmt_result_metadata(statement);
                dataCount = mysql_num_fields(result);
                if (dataCount!=1) {
                    mysql_free_result(result);
                    mysql_stmt_close(statement);
                    cout << "LogMySQL: We are only suppose to receive 1 field. We received the count of " << dataCount << " fields.\n";
                    return;
                }
                fields = mysql_fetch_fields(result);
                
                char rowidData[fields[0].length];
                memset(results, 0, sizeof(results));
                results[0].buffer_type = MYSQL_TYPE_STRING;
                results[0].buffer = (void *)rowidData;
                results[0].buffer_length = fields[0].length;
                results[0].length = &length;
                
                status = mysql_stmt_bind_result(statement, results);
                if (status!=0) {
                    mysql_free_result(result);
                    mysql_stmt_close(statement);
                    return;
                }
                
                while (true) {
                    status = mysql_stmt_fetch(statement);
                    if (status!=0)
                        break;
                    
                    CString rowid = CString(rowidData, length);
                    
                    MYSQL_STMT *statement2 = mysql_stmt_init(database);
                    status = mysql_stmt_prepare(statement2, "DELETE FROM `messages` WHERE `rowid`=?", 38);
                    if (status!=0)
                        continue;
                    
                    MYSQL_BIND bind2[1];
                    memset(bind, 0, sizeof(bind));
                    
                    bind2[0].buffer_type = MYSQL_TYPE_STRING;
                    bind2[0].buffer = (void*)rowid.c_str();
                    bind2[0].buffer_length = rowid.length();
                    bind2[0].is_null = false;
                    
                    status = mysql_stmt_bind_param(statement2, bind2);
                    if (status!=0) {
                        mysql_stmt_close(statement2);
                        continue;
                    }
                    mysql_stmt_execute(statement2);
                    mysql_stmt_close(statement2);
                }
                mysql_free_result(result);
                mysql_stmt_close(statement);
            }
        }
    }
    
    void SetSetting(const CString& name, const CString& value) {
        if (!databaseConnected)
            return;
        bool exists = false;
        MYSQL_STMT *statement = mysql_stmt_init(database);
        int status = mysql_stmt_prepare(statement, "SELECT `value` FROM `settings` WHERE `name`=?", 45);
        if (status==0) {
            MYSQL_BIND bind[1];
            memset(bind, 0, sizeof(bind));
            
            bind[0].buffer_type = MYSQL_TYPE_STRING;
            bind[0].buffer = (void*)name.c_str();
            bind[0].buffer_length = name.length();
            bind[0].is_null = false;
            
            status = mysql_stmt_bind_param(statement, bind);
            if (status==0) {
                mysql_stmt_execute(statement);
                status = mysql_stmt_fetch(statement);
                if (status==0) {
                    exists = true;
                }
            }
            mysql_stmt_close(statement);
        }
        
        if (exists) {
            statement = mysql_stmt_init(database);
            status = mysql_stmt_prepare(statement, "UPDATE `settings` SET `value`=? WHERE `name`=?", 46);
            if (status!=0)
                return;
            
            MYSQL_BIND bind[2];
            memset(bind, 0, sizeof(bind));
            
            bind[0].buffer_type = MYSQL_TYPE_STRING;
            bind[0].buffer = (void*)value.c_str();
            bind[0].buffer_length = value.length();
            bind[0].is_null = false;
            
            bind[1].buffer_type = MYSQL_TYPE_STRING;
            bind[1].buffer = (void*)name.c_str();
            bind[1].buffer_length = name.length();
            bind[1].is_null = false;
            
            status = mysql_stmt_bind_param(statement, bind);
            if (status!=0) {
                mysql_stmt_close(statement);
                return;
            }
            mysql_stmt_execute(statement);
            mysql_stmt_close(statement);
        } else {
            statement = mysql_stmt_init(database);
            status = mysql_stmt_prepare(statement, "INSERT INTO `settings` (`name`,`value`) VALUES (?,?)", 52);
            if (status!=0)
                return;
            
            MYSQL_BIND bind[2];
            memset(bind, 0, sizeof(bind));
            
            bind[0].buffer_type = MYSQL_TYPE_STRING;
            bind[0].buffer = (void*)name.c_str();
            bind[0].buffer_length = name.length();
            bind[0].is_null = false;
            
            bind[1].buffer_type = MYSQL_TYPE_STRING;
            bind[1].buffer = (void*)value.c_str();
            bind[1].buffer_length = value.length();
            bind[1].is_null = false;
            
            status = mysql_stmt_bind_param(statement, bind);
            if (status!=0) {
                mysql_stmt_close(statement);
                return;
            }
            mysql_stmt_execute(statement);
            mysql_stmt_close(statement);
        }
    }
    CString GetSetting(const CString& name) {
        CString stringValue;
        if (!databaseConnected)
            return stringValue;
        MYSQL_STMT *statement = mysql_stmt_init(database);
        int status = mysql_stmt_prepare(statement, "SELECT `value` FROM `settings` WHERE `name`=?", 45);
        if (status!=0)
            return stringValue;
        MYSQL_BIND bind[1];
        memset(bind, 0, sizeof(bind));
        
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (void*)name.c_str();
        bind[0].buffer_length = name.length();
        bind[0].is_null = false;
        
        status = mysql_stmt_bind_param(statement, bind);
        if (status!=0) {
            mysql_stmt_close(statement);
            return stringValue;
        }
        status = mysql_stmt_execute(statement);
        if (status!=0) {
            mysql_stmt_close(statement);
            return stringValue;
        }
        
        MYSQL_RES *result = mysql_stmt_result_metadata(statement);
        unsigned int dataCount = mysql_num_fields(result);
        if (dataCount!=1) {
            mysql_free_result(result);
            mysql_stmt_close(statement);
            cout << "LogMySQL: Settings are only suppose to return 1 as the field count. We received the count of " << dataCount << " fields.\n";
            return stringValue;
        }
        MYSQL_FIELD *fields = mysql_fetch_fields(result);
        
        unsigned long length;
        char valueData[fields[0].length];
        MYSQL_BIND results[1];
        memset(results, 0, sizeof(results));
        results[0].buffer_type = MYSQL_TYPE_STRING;
        results[0].buffer = (void *)valueData;
        results[0].buffer_length = fields[0].length;
        results[0].length = &length;
        
        status = mysql_stmt_bind_result(statement, results);
        if (status!=0) {
            mysql_free_result(result);
            mysql_stmt_close(statement);
            return stringValue;
        }
        
        while (true) {
            status = mysql_stmt_fetch(statement);
            if (status!=0)
                break;
            
            stringValue = CString(valueData, length);
            break;
        }
        mysql_free_result(result);
        mysql_stmt_close(statement);
        return stringValue;
    }
    
    void UpdateIgnoreLists() {
        if (!databaseConnected)
            return;
        nickIgnoreList.clear();
        MYSQL_STMT *statement = mysql_stmt_init(database);
        int status = mysql_stmt_prepare(statement, "SELECT `target` FROM `ignorelist` WHERE `type`='nick'", 53);
        if (status==0) {
            status = mysql_stmt_execute(statement);
            if (status==0) {
                MYSQL_RES *result = mysql_stmt_result_metadata(statement);
                unsigned int dataCount = mysql_num_fields(result);
                if (dataCount==1) {
                    MYSQL_FIELD *fields = mysql_fetch_fields(result);
                    
                    unsigned long length;
                    char targetData[fields[0].length];
                    MYSQL_BIND results[1];
                    memset(results, 0, sizeof(results));
                    results[0].buffer_type = MYSQL_TYPE_STRING;
                    results[0].buffer = (void *)targetData;
                    results[0].buffer_length = fields[0].length;
                    results[0].length = &length;
                    
                    status = mysql_stmt_bind_result(statement, results);
                    if (status==0) {
                        while (true) {
                            status = mysql_stmt_fetch(statement);
                            if (status!=0)
                                break;
                            
                            CString ignore = CString(targetData, length);
                            nickIgnoreList.push_back(ignore);
                            break;
                        }
                    }
                }
                mysql_free_result(result);
            }
            mysql_stmt_close(statement);
        }
        chanIgnoreList.clear();
        statement = mysql_stmt_init(database);
        status = mysql_stmt_prepare(statement, "SELECT `target` FROM `ignorelist` WHERE `type`='chan'", 53);
        if (status==0) {
            status = mysql_stmt_execute(statement);
            if (status==0) {
                MYSQL_RES *result = mysql_stmt_result_metadata(statement);
                unsigned int dataCount = mysql_num_fields(result);
                if (dataCount==1) {
                    MYSQL_FIELD *fields = mysql_fetch_fields(result);
                    
                    unsigned long length;
                    char targetData[fields[0].length];
                    MYSQL_BIND results[1];
                    memset(results, 0, sizeof(results));
                    results[0].buffer_type = MYSQL_TYPE_STRING;
                    results[0].buffer = (void *)targetData;
                    results[0].buffer_length = fields[0].length;
                    results[0].length = &length;
                    
                    status = mysql_stmt_bind_result(statement, results);
                    if (status==0) {
                        while (true) {
                            status = mysql_stmt_fetch(statement);
                            if (status!=0)
                                break;
                            
                            CString ignore = CString(targetData, length);
                            chanIgnoreList.push_back(ignore);
                            break;
                        }
                    }
                }
                mysql_free_result(result);
            }
            mysql_stmt_close(statement);
        }
    }
    bool AddIgnore(const CString& type, const CString& target) {
        if (!IgnoreExists(type, target)) {
            MYSQL_STMT *statement = mysql_stmt_init(database);
            int status = mysql_stmt_prepare(statement, "INSERT INTO `ignorelist` (`type`, `target`) VALUES (?,?)", 56);
            if (status!=0)
                return false;
            
            MYSQL_BIND bind[2];
            memset(bind, 0, sizeof(bind));
            
            bind[0].buffer_type = MYSQL_TYPE_STRING;
            bind[0].buffer = (void*)type.c_str();
            bind[0].buffer_length = type.length();
            bind[0].is_null = false;
            
            bind[1].buffer_type = MYSQL_TYPE_STRING;
            bind[1].buffer = (void*)target.c_str();
            bind[1].buffer_length = target.length();
            bind[1].is_null = false;
            
            status = mysql_stmt_bind_param(statement, bind);
            if (status!=0) {
                mysql_stmt_close(statement);
                return false;
            }
            
            mysql_stmt_execute(statement);
            mysql_stmt_close(statement);
            
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
            MYSQL_STMT *statement = mysql_stmt_init(database);
            int status = mysql_stmt_prepare(statement, "DELETE FROM `ignorelist` WHERE `type`=? AND `target`=?", 54);
            if (status!=0)
                return false;
            
            MYSQL_BIND bind[2];
            memset(bind, 0, sizeof(bind));
            
            bind[0].buffer_type = MYSQL_TYPE_STRING;
            bind[0].buffer = (void*)type.c_str();
            bind[0].buffer_length = type.length();
            bind[0].is_null = false;
            
            bind[1].buffer_type = MYSQL_TYPE_STRING;
            bind[1].buffer = (void*)target.c_str();
            bind[1].buffer_length = target.length();
            bind[1].is_null = false;
            
            status = mysql_stmt_bind_param(statement, bind);
            if (status!=0) {
                mysql_stmt_close(statement);
                return false;
            }
            
            mysql_stmt_execute(statement);
            mysql_stmt_close(statement);
            
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
        if (!m_pNetwork->IsUserAttached()) {
            SetSetting("clientDisconnected",GetUNIXTime());
        }
    }
    
    void Replay() {
        if (!databaseConnected)
            return;
        PutUser(":*LogMySQL!LogMySQL@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :Buffer Playback...");
        
        CString lastOnline = GetSetting("clientDisconnected");
        
        MYSQL_STMT *statement = mysql_stmt_init(database);
        int status = 0;
        if (!replayAll && !lastOnline.empty()) {
            status = mysql_stmt_prepare(statement, "SELECT * FROM `messages` WHERE `time`>? ORDER BY `time`", 55);
            if (status!=0) {
                PutUser(":*LogMySQL!LogMySQL@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :Playback failed due to sql problem.");
                return;
            }
            
            MYSQL_BIND bind[1];
            memset(bind, 0, sizeof(bind));
            
            bind[0].buffer_type = MYSQL_TYPE_STRING;
            bind[0].buffer = (void*)lastOnline.c_str();
            bind[0].buffer_length = lastOnline.length();
            bind[0].is_null = false;
            
            status = mysql_stmt_bind_param(statement, bind);
            if (status!=0) {
                PutUser(":*LogMySQL!LogMySQL@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :Playback failed due to sql problem.");
                mysql_stmt_close(statement);
                return;
            }
        } else {
            status = mysql_stmt_prepare(statement, "SELECT * FROM `messages` ORDER BY `time`", 40);
            if (status!=0) {
                PutUser(":*LogMySQL!LogMySQL@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :Playback failed due to sql problem.");
                return;
            }
        }
        
        status = mysql_stmt_execute(statement);
        if (status!=0) {
            PutUser(":*LogMySQL!LogMySQL@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :Playback failed due to sql problem.");
            mysql_stmt_close(statement);
            return;
        }
        
        MYSQL_RES *result = mysql_stmt_result_metadata(statement);
        
        unsigned int columnCount = mysql_num_fields(result);
        MYSQL_FIELD *fields = mysql_fetch_fields(result);    
        MYSQL_BIND results[columnCount];
        memset(results, 0, sizeof(results));
        map<unsigned int,char *> columnData;
        map<unsigned int,unsigned long> columnLength;
        map<unsigned int,bool> columnNull;
        map<unsigned int,CString> columns;
        for (unsigned int i=0; i<columnCount; i++) {
            columns[i] = CString(fields[i].name);
            if (columns[i].empty()) {
                char count[20];
                snprintf(count, sizeof(count), "%u", i);
                columns[i] = count;
            }
            columnData[i] = (char *)malloc(fields[i].length);
            results[i].buffer_type = MYSQL_TYPE_STRING;
            results[i].buffer = (void *)columnData[i];
            results[i].buffer_length = fields[i].length;
            results[i].length = &columnLength[i];
            results[i].is_null = (my_bool *)&columnNull[i];
        }
        
        status = mysql_stmt_bind_result(statement, results);
        if (status!=0) {
            PutUser(":*LogMySQL!LogMySQL@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :Playback failed due to sql problem.");
            mysql_free_result(result);
            mysql_stmt_close(statement);
            return;
        }
        
        while (true) {
            status = mysql_stmt_fetch(statement);
            if (status!=0)
                break;
            
            map<CString,CString> data;
            for (unsigned int i=0; i<columnCount; i++) {
                if (columnNull[i]) {
                    data[columns[i]] = "";
                } else {
                    data[columns[i]] = CString(columnData[i], columnLength[i]);
                }
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
                PutUser(":*LogMySQL!LogMySQL@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :["+timeStr+"] Server Disconnected");
            } else if (data["type"].Equals("CONNECT")) {
                PutUser(":*LogMySQL!LogMySQL@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :["+timeStr+"] Server Connected");
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
        
        for (unsigned int i=0; i<columnCount; i++) {
            free(columnData[i]);
        }
        
        mysql_free_result(result);
        mysql_stmt_close(statement);
        
        if (logLimit==1) {
            statement = mysql_stmt_init(database);
            status = mysql_stmt_prepare(statement, "DELETE FROM `messages`", 22);
            if (status==0) {
                mysql_stmt_execute(statement);
                mysql_stmt_close(statement);
            }
        }
        
        PutUser(":*LogMySQL!LogMySQL@znc.in NOTICE "+m_pNetwork->GetIRCNick().GetNickMask()+" :Playback Complete.");
    }
    
private:
    MYSQL *database;
    bool databaseConnected;
    bool connected;
    bool replayAll;
    unsigned long logLimit;
    int logLevel;
    
    vector<CString> nickIgnoreList;
    vector<CString> chanIgnoreList;
};

template<> void TModInfo<CLogMySQL>(CModInfo& Info) {
    Info.SetWikiPage("logmysql");
}

NETWORKMODULEDEFS(CLogMySQL, "Add logging to MySQL")