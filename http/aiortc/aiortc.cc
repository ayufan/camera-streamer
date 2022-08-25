#define PY_SSIZE_T_CLEAN

extern "C" {
#include "http/http.h"
#include "device/buffer.h"
#include "device/buffer_list.h"
#include "device/device.h"
#include "opts/log.h"
#include "opts/fourcc.h"
#include "opts/control.h"
#include "aiortc.h"
};

#ifdef USE_PYTHON

extern "C" {
#include <Python.h>
};

#include <string>

extern "C" unsigned char http_aiortc_aiortc_py[];
extern "C" unsigned int http_aiortc_aiortc_py_len;

static pthread_t webrtc_thread;
static PyObject * webrtc_module;

extern "C" bool http_webrtc_call_function(const char *method_name)
{
  if (webrtc_module == NULL)
    return false;

  bool result = false;
  PyGILState_STATE gstate = PyGILState_Ensure();
  auto wants_frame = PyObject_GetAttrString(webrtc_module, method_name);
  if (wants_frame != NULL) {
    auto wants_frame_result = PyObject_CallNoArgs(wants_frame);
    if (wants_frame_result) {
       result = PyObject_IsTrue(wants_frame_result);
    } else if (PyErr_Occurred()) {
      PyErr_Print();
    }
  }
  PyGILState_Release(gstate);

  return result;
}

extern "C" bool http_webrtc_needs_buffer()
{
  return http_webrtc_call_function("wants_frame");
}

extern "C" void http_webrtc_capture(buffer_t *buf)
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  if (http_webrtc_needs_buffer()) {
    bool key_frame = h264_is_key_frame(buf);
    if (http_webrtc_call_function("request_key_frame")) {
      if (!key_frame) {
        device_video_force_key(buf->buf_list->dev);
        LOG_INFO(buf, "Request key frame from WebRTC");
      }
    }

    auto bytes = PyBytes_FromStringAndSize((const char *)buf->start, buf->used);
    auto push_frame = PyObject_GetAttrString(webrtc_module, "push_frame");
    if (push_frame != NULL) {
      PyObject_CallOneArg(push_frame, bytes);
    }
    if (PyErr_Occurred()) {
      PyErr_Print();
    }
    Py_DECREF(bytes);
  }
  PyGILState_Release(gstate);
}

static void *http_run_webrtc_thread(void *opaque)
{
  std::string code((const char*)http_aiortc_aiortc_py, http_aiortc_aiortc_py_len);

  Py_Initialize();
  PyEval_InitThreads();

  auto module = PyModule_New("webrtc");
  LOG_INFO(NULL, "module=%p", module);

  // Set properties on the new module object
  PyModule_AddStringConstant(module, "__file__", "");
  PyObject *localDict = PyModule_GetDict(module);   // Returns a borrowed reference: no need to Py_DECREF() it once we are done
  PyObject *builtins = PyEval_GetBuiltins();  // Returns a borrowed reference: no need to Py_DECREF() it once we are done
  PyDict_SetItemString(localDict, "__builtins__", builtins);

  // Define code in the newly created module
  webrtc_module = module;
  PyRun_String(code.c_str(), Py_file_input, localDict, localDict);
  if (PyErr_Occurred()) {
    PyErr_Print();
  }
  webrtc_module = NULL;

  Py_DECREF(module);
  // Py_FinalizeEx();
  LOG_INFO(NULL, "webrtc done");
  return NULL;
}

extern "C" void http_run_webrtc()
{
  pthread_create(&webrtc_thread, NULL, http_run_webrtc_thread, NULL);
}

#else // USE_PYTHON

extern "C" bool http_webrtc_needs_buffer()
{
  return false;
}

extern "C" void http_webrtc_capture(buffer_t *buf)
{
}

extern "C" void http_run_webrtc()
{
}

#endif // USE_PYTHON
