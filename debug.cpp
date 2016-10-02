// Copyright (c) 2015 Klaralvdalens Datakonsult AB (KDAB).
// All rights reserved. Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "debug.h"
#include <iostream>


std::ostream& operator<<(std::ostream& stream, const wchar_t *input)
{
  std::wstring ws(input);
  std::string str(ws.begin(), ws.end());
  return stream << str;
}



//-------
LoggingCategory::LoggingCategory(const std::string& inName,int inLogFilter)
:m_name(inName)
,m_enabledDebug(false)
,m_enabledWarning(false)
,m_enabledCritical(false)
,m_enabledFatal(false)
{
  if(inLogFilter & eLogDebug)
    m_enabledDebug = true;
  if(inLogFilter & eLogWarn)
    m_enabledWarning = true;
  if(inLogFilter & eLogCritical)
    m_enabledCritical = true;
  if(inLogFilter & eLogFatal)
    m_enabledFatal = true;
    
}

Logger::Logger(LoggingCategory& inLogCat):
m_LogCat(inLogCat)
{
  
}



Logger& Logger::fatal(const std::string&  msg )
{
  if(m_LogCat.m_enabledFatal & !msg.empty() )
    std::cerr << msg << std::endl;
  
  return *this;
}

Logger& Logger::critical(const std::string&  msg )
{
  if(m_LogCat.m_enabledCritical & !msg.empty())
    std::cerr << msg << std::endl;
  
  return *this;
  
}

Logger& operator<<(Logger& stream, const CefString& msg)
{
  return stream.debug(msg.ToString());
  
}


Logger& operator<<(Logger& stream, const std::string& msg)
{
  
  return stream.debug(msg);
}


Logger& operator<<(Logger& stream,const  char* msg)
{
  return stream.debug(std::string(msg));
}


Logger& operator<<(Logger& stream, int msg)
{
  return stream.debug(msg);
}

LoggingCategory handler( "phantomjs.handler",LoggingCategory::eLogWarn);
LoggingCategory app("phantomjs.app", LoggingCategory::eLogWarn);
LoggingCategory print("phantomjs.print", LoggingCategory::eLogWarn);
LoggingCategory keyevents("key.keyevents", LoggingCategory::eLogWarn);
