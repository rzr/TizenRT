/* Copyright 2016-present Samsung Electronics Co., Ltd. and other contributors
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

#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "modules/iotjs_module_uart.h"

static unsigned baud_to_constant(unsigned baudRate) {
  switch (baudRate) {
    case 50:
      return B50;
    case 75:
      return B75;
    case 110:
      return B110;
    case 134:
      return B134;
    case 150:
      return B150;
    case 200:
      return B200;
    case 300:
      return B300;
    case 600:
      return B600;
    case 1200:
      return B1200;
    case 1800:
      return B1800;
    case 2400:
      return B2400;
    case 4800:
      return B4800;
    case 9600:
      return B9600;
    case 19200:
      return B19200;
    case 38400:
      return B38400;
    case 57600:
      return B57600;
    case 115200:
      return B115200;
    case 230400:
      return B230400;
  }
  return B0;
}

static int databits_to_constant(int dataBits) {
  switch (dataBits) {
    case 8:
      return CS8;
    case 7:
      return CS7;
    case 6:
      return CS6;
    case 5:
      return CS5;
  }
  return -1;
}

bool iotjs_uart_open(iotjs_uart_t* uart) {
  int fd =
      open(iotjs_string_data(&uart->device_path), O_RDWR | O_NOCTTY | O_NDELAY);
  if (fd < 0) {
    return false;
  }

  struct termios options;
  tcgetattr(fd, &options);
  options.c_cflag = CLOCAL | CREAD;
  options.c_cflag |= (tcflag_t)baud_to_constant(uart->baud_rate);
  options.c_cflag |= (tcflag_t)databits_to_constant(uart->data_bits);
  options.c_iflag = IGNPAR;
  options.c_oflag = 0;
  options.c_lflag = 0;
  tcflush(fd, TCIFLUSH);
  tcsetattr(fd, TCSANOW, &options);

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
    tcdrain(fd);

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
