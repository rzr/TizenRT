/* Copyright 2017-present Samsung Electronics Co., Ltd. and other contributors
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

#if !defined(__TIZENRT__)
#error "Module __FILE__ is for TizenRT only"
#endif

#include "modules/iotjs_module_uart.h"

bool iotjs_uart_open(iotjs_uart_t* uart) {
  int fd =
      open(iotjs_string_data(&uart->device_path), O_RDWR | O_NOCTTY | O_NDELAY);

  if (fd < 0) {
    return false;
  }

  uart->device_fd = fd;
  iotjs_uart_register_read_cb(uart);

  return true;
}

bool iotjs_uart_write(iotjs_uart_t* uart) {
  int bytesWritten = 0;
  unsigned offset = 0;
  int fd = uart->device_fd;
  const char* buf_data = iotjs_string_data(&uart->buf_data);

  DDDLOG("%s - data: %s", __func__, buf_data);

  do {
    errno = 0;
    bytesWritten = write(fd, buf_data + offset, uart->buf_len - offset);

    DDDLOG("%s - size: %d", __func__, uart->buf_len - offset);

    if (bytesWritten != -1) {
      offset += (unsigned)bytesWritten;
      continue;
    }

    if (errno == EINTR) {
      continue;
    }

    return false;

  } while (uart->buf_len > offset);

  return true;
}
