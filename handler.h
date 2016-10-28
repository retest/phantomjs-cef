// Copyright (c) 2015 Klaralvdalens Datakonsult AB (KDAB).
// All rights reserved. Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef CEF_TESTS_PHANTOMJS_HANDLER_H_
#define CEF_TESTS_PHANTOMJS_HANDLER_H_

#include "include/cef_client.h"
#include "include/wrapper/cef_message_router.h"

#include <deque>
#include <map>

#include "json11.hpp"


struct Rect{
  double x;
  double y;
  double width;
  double height;
  bool isValid() const {return true;} //always Rect is valid
};

class PhantomJSHandler : public CefClient,
                      public CefDisplayHandler,
                      public CefLifeSpanHandler,
                      public CefLoadHandler,
                      public CefRenderHandler,
                      public CefRequestHandler,
                      public CefMessageRouterBrowserSide::Handler,
                      public CefDownloadHandler
{
 public:
  PhantomJSHandler();
  ~PhantomJSHandler();

  // Provide access to the single global instance of this object.
  static CefMessageRouterConfig messageRouterConfig();

  CefRefPtr<CefBrowser> createBrowser(const CefString& url, bool isPhantomMain,
                                      const json11::Json& config = {});

  // CefClient methods:
  virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override
  {
    return this;
  }
  virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override
  {
    return this;
  }
  virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override
  {
    return this;
  }
  virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override
  {
    return this;
  }
  virtual CefRefPtr<CefRequestHandler> GetRequestHandler() override
  {
    return this;
  }
  CefRefPtr<CefDownloadHandler> GetDownloadHandler() override
  {
    return this;
  }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override;

  // CefDisplayHandler methods:
  virtual bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                             const CefString& message,
                             const CefString& source,
                             int line) override;

  // CefLifeSpanHandler methods:
  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
  virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  bool OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                     const CefString& target_url, const CefString& target_frame_name,
                     CefLifeSpanHandler::WindowOpenDisposition target_disposition, bool user_gesture,
                     const CefPopupFeatures& popupFeatures, CefWindowInfo& windowInfo,
                     CefRefPtr<CefClient>& client, CefBrowserSettings& settings, bool* no_javascript_access) override;

  // CefLoadHandler methods:
  virtual void OnLoadError(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           ErrorCode errorCode,
                           const CefString& errorText,
                           const CefString& failedUrl) override;
  void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) override;
  void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) override;
  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool isLoading, bool canGoBack, bool canGoForward) override;

  // CefRenderHandler methods:
  virtual bool GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
  virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                       PaintElementType type,
                       const RectList& dirtyRects,
                       const void* buffer, int width, int height) override;

  // CefRequestHandler methods:
  void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                 TerminationStatus status) override;
  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request, bool is_redirect) override;
  CefRequestHandler::ReturnValue OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                                      CefRefPtr<CefFrame> frame,
                                                      CefRefPtr<CefRequest> request,
                                                      CefRefPtr<CefRequestCallback> callback) override;
  bool OnResourceResponse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefRequest> request, CefRefPtr<CefResponse> response) override;
  bool GetAuthCredentials(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, bool isProxy,
                          const CefString & host, int port, const CefString & realm,
                          const CefString & scheme, CefRefPtr<CefAuthCallback> callback) override;

  // CefMessageRouterBrowserSide::Handler methods:
  bool OnQuery(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
               int64 query_id, const CefString& request, bool persistent,
               CefRefPtr<Callback> callback) override;
  void OnQueryCanceled(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                       int64 query_id) override;

  // CefDownloadHandler methods
  void OnBeforeDownload(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefDownloadItem> download_item,
                        const CefString& suggested_name,
                        CefRefPtr<CefBeforeDownloadCallback> callback) override;
  void OnDownloadUpdated(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefDownloadItem> download_item,
                         CefRefPtr<CefDownloadItemCallback> callback) override;

  // Request that all existing browser windows close.
  void CloseAllBrowsers(bool force_close);

private:
  bool canEmitSignal(const CefRefPtr<CefBrowser>& browser) const;
 
  void emitSignal(const CefRefPtr<CefBrowser>& browser, const std::string& signal,
            const json11::Json::array& arguments, bool internal = false);
                
  void handleLoadEnd(CefRefPtr<CefBrowser> browser, int statusCode, const CefString& url, bool success);

  // List of existing browser windows. Only accessed on the CEF UI thread.
  struct BrowserInfo
  {
    CefRefPtr<CefBrowser> browser;
    CefString authName;
    CefString authPassword;
    CefRefPtr<CefMessageRouterBrowserSide::Callback> signalCallback;
    bool firstLoadFinished = false;
  };
  std::map<int, BrowserInfo> m_browsers;

  CefRefPtr<CefMessageRouterBrowserSide> m_messageRouter;
  // NOTE: using QHash prevents a strange ABI issue discussed here: http://www.magpcss.org/ceforum/viewtopic.php?f=6&t=13543
  std::multimap<int32, CefRefPtr<CefMessageRouterBrowserSide::Callback>> m_waitForLoadedCallbacks;
  std::map<int64, CefRefPtr<CefMessageRouterBrowserSide::Callback>> m_pendingQueryCallbacks;
  std::map<int32, std::pair<int, int>> m_viewRects;
  struct PaintInfo
  {
    std::string path;
    std::string format;
    Rect clipRect;
    CefRefPtr<CefMessageRouterBrowserSide::Callback> callback;
  };
  std::map<int32, PaintInfo> m_paintCallbacks;
  struct RequestInfo
  {
    CefRefPtr<CefRequest> request;
    CefRefPtr<CefRequestCallback> callback;
  };
  std::map<uint64, RequestInfo> m_requestCallbacks;
  struct DownloadTargetInfo
  {
    std::string target;
    CefRefPtr<CefMessageRouterBrowserSide::Callback> callback;
  };
  std::map<std::string, DownloadTargetInfo> m_downloadTargets;
  std::map<uint, CefRefPtr<CefBeforeDownloadCallback>> m_beforeDownloadCallbacks;
  std::map<uint, CefRefPtr<CefDownloadItemCallback>> m_downloadItemCallbacks;
  std::map<uint, CefRefPtr<CefMessageRouterBrowserSide::Callback>> m_downloadCallbacks;
  std::multimap<int32, CefRefPtr<CefMessageRouterBrowserSide::Callback>> m_waitForDownloadCallbacks;

  // maps the requested popup url to the parent browser id
  std::deque<uint> m_popupToParentMapping;

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(PhantomJSHandler);
};

#endif  // CEF_TESTS_PHANTOMJS_HANDLER_H_
