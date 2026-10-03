#pragma once

#include <tiny_websockets/internals/ws_common.hpp>
#include <tiny_websockets/network/tcp_client.hpp>

namespace websockets { namespace network {
  template <class WifiClientImpl> 
  class GenericEspTcpClient : public TcpClient {
  public:
    GenericEspTcpClient(WifiClientImpl c) : client(c) {
      // Only enable NoDelay when the wrapped client is actually connected.
      // Calling setNoDelay on a moved-from / unconnected client can hit
      // setSocketOption(): fail on 0, errno: 9, "Bad file number" on some
      // ESP core versions (notably after a failed SSL handshake).
      if (client.connected()) {
        client.setNoDelay(true);
      }
    }

    GenericEspTcpClient() {}

    bool connect(const WSString& host, const int port) {
      yield();
      auto didConnect = client.connect(host.c_str(), port);
      // Bug 2 fix: only tune the socket when connect() actually succeeded.
      // If the SSL/TCP handshake failed the underlying socket is invalid and
      // setNoDelay would emit "Bad file number" (EBADF) noise on ESP32/ESP8266.
      if (didConnect) {
        client.setNoDelay(true);
      }
      return didConnect;
    }

    bool poll() {
      yield();
      return client.available();
    }

    bool available() override {
      return client.connected();
    }

    void send(const WSString& data) override {
      yield();
      client.write(reinterpret_cast<uint8_t*>(const_cast<char*>(data.c_str())), data.size());
      yield();
    }

    void send(const WSString&& data) override {
      yield();
      client.write(reinterpret_cast<uint8_t*>(const_cast<char*>(data.c_str())), data.size());
      yield();
    }

    void send(const uint8_t* data, const uint32_t len) override {
      yield();
      client.write(data, len);
      yield();
    }
    
    WSString readLine() override {
      WSString line = "";

      int ch = -1;

      const uint64_t millisBeforeReadingHeaders = millis();
      while( ch != '\n' && available()) {
        if (millis() - millisBeforeReadingHeaders > _CONNECTION_TIMEOUT) return "";
        ch = client.read();
        if (ch < 0) continue;
        line += (char) ch;
      }

      return line;
    }

    uint32_t read(uint8_t* buffer, const uint32_t len) override {
      yield();
      return client.read(buffer, len);
    }

    void close() override {
      yield();
      // Guard: stop() on an already-closed / moved-from client is safe on ESP
      // cores, but we still avoid the call when we know there is nothing to do.
      if (client.connected()) {
        client.stop();
      }
    }

    virtual ~GenericEspTcpClient() {
      // Defensive: ensure the underlying TCP/SSL socket is released even if
      // the user forgot to call close(). stop() is a no-op on a closed socket.
      if (client.connected()) {
        client.stop();
      }
    }

  protected:
    WifiClientImpl client;

    int getSocket() const override {
      return -1;
    }    
  };
}} // websockets::network
