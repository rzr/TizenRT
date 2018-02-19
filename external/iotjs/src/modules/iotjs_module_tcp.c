/* Copyright 2015-present Samsung Electronics Co., Ltd. and other contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "iotjs_def.h"
#include "iotjs_module_tcp.h"

#include "iotjs_handlewrap.h"
#include "iotjs_module_buffer.h"
#include "iotjs_reqwrap.h"


IOTJS_DEFINE_NATIVE_HANDLE_INFO_THIS_MODULE(tcpwrap);


iotjs_tcpwrap_t* iotjs_tcpwrap_create(jerry_value_t jtcp) {
  iotjs_tcpwrap_t* tcpwrap = IOTJS_ALLOC(iotjs_tcpwrap_t);

  iotjs_handlewrap_initialize(&tcpwrap->handlewrap, jtcp,
                              (uv_handle_t*)(&tcpwrap->handle),
                              &this_module_native_info);

  const iotjs_environment_t* env = iotjs_environment_get();
  uv_tcp_init(iotjs_environment_loop(env), &tcpwrap->handle);

  return tcpwrap;
}


static void iotjs_tcpwrap_destroy(iotjs_tcpwrap_t* tcpwrap) {
  iotjs_handlewrap_destroy(&tcpwrap->handlewrap);
  IOTJS_RELEASE(tcpwrap);
}


iotjs_tcpwrap_t* iotjs_tcpwrap_from_handle(uv_tcp_t* tcp_handle) {
  uv_handle_t* handle = (uv_handle_t*)(tcp_handle);
  iotjs_handlewrap_t* handlewrap = iotjs_handlewrap_from_handle(handle);
  iotjs_tcpwrap_t* tcpwrap = (iotjs_tcpwrap_t*)handlewrap;
  IOTJS_ASSERT(iotjs_tcpwrap_tcp_handle(tcpwrap) == tcp_handle);
  return tcpwrap;
}


iotjs_tcpwrap_t* iotjs_tcpwrap_from_jobject(jerry_value_t jtcp) {
  iotjs_handlewrap_t* handlewrap = iotjs_handlewrap_from_jobject(jtcp);
  return (iotjs_tcpwrap_t*)handlewrap;
}


uv_tcp_t* iotjs_tcpwrap_tcp_handle(iotjs_tcpwrap_t* tcpwrap) {
  uv_handle_t* handle = iotjs_handlewrap_get_uv_handle(&tcpwrap->handlewrap);
  return (uv_tcp_t*)handle;
}


static void iotjs_connect_reqwrap_destroy(
    iotjs_connect_reqwrap_t* connect_reqwrap);


iotjs_connect_reqwrap_t* iotjs_connect_reqwrap_create(jerry_value_t jcallback) {
  iotjs_connect_reqwrap_t* connect_reqwrap =
      IOTJS_ALLOC(iotjs_connect_reqwrap_t);
  iotjs_reqwrap_initialize(&connect_reqwrap->reqwrap, jcallback,
                           (uv_req_t*)&connect_reqwrap->req);
  return connect_reqwrap;
}


static void iotjs_connect_reqwrap_destroy(
    iotjs_connect_reqwrap_t* connect_reqwrap) {
  iotjs_reqwrap_destroy(&connect_reqwrap->reqwrap);
  IOTJS_RELEASE(connect_reqwrap);
}


static void iotjs_write_reqwrap_destroy(iotjs_write_reqwrap_t* write_reqwrap);


iotjs_write_reqwrap_t* iotjs_write_reqwrap_create(jerry_value_t jcallback) {
  iotjs_write_reqwrap_t* write_reqwrap = IOTJS_ALLOC(iotjs_write_reqwrap_t);
  iotjs_reqwrap_initialize(&write_reqwrap->reqwrap, jcallback,
                           (uv_req_t*)&write_reqwrap->req);
  return write_reqwrap;
}


static void iotjs_write_reqwrap_destroy(iotjs_write_reqwrap_t* write_reqwrap) {
  iotjs_reqwrap_destroy(&write_reqwrap->reqwrap);
  IOTJS_RELEASE(write_reqwrap);
}

static void iotjs_shutdown_reqwrap_destroy(
    iotjs_shutdown_reqwrap_t* shutdown_reqwrap);


iotjs_shutdown_reqwrap_t* iotjs_shutdown_reqwrap_create(
    jerry_value_t jcallback) {
  iotjs_shutdown_reqwrap_t* shutdown_reqwrap =
      IOTJS_ALLOC(iotjs_shutdown_reqwrap_t);
  iotjs_reqwrap_initialize(&shutdown_reqwrap->reqwrap, jcallback,
                           (uv_req_t*)&shutdown_reqwrap->req);
  return shutdown_reqwrap;
}


static void iotjs_shutdown_reqwrap_destroy(
    iotjs_shutdown_reqwrap_t* shutdown_reqwrap) {
  iotjs_reqwrap_destroy(&shutdown_reqwrap->reqwrap);
  IOTJS_RELEASE(shutdown_reqwrap);
}


JS_FUNCTION(TCP) {
  DJS_CHECK_THIS();

  jerry_value_t jtcp = JS_GET_THIS();
  iotjs_tcpwrap_create(jtcp);
  return jerry_create_undefined();
}


// Socket close result handler.
void AfterClose(uv_handle_t* handle) {
  iotjs_handlewrap_t* wrap = iotjs_handlewrap_from_handle(handle);

  // tcp object.
  jerry_value_t jtcp = iotjs_handlewrap_jobject(wrap);

  // callback function.
  jerry_value_t jcallback =
      iotjs_jval_get_property(jtcp, IOTJS_MAGIC_STRING_ONCLOSE);
  if (jerry_value_is_function(jcallback)) {
    iotjs_make_callback(jcallback, jerry_create_undefined(),
                        iotjs_jargs_get_empty());
  }
  jerry_release_value(jcallback);
}


// Close socket
JS_FUNCTION(Close) {
  JS_DECLARE_THIS_PTR(handlewrap, wrap);

  // close uv handle, `AfterClose` will be called after socket closed.
  iotjs_handlewrap_close(wrap, AfterClose);
  return jerry_create_undefined();
}


// Socket binding, this function would be called from server socket before
// start listening.
// [0] address
// [1] port
JS_FUNCTION(Bind) {
  JS_DECLARE_THIS_PTR(tcpwrap, tcp_wrap);

  DJS_CHECK_ARGS(2, string, number);

  iotjs_string_t address = JS_GET_ARG(0, string);
  int port = JS_GET_ARG(1, number);

  sockaddr_in addr;
  int err = uv_ip4_addr(iotjs_string_data(&address), port, &addr);

  if (err == 0) {
    err = uv_tcp_bind(iotjs_tcpwrap_tcp_handle(tcp_wrap),
                      (const sockaddr*)(&addr), 0);
  }

  iotjs_string_destroy(&address);

  return jerry_create_number(err);
}


// Connection request result handler.
static void AfterConnect(uv_connect_t* req, int status) {
  iotjs_connect_reqwrap_t* req_wrap = (iotjs_connect_reqwrap_t*)(req->data);
  IOTJS_ASSERT(req_wrap != NULL);

  // Take callback function object.
  // function afterConnect(status)
  jerry_value_t jcallback = iotjs_reqwrap_jcallback(&req_wrap->reqwrap);
  IOTJS_ASSERT(jerry_value_is_function(jcallback));

  // Only parameter is status code.
  iotjs_jargs_t args = iotjs_jargs_create(1);
  iotjs_jargs_append_number(&args, status);

  // Make callback.
  iotjs_make_callback(jcallback, jerry_create_undefined(), &args);

  // Destroy args
  iotjs_jargs_destroy(&args);

  // Release request wrapper.
  iotjs_connect_reqwrap_destroy(req_wrap);
}


// Create a connection using the socket.
// [0] address
// [1] port
// [2] callback
JS_FUNCTION(Connect) {
  JS_DECLARE_THIS_PTR(tcpwrap, tcp_wrap);

  DJS_CHECK_ARGS(3, string, number, function);

  iotjs_string_t address = JS_GET_ARG(0, string);
  int port = JS_GET_ARG(1, number);
  jerry_value_t jcallback = JS_GET_ARG(2, function);

  sockaddr_in addr;
  int err = uv_ip4_addr(iotjs_string_data(&address), port, &addr);

  if (err == 0) {
    // Create connection request wrapper.
    iotjs_connect_reqwrap_t* req_wrap = iotjs_connect_reqwrap_create(jcallback);

    // Create connection request.
    err = uv_tcp_connect(&req_wrap->req, iotjs_tcpwrap_tcp_handle(tcp_wrap),
                         (const sockaddr*)(&addr), AfterConnect);

    if (err) {
      iotjs_connect_reqwrap_destroy(req_wrap);
    }
  }

  iotjs_string_destroy(&address);

  return jerry_create_number(err);
}


// A client socket wants to connect to this server.
// Parameters:
//   * uv_stream_t* handle - server handle
//   * int status - status code
static void OnConnection(uv_stream_t* handle, int status) {
  // Server tcp wrapper.
  iotjs_tcpwrap_t* tcp_wrap = iotjs_tcpwrap_from_handle((uv_tcp_t*)handle);

  // Tcp object
  jerry_value_t jtcp = iotjs_handlewrap_jobject(&tcp_wrap->handlewrap);

  // `onconnection` callback.
  jerry_value_t jonconnection =
      iotjs_jval_get_property(jtcp, IOTJS_MAGIC_STRING_ONCONNECTION);
  IOTJS_ASSERT(jerry_value_is_function(jonconnection));

  // The callback takes two parameter
  // [0] status
  // [1] client tcp object
  iotjs_jargs_t args = iotjs_jargs_create(2);
  iotjs_jargs_append_number(&args, status);

  if (status == 0) {
    // Create client socket handle wrapper.
    jerry_value_t jcreate_tcp =
        iotjs_jval_get_property(jtcp, IOTJS_MAGIC_STRING_CREATETCP);
    IOTJS_ASSERT(jerry_value_is_function(jcreate_tcp));

    jerry_value_t jclient_tcp =
        iotjs_jhelper_call_ok(jcreate_tcp, jerry_create_undefined(),
                              iotjs_jargs_get_empty());
    IOTJS_ASSERT(jerry_value_is_object(jclient_tcp));

    iotjs_tcpwrap_t* tcp_wrap_client =
        (iotjs_tcpwrap_t*)(iotjs_jval_get_object_native_handle(jclient_tcp));

    uv_stream_t* client_handle =
        (uv_stream_t*)(iotjs_tcpwrap_tcp_handle(tcp_wrap_client));

    int err = uv_accept(handle, client_handle);
    if (err) {
      iotjs_jargs_destroy(&args);
      return;
    }

    iotjs_jargs_append_jval(&args, jclient_tcp);
    jerry_release_value(jcreate_tcp);
    jerry_release_value(jclient_tcp);
  }

  iotjs_make_callback(jonconnection, jtcp, &args);

  jerry_release_value(jonconnection);
  iotjs_jargs_destroy(&args);
}


JS_FUNCTION(Listen) {
  JS_DECLARE_THIS_PTR(tcpwrap, tcp_wrap);
  DJS_CHECK_ARGS(1, number);

  int backlog = JS_GET_ARG(0, number);

  int err = uv_listen((uv_stream_t*)(iotjs_tcpwrap_tcp_handle(tcp_wrap)),
                      backlog, OnConnection);

  return jerry_create_number(err);
}


void AfterWrite(uv_write_t* req, int status) {
  iotjs_write_reqwrap_t* req_wrap = (iotjs_write_reqwrap_t*)(req->data);
  iotjs_tcpwrap_t* tcp_wrap = (iotjs_tcpwrap_t*)(req->handle->data);
  IOTJS_ASSERT(req_wrap != NULL);
  IOTJS_ASSERT(tcp_wrap != NULL);

  // Take callback function object.
  jerry_value_t jcallback = iotjs_reqwrap_jcallback(&req_wrap->reqwrap);

  // Only parameter is status code.
  iotjs_jargs_t args = iotjs_jargs_create(1);
  iotjs_jargs_append_number(&args, status);

  // Make callback.
  iotjs_make_callback(jcallback, jerry_create_undefined(), &args);

  // Destroy args
  iotjs_jargs_destroy(&args);

  // Release request wrapper.
  iotjs_write_reqwrap_destroy(req_wrap);
}


JS_FUNCTION(Write) {
  JS_DECLARE_THIS_PTR(tcpwrap, tcp_wrap);
  DJS_CHECK_ARGS(2, object, function);

  const jerry_value_t jbuffer = JS_GET_ARG(0, object);
  iotjs_bufferwrap_t* buffer_wrap = iotjs_bufferwrap_from_jbuffer(jbuffer);
  size_t len = iotjs_bufferwrap_length(buffer_wrap);

  uv_buf_t buf;
  buf.base = buffer_wrap->buffer;
  buf.len = len;

  jerry_value_t arg1 = JS_GET_ARG(1, object);
  iotjs_write_reqwrap_t* req_wrap = iotjs_write_reqwrap_create(arg1);

  int err = uv_write(&req_wrap->req,
                     (uv_stream_t*)(iotjs_tcpwrap_tcp_handle(tcp_wrap)), &buf,
                     1, AfterWrite);

  if (err) {
    iotjs_write_reqwrap_destroy(req_wrap);
  }

  return jerry_create_number(err);
}


void OnAlloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  if (suggested_size > IOTJS_MAX_READ_BUFFER_SIZE) {
    suggested_size = IOTJS_MAX_READ_BUFFER_SIZE;
  }

  buf->base = iotjs_buffer_allocate(suggested_size);
  buf->len = suggested_size;
}


void OnRead(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
  iotjs_tcpwrap_t* tcp_wrap = iotjs_tcpwrap_from_handle((uv_tcp_t*)handle);

  // tcp handle
  jerry_value_t jtcp = iotjs_handlewrap_jobject(&tcp_wrap->handlewrap);

  // socket object
  jerry_value_t jsocket =
      iotjs_jval_get_property(jtcp, IOTJS_MAGIC_STRING_OWNER);
  IOTJS_ASSERT(jerry_value_is_object(jsocket));

  // onread callback
  jerry_value_t jonread =
      iotjs_jval_get_property(jtcp, IOTJS_MAGIC_STRING_ONREAD);
  IOTJS_ASSERT(jerry_value_is_function(jonread));

  iotjs_jargs_t jargs = iotjs_jargs_create(4);
  iotjs_jargs_append_jval(&jargs, jsocket);
  iotjs_jargs_append_number(&jargs, nread);
  iotjs_jargs_append_bool(&jargs, false);

  if (nread <= 0) {
    if (buf->base != NULL) {
      iotjs_buffer_release(buf->base);
    }
    if (nread < 0) {
      if (nread == UV__EOF) {
        iotjs_jargs_replace(&jargs, 2, jerry_create_boolean(true));
      }

      iotjs_make_callback(jonread, jerry_create_undefined(), &jargs);
    }
  } else {
    jerry_value_t jbuffer = iotjs_bufferwrap_create_buffer((size_t)nread);
    iotjs_bufferwrap_t* buffer_wrap = iotjs_bufferwrap_from_jbuffer(jbuffer);

    iotjs_bufferwrap_copy(buffer_wrap, buf->base, (size_t)nread);

    iotjs_jargs_append_jval(&jargs, jbuffer);
    iotjs_make_callback(jonread, jerry_create_undefined(), &jargs);

    jerry_release_value(jbuffer);
    iotjs_buffer_release(buf->base);
  }

  iotjs_jargs_destroy(&jargs);
  jerry_release_value(jonread);
  jerry_release_value(jsocket);
}


JS_FUNCTION(ReadStart) {
  JS_DECLARE_THIS_PTR(tcpwrap, tcp_wrap);

  int err = uv_read_start((uv_stream_t*)(iotjs_tcpwrap_tcp_handle(tcp_wrap)),
                          OnAlloc, OnRead);

  return jerry_create_number(err);
}


static void AfterShutdown(uv_shutdown_t* req, int status) {
  iotjs_shutdown_reqwrap_t* req_wrap = (iotjs_shutdown_reqwrap_t*)(req->data);
  iotjs_tcpwrap_t* tcp_wrap = (iotjs_tcpwrap_t*)(req->handle->data);
  IOTJS_ASSERT(req_wrap != NULL);
  IOTJS_ASSERT(tcp_wrap != NULL);

  // function onShutdown(status)
  jerry_value_t jonshutdown = iotjs_reqwrap_jcallback(&req_wrap->reqwrap);
  IOTJS_ASSERT(jerry_value_is_function(jonshutdown));

  iotjs_jargs_t args = iotjs_jargs_create(1);
  iotjs_jargs_append_number(&args, status);

  iotjs_make_callback(jonshutdown, jerry_create_undefined(), &args);

  iotjs_jargs_destroy(&args);

  iotjs_shutdown_reqwrap_destroy(req_wrap);
}


JS_FUNCTION(Shutdown) {
  JS_DECLARE_THIS_PTR(tcpwrap, tcp_wrap);

  DJS_CHECK_ARGS(1, function);

  jerry_value_t arg0 = JS_GET_ARG(0, object);
  iotjs_shutdown_reqwrap_t* req_wrap = iotjs_shutdown_reqwrap_create(arg0);

  int err = uv_shutdown(&req_wrap->req,
                        (uv_stream_t*)(iotjs_tcpwrap_tcp_handle(tcp_wrap)),
                        AfterShutdown);

  if (err) {
    iotjs_shutdown_reqwrap_destroy(req_wrap);
  }

  return jerry_create_number(err);
}


// Enable/Disable keepalive option.
// [0] enable
// [1] delay
JS_FUNCTION(SetKeepAlive) {
  JS_DECLARE_THIS_PTR(tcpwrap, tcp_wrap);

  DJS_CHECK_ARGS(2, number, number);

  int enable = JS_GET_ARG(0, number);
  unsigned delay = JS_GET_ARG(1, number);

  int err = uv_tcp_keepalive(iotjs_tcpwrap_tcp_handle(tcp_wrap), enable, delay);

  return jerry_create_number(err);
}

JS_FUNCTION(ErrName) {
  DJS_CHECK_THIS();
  DJS_CHECK_ARGS(1, number);

  int errorcode = JS_GET_ARG(0, number);
  return jerry_create_string_from_utf8(
      (const jerry_char_t*)uv_err_name(errorcode));
}

// used in iotjs_module_udp.cpp
void AddressToJS(jerry_value_t obj, const sockaddr* addr) {
  char ip[INET6_ADDRSTRLEN];
  const sockaddr_in* a4;
  const sockaddr_in6* a6;
  int port;

  switch (addr->sa_family) {
    case AF_INET6: {
      a6 = (const sockaddr_in6*)(addr);
      uv_inet_ntop(AF_INET6, &a6->sin6_addr, ip, sizeof ip);
      port = ntohs(a6->sin6_port);
      iotjs_jval_set_property_string_raw(obj, IOTJS_MAGIC_STRING_ADDRESS, ip);
      iotjs_jval_set_property_string_raw(obj, IOTJS_MAGIC_STRING_FAMILY,
                                         IOTJS_MAGIC_STRING_IPV6);
      iotjs_jval_set_property_number(obj, IOTJS_MAGIC_STRING_PORT, port);
      break;
    }

    case AF_INET: {
      a4 = (const sockaddr_in*)(addr);
      uv_inet_ntop(AF_INET, &a4->sin_addr, ip, sizeof ip);
      port = ntohs(a4->sin_port);
      iotjs_jval_set_property_string_raw(obj, IOTJS_MAGIC_STRING_ADDRESS, ip);
      iotjs_jval_set_property_string_raw(obj, IOTJS_MAGIC_STRING_FAMILY,
                                         IOTJS_MAGIC_STRING_IPV4);
      iotjs_jval_set_property_number(obj, IOTJS_MAGIC_STRING_PORT, port);
      break;
    }

    default: {
      iotjs_jval_set_property_string_raw(obj, IOTJS_MAGIC_STRING_ADDRESS, "");
      break;
    }
  }
}


JS_FUNCTION(GetSockeName) {
  DJS_CHECK_ARGS(1, object);

  iotjs_tcpwrap_t* wrap = iotjs_tcpwrap_from_jobject(JS_GET_THIS());
  IOTJS_ASSERT(wrap != NULL);

  sockaddr_storage storage;
  int addrlen = sizeof(storage);
  sockaddr* const addr = (sockaddr*)(&storage);
  int err = uv_tcp_getsockname(iotjs_tcpwrap_tcp_handle(wrap), addr, &addrlen);
  if (err == 0)
    AddressToJS(JS_GET_ARG(0, object), addr);
  return jerry_create_number(err);
}

jerry_value_t InitTcp() {
  jerry_value_t tcp = jerry_create_external_function(TCP);

  jerry_value_t prototype = jerry_create_object();

  iotjs_jval_set_property_jval(tcp, IOTJS_MAGIC_STRING_PROTOTYPE, prototype);
  iotjs_jval_set_method(tcp, IOTJS_MAGIC_STRING_ERRNAME, ErrName);

  iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_CLOSE, Close);
  iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_CONNECT, Connect);
  iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_BIND, Bind);
  iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_LISTEN, Listen);
  iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_WRITE, Write);
  iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_READSTART, ReadStart);
  iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_SHUTDOWN, Shutdown);
  iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_SETKEEPALIVE,
                        SetKeepAlive);
  iotjs_jval_set_method(prototype, IOTJS_MAGIC_STRING_GETSOCKNAME,
                        GetSockeName);

  jerry_release_value(prototype);

  return tcp;
}
