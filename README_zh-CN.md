[![English](https://img.shields.io/badge/lang-English-blue.svg)](README.md)
[![简体中文](https://img.shields.io/badge/lang-简体中文-red.svg)](README_zh-CN.md)

[![arduino-library-badge](https://www.ardu-badge.com/badge/ArduinoWebsockets.svg?)](https://www.ardu-badge.com/ArduinoWebsockets)  [![Build Status](https://travis-ci.org/gilmaimon/ArduinoWebsockets.svg?branch=master)](https://travis-ci.org/gilmaimon/ArduinoWebsockets)

# Arduino WebSockets

一个被优化了的用于使用 Arduino 编写现代 WebSocket 应用程序的库（支持的平台见[前提条件](https://github.com/gilmaimon/ArduinoWebsockets#prerequisites)）。本项目基于我的另一个项目 [TinyWebsockets](https://github.com/gilmaimon/TinyWebsockets)。

该库为 WebSocket 开发（客户端和服务端）提供了简单易懂的接口。请参阅[基本用法](#基本用法)指南和[完整示例](#完整示例)。

### 更多详情请查阅 [TinyWebsockets Wiki](https://github.com/gilmaimon/TinyWebsockets/wiki)！

## 快速开始
本节将帮助您开始使用该库。如有任何疑问，欢迎提交 Issue。

### 前提条件
目前（0.5.* 版本），该库仅支持 `ESP8266`、`ESP32` 和 `Teensy 4.1`。

### 安装
您可以通过 Arduino IDE 安装该库，或使用 [Github 发布页面](https://github.com/gilmaimon/ArduinoWebsockets/releases) 的 ZIP 压缩包进行安装。
详细说明请见[此处](https://www.ardu-badge.com/ArduinoWebsockets)。

## 基本用法

### 客户端

创建客户端并连接到服务端：
```c++
WebsocketsClient client;
client.connect("ws://your-server-ip:port/uri");
```

发送消息：
```c++
client.send("Hello Server!");
```

接收消息：
```c++
client.onMessage([](WebsocketsMessage msg){
    Serial.println("Got Message: " + msg.data());
});
```

为了持续接收消息，您应该：
```c++
void loop() {
    client.poll();
}
```

### 服务端

创建服务端并监听连接：
```c++
WebsocketsServer server;
server.listen(8080);
```

接受连接：
```c++
WebsocketsClient client = server.accept();
// 像前面描述的那样处理客户端 :)
```

## 完整示例

### 客户端

```c++
#include <ArduinoWebsockets.h>
#include <ESP8266WiFi.h>

const char* ssid = "ssid"; // 输入 SSID
const char* password = "password"; // 输入密码
const char* websockets_server = "www.myserver.com:8080"; // 服务器地址和端口

using namespace websockets;

void onMessageCallback(WebsocketsMessage message) {
    Serial.print("Got Message: ");
    Serial.println(message.data());
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Connnection Opened");
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Connnection Closed");
    } else if(event == WebsocketsEvent::GotPing) {
        Serial.println("Got a Ping!");
    } else if(event == WebsocketsEvent::GotPong) {
        Serial.println("Got a Pong!");
    }
}

WebsocketsClient client;
void setup() {
    Serial.begin(115200);
    // 连接 WiFi
    WiFi.begin(ssid, password);

    // 等待一段时间以连接 WiFi
    for(int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
        Serial.print(".");
        delay(1000);
    }

    // 设置回调
    client.onMessage(onMessageCallback);
    client.onEvent(onEventsCallback);
    
    // 连接到服务器
    client.connect(websockets_server);

    // 发送消息
    client.send("Hi Server!");
    // 发送 Ping
    client.ping();
}

void loop() {
    client.poll();
}
```
***注意：** 对于 ESP32，您只需更改连接 WiFi 的代码（将 `#include <ESP8266WiFi.h>` 替换为 `#include <WiFi.h>`），其余部分保持不变。*

### 服务端
```c++
#include <ArduinoWebsockets.h>
#include <ESP8266WiFi.h>

const char* ssid = "ssid"; // 输入 SSID
const char* password = "password"; // 输入密码

using namespace websockets;

WebsocketsServer server;
void setup() {
  Serial.begin(115200);
  // 连接 WiFi
  WiFi.begin(ssid, password);

  // 等待一段时间以连接 WiFi
  for(int i = 0; i < 15 && WiFi.status() != WL_CONNECTED; i++) {
      Serial.print(".");
      delay(1000);
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());   // 您可以获取分配给 ESP 的 IP 地址

  server.listen(80);
  Serial.print("Is server live? ");
  Serial.println(server.available());
}

void loop() {
  auto client = server.accept();
  if(client.available()) {
    auto msg = client.readBlocking();

    // 日志
    Serial.print("Got Message: ");
    Serial.println(msg.data());

    // 返回回显
    client.send("Echo: " + msg.data());

    // 关闭连接
    client.close();
  }
  
  delay(1000);
}
```
***注意：** 对于 ESP32，您只需更改连接 WiFi 的代码（将 `#include <ESP8266WiFi.h>` 替换为 `#include <WiFi.h>`），其余部分保持不变。*

## 二进制数据

对于二进制数据，建议使用返回 `std::string` 的 `msg.rawData()`，或返回 `const char*` 的 `msg.c_str()`。
原因是 `msg.data()` 返回的是 Arduino 的 `String` 对象，它适用于串口打印和非常基础的内存处理，但不适用于大多数二进制数据场景。

更多信息请参见 [issue #32](https://github.com/gilmaimon/ArduinoWebsockets/issues/32)。

## SSL 和 WSS 支持

无论您使用哪种开发板，要使用 WSS（基于 SSL 的 WebSocket），都需要使用：
```c++
client.connect("wss://your-secured-server-ip:port/uri");
```

以下部分描述了在该库中使用 WSS 的特定开发板代码。

### ESP8266
在 ESP8266 上有多种使用 WSS 的方法。默认情况下，`ArduinoWebsockets` 不验证证书链。可以使用以下方法显式设置：
```c++
client.setInsecure();
```

您也可以使用 `SSL 指纹` 来验证 SSL 连接，例如：
```c++
const char ssl_fingerprint[] PROGMEM = "D5 07 4D 79 B2 D2 53 D7 74 E6 1B 46 C5 86 4E FE AD 00 F1 98";

client.setFingerprint(ssl_fingerprint);
```

或者您可以使用 `setKnownKey()` 方法指定证书的公钥，以验证您连接的服务端。
```c++
PublicKey *publicKey = new PublicKey(public_key);
client.setKnownKey(publicKey);
```
或者您可以使用 `setTrustAnchors` 方法指定证书颁发机构 (CA)，如下所示：
```c++
X509List *serverTrustedCA = new X509List(ca_cert);
client.setTrustAnchors(serverTrustedCA);
```

对于客户端证书验证，您可以使用 `setClientRSACert` 或 `setClientECCert` 方法来使用 RSA 或 EC 证书。

### ESP32
在 ESP32 上，您可以提供完整证书，也可以不提供证书。设置 CA 证书的示例：
```c++
const char ssl_ca_cert[] PROGMEM = \
    "-----BEGIN CERTIFICATE-----\n" \
    "MIIEkjCCA3qgAwIBAgIQCgFBQgAAAVOFc2oLheynCDANBgkqhkiG9w0BAQsFADA/\n" \
    "MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n" \
    "DkRTVCBSb290IENBIFgzMB4XDTE2MDMxNzE2NDA0NloXDTIxMDMxNzE2NDA0Nlow\n" \
    "SjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxIzAhBgNVBAMT\n" \
    "GkxldCdzIEVuY3J5cHQgQXV0aG9yaXR5IFgzMIIBIjANBgkqhkiG9w0BAQEFAAOC\n" \
    "AQ8AMIIBCgKCAQEAnNMM8FrlLke3cl03g7NoYzDq1zUmGSXhvb418XCSL7e4S0EF\n" \
    "q6meNQhY7LEqxGiHC6PjdeTm86dicbp5gWAf15Gan/PQeGdxyGkOlZHP/uaZ6WA8\n" \
    "SMx+yk13EiSdRxta67nsHjcAHJyse6cF6s5K671B5TaYucv9bTyWaN8jKkKQDIZ0\n" \
    "Z8h/pZq4UmEUEz9l6YKHy9v6Dlb2honzhT+Xhq+w3Brvaw2VFn3EK6BlspkENnWA\n" \
    "a6xK8xuQSXgvopZPKiAlKQTGdMDQMc2PMTiVFrqoM7hD8bEfwzB/onkxEz0tNvjj\n" \
    "/PIzark5McWvxI0NHWQWM6r6hCm21AvA2H3DkwIDAQABo4IBfTCCAXkwEgYDVR0T\n" \
    "AQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwfwYIKwYBBQUHAQEEczBxMDIG\n" \
    "CCsGAQUFBzABhiZodHRwOi8vaXNyZy50cnVzdGlkLm9jc3AuaWRlbnRydXN0LmNv\n" \
    "bTA7BggrBgEFBQcwAoYvaHR0cDovL2FwcHMuaWRlbnRydXN0LmNvbS9yb290cy9k\n" \
    "c3Ryb290Y2F4My5wN2MwHwYDVR0jBBgwFoAUxKexpHsscfrb4UuQdf/EFWCFiRAw\n" \
    "VAYDVR0gBE0wSzAIBgZngQwBAgEwPwYLKwYBBAGC3xMBAQEwMDAuBggrBgEFBQcC\n" \
    "ARYiaHR0cDovL2Nwcy5yb290LXgxLmxldHNlbmNyeXB0Lm9yZzA8BgNVHR8ENTAz\n" \
    "MDGgL6AthitodHRwOi8vY3JsLmlkZW50cnVzdC5jb20vRFNUUk9PVENBWDNDUkwu\n" \
    "Y3JsMB0GA1UdDgQWBBSoSmpjBH3duubRObemRWXv86jsoTANBgkqhkiG9w0BAQsF\n" \
    "AAOCAQEA3TPXEfNjWDjdGBX7CVW+dla5cEilaUcne8IkCJLxWh9KEik3JHRRHGJo\n" \
    "uM2VcGfl96S8TihRzZvoroed6ti6WqEBmtzw3Wodatg+VyOeph4EYpr/1wXKtx8/\n" \
    "wApIvJSwtmVi4MFU5aMqrSDE6ea73Mj2tcMyo5jMd6jmeWUHK8so/joWUoHOUgwu\n" \
    "X4Po1QYz+3dszkDqMp4fklxBwXRsW10KXzPMTZ+sOPAveyxindmjkW8lGy+QsRlG\n" \
    "PfZ+G6Z6h7mjem0Y+iWlkYcV4PIWL1iwBi8saCbGS5jN2p8M+X+Q7UNKEkROb3N6\n" \
    "KOqkqm57TH2H3eDJAkSnh6/DNFu0Qg==\n" \
    "-----END CERTIFICATE-----\n";

client.setCACert(ssl_ca_cert);
```

### TEENSY 4.1
目前尚未实现 WSS。

## 贡献
欢迎贡献！如果您在使用该库时遇到问题或对如何开始有任何疑问，请提交 Issue。欢迎提交 Pull Request，但请先开启一个 Issue。

## 贡献者
感谢所有报告错误、提出功能建议并为该库开发做出贡献的人。
<table>
  <tr>
    <td align="center"><a href="https://github.com/arnoson"><img src="https://github.com/arnoson.png" width="100px;" alt="arnoson"/><br /><sub><b>⭐️ arnoson</b></sub></a><br /></td>
    <td align="center"><a href="https://github.com/ramdor"><img src="https://github.com/ramdor.png" width="100px;" alt="ramdor"/><br /><sub><b>⭐️ ramdor</b></sub></a><br /></td>
    <td align="center"><a href="https://github.com/xgarb"><img src="https://github.com/xgarb.png" width="100px;" alt="xgarb"/><br /><sub><b>⭐️ xgarb</b></sub></a><br /></td>
    <td align="center"><a href="https://github.com/matsujirushi"><img src="https://github.com/matsujirushi.png" width="100px;" alt="matsujirushi"/><br /><sub><b>matsujirushi</b></sub></a><br /></td>
    <td align="center"><a href="https://github.com/bastienvans"><img src="https://github.com/bastienvans.png" width="100px;" alt="bastienvans"/><br /><sub><b>bastienvans</b></sub></a><br /></td>
    <td align="center"><a href="https://github.com/johneakin"><img src="https://github.com/johneakin.png" width="100px;" alt="johneakin"/><br /><sub><b>johneakin</b></sub></a><br /></td>
    <td align="center"><a href="https://github.com/lalten"><img src="https://github.com/lalten.png" width="100px;" alt="lalten"/><br /><sub><b>lalten</b></sub></a><br /></td>
  </tr>
  <tr>
      <td align="center"><a href="https://github.com/adelin-mcbsoft"><img src="https://github.com/adelin-mcbsoft.png" width="100px;" alt="adelin-mcbsoft"/><br /><sub><b>⭐️ adelin-mcbsoft</b></sub></a><br /></td>
      <td align="center"><a href="https://github.com/Jonty"><img src="https://github.com/Jonty.png" width="100px;" alt="Jonty"/><br /><sub><b>⭐️ Jonty</b></sub></a><br /></td>
      <td align="center"><a href="https://github.com/Nufflee"><img src="https://github.com/Nufflee.png" width="100px;" alt="Nufflee"/><br /><sub><b>Nufflee</b></sub></a><br /></td>
      <td align="center"><a href="https://github.com/mmcArg"><img src="https://github.com/mmcArg.png" width="100px;" alt="mmcArg"/><br /><sub><b>mmcArg</b></sub></a><br /></td>
      <td align="center"><a href="https://github.com/JohnInWI"><img src="https://github.com/JohnInWI.png" width="100px;" alt="JohnInWI"/><br /><sub><b>JohnInWI</b></sub></a><br /></td>
      <td align="center"><a href="https://github.com/logdog2709"><img src="https://github.com/logdog2709.png" width="100px;" alt="logdog2709"/><br /><sub><b>logdog2709</b></sub></a><br /></td>
      <td align="center"><a href="https://github.com/elC0mpa"><img src="https://github.com/elC0mpa.png" width="100px;" alt="elC0mpa"/><br /><sub><b>elC0mpa</b></sub></a><br /></td>
 </tr>
    
 <tr>
      <td align="center"><a href="https://github.com/oofnik"><img src="https://github.com/oofnik.png" width="100px;" alt="oofnik"/><br /><sub><b>⭐️ oofnik</b></sub></a><br /></td>
      <td align="center"><a href="https://github.com/zastrixarundell"><img src="https://github.com/zastrixarundell.png" width="100px;" alt="zastrixarundell"/><br /><sub><b>⭐️ zastrixarundell</b></sub></a><br /></td>
      <td align="center"><a href="https://github.com/elielmarcos"><img src="https://github.com/elielmarcos.png" width="100px;" alt="elielmarcos"/><br /><sub><b>elielmarcos</b></sub></a><br /></td>
      <td align="center"><a href="https://github.com/ioqite"><img src="https://github.com/ioqite.png" width="100px;" alt="Ioqit"/><br /><sub><b>Ioqit</b></sub></a><br /></td>

 </tr>
 
</table>

## 更新日志
- **14/02/2019 (v0.1.1)** - 初始提交，支持 ESP32 和 ESP8266 WebSocket 客户端。
- **16/02/2019 (v0.1.2)** - 添加对事件（Ping、Pong）的支持及更多内部改进（根据 [RFC-6455](https://tools.ietf.org/html/rfc6455) 处理事件）。
- **20/02/2019 (v0.1.3)** - 用户不再需要指定 TCP 客户端类型（ESP8266/ESP32），它们会被自动选择。
- **21/02/2019 (v0.1.5)** - 修复 Bug。客户端现在提供单一的字符串连接接口。
- **24/02/2019 (v0.2.0)** - 用户接口现在使用 Arduino 的 `String` 类实现。合并了 TinyWebsockets 的更多更改（主要是优化）。
- **25/02/2019 (v0.2.1)** - 微小补丁。修复了客户端接口缺失的用户可见字符串。
- **07/03/2019 (v0.3.0)** - 版本更新。现在支持 WebSocket 服务端，更好地支持分片消息和流。修复 Bug 并优化网络实现。
- **08/03/2019 (v0.3.1)** - 小补丁。合并了 TinyWebsockets 的更改 - 回调接口更改（部分回调不再将 WebsocketsClient& 作为第一个参数）。
- **12/03/2019 (v0.3.2)** - 修复了 `WebsocketsClient` 行为的 Bug（拷贝构造函数和赋值运算符）。添加了来自 TinyWebsockets 的关闭状态码。感谢 [@ramdor](https://github.com/gilmaimon/ArduinoWebsockets/issues/2)。
- **13/03/2019 (v0.3.3)** - 修复了 ESP8266 网络实现中的 Bug。感谢 [@ramdor](https://github.com/gilmaimon/ArduinoWebsockets/issues/2)。
- **14/03/2019 (v0.3.4)** - 更改 ESP8266 和 ESP32 的底层 TCP 实现以使用 `setNoDelay(true)` 代替同步通信。这使得通信比默认情况更快、更可靠。感谢 @ramdor 指出这些方法。
- **06/04/2019 (v0.3.5)** - 为 ESP8266 添加了非常基础的 WSS 支持（不支持指纹/CA 或任何形式的证书链验证）。
- **22/04/2019 (v0.4.0)** - 为 ESP8266 和 ESP32 添加了 WSS 支持。ESP8266 可以使用 `client.setInsecure()`（不验证证书链）或 `client.setFingerprint(fingerprint)` 来使用 WSS。ESP32 可以使用 `client.setCACert(certificate)`。（用法与内置的 `WiFiClientSecure` 相同）。
- **18/05/2019 (v0.4.1)** - 补丁！解决了与某些服务端的错误。该 Bug 最初在 [issue #9](https://github.com/gilmaimon/ArduinoWebsockets/issues/9) 中被指出。原因是某些服务端会丢弃未使用掩码的连接，而 TinyWebsockets 默认不使用掩码。TinyWebsockets 已更改此行为，本库也合并了该更改。
- **24/05/2019 (v0.4.2)** - 补丁！解决了掩码问题 - 服务端到客户端的消息会被错误地加上掩码，而 RFC 禁止此行为。（合并了 TinyWebsockets 的更改）。
- **07/06/2019 (v0.4.3)** - 补丁！修复了某些客户端（主要是 Firefox WebSocket 实现）的 Bug。感谢 @xgarb（[相关 issue](https://github.com/gilmaimon/ArduinoWebsockets/issues/11)）。
- **09/06/2019 (v0.4.4)** - 补丁！修复了某些情况下（例如突然断开连接）未调用 `close` 事件回调的问题。感谢 @adelin-mcbsoft 指出该问题（[相关 issue](https://github.com/gilmaimon/ArduinoWebsockets/issues/14)）。
- **14/06/2019 (v0.4.5)** - 补丁！修复了内存泄漏及掩码消息时不必要的堆内存使用。这得益于 [issue #16](https://github.com/gilmaimon/ArduinoWebsockets/issues/16) 的发现。感谢 [xgarb](https://github.com/xgarb)！
- **13/07/2019 (v0.4.6)** - 极小更新。修改 README 以记录 ESP32 安全客户端的行为（[如 issue #18 中讨论](https://github.com/gilmaimon/ArduinoWebsockets/issues/18)）。此外，ESP32 版本的 WebsocketsClient 现在也有了 `setInsecure` 方法。感谢 [adelin-mcbsoft](https://github.com/adelin-mcbsoft)！
- **25/07/2019 (v0.4.7)** - 修复 Bug。修复了接收大消息时的问题（未检查的读取），该问题在 [issue #21](https://github.com/gilmaimon/ArduinoWebsockets/issues/21) 中被指出。感谢 [Jonty](https://github.com/Jonty)！
- **26/07/2019 (v0.4.8)** - 新功能。添加了 `addHeader` 方法，如 [issue #22](https://github.com/gilmaimon/ArduinoWebsockets/issues/21) 中所建议。感谢 [mmcArg](https://github.com/mmcArg)！
- **01/08/2019 (v0.4.9)** - 补丁 - 修复 Bug。绕过了连接到不可用端点时不返回 false 的 Bug（这是 `WiFiClient` 库本身的 Bug）。添加了一些缺失的关键字。感谢 [Nufflee](https://github.com/Nufflee) 指出该 [issue](https://github.com/gilmaimon/ArduinoWebsockets/issues/25)！
- **10/08/2019 (v0.4.10)** - 补丁 - 修复 Bug。修复了由未检查和不安全的套接字读取操作引起的 Bug（及整体不稳定性）。同时改进了内存使用和管理。感谢 [Jonty](https://github.com/Jonty) 开启并协助解决该 [issue](https://github.com/gilmaimon/ArduinoWebsockets/issues/26)！
- **14/09/2019 (v0.4.11)** - 修复 Bug - 修复了在 `WebsocketClient` 实例间赋值时掩码设置未被复制的问题。此外，握手验证现在不区分大小写。感谢 [logdog2709](https://github.com/logdog2709) 指出该 [issue](https://github.com/gilmaimon/ArduinoWebsockets/issues/34)。
- **12/10/2019 (v0.4.12)** - 补丁 - 消息现在作为单个 TCP 缓冲区发送，而不是分开的消息。感谢 [elC0mpa](https://github.com/elC0mpa) 发布该 [issue](https://github.com/gilmaimon/ArduinoWebsockets/issues/44)。
- **19/10/2019 (v0.4.13)** - 补丁 - 添加了 `yield` 调用，以防止 ESP8266 在处理长消息时发生软件看门狗重置。感谢 [elC0mpa](https://github.com/elC0mpa) 记录并协助解决该 [issue](https://github.com/gilmaimon/ArduinoWebsockets/issues/43)。
- **22/11/2019 (v0.4.14)** - 在 `WebsocketsMessage` 中添加了 `rawData` 和 `c_str` 访问器，现在可以访问原始数据，这应该能解决 issue #32 且不破坏任何现有代码。
- **24/02/20 (v0.4.15)** - 在库发送的请求中添加了 `Origin` 和 `User-Agent` 请求头，某些服务端似乎需要这些。感谢 [imesut](https://github.com/imesut) 指出该问题。
- **21/04/20 (v0.4.16)** - 合并了 @oofnik 的 PR，为 ESP32 和 ESP8266 添加了双向 SSL 认证。非常感谢 [oofnik](https://github.com/oofnik) 的贡献。
- **25/04/20 (v0.4.17)** - 合并了 Luka Bodroža (@zastrixarundell) 的 PR，修复了 [issue #69](https://github.com/gilmaimon/ArduinoWebsockets/issues/69) - 现在可以通过 `addHeader` 方法自定义默认请求头（如 Origin、Host）。感谢 [zastrixarundell](https://github.com/zastrixarundell) 的贡献。
- **23/07/20 (v0.4.18)** - 合并了 Adelin U (@adelin-mcbsoft) 的 PR，修复了 [issue #84](https://github.com/gilmaimon/ArduinoWebsockets/issues/84) - SSL Bug 修复，实现了公钥证书验证和客户端 EC 证书。感谢 Adelin！
- **28/11/20 (v0.5.0)** - 由出色的 [@arnoson](https://github.com/arnoson) 添加了对 Teensy 4.1 的支持。支持明文客户端/服务端通信，并提供了新的实用示例。感谢 arnoson！
- **10/05/21 (v0.5.1)** - 示例中的指纹和证书由 [@Khoi Hoang](https://github.com/khoih-prog) 更新。感谢 Khoi！
- **29/07/21 (v0.5.2)** - 合并了 [ONLYstcm](https://github.com/ONLYstcm) 的 PR，为连接添加了（可配置的）超时。感谢 ONLYstcm。
- **06/08/21 (v0.5.3)** - 合并了 [ln-12](https://github.com/ln-12) 的 PR，添加了 `connectSecure` 方法以支持经典接口（host, port, path）的 WSS 连接。感谢！
- **03/10/2026 (v0.5.5)** - 修复内存泄漏、ESP32 SSL 回退及新增 cleanup() API，详见 [v0.5.5-change-log(zh-cn).md](v0.5.5-change-log(zh-cn).md)

