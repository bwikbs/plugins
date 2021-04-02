#ifndef FLUTTER_PLUGIN_WEBVIEW_FLUTTER_TIZEN_WEVIEW_H_
#define FLUTTER_PLUGIN_WEBVIEW_FLUTTER_TIZEN_WEVIEW_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar.h>
#include <flutter/standard_message_codec.h>
#include <flutter/standard_method_codec.h>
#include <flutter_platform_view.h>
#include <flutter_tizen_texture_registrar.h>
#include <tbm_surface.h>

#define USE_LWE

#if defined(USE_LWE)
namespace LWE {
class WebContainer;
}
#else
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Ecore_Wl2.h>
#include <Elementary.h>
#include <Evas.h>

#include "EWebKit_internal.h"
#include "EWebKit_product.h"
#endif

class TextInputChannel;

class WebView : public PlatformView {
 public:
  WebView(flutter::PluginRegistrar* registrar, int viewId,
          FlutterTextureRegistrar* textureRegistrar, double width,
          double height, flutter::EncodableMap& params, void* winHandle);
  ~WebView();
  virtual void Dispose() override;
  virtual void Resize(double width, double height) override;
  virtual void Touch(int type, int button, double x, double y, double dx,
                     double dy) override;
  virtual void SetDirection(int direction) override;
  virtual void ClearFocus() override;

  // Key input event
  virtual void DispatchKeyDownEvent(Ecore_Event_Key* key) override;
  virtual void DispatchKeyUpEvent(Ecore_Event_Key* key) override;
  virtual void DispatchCompositionUpdateEvent(const char* str,
                                              int size) override;
  virtual void DispatchCompositionEndEvent(const char* str, int size) override;

  virtual void SetSoftwareKeyboardContext(Ecore_IMF_Context* context) override;

#if defined(USE_LWE)
  LWE::WebContainer* GetWebViewInstance() { return webview_instance_; }
#else
  Evas_Object* GetWebViewInstance() { return webview_instance_; }
#endif
  void HidePanel();
  void ShowPanel();

 private:
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue>& method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
  void HandleCookieMethodCall(
      const flutter::MethodCall<flutter::EncodableValue>& method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
  std::string GetChannelName();
  void InitWebView();

  void RegisterJavaScriptChannelName(const std::string& name);
  void ApplySettings(flutter::EncodableMap);

  FlutterTextureRegistrar* texture_registrar_;
#if defined(USE_LWE)
  LWE::WebContainer* webview_instance_;
#else
  static void OnFrameRendered(void* data, Evas_Object* , void* buffer);
  static void OnLoadStarted(void* data, Evas_Object* , void* );
  static void OnLoadInProgress(void* data, Evas_Object* , void* );
  static void OnLoadFinished(void* data, Evas_Object* , void* );
  static void OnLoadError(void* data, Evas_Object* , void* rawError);
  static void OnUrlChanged(void* data, Evas_Object* , void* newUrl);
  static void OnConsoleMessage(void* , Evas_Object* , void* eventInfo);
  static void OnEdgeLeft(void* data, Evas_Object* , void* );
  static void OnEdgeRight(void* data, Evas_Object* , void* );
  static void OnEdgeTop(void* data, Evas_Object* , void* );
  static void OnEdgeBottom(void* data, Evas_Object* , void* );
  static void OnFormRepostDecisionRequest(void* data, Evas_Object*, void* eventInfo);
  static void OnEvaluateJavaScript(Evas_Object* o, const char* result, void* data);
  static void OnJavaScriptMessage(Evas_Object* o, Ewk_Script_Message message);
  static Eina_Bool OnJavaScriptAlert(Evas_Object* o, const char* alert_text, void*);
  static Eina_Bool OnJavaScriptConfirm(Evas_Object* o, const char* message, void*);
  static Eina_Bool OnJavaScriptPrompt(Evas_Object* o, const char* message, const char* default_value, void* );

  Evas_Object* webview_instance_;
#endif
  double width_;
  double height_;
#if defined(USE_LWE)
  tbm_surface_h tbm_surface_;
  bool is_mouse_lbutton_down_;
  Ecore_IMF_Context* context_;
#endif
  bool has_navigation_delegate_;
  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel_;
};

#endif  // FLUTTER_PLUGIN_WEBVIEW_FLUTTER_TIZEN_WEVIEW_H_
