// Copyright (c) 2015 Klaralvdalens Datakonsult AB (KDAB).
// All rights reserved. Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#if OS_WIN
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

#include <iostream>
#include "app.h"
#include <locale>
#include <codecvt>

#include "include/base/cef_logging.h"

#if OS_WIN
#include "include/cef_sandbox_win.h"

#if defined(CEF_USE_SANDBOX)
// The cef_sandbox.lib static library is currently built with VS2013. It may not
// link successfully with other VS versions.
#pragma comment(lib, "cef_sandbox.lib")
#endif

Q_IMPORT_PLUGIN (QWindowsIntegrationPlugin);

#elif defined(IMPORT_STATIC_QT_PLUGIN)

//Q_IMPORT_PLUGIN (QMinimalIntegrationPlugin);

#endif

#include <iostream>

int main(int argc, char** argv)
{
  void* sandbox_info = NULL;

#if OS_WIN
  // Enable High-DPI support on Windows 7 or newer.
  CefEnableHighDPISupport();

#if defined(CEF_USE_SANDBOX)
  // Manage the life span of the sandbox information object. This is necessary
  // for sandbox support on Windows. See cef_sandbox_win.h for complete details.
  CefScopedSandboxInfo scoped_sandbox;
  sandbox_info = scoped_sandbox.sandbox_info();
#endif
  HINSTANCE hInstance = GetModuleHandle(NULL);
  CefMainArgs main_args(hInstance);
#else
  CefMainArgs main_args(argc, argv);
#endif

  // PhantomJSApp implements application-level callbacks. It will create the first
  // browser instance in OnContextInitialized() after CEF has initialized.
  CefRefPtr<PhantomJSApp> app(new PhantomJSApp);

  // CEF applications have multiple sub-processes (render, plugin, GPU, etc)
  // that share the same executable. This function checks the command-line and,
  // if this is a sub-process, executes the appropriate logic.
  int exit_code = CefExecuteProcess(main_args, app, sandbox_info);
  if (exit_code >= 0) {
    // The sub-process has completed so return here.
    return exit_code;
  }

  if (argc < 2) {
    std::cerr << "Missing script parameter.\n";
    return 1;
  }

  // Specify CEF global settings here.
  CefSettings settings;
  // TODO: make this configurable like previously in phantomjs
  settings.ignore_certificate_errors = true;
  settings.remote_debugging_port = 12345;
  settings.windowless_rendering_enabled = true;
  // NOTE: accessing the file system requires us to disable sandboxing
  settings.no_sandbox = true;

  
#if OS_WIN
  const auto wPath = cachePath.toStdWString();
  cef_string_set(wPath.data(), wPath.size(), &settings.cache_path, 1);
#else
  passwd* pw = getpwuid(getuid());
  std::string cachePath(pw->pw_dir);
  cachePath+= "/.cache/phantomjs";

  cef_string_utf8_to_utf16(cachePath.c_str(),cachePath.size(), &settings.cache_path);
  
#endif

  //   settings.log_severity = LOGSEVERITY_VERBOSE;

  // Initialize CEF for the browser process.
  CefInitialize(main_args, settings, app, NULL);

  // Run the CEF message loop. This will block until CefQuitMessageLoop() is
  // called.
  CefRunMessageLoop();

  // Shut down CEF.
  CefShutdown();

  return 0;
}
