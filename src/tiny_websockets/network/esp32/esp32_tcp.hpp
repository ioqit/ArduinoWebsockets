#pragma once

#ifdef ESP32 

#include <tiny_websockets/internals/ws_common.hpp>
#include <tiny_websockets/network/tcp_client.hpp>
#include <tiny_websockets/network/tcp_server.hpp>
#include <tiny_websockets/network/generic_esp/generic_esp_clients.hpp>

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace websockets { namespace network {
  typedef GenericEspTcpClient<WiFiClient> Esp32TcpClient;
  
  class SecuredEsp32TcpClient : public GenericEspTcpClient<WiFiClientSecure> {
  public:
    // Bug 1 fix: ESP32 lacked an explicit setInsecure() entry point on the
    // TcpClient wrapper, which meant upgradeToSecuredConnection() had no way
    // to put the underlying WiFiClientSecure into "skip certificate
    // verification" mode. Without this, an ESP32 with no CA cert configured
    // would fail the SSL handshake on most ESP32 core versions because there
    // was no trust anchor at all.
    void setInsecure() {
      this->client.setInsecure();
    }

    void setCACert(const char* ca_cert) {
      this->client.setCACert(ca_cert);
    }

    void setCertificate(const char* client_ca) {
      this->client.setCertificate(client_ca);
    }
    
    void setPrivateKey(const char* private_key) {
      this->client.setPrivateKey(private_key);
    }    
  };


  class Esp32TcpServer : public TcpServer {
  public:
    Esp32TcpServer() {}
    bool poll() override {
      yield();
      return server.hasClient();
    }

    bool listen(const uint16_t port) override {
      yield();
      server = WiFiServer(port);
      server.begin(port);
      return available();
    }
    
    TcpClient* accept() override {
      while(available()) {
        // ESP32 Arduino core 3.x deprecated WiFiServer::available() in favor
        // of accept(). Use the new API on >= 3.x to silence the deprecation
        // warning; keep the old API on 2.x where accept() does not exist.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
        auto client = server.accept();
#else
        auto client = server.available();
#endif
        if(client) {
          return new Esp32TcpClient{client};
        }
      }
      return new Esp32TcpClient;
    }

    bool available() override {
      yield();
      return static_cast<bool>(server);
    }
    
    void close() override {
      yield();
      server.close();
    }

    virtual ~Esp32TcpServer() {
      if(available()) close();
    }

  protected:
    int getSocket() const override {
      return -1; // Not Implemented
    }
  
  private:
    WiFiServer server;
  };
}} // websockets::network

#endif // #ifdef ESP32 
