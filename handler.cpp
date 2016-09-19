// Copyright (c) 2015 Klaralvdalens Datakonsult AB (KDAB).
// All rights reserved. Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "handler.h"

#include <sstream>
#include <string>
#include <iostream>
#include <locale>
#include <algorithm>
#include <experimental/filesystem>
#include <cctype>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPageSize>
#include <QRect>
#include <QImage>
#include <QBuffer>
#include <QImageWriter>
#include <QDateTime>

#include <QMessageLogger>
#include <QUrl>

#include "include/base/cef_bind.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

#include "print_handler.h"
#include "debug.h"

#include "WindowsKeyboardCodes.h"
#include "keyevents.h"




namespace fs = std::experimental::filesystem;
namespace {

/**
 * Workaround an issue where the frame's IsMain property is not updated to
 * reflect the actual main frame set in the browser.
 */
bool isMain(const CefRefPtr<CefFrame>& frame)
{
  return frame->IsMain() || frame->GetIdentifier() == frame->GetBrowser()->GetMainFrame()->GetIdentifier();
}

template<typename T, typename K>
T takeCallback(std::map<K, T>* callbacks, K key)
{
  auto it = callbacks->find(key);
  if (it != callbacks->end()) {
    auto ret = it->second;
    callbacks->erase(it);
    return ret;
  }
  return {};
}

template<typename T>
T takeCallback(std::map<int32, T>* callbacks, const CefRefPtr<CefBrowser>& browser)
{
  return takeCallback(callbacks, browser->GetIdentifier());
}

template<typename T, typename K>
T takeCallback(std::multimap<K, T>* callbacks, K key)
{
  auto it = callbacks->find(key);
  if (it != callbacks->end()) {
    auto ret = it->second;
    callbacks->erase(it);
    return ret;
  }
  return {};
}

template<typename T>
T takeCallback(std::multimap<int32, T>* callbacks, const CefRefPtr<CefBrowser>& browser)
{
  return takeCallback(callbacks, browser->GetIdentifier());
}



void initWindowInfo(CefWindowInfo& window_info, bool isPhantomMain)
{
#if defined(OS_WIN)
  // On Windows we need to specify certain flags that will be passed to
  // CreateWindowEx().
  window_info.SetAsPopup(NULL, "phantomjs");
#endif
  if (isPhantomMain || !qEnvironmentVariableIsSet("PHANTOMJS_CEF_SHOW_WINDOW")) {
    window_info.SetAsWindowless(0, true);
  }
}


cef_state_t toState(const rapidjson::Value& value)
{
  
  
  if (value.IsBool()) {
    return value.GetBool() ? STATE_ENABLED : STATE_DISABLED;
  } else if (value.IsString()) {
    std::string stringValue = value.GetString(),strON("ON"), strYes("YES");
    transform(stringValue.begin(), stringValue.end(), stringValue.begin(), toupper);
    
    if(stringValue == strON || stringValue != strYes )
    {
      return STATE_ENABLED;  
    }
    
  }

  return STATE_DEFAULT;
}
/*cef_state_t toState(const QJsonValue& value)
{
  if (value.isBool()) {
    return value.toBool() ? STATE_ENABLED : STATE_DISABLED;
  } else if (value.isString()) {
    const auto stringValue = value.toString();
    if (!stringValue.compare(QLatin1String("on"), Qt::CaseInsensitive)
        || stringValue.compare(QLatin1String("yes"), Qt::CaseInsensitive))
    {
      return STATE_ENABLED;
    }
  }

  return STATE_DEFAULT;
}
*/

void   initBrowserSettings(CefBrowserSettings& browser_settings, bool isPhantomMain,
                         const rapidjson::Value& config)
{
  
 
  // TODO: make this configurable
  if (isPhantomMain) {
    browser_settings.web_security = STATE_DISABLED;
    browser_settings.universal_access_from_file_urls = STATE_ENABLED;
    browser_settings.file_access_from_file_urls = STATE_ENABLED;
  } else if(!config.IsObject())  {
    
     browser_settings.web_security = STATE_DEFAULT;
    browser_settings.universal_access_from_file_urls = STATE_DEFAULT;
    browser_settings.image_loading =STATE_DEFAULT;
    browser_settings.javascript = STATE_DEFAULT;
    browser_settings.javascript_open_windows = STATE_DEFAULT;
    browser_settings.javascript_close_windows = STATE_DEFAULT;
  }
  else  {
    browser_settings.web_security = toState(config["webSecurityEnabled"]);
    browser_settings.universal_access_from_file_urls = toState(config["localToRemoteUrlAccessEnabled"]);
    browser_settings.image_loading = toState(config["loadImages"]);
    browser_settings.javascript = toState(config["javascriptEnabled"]);
    browser_settings.javascript_open_windows = toState(config["javascriptOpenWindows"]);
    browser_settings.javascript_close_windows = toState(config["javascriptCloseWindows"]);
    /// TODO: extend
  }
}
/*void initBrowserSettings(CefBrowserSettings& browser_settings, bool isPhantomMain,
                         const QJsonObject& config)
{
  // TODO: make this configurable
  if (isPhantomMain) {
    browser_settings.web_security = STATE_DISABLED;
    browser_settings.universal_access_from_file_urls = STATE_ENABLED;
    browser_settings.file_access_from_file_urls = STATE_ENABLED;
  } else {
    browser_settings.web_security = toState(config.value(QStringLiteral("webSecurityEnabled")));
    browser_settings.universal_access_from_file_urls = toState(config.value(QStringLiteral("localToRemoteUrlAccessEnabled")));
    browser_settings.image_loading = toState(config.value(QStringLiteral("loadImages")));
    browser_settings.javascript = toState(config.value(QStringLiteral("javascriptEnabled")));
    browser_settings.javascript_open_windows = toState(config.value(QStringLiteral("javascriptOpenWindows")));
    browser_settings.javascript_close_windows = toState(config.value(QStringLiteral("javascriptCloseWindows")));
    /// TODO: extend
  }
}
*/
#if CHROME_VERSION_BUILD >= 2526
const bool PRINT_SETTINGS = false;

void printValue(const CefString& key, const CefRefPtr<CefValue>& value) {
  switch (value->GetType()) {
    case VTYPE_INVALID:
      qDebug() << key << "invalid";
      break;
    case VTYPE_NULL:
      qDebug() << key << "null";
      break;
    case VTYPE_BOOL:
      qDebug() << key << value->GetBool();
      break;
    case VTYPE_INT:
      qDebug() << key << value->GetInt();
      break;
    case VTYPE_DOUBLE:
      qDebug() << key << value->GetDouble();
      break;
    case VTYPE_STRING:
      qDebug() << key << value->GetString();
      break;
    case VTYPE_BINARY:
      qDebug() << key << "binary";
      break;
    case VTYPE_DICTIONARY: {
      qDebug() << key << "dictionary";
      auto dict = value->GetDictionary();
      CefDictionaryValue::KeyList keys;
      dict->GetKeys(keys);
      for (const auto& subKey : keys) {
        printValue(key.ToString() + "." + subKey.ToString(), dict->GetValue(subKey));
      }
      break;
    }
    case VTYPE_LIST:
      qDebug() << key << "list";
      auto list = value->GetList();
      for (int i = 0; i < static_cast<int>(list->GetSize()); ++i) {
        printValue(key.ToString() + "[" + std::to_string(i) + "]", list->GetValue(i));
      }
      break;
  }
}
#endif

QJsonObject toJson(const CefRefPtr<CefDownloadItem>& item)
{
  QJsonObject object;
  object[QStringLiteral("isInProgress")] = item->IsInProgress();
  object[QStringLiteral("isComplete")] = item->IsComplete();
  object[QStringLiteral("isCanceled")] = item->IsCanceled();
  if (!item->GetSuggestedFileName().empty()) {
    object[QStringLiteral("suggestedFileName")] = QString::fromStdString(item->GetSuggestedFileName());
  }
  object[QStringLiteral("fullPath")] = QString::fromStdString(item->GetFullPath());
  object[QStringLiteral("url")] = QString::fromStdString(item->GetURL());
  object[QStringLiteral("originalUrl")] = QString::fromStdString(item->GetOriginalUrl());
  object[QStringLiteral("mimeType")] = QString::fromStdString(item->GetMimeType());
  object[QStringLiteral("contentDisposition")] = QString::fromStdString(item->GetContentDisposition());
  object[QStringLiteral("totalBytes")] = static_cast<qint64>(item->GetTotalBytes());
  object[QStringLiteral("receivedBytes")] = static_cast<qint64>(item->GetReceivedBytes());
  object[QStringLiteral("currentSpeed")] = static_cast<qint64>(item->GetCurrentSpeed());
  object[QStringLiteral("percentComplete")] = item->GetPercentComplete();
  object[QStringLiteral("startTime")] = QDateTime::fromTime_t(item->GetStartTime().GetTimeT()).toUTC().toString(Qt::ISODate);
  if (item->GetEndTime().GetTimeT()) {
    // this was always invalid in my tests
    object[QStringLiteral("endTime")] = QDateTime::fromTime_t(item->GetEndTime().GetTimeT()).toUTC().toString(Qt::ISODate);
  }
  return object;
}
}

PhantomJSHandler::PhantomJSHandler()
    : m_messageRouter(CefMessageRouterBrowserSide::Create(messageRouterConfig()))
{
  m_messageRouter->AddHandler(this, false);
}

PhantomJSHandler::~PhantomJSHandler()
{
}

CefMessageRouterConfig PhantomJSHandler::messageRouterConfig()
{
  CefMessageRouterConfig config;
  config.js_cancel_function = "cancelPhantomJsQuery";
  config.js_query_function = "startPhantomJsQuery";
  return config;
}

CefRefPtr<CefBrowser> PhantomJSHandler::createBrowser(const CefString& url, bool isPhantomMain,
                                                      const rapidjson::Value& config)
{
  CefWindowInfo window_info;
  initWindowInfo(window_info, isPhantomMain);

  CefBrowserSettings browser_settings;
  initBrowserSettings(browser_settings, isPhantomMain, config);

  //qCDebug(handler) << url << isPhantomMain << config;

  return CefBrowserHost::CreateBrowserSync(window_info, this, url, browser_settings,
                                           NULL);
}

bool PhantomJSHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                                CefProcessId source_process,
                                                CefRefPtr<CefProcessMessage> message)
{
  if (m_messageRouter->OnProcessMessageReceived(browser, source_process, message)) {
    return true;
  }
  if (message->GetName() == "exit") {
    CloseAllBrowsers(true);
    return true;
  }
  return false;
}

bool PhantomJSHandler::OnConsoleMessage(CefRefPtr<CefBrowser> browser, const CefString& message, const CefString& source, int line)
{
  if (!canEmitSignal(browser)) {
    fs::path  filepath = source.ToString();
    
    auto shortSource = fs::absolute(filepath).filename().string();
    QMessageLogger(shortSource.c_str(), line, 0).debug() << message;
  } else {
    emitSignal(browser, std::string("onConsoleMessage"),
        {QString::fromStdString(message), QString::fromStdString(source), line});
  }
  return true;
}

void PhantomJSHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
  CEF_REQUIRE_UI_THREAD();

  qCDebug(handler) << browser->GetIdentifier();
   if ( m_browsers.find(browser->GetIdentifier()) == m_browsers.end() )
  {
    m_browsers.emplace(browser->GetIdentifier(),PhantomJSHandler::BrowserInfo());
  }
  auto& browserInfo = m_browsers.at(browser->GetIdentifier());
  browserInfo.browser = browser;
  if (!m_popupToParentMapping.empty()) {
    auto parentBrowser = m_popupToParentMapping.front();
    //m_popupToParentMapping.pop_front();
    // we don't open about:blank for popups
    browserInfo.firstLoadFinished = true;
    emitSignal(m_browsers.at(parentBrowser).browser, std::string("onPopupCreated"),
               {browser->GetIdentifier()}, true);
  }

#if CHROME_VERSION_BUILD >= 2526
  if (PRINT_SETTINGS) {
    auto prefs = browser->GetHost()->GetRequestContext()->GetAllPreferences(true);
    CefDictionaryValue::KeyList keys;
    prefs->GetKeys(keys);
    for (const auto& key : keys) {
      printValue(key, prefs->GetValue(key));
    }
  }
#endif
}

bool PhantomJSHandler::DoClose(CefRefPtr<CefBrowser> browser)
{
  CEF_REQUIRE_UI_THREAD();

  // Allow the close. For windowed browsers this will result in the OS close
  // event being sent.
  return false;
}

void PhantomJSHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
  CEF_REQUIRE_UI_THREAD();

  qCDebug(handler) << browser->GetIdentifier();

  m_messageRouter->OnBeforeClose(browser);

  m_browsers.erase(browser->GetIdentifier());

  if (m_browsers.empty()) {
    // All browser windows have closed. Quit the application message loop.
    CefQuitMessageLoop();
  }
}

bool PhantomJSHandler::OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                     const CefString& target_url, const CefString& target_frame_name,
                                     CefLifeSpanHandler::WindowOpenDisposition target_disposition, bool user_gesture,
                                     const CefPopupFeatures& popupFeatures, CefWindowInfo& windowInfo,
                                     CefRefPtr<CefClient>& client, CefBrowserSettings& settings,
                                     bool* no_javascript_access)
{
  qCDebug(handler) << browser->GetIdentifier() << frame->GetURL() << target_url << target_frame_name;
  // TODO: inherit settings? manipulate?
  initWindowInfo(windowInfo, false);
  initBrowserSettings(settings, false, rapidjson::Value());
  client = this;
  // TODO: is it enough to assume the next browser that will be created belongs to this popup request?
  m_popupToParentMapping.push_front(browser->GetIdentifier());
  return false;
}

void PhantomJSHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString& errorText,
                                const CefString& failedUrl)
{
  CEF_REQUIRE_UI_THREAD();

  qCDebug(handler) << browser->GetIdentifier() << isMain(frame) << errorCode << errorText << failedUrl;

  if (isMain(frame)) {
    handleLoadEnd(browser, errorCode, failedUrl, false);
  }

  // Don't display an error for downloaded files.
  if (errorCode == ERR_ABORTED)
    return;

  // Display a load error message.
  std::stringstream ss;
  ss << "<html><body bgcolor=\"white\">"
        "<h2>Failed to load URL " << std::string(failedUrl) <<
        " with error " << std::string(errorText) << " (" << errorCode <<
        ").</h2></body></html>";
  frame->LoadString(ss.str(), failedUrl);
}

void PhantomJSHandler::OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame)
{
  CEF_REQUIRE_UI_THREAD();

  qCDebug(handler) << browser->GetIdentifier() << frame->GetURL() << isMain(frame);

  // filter out events from sub frames
  if (!isMain(frame) || !canEmitSignal(browser) || !m_browsers.at(browser->GetIdentifier()).firstLoadFinished ) {
    return;
  }

  emitSignal(browser, std::string("onLoadStarted"), {QString::fromStdString(frame->GetURL())});
}

void PhantomJSHandler::emitSignal(const CefRefPtr<CefBrowser>& browser, const std::string& signal,
                                  const QJsonArray& arguments, bool internal)
{
 if ( m_browsers.find(browser->GetIdentifier()) == m_browsers.end() )
  {
    m_browsers.emplace( browser->GetIdentifier(),PhantomJSHandler::BrowserInfo());
  }
  auto callback = m_browsers.at(browser->GetIdentifier()).signalCallback;
  if (!callback) {
    //qDebug() << "no signal callback for browser" << browser->GetIdentifier() << signal;
    return;
  }
  QJsonObject data = {
    {QStringLiteral("signal"), QString::fromStdString(signal)},
    {QStringLiteral("args"), arguments}
  };
  if (internal) {
    data[QStringLiteral("internal")] = true;
  }
  callback->Success(QJsonDocument(data).toJson().constData());
}

bool PhantomJSHandler::canEmitSignal(const CefRefPtr<CefBrowser>& browser) const
{
  /*if ( m_browsers.find(browser->GetIdentifier()) == m_browsers.end() )
  {
    m_browsers.emplace(browser->GetIdentifier(),PhantomJSHandler::BrowserInfo());
  }*/
  if(m_browsers.find(browser->GetIdentifier()) == m_browsers.end())
  {
    return  CefRefPtr<CefMessageRouterBrowserSide::Callback>();
  }
  return m_browsers.at(browser->GetIdentifier()).signalCallback;
}

void PhantomJSHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode)
{
  CEF_REQUIRE_UI_THREAD();

  qCDebug(handler) << browser->GetIdentifier() << frame->GetURL() << isMain(frame) << httpStatusCode;

  // filter out events from sub frames
  if (!isMain(frame)) {
    return;
  }

  /// TODO: is this OK?
  const bool success = httpStatusCode < 400;
  handleLoadEnd(browser, httpStatusCode, frame->GetURL(), success);
}

void PhantomJSHandler::OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool isLoading, bool canGoBack, bool canGoForward)
{
  qCDebug(handler) << browser->GetIdentifier() << isLoading;
}

void PhantomJSHandler::handleLoadEnd(CefRefPtr<CefBrowser> browser, int statusCode, const CefString& url, bool success)
{
  qCDebug(handler) << browser->GetIdentifier() << statusCode << url << success;

  if ( m_browsers.find(browser->GetIdentifier()) == m_browsers.end() )
  {
    m_browsers.emplace(browser->GetIdentifier(),PhantomJSHandler::BrowserInfo());
  }
  auto& browserInfo = m_browsers.at(browser->GetIdentifier());
  if (!browserInfo.firstLoadFinished) {
    browserInfo.firstLoadFinished = true;
    return;
  }

  if (canEmitSignal(browser)) {
    emitSignal(browser, std::string("onLoadEnd"), {QString::fromStdString(url), success}, true);
  }

  while (auto callback = takeCallback(&m_waitForLoadedCallbacks, browser)) {
    if (success) {
      callback->Success(std::to_string(statusCode));
    } else {
      callback->Failure(statusCode, "Failed to load URL: " + url.ToString());
    }
  }
}

bool PhantomJSHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
  //qCDebug(handler) << browser->GetIdentifier() << m_viewRects.at(browser->GetIdentifier());
  if(m_viewRects.find(browser->GetIdentifier()) == m_viewRects.end())
  {
    m_viewRects.insert(std::make_pair(browser->GetIdentifier(), std::pair<int,int>(800, 600)));
  }
  const auto size = m_viewRects.at(browser->GetIdentifier());
  rect.Set(0, 0, size.first, size.second);
  return true;
}

void PhantomJSHandler::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer, int width, int height)
{
  qCDebug(handler) << browser->GetIdentifier() << type << width << height;

  if (!canEmitSignal(browser)) {
    return;
  }

  auto info = takeCallback(&m_paintCallbacks, browser);
  if (info.callback) {
    QImage image(reinterpret_cast<const uchar*>(buffer), width, height, QImage::Format_ARGB32);
    if (info.clipRect.isValid()) {
      image = image.copy(info.clipRect);
    }
    if (info.format.empty()) {
      if (image.save(QString::fromStdString(info.path))) {
        info.callback->Success({});
      } else {
        info.callback->Failure(1, QStringLiteral("Failed to render page to \"%1\".").arg(QString::fromStdString(info.path)).toStdString());
      }
    } else {
      QByteArray ba;
      QBuffer buffer(&ba);
      buffer.open(QIODevice::WriteOnly);
      if (image.save(&buffer, info.format.c_str())) {
        const auto data = ba.toBase64();
        info.callback->Success(std::string(data.constData(), data.size()));
      } else {
        std::string error = "Failed to render page into Base64 encoded buffer of format \"";
        error += qPrintable(QString::fromStdString(info.format));
        error += "\". Available formats are: ";
        bool first = true;
        foreach (const auto& format, QImageWriter::supportedImageFormats()) {
          if (!first) {
            error += ", ";
          }
          error += std::string(format);
          first = false;
        }
        info.callback->Failure(1, error);
      }
    }
  }

  QJsonArray jsonDirtyRects;
  for (const auto& rect : dirtyRects) {
    QJsonObject jsonRect = {
      {QStringLiteral("x"), rect.x},
      {QStringLiteral("y"), rect.y},
      {QStringLiteral("width"), rect.width},
      {QStringLiteral("height"), rect.height}
    };
    jsonDirtyRects.push_back(jsonRect);
  }

  emitSignal(browser, std::string("onPaint"), {jsonDirtyRects, width, height, type});
}

void PhantomJSHandler::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser, TerminationStatus status)
{
  m_messageRouter->OnRenderProcessTerminated(browser);
}

bool PhantomJSHandler::OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, bool is_redirect)
{
  m_messageRouter->OnBeforeBrowse(browser, frame);
  return false;
}

template<typename T>
QJsonObject headerMapToJson(const CefRefPtr<T>& r)
{
  QJsonObject jsonHeaders;
  CefRequest::HeaderMap headers;
  r->GetHeaderMap(headers);
  for (const auto& header : headers) {
    jsonHeaders[QString::fromStdString(header.first)] = QString::fromStdString(header.second);
  }
  return jsonHeaders;
}

CefRequestHandler::ReturnValue PhantomJSHandler::OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                                   CefRefPtr<CefRequest> request, CefRefPtr<CefRequestCallback> callback)
{
  if (!canEmitSignal(browser)) {
    return RV_CONTINUE;
  }

  qCDebug(handler) << browser->GetIdentifier() << frame->GetURL() << request->GetURL();

  QJsonArray jsonPost;
  if (const auto post = request->GetPostData()) {
    CefPostData::ElementVector elements;
    post->GetElements(elements);
    for (const auto& element : elements) {
      QJsonObject elementJson = {{QStringLiteral("type"), element->GetType()}};
      switch (element->GetType()) {
        case PDE_TYPE_BYTES: {
          QByteArray bytes;
          bytes.resize(static_cast<int>(element->GetBytesCount()));
          element->GetBytes(bytes.size(), bytes.data());
          const auto STRING_BYTES = QStringLiteral("bytes");
          elementJson[STRING_BYTES] = QString::fromUtf8(bytes.toBase64());
          break;
        }
        case PDE_TYPE_FILE: {
          const auto STRING_FILE = QStringLiteral("file");
          elementJson[STRING_FILE] = QString::fromStdString(element->GetFile().ToString());
          break;
        }
        case PDE_TYPE_EMPTY:
          break;
      }
      jsonPost.append(elementJson);
    }
  }

  QJsonObject jsonRequest;
  jsonRequest[QStringLiteral("headers")] = headerMapToJson(request);
  jsonRequest[QStringLiteral("post")] = jsonPost;
  jsonRequest[QStringLiteral("url")] = QString::fromStdString(request->GetURL().ToString());
  jsonRequest[QStringLiteral("method")] = QString::fromStdString(request->GetMethod().ToString());
  jsonRequest[QStringLiteral("flags")] = request->GetFlags();
  jsonRequest[QStringLiteral("resourceType")] = static_cast<int>(request->GetResourceType());
  jsonRequest[QStringLiteral("transitionType")] = static_cast<int>(request->GetTransitionType());

  if(m_requestCallbacks.find(request->GetIdentifier()) == m_requestCallbacks.end())
  {
    m_requestCallbacks.insert(std::pair<uint64, RequestInfo>(request->GetIdentifier(), {request, callback}));
  }
  else
    m_requestCallbacks.at(request->GetIdentifier()) = {request, callback};

  emitSignal(browser, std::string("onBeforeResourceLoad"),
             {jsonRequest, QString::number(request->GetIdentifier())}, true);

  return RV_CONTINUE_ASYNC;
}

bool PhantomJSHandler::OnResourceResponse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                          CefRefPtr<CefRequest> request, CefRefPtr<CefResponse> response)
{
  if (canEmitSignal(browser)) {
    QJsonObject jsonResponse;
    jsonResponse[QStringLiteral("status")] = response->GetStatus();
    jsonResponse[QStringLiteral("statusText")] = QString::fromStdString(response->GetStatusText());
    jsonResponse[QStringLiteral("contentType")] = QString::fromStdString(response->GetMimeType());
    jsonResponse[QStringLiteral("headers")] = headerMapToJson(response);
    jsonResponse[QStringLiteral("url")] = QString::fromStdString(request->GetURL());
    jsonResponse[QStringLiteral("id")] = QString::number(request->GetIdentifier());
    /// TODO: time, stage, bodySize, redirectUrl
    emitSignal(browser, std::string("onResourceReceived"), {jsonResponse});
  }
  return false;
}

bool PhantomJSHandler::GetAuthCredentials(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                          bool isProxy, const CefString& host, int port, const CefString& realm, const CefString& scheme,
                                          CefRefPtr<CefAuthCallback> callback)
{
  if ( m_browsers.find(browser->GetIdentifier()) == m_browsers.end() )
  {
    m_browsers.emplace(browser->GetIdentifier(),PhantomJSHandler::BrowserInfo());
  }
  const auto& info = m_browsers.at(browser->GetIdentifier());
  if (info.authName.empty() || info.authPassword.empty()) {
    return false;
  }
  // TODO: this old PhantomJS API is really bad, we should rather delegate that to the script and delay the callback execution
  callback->Continue(info.authName, info.authPassword);
  return true;
}

void PhantomJSHandler::CloseAllBrowsers(bool force_close)
{
  qCDebug(handler) << force_close;

  m_messageRouter->CancelPending(nullptr, nullptr);

  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI,
        base::Bind(&PhantomJSHandler::CloseAllBrowsers, this, force_close));
    return;
  }

  // iterate over list of values to ensure we really close all browsers
  foreach (const auto& info, m_browsers) {
    info.second.browser->GetHost()->CloseBrowser(force_close);
  }
}

bool PhantomJSHandler::OnQuery(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                               int64 query_id, const CefString& request, bool persistent,
                               CefRefPtr<Callback> callback)
{
  CEF_REQUIRE_UI_THREAD();

  const auto data = QByteArray(request.ToString().data());

  
  rapidjson::Document json;
  
  
  if(json.Parse(request.ToString().data()).HasParseError())
  {
     //TODO: warning 
     return false;
  }
  //qCDebug(handler) << browser->GetIdentifier() << frame->GetURL() << json;
 

  const auto type = std::string(json["type"].GetString());
  //std::cout << "query type: " << type << std::endl;
  if (type == "createBrowser") {
    const auto& settings = json["settings"].GetObject();
    auto subBrowser = createBrowser("about:blank", false, settings);
    if ( m_browsers.find(subBrowser->GetIdentifier()) == m_browsers.end() )
    {
      m_browsers.emplace(subBrowser->GetIdentifier(),PhantomJSHandler::BrowserInfo());
    }
    auto& info = m_browsers.at(subBrowser->GetIdentifier());
    if(settings.HasMember("userName"))
      info.authName = settings["userName"].GetString();
    if(settings.HasMember("password"))
    info.authPassword = settings["password"].GetString();
    callback->Success(std::to_string(subBrowser->GetIdentifier()));
    return true;
  } else if (type == "returnEvaluateJavaScript") {
    int otherQueryId = -1;
    if(json.HasMember("queryId"))
      otherQueryId = json["queryId"].GetInt();
    auto it = m_pendingQueryCallbacks.find(otherQueryId);
    if (it != m_pendingQueryCallbacks.end()) {
      auto otherCallback = it->second;
      if (json.HasMember("exception")) {
        auto& exception = json["exception"];
        otherCallback->Failure(1, exception.GetString());
      } else {
        auto retval = json["retval"].GetString();
        otherCallback->Success(retval);
      }
      m_pendingQueryCallbacks.erase(it);
      callback->Success({});
      return true;
    }
  } else if (type =="beforeResourceLoadResponse" ) {
    //json["requestId"] is String, convert it to uint64_t 
    const auto requestId = static_cast<uint64>(std::stoull(json["requestId"].GetString()));
    auto callback = takeCallback(&m_requestCallbacks, requestId);
    if (!callback.callback || !callback.request) {
     // qCWarning(handler) << "Unknown request with id" << requestId << "for query" << json;
      return false;
    }
    const auto allow = json["allow"].GetBool();
    if (!allow) {
      callback.callback->Continue(allow);
      return true;
    }
    auto requestData = json["request"].GetObject();
    callback.request->SetURL(requestData["url"].GetString());
    CefRequest::HeaderMap headers;
    const auto& jsonHeaders = requestData["headers"].GetObject();
    for (auto it = jsonHeaders.MemberBegin(); it != jsonHeaders.MemberEnd(); ++it) {
      headers.insert(std::make_pair(it->name.GetString(), it->  value.GetString()));
    }
    callback.request->SetHeaderMap(headers);
    // TODO: post support
    callback.callback->Continue(true);
    return true;
  } else if (type =="beforeDownloadResponse") {
    const auto requestId = static_cast<uint64>(json["requestId"].GetUint64());
    const auto target = json["target"].GetString();
    auto callback = m_beforeDownloadCallbacks.at(requestId);
    m_beforeDownloadCallbacks.erase(requestId);
    if (!callback) {
      //qCWarning(handler) << "Unknown request with id" << requestId << "for query" << json;
      return false;
    }
    callback->Continue(target, false);
    return true;
  } else if (type =="cancelDownload") {
    const auto requestId = static_cast<uint64>(json["requestId"].GetUint64());
    auto callback = m_downloadItemCallbacks.at(requestId);
    m_downloadItemCallbacks.erase(requestId);
    if (!callback) {
      //qCWarning(handler) << "Unknown request with id" << requestId << "for query" << json;
      return false;
    }
    callback->Cancel();
    return true;
  }
  int subBrowserId = -1;
  
  if(json.HasMember("browser"))
    subBrowserId = json["browser"].GetInt();
  if ( m_browsers.find(subBrowserId) == m_browsers.end() )
  {
    m_browsers.emplace(subBrowserId,PhantomJSHandler::BrowserInfo());
  }
  auto& subBrowserInfo = m_browsers.at(subBrowserId);
  const auto& subBrowser = subBrowserInfo.browser;
  if (!subBrowser) {
    //qCWarning(handler) << "Unknown browser with id" << subBrowserId << "for request" << json;
    return false;
  }

  // below, all queries work on a browser
  if (type == "webPageSignals") {
    subBrowserInfo.signalCallback = callback;
    Q_ASSERT(persistent);
    return true;
  } else if (type == "openWebPage") {
    const auto url = QUrl::fromUserInput(json["url"].GetString(),
                                         json["libraryPath"].GetString(),
                                         QUrl::AssumeLocalFile);
    subBrowser->GetMainFrame()->LoadURL(url.toString().toStdString());
    m_waitForLoadedCallbacks.insert(std::pair<int32, CefRefPtr<CefMessageRouterBrowserSide::Callback> >(subBrowser->GetIdentifier(), callback));
    return true;
  } else if (type == "waitForLoaded") {
      m_waitForLoadedCallbacks.insert(std::pair<int32, CefRefPtr<CefMessageRouterBrowserSide::Callback> >(subBrowser->GetIdentifier(), callback));
    return true;
  } else if (type == "waitForDownload") {
    m_waitForLoadedCallbacks.insert(std::pair<int32, CefRefPtr<CefMessageRouterBrowserSide::Callback> >(subBrowser->GetIdentifier(), callback));
    return true;
  } else if (type =="stopWebPage") {
    subBrowser->StopLoad();
    callback->Success({});
    return true;
  } else if (type == "closeWebPage") {
    subBrowser->GetHost()->CloseBrowser(true);
    callback->Success({});
    return true;
  } else if (type == "evaluateJavaScript") {
    std::string code = json["code"].GetString();
    std::string url = "phantomjs://evaluateJavaScript";
    if(json.HasMember("url"))
      url = json["url"].GetString();
    int line = 1;
    if(json.HasMember("line"))
      line = json["line"].GetInt();
    std::string args = "[]";
    if(json.HasMember("args"))
      args = json["args"].GetString();
      
    if(m_pendingQueryCallbacks.find(query_id) == m_pendingQueryCallbacks.end())
    {
      m_pendingQueryCallbacks.insert(std::make_pair(query_id,callback));
    }
    else
      m_pendingQueryCallbacks[query_id] = callback;
    
    std::string tmpCode = "phantom.internal.handleEvaluateJavaScript(";
    tmpCode+= code;
    tmpCode+= ", " ;
    tmpCode+=args;
    tmpCode+=", ";
    tmpCode+=std::to_string(query_id);
    tmpCode+= ")";
    code = tmpCode;
    subBrowser->GetMainFrame()->ExecuteJavaScript(code ,url, line);
    return true;
  } else if (type == "setProperty") {
    const std::string name = json["name"].GetString();
    
    if (name == "viewportSize") {
      if(json["value"].IsObject())
      {
        const auto& value = json["value"].GetObject();
    
        int width = -1;
        int height = -1;
        if(value.HasMember("width"))
          width = value["width"].GetInt();
        if(value.HasMember("height"))
          height = value["height"].GetInt();
        if (width < 0 || height < 0) {
          callback->Failure(1, "Invalid viewport size.");
          return true;
        } else {
          const auto newSize = std::pair<int,int>(width, height);
          auto& oldSize = m_viewRects.at(subBrowserId);
          if (newSize != oldSize) {
            m_viewRects.at(subBrowserId) = newSize;
            subBrowser->GetHost()->WasResized();
          }
        }
      }
      
    } else if (name == "zoomFactor") {
      double value = 1.0;
      if(json.HasMember("value")  )
        value = json["value"].GetDouble();
      /// TODO: this doesn't seem to work
      subBrowser->GetHost()->SetZoomLevel(value);
    } else {
      callback->Failure(1, "unknown property: " + name);
      return true;
    }
    callback->Success({});
    return true;
  } else if (type == "renderImage") {
    const std::string path = json["path"].GetString();
    const std::string format = json["format"].GetString();
    const auto clipRectJson = json["clipRect"].GetObject();
    const auto clipRect = QRect(
      clipRectJson["left"].GetDouble(),
      clipRectJson["top"].GetDouble(),
      clipRectJson["width"].GetDouble(),
      clipRectJson["height"].GetDouble()
    );
    if(m_paintCallbacks.find(subBrowserId) == m_paintCallbacks.end())
    {
      m_paintCallbacks.insert(std::pair<int32,PaintInfo>(subBrowserId,{path, format, clipRect, callback}));
    }
    else
      m_paintCallbacks.at(subBrowserId) = {path, format, clipRect, callback};
    subBrowser->GetHost()->Invalidate(PET_VIEW);
    return true;
  } else if (type == "printPdf") {
    const std::string path = json["path"].GetString();
    CefPdfPrintSettings settings;
    const auto paperSize = json["paperSize"].GetObject();
    std::string pageOrientation = paperSize["orientation"].GetString(), strlandScape("LANDSCAPE");
    std::transform(pageOrientation.begin(),pageOrientation.end(),pageOrientation.end(),toupper);
    
    if (pageOrientation != strlandScape) {
      settings.landscape = true;
    }
    QPageSize pageSize;
    if (paperSize.HasMember("format")) {
      std::string paperSizeFormat = paperSize["format"].GetString();;
      pageSize = pageSizeForName(QString::fromStdString(paperSizeFormat));
    } else if (paperSize.HasMember("width") && paperSize.HasMember("height")) {
      auto width = stringToPointSize(QString::fromStdString(paperSize["width"].GetString()));
      auto height = stringToPointSize(QString::fromStdString(paperSize["height"].GetString()));
      pageSize = QPageSize(QSize(width, height), QPageSize::Point);
    }
    auto rect = pageSize.rect(QPageSize::Millimeter);
    settings.page_height = rect.height() * 1000;
    settings.page_width = rect.width() * 1000;

    const auto& margin = paperSize["margin"];
    if (margin.IsString()) {
      const std::string marginString = margin.GetString();
      if (marginString == "default") {
        settings.margin_type = PDF_PRINT_MARGIN_DEFAULT;
      } else if (marginString == "minimum") {
        settings.margin_type = PDF_PRINT_MARGIN_MINIMUM;
      } else if (marginString == "none") {
        settings.margin_type = PDF_PRINT_MARGIN_NONE;
      } else {
        settings.margin_type = PDF_PRINT_MARGIN_CUSTOM;
        int intMargin = stringToMillimeter(QString::fromStdString(marginString));
        settings.margin_left = intMargin;
        settings.margin_top = intMargin;
        settings.margin_right = intMargin;
        settings.margin_bottom = intMargin;
      }
    } else if (margin.IsObject()) {
      auto marginObject = margin.GetObject();
      settings.margin_type = PDF_PRINT_MARGIN_CUSTOM;
      settings.margin_left = stringToMillimeter(marginObject["left"].GetString());
      settings.margin_top = stringToMillimeter(marginObject["top"].GetString());
      settings.margin_right =stringToMillimeter(marginObject["right"].GetString());
      settings.margin_bottom = stringToMillimeter(marginObject["bottom"].GetString());
    }
  /*  qCDebug(print) << paperSize << pageSize.name() << settings.page_height << settings.page_width << "landscape:" << settings.landscape
                    << "margins:"<< settings.margin_bottom << settings.margin_left << settings.margin_top << settings.margin_right << "margin type:" << settings.margin_type;*/
    subBrowser->GetHost()->PrintToPDF(path, settings, makePdfPrintCallback([callback] (const CefString& path, bool success) {
      if (success) {
        callback->Success(path);
      } else {
        callback->Failure(1, std::string("failed to print to path ") + path.ToString());
      }
    }));
    return true;
  } else if (type == "sendEvent") {
    const std::string event = json["event"].GetString();
    const auto modifiers = json["modifiers"].GetInt();
    if (event == "keydown" || event == "keyup" || event == "keypress") {
      CefKeyEvent keyEvent;
      keyEvent.modifiers = modifiers;
      auto& arg1 = json["arg1"];
      if (arg1.IsString()) {
   	   // qCDebug(handler) << json << event << "string";
   	   std::string arg1Str = arg1.GetString();
        for (auto& c : arg1Str) {
          keyEvent.character = c;
          keyEvent.windows_key_code = c;
          keyEvent.native_key_code = c;
          if (event == "keydown") {
            keyEvent.type = KEYEVENT_KEYDOWN;
          } else if (event == "keyup") {
            keyEvent.type = KEYEVENT_KEYUP;
          } else {
            keyEvent.type = KEYEVENT_CHAR;
          }
          subBrowser->GetHost()->SendKeyEvent(keyEvent);
        }
      } else {
   	   // qCDebug(handler) << json << event << "char";
        if (event == "keydown") {
          keyEvent.type = KEYEVENT_KEYDOWN;
        } else if (event == "keyup") {
          keyEvent.type = KEYEVENT_KEYUP;
        } else {
          keyEvent.type = KEYEVENT_CHAR;
        }
        //qCDebug(handler) << "~~~~~" << arg1.toInt();
        keyEvent.windows_key_code = arg1.GetInt();
        keyEvent.native_key_code = vkToNative(keyEvent.native_key_code);
        keyEvent.character = arg1.GetInt();
        if (keyEvent.type != KEYEVENT_CHAR) {
          subBrowser->GetHost()->SendKeyEvent(keyEvent);
        } else {
          keyEvent.type = KEYEVENT_KEYDOWN;
          subBrowser->GetHost()->SendKeyEvent(keyEvent);
          keyEvent.type = KEYEVENT_CHAR;
          subBrowser->GetHost()->SendKeyEvent(keyEvent);
          keyEvent.type = KEYEVENT_KEYUP;
          subBrowser->GetHost()->SendKeyEvent(keyEvent);
        }
      }
    } else if (event == "click" || event == "doubleclick"
            || event == "mousedown" || event == "mouseup"
            || event == "mousemove")
    {
      CefMouseEvent mouseEvent;
      mouseEvent.modifiers = modifiers;
      if (!modifiers) {
        mouseEvent.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
      }
      mouseEvent.x = json["arg1"].GetDouble();
      mouseEvent.y = json["arg2"].GetDouble();
      cef_mouse_button_type_t type = MBT_LEFT;
      const std::string typeString = json["arg3"].GetString();
      if (typeString == "right") {
        type = MBT_RIGHT;
      } else if (typeString == "middle") {
        type = MBT_MIDDLE;
      }
      if (event == "doubleclick") {
        subBrowser->GetHost()->SendMouseClickEvent(mouseEvent, type, false, 2);
        subBrowser->GetHost()->SendMouseClickEvent(mouseEvent, type, true, 2);
      } else if (event == "click") {
        subBrowser->GetHost()->SendMouseClickEvent(mouseEvent, type, false, 1);
        subBrowser->GetHost()->SendMouseClickEvent(mouseEvent, type, true, 1);
      } else if (event == "mousemove") {
        subBrowser->GetHost()->SendMouseMoveEvent(mouseEvent, false);
      } else {
        subBrowser->GetHost()->SendMouseClickEvent(mouseEvent, type, event == "mouseup", 1);
      }
    } else {
      callback->Failure(1, "invalid event type passed to sendEvent: " + event);
      return true;
    }
    callback->Success({});
    return true;
  } else if (type == "download") {
    const auto source = json["source"].GetString();
    const auto target = json["target"].GetString();
    const std::string  sourceStr = source;
    const std::string  targetStr = target;
    if(m_downloadTargets.find(sourceStr) != m_downloadTargets.end())
    {
      m_downloadTargets.at(sourceStr) = {targetStr, callback};
    }
    else
      m_downloadTargets.emplace(std::pair<std::string,DownloadTargetInfo>(sourceStr,{targetStr, callback}));
    
    subBrowser->GetHost()->StartDownload(sourceStr);
    return true;
  }
  return false;
}

void PhantomJSHandler::OnQueryCanceled(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                       int64 query_id)
{
  CEF_REQUIRE_UI_THREAD();

  m_waitForLoadedCallbacks.erase(browser->GetIdentifier());
  m_pendingQueryCallbacks.erase(query_id);
  m_paintCallbacks.erase(browser->GetIdentifier());
}

void PhantomJSHandler::OnBeforeDownload(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> download_item, const CefString& suggested_name, CefRefPtr<CefBeforeDownloadCallback> callback)
{
  const auto source = download_item->GetOriginalUrl();
  const auto target = m_downloadTargets.at(source);
  m_downloadTargets.erase(source);

  //qCDebug(handler) << browser->GetIdentifier() << source << target.target;

  if (!target.callback) {
    // not a manually triggered page.download() call but an indirect one
    // call back to the user script to ask for a download location
    m_beforeDownloadCallbacks.at(download_item->GetId())= callback;

    emitSignal(browser, std::string("onBeforeDownload"), {
                QString::number(download_item->GetId()),
                QString::fromStdString(download_item->GetOriginalUrl())
              }, true);
    return;
  }

  m_downloadCallbacks.at(download_item->GetId()) = target.callback;
  callback->Continue(target.target, false);
}

void PhantomJSHandler::OnDownloadUpdated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> download_item, CefRefPtr<CefDownloadItemCallback> callback)
{
  qCDebug(handler) << browser->GetIdentifier() << download_item->GetURL() << download_item->GetPercentComplete() << download_item->IsComplete() << download_item->IsCanceled() << download_item->IsInProgress();
  
  if(m_downloadItemCallbacks.find(download_item->GetId()) == m_downloadItemCallbacks.end())
  {
     m_downloadItemCallbacks.emplace(std::pair<uint, CefRefPtr<CefDownloadItemCallback> >(download_item->GetId(), callback));
  }
  else
  {
    m_downloadItemCallbacks.at(download_item->GetId()) = callback;
  }
  
  const auto jsonDownloadItem = toJson(download_item);

  emitSignal(browser, std::string("onDownloadUpdated"),
             {QString::number(download_item->GetId()), jsonDownloadItem}, true);

  if (!download_item->IsInProgress()) {
    QByteArray data;
    std::string errorMessage;
    if (download_item->IsCanceled()) {
      errorMessage = "Download of " + download_item->GetURL().ToString() + " canceled.";
    } else if (!download_item->IsComplete()) {
      errorMessage = "Download of " + download_item->GetURL().ToString() + " ailed.";
    } else {
       data = QJsonDocument(jsonDownloadItem).toJson();
    }
    if(m_downloadCallbacks.find(download_item->GetId()) != m_downloadCallbacks.end())
    {
       auto callback = m_downloadCallbacks.at(download_item->GetId()); 
       if (callback) {
        if (errorMessage.empty()) {
          callback->Success(data.constData());
        } else {
          callback->Failure(download_item->IsCanceled() ? 0 : 1, errorMessage);
        }
      }
    }
    
    
    while (auto callback = takeCallback(&m_waitForDownloadCallbacks, browser)) {
      if (errorMessage.empty()) {
        callback->Success(data.constData());
      } else {
        callback->Failure(download_item->IsCanceled() ? 0 : 1, errorMessage);
      }
    }
  }
}
