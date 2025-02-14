/*
 *  Copyright (c) 2019-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#pragma once

#include <iostream>
#include <string>

#include <boost/algorithm/string.hpp>
#include <fizz/server/AeadTicketCipher.h>
#include <fizz/server/CertManager.h>
#include <fizz/server/TicketCodec.h>
#include <folly/FileUtil.h>
#include <folly/io/async/EventBaseManager.h>
#include <proxygen/httpserver/HTTPTransactionHandlerAdaptor.h>
#include <proxygen/httpserver/HTTPServer.h>
#include <proxygen/httpserver/RequestHandlerFactory.h>
#include <proxygen/httpserver/samples/hq/SampleHandlers.h>
#include <proxygen/lib/http/session/HQDownstreamSession.h>
#include <proxygen/lib/http/session/HTTPSessionController.h>
#include <proxygen/lib/utils/WheelTimerInstance.h>
#include <quic/congestion_control/CongestionControllerFactory.h>
#include <quic/logging/FileQLogger.h>
#include <quic/server/QuicServer.h>
#include <quic/server/QuicServerTransport.h>
#include <quic/server/QuicSharedUDPSocketFactory.h>

namespace quic { namespace samples {

const std::string kDefaultCertData = R"(
-----BEGIN CERTIFICATE-----
MIIGGzCCBAOgAwIBAgIJAPowD79hiDyZMA0GCSqGSIb3DQEBCwUAMIGjMQswCQYD
VQQGEwJVUzETMBEGA1UECAwKQ2FsaWZvcm5pYTETMBEGA1UEBwwKTWVubG8gUGFy
azERMA8GA1UECgwIUHJveHlnZW4xETAPBgNVBAsMCFByb3h5Z2VuMREwDwYDVQQD
DAhQcm94eWdlbjExMC8GCSqGSIb3DQEJARYiZmFjZWJvb2stcHJveHlnZW5AZ29v
Z2xlZ3JvdXBzLmNvbTAeFw0xOTA1MDgwNjU5MDBaFw0yOTA1MDUwNjU5MDBaMIGj
MQswCQYDVQQGEwJVUzETMBEGA1UECAwKQ2FsaWZvcm5pYTETMBEGA1UEBwwKTWVu
bG8gUGFyazERMA8GA1UECgwIUHJveHlnZW4xETAPBgNVBAsMCFByb3h5Z2VuMREw
DwYDVQQDDAhQcm94eWdlbjExMC8GCSqGSIb3DQEJARYiZmFjZWJvb2stcHJveHln
ZW5AZ29vZ2xlZ3JvdXBzLmNvbTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoC
ggIBALXZs4+YnCE8aMAL5gWNjLRm2EZiFHWoKpt42on8y+SZdb1xdSZ0rx6/jl4w
8V5aiLLNmboa1ULNWLS40mEUoqRPEUiBiN3T/3HomzMCLZ52xaaKS1sW9+ZPsSlT
omwV4HupJWKQaxpu+inY98mxGaZjzHie3AoydovD+rWWLj4mSX9DchWbC8DYq7xu
4qKedgHMJlsP3luYgnRSsZ+vlTEe/K41Czt+GGhViRNL8Nm3wZrxAGYqTx/zrqsT
R8qA3gwfPPqJJH5UprtvHXDS99yiy6MYyWBr/BbZ37A5X9pWCL09aLEIrQGQWtVu
CnBNCrQgYDgD7Y4+Q4Lfouap7I3YpuJM5cP1NO1x0Voyv2km1tmZpjUavnKyYT/v
XUCkGrWxeuMkqm68eOnadA7A8BM9b++f6NIgaexb9+Rq8QK74MpMm7/+XMWiAS9z
62hgKBd4mtUulJH1YxoQBIkfRa8pkB45nGiTrL2zzpIOoOirNe3/7FVI9LqPphPN
64ojfqZsTiGrC50R/86/p2jBs0fwrXy8opWM7Kmp1h2oNPqtgOC0Zj7IcmvEp2xa
wI6jN4XxbhDQpo3Iz/KRDxXFT4kAjdLDibWH41PccwSbHvg8zjmAGCxW6sC6bmp6
lywMzonS1VWkp1iNQ2u4bdMeDGnsaN0hOBemBLr/p3L1ee/RAgMBAAGjUDBOMB0G
A1UdDgQWBBSHFEM/GlCxZgg9qpi9REqm/RDkZDAfBgNVHSMEGDAWgBSHFEM/GlCx
Zgg9qpi9REqm/RDkZDAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBCwUAA4ICAQBG
AtowRS0Wsr9cVRKVxEM/7ZxCDcrTg7gUBD/S8RYnS2bJp5ut/3SgO0FZsQKG4k8O
CXE/dQgwIaBqxSioE3L/l+m/+gedZgqaXg7l6EJLr20sUB5PVrJoQznMIwr/FuYZ
LG4nKK/K7eKf2m1Gn54kpeWz+BtgIRU4YPkZHGtQW3ER+wnmlPQfGDiN0JymqR80
TTXlgg03L6jCFQpYGKCLbKpql+cBixmI6TeUtArosCsqZokUXNM7j5u7m1IhY1EL
pNpSaUMU7LmHOmfnxIHzmNzages+mxKOHJLKBbuQx0u87uGy3HInwbNK7hDHXWLF
mXPXDhrWjBbm1RPnq8cX9nFuPS6Cd+hROEr+VB7m+Sij5QyV5pRBS0x/54tiiEv3
8eIFl6aYqTBcCMrtlxVn8sHcA/iGrysIuidWVxQfs4wmM/apR5YgSjTvN/OAB5Mo
/5RWdxBg3jNPGk/GzPDk6FcN5kp7yRLLyAOAnPDUQRC8CkSkyOwriOMe310CnTL4
KCWp7UpoF/qZJEGhYffH85SORpxj09284tZUnLSthnRmIdYB2kWg9AARu3Vhugx8
E9HGSZzTGAsPEBikDbpUimN0zWLw8VJKL+KJURl4dX4tDRe+R2u5cWm8x3HOcDUI
j9aXkPagbL/an2g05K0hIhyANbER7HAZlJ21pJdCIQ==
-----END CERTIFICATE-----
)";

// The private key below is only used for test purposes
// @lint-ignore-every PRIVATEKEY
const std::string kDefaultKeyData = R"(
-----BEGIN RSA PRIVATE KEY-----
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
-----END RSA PRIVATE KEY-----
)";

static std::atomic<bool> shouldPassHealthChecks{true};

struct PartiallyReliableHandlerParams {
  bool partialReliabilityEnabled{false};
  folly::Optional<uint64_t> prChunkSize;
  folly::Optional<uint64_t> prChunkDelayMs;
};

class Dispatcher {
 public:
  static proxygen::HTTPTransactionHandler* getRequestHandler(
      proxygen::HTTPMessage* msg,
      const std::string& version,
      folly::Optional<PartiallyReliableHandlerParams> prParams = folly::none) {
    DCHECK(msg);
    const std::string& path = msg->getPath();
    if (path == "/" || path == "/echo") {
      return new EchoHandler(version);
    }
    if (path == "/continue") {
      return new ContinueHandler(version);
    }
    if (path.size() > 1 && path[0] == '/' && std::isdigit(path[1])) {
      return new RandBytesGenHandler(version);
    }
    if (path == "/status") {
      return new HealthCheckHandler(shouldPassHealthChecks, version);
    }
    if (path == "/status_ok") {
      shouldPassHealthChecks = true;
      return new HealthCheckHandler(true, version);
    }
    if (path == "/status_fail") {
      shouldPassHealthChecks = false;
      return new HealthCheckHandler(true, version);
    }

    if (path == "/wait" || path == "/release") {
      return new WaitReleaseHandler(
          folly::EventBaseManager::get()->getEventBase(), version);
    }

    if (path == "/pr_cat") {
      if (!prParams || !prParams->partialReliabilityEnabled) {
        LOG(ERROR) << "/pr_cat can only be accessed via partially reliable "
                      "transaction";
      } else {
        return new PrCatHandler(
            version, prParams->prChunkSize, prParams->prChunkDelayMs);
      }
    }

    if (boost::algorithm::starts_with(path, "/push")) {
      return new ServerPushHandler(version);
    }

    return new DummyHandler;
  }
};

class HQSessionController :
 public proxygen::HTTPSessionController, proxygen::HTTPSession::InfoCallback {
 public:
  using StreamData = std::pair<folly::IOBufQueue, bool>;

  HQSessionController(const std::string& version,
                      folly::Optional<uint64_t> prChunkSize = folly::none,
                      folly::Optional<uint64_t> prChunkDelayMs = folly::none)
      : version_(version) {
    prParams.prChunkSize = prChunkSize;
    prParams.prChunkDelayMs = prChunkDelayMs;
  }

  ~HQSessionController() override = default;

  proxygen::HQSession* createSession(
      const std::chrono::milliseconds txnTimeout) {
    wangle::TransportInfo tinfo;
    session_ = new proxygen::HQDownstreamSession(
                txnTimeout,
                this,
                tinfo,
                this);
    return session_;
  }

  void startSession(std::shared_ptr<quic::QuicSocket> sock) {
    CHECK(session_);
    session_->setSocket(std::move(sock));
    session_->startNow();
  }

  void onDestroy(const proxygen::HTTPSessionBase&) override {
    if (!qLoggerPath_.empty()) {
      qLogger_->outputLogsToFile(qLoggerPath_, prettyJson_);
    }
  }

  proxygen::HTTPTransactionHandler* getRequestHandler(
      proxygen::HTTPTransaction& /*txn*/, proxygen::HTTPMessage* msg) override {
    prParams.partialReliabilityEnabled =
        session_->isPartialReliabilityEnabled();
    return Dispatcher::getRequestHandler(msg, version_, prParams);
  }

  proxygen::HTTPTransactionHandler* getParseErrorHandler(
      proxygen::HTTPTransaction* /*txn*/,
      const proxygen::HTTPException& /*error*/,
      const folly::SocketAddress& /*localAddress*/) override {
    return nullptr;
  }

  proxygen::HTTPTransactionHandler* getTransactionTimeoutHandler(
      proxygen::HTTPTransaction* /*txn*/,
      const folly::SocketAddress& /*localAddress*/) override {
    return nullptr;
  }

  void attachSession(proxygen::HTTPSessionBase* /*session*/) override {
  }

  void detachSession(const proxygen::HTTPSessionBase* /*session*/) override {
    delete this;
  }

 void setQLoggerInfo(
   std::shared_ptr<FileQLogger> qLogger, std::string qLoggerPath,
   bool prettyJson) {
    qLogger_ = qLogger;
    qLoggerPath_ = std::move(qLoggerPath);
    prettyJson_ = prettyJson;
 }

 private:
  proxygen::HQSession* session_{nullptr};
  std::string version_;
  std::shared_ptr<FileQLogger> qLogger_;
  std::string qLoggerPath_;
  bool prettyJson_;
  PartiallyReliableHandlerParams prParams;
};

class HQServerTransportFactory : public quic::QuicServerTransportFactory {
 public:
  ~HQServerTransportFactory() override {
  }

  HQServerTransportFactory(folly::SocketAddress localAddr,
                           const std::string& version,
                           const std::chrono::milliseconds txnTimeout,
                           std::string qLoggerPath,
                           bool prettyJson,
                           folly::Optional<uint64_t> prChunkSize = folly::none,
                           folly::Optional<uint64_t> prChunkDelayMs = folly::none)
      : localAddr_(std::move(localAddr)),
        txnTimeout_(txnTimeout),
        version_(version),
        qLoggerPath_(std::move(qLoggerPath)),
        prettyJson_(prettyJson),
        prChunkSize_(prChunkSize),
        prChunkDelayMs_(prChunkDelayMs) {
  }

  quic::QuicServerTransport::Ptr make(
      folly::EventBase* evb,
      std::unique_ptr<folly::AsyncUDPSocket> socket,
      const folly::SocketAddress& /* peerAddr */,
      std::shared_ptr<const fizz::server::FizzServerContext>
          ctx) noexcept override {
    // Session controller is self owning
    auto hqSessionController =
        new HQSessionController(version_, prChunkSize_, prChunkDelayMs_);
    std::shared_ptr<FileQLogger> qLogger;
    if (!qLoggerPath_.empty()) {
      qLogger = std::make_shared<quic::FileQLogger>();
      hqSessionController->setQLoggerInfo(qLogger, qLoggerPath_, prettyJson_);
    }
    auto session = hqSessionController->createSession(txnTimeout_);
    CHECK(evb == socket->getEventBase());
    auto transport =
        quic::QuicServerTransport::make(evb, std::move(socket), *session, ctx);
    if (!qLoggerPath_.empty()) {
      transport->setQLogger(qLogger);
    }
    hqSessionController->startSession(transport);
    return transport;
  }

 private:
  folly::SocketAddress localAddr_;
  std::chrono::milliseconds txnTimeout_;
  std::string version_;
  std::string qLoggerPath_;
  bool prettyJson_;
  folly::Optional<uint64_t> prChunkSize_;
  folly::Optional<uint64_t> prChunkDelayMs_;
};

class HQServer {
 public:
  explicit HQServer(
      const std::string& host,
      uint16_t port,
      const std::string& version,
      const std::chrono::milliseconds txnTimeout,
      quic::TransportSettings transportSettings = quic::TransportSettings(),
      folly::Optional<quic::QuicVersion> draftVersion = folly::none,
      bool useDraftFirst = true,
      std::string qLoggerPath = "",
      bool prettyJson = true,
      folly::Optional<uint64_t> prChunkSize = folly::none,
      folly::Optional<uint64_t> prChunkDelayMs = folly::none)
      : host_(host),
        port_(port),
        txnTimeout_(txnTimeout),
        server_(quic::QuicServer::createQuicServer()),
        qLoggerPath_(std::move(qLoggerPath)),
        prettyJson_(prettyJson) {
    localAddr_.setFromHostPort(host_, port_);
    server_->setCongestionControllerFactory(
        std::make_shared<DefaultCongestionControllerFactory>());
    server_->setTransportSettings(transportSettings);
    server_->setQuicServerTransportFactory(std::make_unique<HQServerTransportFactory>(
        localAddr_, version, txnTimeout_, qLoggerPath_, prettyJson_, prChunkSize, prChunkDelayMs));
    server_->setQuicUDPSocketFactory(
        std::make_unique<QuicSharedUDPSocketFactory>());
    server_->setHealthCheckToken("health");

    std::vector<QuicVersion> versions;
    if (useDraftFirst && draftVersion) {
      versions.push_back(*draftVersion);
    }
    versions.push_back(QuicVersion::MVFST);
    versions.push_back(QuicVersion::MVFST_OLD);
    if (!useDraftFirst && draftVersion) {
      versions.push_back(*draftVersion);
    }
    server_->setSupportedVersion(std::move(versions));
  }

  static std::shared_ptr<const fizz::server::FizzServerContext>
  createFizzContext(const std::string& certfile,
                    const std::string& keyfile,
                    fizz::server::ClientAuthMode clientAuth =
                        fizz::server::ClientAuthMode::None) {
    std::string certData = kDefaultCertData;
    if (!certfile.empty()) {
      folly::readFile(certfile.c_str(), certData);
    }
    std::string keyData = kDefaultKeyData;
    if (!keyfile.empty()) {
      folly::readFile(keyfile.c_str(), keyData);
    }
    auto cert = fizz::CertUtils::makeSelfCert(certData, keyData);
    auto certManager = std::make_unique<fizz::server::CertManager>();
    certManager->addCert(std::move(cert), true);

    auto serverCtx = std::make_shared<fizz::server::FizzServerContext>();
    serverCtx->setCertManager(std::move(certManager));
    auto ticketCipher = std::make_shared<fizz::server::AeadTicketCipher<
        fizz::OpenSSLEVPCipher<fizz::AESGCM128>,
        fizz::server::TicketCodec<fizz::server::CertificateStorage::X509>,
        fizz::HkdfImpl<fizz::Sha256>>>();
    std::array<uint8_t, 32> ticketSeed;
    folly::Random::secureRandom(ticketSeed.data(), ticketSeed.size());
    ticketCipher->setTicketSecrets({{folly::range(ticketSeed)}});
    serverCtx->setTicketCipher(ticketCipher);
    serverCtx->setClientAuthMode(clientAuth);
    serverCtx->setSupportedAlpns({"h1q-fb",
                                  "h1q-fb-v2",
                                  proxygen::kH3FBCurrentDraft,
                                  proxygen::kH3CurrentDraft,
                                  proxygen::kHQCurrentDraft});
    serverCtx->setSendNewSessionTicket(false);
    serverCtx->setEarlyDataFbOnly(false);
    serverCtx->setVersionFallbackEnabled(false);

    fizz::server::ClockSkewTolerance tolerance;
    tolerance.before = std::chrono::minutes(-5);
    tolerance.after = std::chrono::minutes(5);

    std::shared_ptr<fizz::server::ReplayCache> replayCache =
        std::make_shared<fizz::server::AllowAllReplayReplayCache>();

    serverCtx->setEarlyDataSettings(true, tolerance, std::move(replayCache));

    return serverCtx;
  }

  void setTlsSettings(const std::string& certfile,
                      const std::string& keyfile,
                      fizz::server::ClientAuthMode clientAuth) {
    server_->setFizzContext(createFizzContext(certfile, keyfile, clientAuth));
  }

  void start() {
    server_->start(localAddr_, std::thread::hardware_concurrency());
  }

  void run() {
    eventbase_.loopForever();
  }

  const folly::SocketAddress getAddress() const {
    server_->waitUntilInitialized();
    auto boundAddr = server_->getAddress();
    LOG(INFO) << "HQ server started at: " << boundAddr.describe();
    return boundAddr;
  }

  void stop() {
    server_->shutdown();
    eventbase_.terminateLoopSoon();
  }

  void rejectNewConnections(bool reject) {
    server_->rejectNewConnections(reject);
  }

 private:
  std::string host_;
  uint16_t port_;
  folly::SocketAddress localAddr_;
  std::chrono::milliseconds txnTimeout_;
  folly::EventBase eventbase_;
  std::shared_ptr<quic::QuicServer> server_;
  folly::Baton<> cv_;
  std::string qLoggerPath_;
  bool prettyJson_{true};
};

class H2Server {
  class SampleHandlerFactory : public proxygen::RequestHandlerFactory {
   public:
    void onServerStart(folly::EventBase* /*evb*/) noexcept override {}
    void onServerStop() noexcept override {}
    proxygen::RequestHandler* onRequest(
        proxygen::RequestHandler*,
        proxygen::HTTPMessage* msg) noexcept override {
      return new proxygen::HTTPTransactionHandlerAdaptor(
          Dispatcher::getRequestHandler(msg, "1.1"));
    }
  };
 public:
  static std::thread run(folly::SocketAddress addr,
                  std::string& certFile,
                  std::string& keyFile,
                  uint32_t connFlowControl,
                  uint32_t streamFlowControl) {
    std::vector<proxygen::HTTPServer::IPConfig> IPs = {
      {addr, proxygen::HTTPServer::Protocol::HTTP2}
    };

    proxygen::HTTPServerOptions options;
    // This is fine for now.
    options.threads = 1;
    options.idleTimeout = std::chrono::milliseconds(60000);
    options.shutdownOn = {SIGINT, SIGTERM};
    options.enableContentCompression = false;
    options.handlerFactories = proxygen::RequestHandlerChain()
      .addThen<SampleHandlerFactory>()
      .build();
    options.initialReceiveWindow = uint32_t(streamFlowControl);
    options.receiveStreamWindowSize = uint32_t(connFlowControl);
    options.receiveSessionWindowSize = connFlowControl;
    options.h2cEnabled = false;
    wangle::SSLContextConfig sslCfg;
    sslCfg.isDefault = true;
    if (!certFile.empty() && !keyFile.empty()) {
      sslCfg.setCertificate(certFile, keyFile, "");
    } else {
      sslCfg.setCertificateBuf(kDefaultCertData, kDefaultKeyData);
    }
    sslCfg.setNextProtocols({"h2"});
    IPs[0].sslConfigs.emplace_back(sslCfg);


    // Start HTTPServer mainloop in a separate thread
    std::thread t([IPs=std::move(IPs), options=std::move(options)] () mutable  {
        {
          proxygen::HTTPServer server(std::move(options));
          server.bind(IPs);
          server.start();
        }
        // HTTPServer traps the SIGINT.  resignal HQServer
        raise(SIGINT);
      });

    return t;
  }
};


}} // namespace quic::samples
