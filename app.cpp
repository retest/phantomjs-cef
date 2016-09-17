// Copyright (c) 2015 Klaralvdalens Datakonsult AB (KDAB).
// All rights reserved. Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "app.h"

#include <experimental/filesystem>
#include <system_error>


#include <string>
#include <iostream>
#include <fstream>
#include <streambuf>
#include <chrono>
#include <ctime>
#include <vector>
#include <algorithm>

#include "handler.h"
#include "print_handler.h"
#include "debug.h"

#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_closure_task.h"

namespace fs = std::experimental::filesystem;

PhantomJSApp::PhantomJSApp()
  : m_printHandler(new PrintHandler)
  , m_messageRouter(CefMessageRouterRendererSide::Create(PhantomJSHandler::messageRouterConfig()))
{
}

PhantomJSApp::~PhantomJSApp()
{
}

void PhantomJSApp::OnRegisterCustomSchemes(CefRefPtr<CefSchemeRegistrar> registrar)
{
  // overwrite settings of the file scheme to allow extended access to it
  // without this, the about:blank page cannot load the user provided script
  // TODO: check whether that allso allows remote files access to local ones...
  registrar->AddCustomScheme("file", false, true, true);
}

void PhantomJSApp::OnContextInitialized()
{
  CEF_REQUIRE_UI_THREAD();

  // PhantomJSHandler implements browser-level callbacks.
  CefRefPtr<PhantomJSHandler> handler(new PhantomJSHandler());

  // Create the first browser window with empty content to get our hands on a frame
  auto browser = handler->createBrowser("about:blank", true);
  auto frame = browser->GetMainFrame();

  auto command_line = CefCommandLine::GetGlobalCommandLine();
  CefCommandLine::ArgumentList arguments;
  command_line->GetArguments(arguments);
  
  fs::path scriptFileInfo=arguments.front().ToString();
  
  const auto scriptPath = fs::absolute(scriptFileInfo).string();

  // now inject user provided js file and some meta data such as cli arguments
  // which are otherwise not available on the renderer process
  std::ostringstream content;
  content << "<html><head>\n";
  content << "<script type\"text/javascript\">\n";
  // forward extension code into global namespace
  content << "window.onerror = phantom.internal.propagateOnError;\n";
  content << "window.require = phantom.require;\n";
  // send arguments to script
  content << "phantom.args = [";
  for (const auto& arg : arguments) {
    content << '\"' << arg << "\",";
  }
  content << "];\n";
  // default initialize the library path to the folder of the script that will be executed
  content << "phantom.libraryPath = \"" << scriptPath << "\";\n";
  content <<"</script>\n";
  // then load the actual script
  content << "<script type=\"text/javascript\" src=\"file://" << scriptPath << "\" onerror=\"phantom.internal.onScriptLoadError();\"></script>\n";
  content << "</head><body></body></html>";
  frame->LoadString(content.str(), "phantomjs://" + scriptPath);
}

CefRefPtr<CefPrintHandler> PhantomJSApp::GetPrintHandler()
{
  return m_printHandler;
}

namespace {

std::string findLibrary(const std::string& filePath, const std::string& libraryPath)
{
  
  //
  fs::path currentPath = fs::current_path();
  fs::path libPath = libraryPath;
  
  
  
  for (const auto& pt : {currentPath, libPath}) {
    fs::path tmp_path = pt;
    tmp_path+="/";
    tmp_path+=filePath;
    
    if (!is_regular_file(tmp_path)) {
      continue;
    }
    return tmp_path.string();
  }
  return {};
}

std::string readFile(const std::string& filePath)
{
  std::ifstream in(filePath);
  if (!in) {
    return {};
  }
  std::string contents;
  in.seekg(0, std::ios::end);
  contents.resize(in.tellg());
  in.seekg(0, std::ios::beg);
  contents.assign(std::istreambuf_iterator<char>(in),
                  std::istreambuf_iterator<char>());
  return contents;
}

bool writeFile(const std::string& filePath, const std::string& contents, const std::string& m)
{
  std::ios_base::openmode mode = std::ios_base::out;
  if (m.find('b') != std::string::npos || m.find('B') != std::string::npos) {
    mode |= std::ios_base::binary;
  }
  if (m.find('a') != std::string::npos || m.find('A') != std::string::npos || m.find('+') != std::string::npos) {
    mode |= std::ios_base::app;
  } else {
    mode |= std::ios_base::trunc;
  }
  std::ofstream out(filePath, mode);
  if (out) {
    out << contents;
    return true;
  }
  return false;
}

bool remove(const std::string& path)
{
  
  fs::path info = path;
  if(is_directory(info))
  {
    return (fs::remove_all(info) ==0 ) ? true : false; 
  }
  return (fs::remove(info) ==0 ) ? true : false; 
  
}

bool copyRecursively(const std::string& srcFilePath, const std::string& tgtFilePath)
{
  
  fs::path srcFileInfo = srcFilePath;
  fs::path tgtFileInfo = tgtFilePath;
  if( fs::is_directory(srcFileInfo))
  {
    if(fs::exists(tgtFileInfo))
      return false;
    
    fs::create_directory(tgtFileInfo);
    std::error_code rc;
    fs::copy(srcFileInfo,tgtFileInfo,fs::copy_options::recursive | fs::copy_options::skip_symlinks,rc);
    if((bool)rc == true)
      return false;
    else
      return true;
    
  }
  else
  {
    std::error_code rc;
    fs::copy(srcFileInfo,tgtFileInfo,fs::copy_options::recursive | fs::copy_options::skip_symlinks,rc);
    if((bool)rc == true)
      return false;
    else
      return true;
  }

}

class V8Handler : public CefV8Handler
{
public:
  bool Execute(const CefString& name, CefRefPtr<CefV8Value> object,
               const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval,
               CefString& exception) override
  {
    auto context = CefV8Context::GetCurrentContext();
    static const std::string phantomjs_scheme = "phantomjs://";
    const auto frameURL = context->GetFrame()->GetURL().ToString();
    if (context->GetBrowser()->GetIdentifier() != 1 || frameURL.compare(0, phantomjs_scheme.size(), phantomjs_scheme)) {
      exception = "Access to PhantomJS function \"" + name.ToString() + "\" not allowed from URL \"" + frameURL + "\".";
      return true;
    }
    if (name == "exit") {
      context->GetBrowser()->SendProcessMessage(PID_BROWSER, CefProcessMessage::Create("exit"));
      return true;
    } else if (name == "printError" && !arguments.empty()) {
      std::cerr << arguments.at(0)->GetStringValue() << '\n';
      return true;
    } else if (name == "findLibrary") {
      const auto filePath =arguments.at(0)->GetStringValue();
      const auto libraryPath =arguments.at(1)->GetStringValue();
      retval = CefV8Value::CreateString(findLibrary(filePath, libraryPath));
      return true;
    } else if (name == "executeJavaScript") {
      const auto code = arguments.at(0)->GetStringValue();
      const auto file = arguments.at(1)->GetStringValue();
      context->GetFrame()->ExecuteJavaScript(code, "file://" + file.ToString(), 1);
      return true;
    } else if (name == "write") {
      const auto filename = arguments.at(0)->GetStringValue();
      const auto contents = arguments.at(1)->GetStringValue();
      const auto mode = arguments.at(2)->GetStringValue();
      retval = CefV8Value::CreateBool(writeFile(filename, contents, mode));
      return true;
    } else if (name == "read" || name == "readFile") {
      const auto file = arguments.at(0)->GetStringValue();
      retval = CefV8Value::CreateString(readFile(file));
      return true;
    } else if (name == "touch"){
      const auto filename = arguments.at(0)->GetStringValue();
      std::fstream fileopner;
      fileopner.open(filename);
      if(fileopner.is_open())
        retval = CefV8Value::CreateBool(true);
      else
        retval = CefV8Value::CreateBool(false);
      return true;
    } else if (name == "makeDirectory"){
      const auto path = arguments.at(0)->GetStringValue().ToString();
      retval = CefV8Value::CreateBool(fs::create_directory(path));
      return true;
    } else if (name == "makeTree"){
      const auto path = arguments.at(0)->GetStringValue().ToString();
       retval = CefV8Value::CreateBool(fs::create_directory(path));
      return true;
    } else if (name == "tempPath"){
      auto pp = fs::temp_directory_path();
      retval = CefV8Value::CreateString(pp.string());
      return true;
    } else if (name == "lastModified") {
      const auto filename = arguments.at(0)->GetStringValue().ToString();
      fs::path filepath = filename;
      auto ftime = fs::last_write_time(filepath);
      std::time_t cftime = decltype(ftime)::clock::to_time_t(ftime);
      
      retval = CefV8Value::CreateString(std::string(std::ctime(&cftime)));
      return true;
    } else if (name == "exists") {
      const auto filename = arguments.at(0)->GetStringValue();
      fs::path filepath= filename.ToString();
      retval = CefV8Value::CreateBool(fs::exists(filepath));
      return true;
    } else if (name == "isFile") {
      const auto filename = arguments.at(0)->GetStringValue();
      fs::path filepath=filename.ToString();
      retval = CefV8Value::CreateBool(fs::is_regular_file(filepath));
      return true;
    } else if (name == "isDirectory") {
      const auto filename = arguments.at(0)->GetStringValue();
      fs::path filepath = filename.ToString();
      retval = CefV8Value::CreateBool(fs::is_directory(filepath));
      return true;
    } else if (name == "copy") {
      const auto src = arguments.at(0)->GetStringValue();
      const auto dest = arguments.at(1)->GetStringValue();
      bool r = copyRecursively(src, dest);
      retval = CefV8Value::CreateBool(r);
      return true;
    } else if (name == "remove") {
      const auto src = arguments.at(0)->GetStringValue();
      bool r = remove(src);
      retval = CefV8Value::CreateBool(r);
      return true;
    } else if (name == "size") {
      const auto filename = arguments.at(0)->GetStringValue();
      fs::path filepath = filename.ToString();
      retval = CefV8Value::CreateInt(fs::file_size(filepath));
      return true;
    } else if (name == "list") {
      const auto path = arguments.at(0)->GetStringValue();
      
      fs::path dirpath = path.ToString();
      //get entry numbers
      int entrySize = 0;
      for(auto& p: fs::directory_iterator(dirpath))
      {
        (void)p;
        entrySize++;
      }
      
      CefRefPtr<CefV8Value> arr = CefV8Value::CreateArray(entrySize);
      int n =0;
      for(auto& p: fs::directory_iterator(dirpath))
      {
          arr->SetValue(n, CefV8Value::CreateString(p.path().string()));
          n++;
      }
      
      retval = arr;
      return true;
    }
    exception = std::string("Unknown PhantomJS function: ") + name.ToString();
    return true;
  }
private:
  IMPLEMENT_REFCOUNTING(V8Handler);
};
}

void PhantomJSApp::OnWebKitInitialized()
{
  CefRefPtr<CefV8Handler> handler = new V8Handler();

  //TODO(ayoub): use compiled resources bundled  into binary instead of looking up file from disk

  fs::path modules = "./modules";
  if(!fs::exists(modules))
  {
    std::cerr << "No modules found. This is a setup issue with the resource system - try to run CMake again." << std::endl; 
    exit(1);
  }
  
  std::vector<fs::path>  pathListes;
  
  for(const auto& module : fs::directory_iterator(modules))
  {
    pathListes.push_back(module.path()); 
  }
  
  std::sort(pathListes.begin(),pathListes.end());
  

  for(const auto& module : pathListes)
   {
     std::string modulepath = fs::absolute(module).string();
     if(fs::file_size(module) == 0)
     {
       std::cerr << "Module "  << modulepath << " is empty. This is a setup issue with the resource system - try to run CMake again." << std::endl;
       exit(1);
     }
      std::ifstream fstm(modulepath);
      std::string content((std::istreambuf_iterator<char>(fstm)),
                 std::istreambuf_iterator<char>());
      
     CefRegisterExtension(module.filename().string(), content, handler);
      
   }

}
void PhantomJSApp::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefV8Context> context)
{
  m_messageRouter->OnContextCreated(browser, frame, context);
}

void PhantomJSApp::OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                     CefRefPtr<CefV8Context> context)
{
  m_messageRouter->OnContextReleased(browser, frame, context);
}

bool PhantomJSApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefProcessId source_process,
                                            CefRefPtr<CefProcessMessage> message)
{
  return m_messageRouter->OnProcessMessageReceived(browser, source_process, message);
}
