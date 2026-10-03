#include <tiny_websockets/internals/ws_common.hpp>
#include <tiny_websockets/network/tcp_client.hpp>
#include <tiny_websockets/message.hpp>
#include <tiny_websockets/client.hpp>
#include <tiny_websockets/internals/wscrypto/crypto.hpp>

namespace websockets {
    WebsocketsClient::WebsocketsClient() : WebsocketsClient(std::make_shared<WSDefaultTcpClient>()) {
        // Empty
    }

    WebsocketsClient::WebsocketsClient(std::shared_ptr<network::TcpClient> client) :
        _client(client),
        _endpoint(client),
        _connectionOpen(client->available()),
        _messagesCallback([](WebsocketsClient&, WebsocketsMessage){}),
        _eventsCallback([](WebsocketsClient&, WebsocketsEvent, WSInterfaceString){}),
        _sendMode(SendMode_Normal) {
        // Empty
    }

    WebsocketsClient::WebsocketsClient(const WebsocketsClient& other) :
        _client(other._client),
        _endpoint(other._endpoint),
        // Null-safe: after cleanup() or a move, other._client may be null.
        _connectionOpen(other._client ? other._client->available() : false),
        _messagesCallback(other._messagesCallback),
        _eventsCallback(other._eventsCallback),
        _sendMode(other._sendMode) {

        // delete other's client
        const_cast<WebsocketsClient&>(other)._client = nullptr;
        const_cast<WebsocketsClient&>(other)._connectionOpen = false;
    }

    WebsocketsClient::WebsocketsClient(const WebsocketsClient&& other) :
        _client(other._client),
        _endpoint(other._endpoint),
        // Null-safe: after cleanup() or a move, other._client may be null.
        _connectionOpen(other._client ? other._client->available() : false),
        _messagesCallback(other._messagesCallback),
        _eventsCallback(other._eventsCallback),
        _sendMode(other._sendMode) {

        // delete other's client
        const_cast<WebsocketsClient&>(other)._client = nullptr;
        const_cast<WebsocketsClient&>(other)._connectionOpen = false;
    }

    WebsocketsClient& WebsocketsClient::operator=(const WebsocketsClient& other) {
        // call endpoint's copy operator
        _endpoint = other._endpoint;

        // get callbacks and data from other
        this->_client = other._client;
        this->_messagesCallback = other._messagesCallback;
        this->_eventsCallback = other._eventsCallback;
        // Null-safe: after cleanup() or a move, other._client may be null.
        this->_connectionOpen = other._client ? other._client->available() : false;
        this->_sendMode = other._sendMode;

        // delete other's client
        const_cast<WebsocketsClient&>(other)._client = nullptr;
        const_cast<WebsocketsClient&>(other)._connectionOpen = false;
        return *this;
    }

    WebsocketsClient& WebsocketsClient::operator=(const WebsocketsClient&& other) {
        // call endpoint's copy operator
        _endpoint = other._endpoint;

        // get callbacks and data from other
        this->_client = other._client;
        this->_messagesCallback = other._messagesCallback;
        this->_eventsCallback = other._eventsCallback;
        // Null-safe: after cleanup() or a move, other._client may be null.
        this->_connectionOpen = other._client ? other._client->available() : false;
        this->_sendMode = other._sendMode;

        // delete other's client
        const_cast<WebsocketsClient&>(other)._client = nullptr;
        const_cast<WebsocketsClient&>(other)._connectionOpen = false;
        return *this;
    }

    struct HandshakeRequestResult {
        WSString requestStr;
        WSString expectedAcceptKey;
    };

    bool shouldAddDefaultHeader(const std::string& keyWord, const std::vector<std::pair<WSString, WSString>>& customHeaders) {
        for (const auto& header : customHeaders) {
            if(!keyWord.compare(header.first)) {
                return false;
            }
        }

        return true;
    }

    HandshakeRequestResult generateHandshake(const WSString& host, const WSString& uri,
                                             const std::vector<std::pair<WSString, WSString>>& customHeaders) {

        WSString key = crypto::base64Encode(crypto::randomBytes(16));

        WSString handshake = "GET " + uri + " HTTP/1.1\r\n";
        handshake += "Host: " + host + "\r\n";
        handshake += "Sec-WebSocket-Key: " + key + "\r\n";

        for (const auto& header: customHeaders) {
            handshake += header.first + ": " + header.second + "\r\n";
        }

        if (shouldAddDefaultHeader("Upgrade", customHeaders)) {
            handshake += "Upgrade: websocket\r\n";
        }

        if (shouldAddDefaultHeader("Connection", customHeaders)) {
            handshake += "Connection: Upgrade\r\n";
        }

        if (shouldAddDefaultHeader("Sec-WebSocket-Version", customHeaders)) {
            handshake += "Sec-WebSocket-Version: 13\r\n";
        }

        if (shouldAddDefaultHeader("User-Agent", customHeaders)) {
            handshake += "User-Agent: TinyWebsockets Client\r\n";
        }

        if (shouldAddDefaultHeader("Origin", customHeaders)) {
            handshake += "Origin: https://github.com/gilmaimon/TinyWebsockets\r\n";
        }

        handshake += "\r\n";

        HandshakeRequestResult result;
        result.requestStr = handshake;
#ifndef _WS_CONFIG_SKIP_HANDSHAKE_ACCEPT_VALIDATION
        result.expectedAcceptKey = crypto::websocketsHandshakeEncodeKey(key);
#endif
        return result;
    }

    bool isWhitespace(char ch) {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    }

    struct HandshakeResponseResult {
        bool isSuccess;
        WSString serverAccept;
    };

    bool isCaseInsensetiveEqual(const WSString lhs, const WSString rhs) {
      if (lhs.size() != rhs.size()) return false;

      for (size_t i = 0; i < lhs.size(); i++) {
        char leftLowerCaseChar = lhs[i] >= 'A' && lhs[i] <= 'Z' ? lhs[i] - 'A' + 'a' : lhs[i];
        char righerLowerCaseChar = rhs[i] >= 'A' && rhs[i] <= 'Z' ? rhs[i] - 'A' + 'a' : rhs[i];
        if (leftLowerCaseChar != righerLowerCaseChar) return false;
      }

      return true;
    }

    HandshakeResponseResult parseHandshakeResponse(std::vector<WSString> responseHeaders) {
        bool didUpgradeToWebsockets = false, isConnectionUpgraded = false;
        WSString serverAccept = "";
        for(WSString header : responseHeaders) {
            auto colonIndex = header.find_first_of(':');

            WSString key = header.substr(0, colonIndex);
            WSString value = header.substr(colonIndex + 2); // +2 (ignore space and ':')

            if(isCaseInsensetiveEqual(key, "upgrade")) {
                didUpgradeToWebsockets = isCaseInsensetiveEqual(value, "Websocket");
            } else if(isCaseInsensetiveEqual(key, "connection")) {
                isConnectionUpgraded = isCaseInsensetiveEqual(value, "upgrade");
            } else if(isCaseInsensetiveEqual(key, "Sec-WebSocket-Accept")) {
                serverAccept = value;
            }
        }

        HandshakeResponseResult result;
        result.isSuccess = serverAccept != "" && didUpgradeToWebsockets && isConnectionUpgraded;
        result.serverAccept = serverAccept;
        return result;
    }

    bool doestStartsWith(WSString str, WSString prefix) {
        if(str.size() < prefix.size()) return false;
        for(size_t i = 0; i < prefix.size(); i++) {
            if(str[i] != prefix[i]) return false;
        }

        return true;
    }

    void WebsocketsClient::upgradeToSecuredConnection() {
    #ifndef _WS_CONFIG_NO_SSL
        auto client = new WSDefaultSecuredTcpClient;

    #ifdef ESP8266
        if(
                                this->_optional_ssl_fingerprint
                        ||      (this->_optional_ssl_rsa_cert && this->_optional_ssl_rsa_private_key)
                        ||      (this->_optional_ssl_ec_cert && this->_optional_ssl_ec_private_key)
                        ||      this->_optional_ssl_trust_anchors
                        ||      this->_optional_ssl_known_key
                ) {
            if(this->_optional_ssl_fingerprint) {
                client->setFingerprint(this->_optional_ssl_fingerprint);
            }
            if(this->_optional_ssl_trust_anchors) {
                client->setTrustAnchors(this->_optional_ssl_trust_anchors);
            }
                        if(this->_optional_ssl_known_key) {
                                client->setKnownKey(this->_optional_ssl_known_key);
                        }
            if(this->_optional_ssl_rsa_cert && this->_optional_ssl_rsa_private_key) {
                client->setClientRSACert(this->_optional_ssl_rsa_cert, this->_optional_ssl_rsa_private_key);
            }
                        if(this->_optional_ssl_ec_cert && this->_optional_ssl_ec_private_key) {
                client->setClientECCert(this->_optional_ssl_ec_cert, this->_optional_ssl_ec_private_key);
            }
        } else {
            client->setInsecure();
        }
    #elif defined(ESP32)
        // Bug 1 fix: ESP32 now has the same fallback as ESP8266. If the user
        // has not configured any CA cert / client cert / private key we
        // explicitly call setInsecure() on the underlying WiFiClientSecure.
        // Without this, many ESP32 core versions will fail the SSL handshake
        // because WiFiClientSecure has no trust anchor at all and refuses to
        // proceed even when the user is happy to skip verification.
        if(this->_optional_ssl_ca_cert) {
            client->setCACert(this->_optional_ssl_ca_cert);
        }
        if(this->_optional_ssl_client_ca) {
            client->setCertificate(this->_optional_ssl_client_ca);
        }
        if(this->_optional_ssl_private_key) {
            client->setPrivateKey(this->_optional_ssl_private_key);
        }
        if(!this->_optional_ssl_ca_cert
            && !this->_optional_ssl_client_ca
            && !this->_optional_ssl_private_key) {
            client->setInsecure();
        }
    #endif

        this->_client = std::shared_ptr<WSDefaultSecuredTcpClient>(client);
        this->_endpoint.setInternalSocket(this->_client);
    #endif //_WS_CONFIG_NO_SSL
    }

    void WebsocketsClient::addHeader(const WSInterfaceString key, const WSInterfaceString value) {
        _customHeaders.push_back({internals::fromInterfaceString(key), internals::fromInterfaceString(value)});
    }

    bool WebsocketsClient::connect(WSInterfaceString _url) {
        // Hardening: release any previous TCP/SSL state before reconnecting.
        // This MUST run before upgradeToSecuredConnection() — otherwise the
        // new SecuredEsp32TcpClient allocated by upgrade would be immediately
        // destroyed by cleanup(). Repeated connect()/close() cycles on ESP32
        // would otherwise keep the previous WiFiClientSecure (and its ~45 KB
        // mbedtls context) alive via the shared_ptr in _endpoint, while the
        // new connection allocates another one on top — a classic heap-growth
        // pattern. cleanup() is idempotent and null-guarded, so calling it
        // unconditionally here is safe even on a fresh / already-clean state.
        cleanup();

        WSString url = internals::fromInterfaceString(_url);
        WSString protocol = "";
        int defaultPort = 0;

        if(doestStartsWith(url, "http://")) {
            defaultPort = 80;
            protocol = "http";
            url = url.substr(7); //strlen("http://") == 7
        } else if(doestStartsWith(url, "ws://")) {
            defaultPort = 80;
            protocol = "ws";
            url = url.substr(5); //strlen("ws://") == 5
        }

    #ifndef _WS_CONFIG_NO_SSL
        else if(doestStartsWith(url, "wss://")) {
            defaultPort = 443;
            protocol = "wss";
            url = url.substr(6); //strlen("wss://") == 6

            upgradeToSecuredConnection();
        } else if(doestStartsWith(url, "https://")) {
            defaultPort = 443;
            protocol = "https";
            url = url.substr(8); //strlen("https://") == 8

            upgradeToSecuredConnection();
        }
    #endif

        else {
            return false;
            // Not supported
        }

        auto uriBeg = url.find_first_of('/');
        std::string host = url, uri = "/";

        if(static_cast<int>(uriBeg) != -1) {
            uri = url.substr(uriBeg);
            host = url.substr(0, uriBeg);
        }

        auto portIdx = host.find_first_of(':');
        int port = defaultPort;
        if(static_cast<int>(portIdx) != -1) {
            auto onlyHost = host.substr(0, portIdx);
            ++portIdx;
            port = 0;
            while(portIdx < host.size() && host[portIdx] >= '0' && host[portIdx] <= '9') {
                port = port * 10 + (host[portIdx] - '0');
                ++portIdx;
            }

            host = onlyHost;
        }

        return this->connect(
            internals::fromInternalString(host),
            port,
            internals::fromInternalString(uri)
        );
    }

    bool WebsocketsClient::connect(WSInterfaceString host, int port, WSInterfaceString path) {
        // If _client is null (e.g. after cleanup() or after a moved-from
        // state), rebuild a default plain TCP client so the upcoming handshake
        // has something to talk through. If the caller used wss:// / https://
        // then upgradeToSecuredConnection() was already invoked upstream and
        // replaced _client with a secured one — in that case _client is
        // non-null here and we leave it alone.
        if (!this->_client) {
            this->_client = std::make_shared<WSDefaultTcpClient>();
            this->_endpoint.setInternalSocket(this->_client);
        }

        this->_connectionOpen = this->_client->connect(internals::fromInterfaceString(host), port);
        if (!this->_connectionOpen) return false;

        auto handshake = generateHandshake(internals::fromInterfaceString(host), internals::fromInterfaceString(path), _customHeaders);
        this->_client->send(handshake.requestStr);

        // This check is needed because of an ESP32 lib bug that wont signal that the connection had
        // failed in `->connect` (called above), sometimes the disconnect will only be noticed here (after a `send`)
        if(!available()) {
            return false;
        }

        auto head = this->_client->readLine();
        if(!doestStartsWith(head, "HTTP/1.1 101")) {
            close(CloseReason_ProtocolError);
            return false;
        }

        std::vector<WSString> serverResponseHeaders;
        WSString line = "";
        while (true) {
            line = this->_client->readLine();

            if (line.size() < 2) {
                close(CloseReason_ProtocolError);
                return false;
            }

            if (line == "\r\n") break;
            // remove /r/n from line end
            line = line.substr(0, line.size() - 2);

            serverResponseHeaders.push_back(line);
        }
        auto parsedResponse = parseHandshakeResponse(serverResponseHeaders);

#ifdef _WS_CONFIG_SKIP_HANDSHAKE_ACCEPT_VALIDATION
        bool serverAcceptMismatch = false;
#else
        bool serverAcceptMismatch = parsedResponse.serverAccept != handshake.expectedAcceptKey;
#endif
        if(parsedResponse.isSuccess == false || serverAcceptMismatch) {
            close(CloseReason_ProtocolError);
            return false;
        }

        this->_eventsCallback(*this, WebsocketsEvent::ConnectionOpened, {});
        return true;
    }

    bool WebsocketsClient::connectSecure(WSInterfaceString host, int port, WSInterfaceString path) {
        // Hardening: release any previous TCP/SSL state before reconnecting.
        // See connect(WSInterfaceString url) for the rationale — cleanup()
        // MUST run before upgradeToSecuredConnection() to avoid destroying
        // the freshly allocated SecuredEsp32TcpClient.
        cleanup();

        upgradeToSecuredConnection();

        return connect(host, port, path);
    }

    void WebsocketsClient::onMessage(MessageCallback callback) {
        this->_messagesCallback = callback;
    }

    void WebsocketsClient::onMessage(PartialMessageCallback callback) {
        this->_messagesCallback = [callback](WebsocketsClient&, WebsocketsMessage msg) {
            callback(msg);
        };
    }

    void WebsocketsClient::onEvent(EventCallback callback) {
        this->_eventsCallback = callback;
    }

    void WebsocketsClient::onEvent(PartialEventCallback callback) {
        this->_eventsCallback = [callback](WebsocketsClient&, WebsocketsEvent event, WSInterfaceString data) {
            callback(event, data);
        };
    }

    bool WebsocketsClient::poll() {
        bool messageReceived = false;
        while(available() && _endpoint.poll()) {
            auto msg = _endpoint.recv();
            if(msg.isEmpty()) {
                continue;
            }
            messageReceived = true;

            if(msg.isBinary() || msg.isText()) {
                this->_messagesCallback(*this, std::move(msg));
            } else if(msg.isContinuation()) {
                // continuation messages will only be returned when policy is appropriate
                this->_messagesCallback(*this, std::move(msg));
            } else if(msg.isPing()) {
                _handlePing(std::move(msg));
            } else if(msg.isPong()) {
                _handlePong(std::move(msg));
            } else if(msg.isClose()) {
                this->_connectionOpen = false;
                _handleClose(std::move(msg));
            }
        }

        return messageReceived;
    }

    WebsocketsMessage WebsocketsClient::readBlocking() {
        while(available()) {
#ifdef PLATFORM_DOES_NOT_SUPPORT_BLOCKING_READ
            while(available() && _endpoint.poll() == false) continue;
#endif
            auto msg = _endpoint.recv();
            if(!msg.isEmpty()) return msg;
        }
        return {};
    }

    bool WebsocketsClient::send(const WSInterfaceString& data) {
        auto str = internals::fromInterfaceString(data);
        return this->send(str.c_str(), str.size());
    }

    bool WebsocketsClient::send(const WSInterfaceString&& data) {
        auto str = internals::fromInterfaceString(data);
        return this->send(str.c_str(), str.size());
    }

    bool WebsocketsClient::send(const char* data) {
        return this->send(data, strlen(data));
    }

    bool WebsocketsClient::send(const char* data, const size_t len) {
        if(available()) {
            // if in normal mode
            if(this->_sendMode == SendMode_Normal) {
                // send a normal message
                return _endpoint.send(
                    data,
                    len,
                    internals::ContentType::Text,
                    true
                );
            }
            // if in streaming mode
            else if(this->_sendMode == SendMode_Streaming) {
                // send a continue frame
                return _endpoint.send(
                    data,
                    len,
                    internals::ContentType::Continuation,
                    false
                );
            }
        }
        return false;
    }

    bool WebsocketsClient::sendBinary(WSInterfaceString data) {
        auto str = internals::fromInterfaceString(data);
        return this->sendBinary(str.c_str(), str.size());
    }

    bool WebsocketsClient::sendBinary(const char* data, const size_t len) {
        if(available()) {
            // if in normal mode
            if(this->_sendMode == SendMode_Normal) {
                // send a normal message
                return _endpoint.send(
                    data,
                    len,
                    internals::ContentType::Binary,
                    true
                );
            }
            // if in streaming mode
            else if(this->_sendMode == SendMode_Streaming) {
                // send a continue frame
                return _endpoint.send(
                    data,
                    len,
                    internals::ContentType::Continuation,
                    false
                );
            }
        }
        return false;
    }

    bool WebsocketsClient::stream(const WSInterfaceString data) {
        if(available() && this->_sendMode == SendMode_Normal) {
            this->_sendMode = SendMode_Streaming;
            return _endpoint.send(
                internals::fromInterfaceString(data),
                internals::ContentType::Text,
                false
            );
        }
        return false;
    }


    bool WebsocketsClient::streamBinary(const WSInterfaceString data) {
        if(available() && this->_sendMode == SendMode_Normal) {
            this->_sendMode = SendMode_Streaming;
            return _endpoint.send(
                internals::fromInterfaceString(data),
                internals::ContentType::Binary,
                false
            );
        }
        return false;
    }

    bool WebsocketsClient::end(const WSInterfaceString data) {
        if(available() && this->_sendMode == SendMode_Streaming) {
            this->_sendMode = SendMode_Normal;
            return _endpoint.send(
                internals::fromInterfaceString(data),
                internals::ContentType::Continuation,
                true
            );
        }
        return false;
    }

    void WebsocketsClient::setFragmentsPolicy(const FragmentsPolicy newPolicy) {
        _endpoint.setFragmentsPolicy(newPolicy);
    }

    bool WebsocketsClient::available(const bool activeTest) {
        if(activeTest)  {
            _endpoint.ping("");
        }

        bool updatedConnectionOpen = this->_connectionOpen && this->_client && this->_client->available();

        if(updatedConnectionOpen != this->_connectionOpen) {
            _endpoint.close(CloseReason_AbnormalClosure);
            this->_eventsCallback(*this, WebsocketsEvent::ConnectionClosed, "");
        }

        this->_connectionOpen = updatedConnectionOpen;
        return this->_connectionOpen;
    }

    bool WebsocketsClient::ping(const WSInterfaceString data) {
        if(available()) {
            return _endpoint.ping(internals::fromInterfaceString(data));
        }
        return false;
    }

    bool WebsocketsClient::pong(const WSInterfaceString data) {
        if(available()) {
            return _endpoint.pong(internals::fromInterfaceString(data));
        }
        return false;
    }

    void WebsocketsClient::close(const CloseReason reason) {
        if(available()) {
            this->_connectionOpen = false;
            _endpoint.close(reason);
            _handleClose({});
        }
    }

    CloseReason WebsocketsClient::getCloseReason() const {
        return _endpoint.getCloseReason();
    }

    void WebsocketsClient::_handlePing(const WebsocketsMessage message) {
        this->_eventsCallback(*this, WebsocketsEvent::GotPing, message.data());
    }

    void WebsocketsClient::_handlePong(const WebsocketsMessage message) {
        this->_eventsCallback(*this, WebsocketsEvent::GotPong, message.data());
    }

    void WebsocketsClient::_handleClose(const WebsocketsMessage message) {
        this->_eventsCallback(*this, WebsocketsEvent::ConnectionClosed, message.data());
    }


#ifdef ESP8266
    void WebsocketsClient::setFingerprint(const char* fingerprint) {
        this->_optional_ssl_fingerprint = fingerprint;
    }

    void WebsocketsClient::setInsecure() {
        this->_optional_ssl_fingerprint = nullptr;
        this->_optional_ssl_rsa_cert = nullptr;
        this->_optional_ssl_rsa_private_key = nullptr;
                this->_optional_ssl_ec_cert = nullptr;
        this->_optional_ssl_ec_private_key = nullptr;
        this->_optional_ssl_trust_anchors = nullptr;
                this->_optional_ssl_known_key = nullptr;
    }

    void WebsocketsClient::setClientRSACert(const X509List *cert, const PrivateKey *sk) {
        this->_optional_ssl_rsa_cert = cert;
        this->_optional_ssl_rsa_private_key = sk;
        }

        void WebsocketsClient::setClientECCert(const X509List *cert, const PrivateKey *sk) {
        this->_optional_ssl_ec_cert = cert;
        this->_optional_ssl_ec_private_key = sk;
        }

    void WebsocketsClient::setTrustAnchors(const X509List *ta){
        this->_optional_ssl_trust_anchors = ta;
        }

        void WebsocketsClient::setKnownKey(const PublicKey *pk) {
                this->_optional_ssl_known_key = pk;
        }

#elif defined(ESP32)
    void WebsocketsClient::setCACert(const char* ca_cert) {
        this->_optional_ssl_ca_cert = ca_cert;
    }

    void WebsocketsClient::setCertificate(const char* client_ca) {
        this->_optional_ssl_client_ca = client_ca;
    }

    void WebsocketsClient::setPrivateKey(const char* private_key) {
        this->_optional_ssl_private_key = private_key;
    }

    void WebsocketsClient::setInsecure() {
        this->_optional_ssl_ca_cert = nullptr;
        this->_optional_ssl_client_ca = nullptr;
        this->_optional_ssl_private_key = nullptr;
    }
#endif

    WebsocketsClient::~WebsocketsClient() {
        // Hardening: unconditionally tear everything down. Even if the user
        // forgot to call close()/cleanup(), going out of scope must release
        // the underlying TCP/SSL socket and the ~45 KB mbedtls context held
        // by WiFiClientSecure on ESP32. cleanup() is idempotent and fully
        // null-guarded, so this is safe on a moved-from or already-clean
        // instance.
        cleanup();
    }

    void WebsocketsClient::cleanup() {
        // Step 1: if the WebSocket connection is still open, send a close
        // frame and stop the underlying TCP/SSL socket. Null-guarded so this
        // is safe on a moved-from instance (where _client is null) and on an
        // already-cleaned instance (where _client is also null).
        if (this->_client) {
            if (this->_connectionOpen) {
                // _endpoint.close() will:
                //   - bail out cleanly if _endpoint's internal socket is null
                //     or already disconnected (null guard added in endpoint)
                //   - otherwise send a WS close frame with GoingAway and call
                //     _client->close() which stops the TCP/SSL socket
                _endpoint.close(CloseReason_GoingAway);
                this->_connectionOpen = false;
            }
            // Defensive: ensure the underlying TCP/SSL socket is released
            // even if _endpoint.close() short-circuited (e.g. peer already
            // disconnected). close() on the TcpClient is a no-op on an
            // already-closed socket.
            this->_client->close();
        }

        // Step 2: drop the WebsocketsClient's reference to the TcpClient.
        // This alone does NOT free the ~45 KB — _endpoint still holds another
        // reference. Step 3 is what actually drops the refcount to 0.
        this->_client.reset();

        // Step 3: rebuild _endpoint with a null _client. This releases the
        // endpoint's reference too, so the TcpClient's refcount finally hits
        // 0 and the destructor chain runs:
        //   ~SecuredEsp32TcpClient -> ~GenericEspTcpClient<WiFiClientSecure>
        //   -> ~WiFiClientSecure -> mbedtls context free (~45 KB on ESP32).
        // It also prevents any subsequent call into _endpoint (e.g. via a
        // stale callback) from dereferencing a dangling pointer — the
        // endpoint methods are null-guarded and will short-circuit.
        this->_endpoint.setInternalSocket(nullptr);

        // Step 4: clear custom headers so a future connect() does not reuse
        // stale Authorization / Cookie / etc. headers from the previous
        // connection.
        this->_customHeaders.clear();

        // Step 5: reset send mode so a future connect() starts in the normal
        // (non-streaming) mode even if the previous session died mid-stream.
        this->_sendMode = SendMode_Normal;
    }
}
