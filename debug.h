// Copyright (c) 2015 Klaralvdalens Datakonsult AB (KDAB).
// All rights reserved. Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef PHANTOMJS_DEBUG_H
#define PHANTOMJS_DEBUG_H

#include <include/internal/cef_string.h>
#include <string>
#include <iostream>


class Logger; 

class LoggingCategory 
{
    public:
        enum LogFilter{
            eLogDebug =1,
            eLogWarn = 2,
            eLogCritical = 4,
            eLogFatal = 8
            
        };
        LoggingCategory(const std::string& inName,int inLogFilter);
        bool isDebugEnabled() const {return m_enabledDebug;}
        bool isFatalEnabled() const {return m_enabledFatal;}
        bool isWarningEnabled() const {return m_enabledWarning;}
        bool isCriticalEnabled() const {return m_enabledCritical;}
    private: 
        std::string  m_name;
        bool m_enabledDebug;
        bool m_enabledWarning;
        bool m_enabledCritical;
        bool m_enabledFatal;
        friend class Logger;
    
};


class Logger
{
public:
    Logger(LoggingCategory& inLogCat);
    
    template<typename T>
    Logger&  debug(T  msg )
    {
      if(m_LogCat.m_enabledDebug)
        std::cerr << msg << std::endl;
      
      return *this;
    }
    
    
    Logger& debug(std::string  msg )
    {
      if(m_LogCat.m_enabledDebug & !msg.empty() )
        std::cerr << msg << std::endl;
      
      return *this;
    }
    
    template<typename T>
    Logger&  warn(T  msg )
    {
      if(m_LogCat.m_enabledWarning)
        std::cerr << msg << std::endl;
      
      return *this;
    }
    
    Logger& warn(const std::string&  msg )
    {
      if(m_LogCat.m_enabledWarning & !msg.empty())
        std::cerr << msg << std::endl;
      
      return *this;
    }
    Logger&  fatal(const std::string&  msg );
    Logger&  critical(const std::string&  msg );
private:
   LoggingCategory& m_LogCat;
};

extern LoggingCategory handler;
extern LoggingCategory app;
extern LoggingCategory print;
extern LoggingCategory keyevents;



Logger& operator<<(Logger& stream, const CefString& msg);
Logger& operator<<(Logger& stream, const std::string& msg);
Logger& operator<<(Logger& stream, int msg);
Logger& operator<<(Logger& stream, const char* msg);





#undef qCDebug
#undef qCWarning
#undef qDebug

#define qCDebug(category) \
    if(category.isDebugEnabled()) Logger(category).debug(std::string(""))
#define qCWarning(category) \
    if(category.isWarningEnabled()) Logger(category).warn(std::string(""))

#define qDebug() \
    std::cerr 

#endif // PHANTOMJS_DEBUG_H
