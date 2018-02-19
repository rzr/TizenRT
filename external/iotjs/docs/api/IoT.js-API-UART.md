### Platform Support

The following shows uart module APIs available for each platform.

|  | Linux<br/>(Ubuntu) | Raspbian<br/>(Raspberry Pi) | NuttX<br/>(STM32F4-Discovery) | TizenRT<br/>(Artik053) |
| :---: | :---: | :---: | :---: | :---: |
| uart.open             | O | O | O | O |
| uart.openSync         | O | O | O | O |
| uartport.write        | O | O | O | O |
| uartport.writeSync    | O | O | O | O |
| uartport.close        | O | O | X | O |
| uartport.closeSync    | O | O | X | O |

## Class: UART

The UART (Universal Asynchronous Receiver/Transmitter) class supports asynchronous serial communication.

### uart.open(configuration, callback)
* `configuration` {Object}
  * `device` {string} Mandatory configuration.
  * `baudRate` {number} Specifies how fast data is sent over a serial line. **Default:** `9600`.
  * `dataBits` {number} Number of data bits that are being transmitted. **Default:** `8`.
* `callback` {Function}.
  * `err` {Error|null}.
* Returns: {UARTPort}.

Opens an UARTPort object with the specified configuration.

The `baudRate` must be equal to one of these values: [50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400].

The `dataBits` must be equal to one of these values: [5, 6, 7, 8].

On NuttX, you also need to set the properties of the `configuration` in the NuttX configuration file. Using the NuttX menuconfig, it can be found at the `Device Drivers -> Serial Driver Support -> U[S]ART(N) Configuration` section.

You can read more information about the usage of the UART on stm32f4-discovery board: [STM32F4-discovery](../targets/nuttx/stm32f4dis/IoT.js-API-Stm32f4dis.md#uart).

**Example**

```js
var uart = require('uart');

var configuration = {
  device: '/dev/ttyUSB0'
  baudRate: 115200,
  dataBits: 8,
}

var serial = uart.open(configuration, function(err) {
  // Do something.
});

serial.closeSync();

```

### uart.openSync(configuration)
* `configuration` {Object}
  * `device` {string} Mandatory configuration.
  * `baudRate` {number} Specifies how fast data is sent over a serial line. **Default:** `9600`.
  * `dataBits` {number} Number of data bits that are being transmitted. **Default:** `8`.
* Returns: {UARTPort}.

Opens an UARTPort object with the specified configuration.

**Example**

```js
var uart = require('uart');

var configuration = {
  device: '/dev/ttyUSB0'
  baudRate: 115200,
  dataBits: 8,
}

var serial = uart.openSync(configuration);

serial.closeSync();

```

## Class: UARTPort
The UARTPort class is responsible for transmitting and receiving serial data.

### uartport.write(data, callback).
* `data` {string}.
* `callback` {Function}.
  * `err` {Error|null}.

Writes the given `data` to the UART device asynchronously.

**Example**

```js
var serial = uart.openSync({device: '/dev/ttyUSB0'});

serial.write('Hello?', function(err) {
  if (err) {
    // Do something.
  }
  serial.closeSync();
});

```

### uartport.writeSync(data)
* `data` {string}.

Writes the given `data` to the UART device synchronously.

**Example**

```js
var serial = uart.openSync({device: '/dev/ttyUSB0'});
serial.writeSync('Hello?');
serial.closeSync();

```

### uartport.close([callback])
* `callback` {Function}.
  * `err` {Error|null)}.

Closes the UART device asynchronously.

On NuttX/STM32F4Discovery, Uart.close() blocks after close().
It seems that poll() does not work properly on NuttX for some cases.

### uartport.closeSync()

Closes the UART device synchronously.

On NuttX/STM32F4Discovery, Uart.close() blocks after close().
It seems that poll() does not work properly on NuttX for some cases.

### Event: 'data'
* `callback` {Function}
  * `data` {string} A string from the sender.

**Example**

```js

/* ... */

serial.on('data', function(data) {
  console.log('read result: ' + data.toString());
});

```
