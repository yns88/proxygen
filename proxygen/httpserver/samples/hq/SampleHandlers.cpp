/*
 *  Copyright (c) 2019-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include <string>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <proxygen/httpserver/samples/hq/SampleHandlers.h>

namespace quic { namespace samples {

class WaitReleaseHandler;

std::unordered_map<uint, WaitReleaseHandler*>&
WaitReleaseHandler::getWaitingHandlers() {
  static std::unordered_map<uint, WaitReleaseHandler*> waitingHandlers;
  return waitingHandlers;
}

std::mutex& WaitReleaseHandler::getMutex() {
  static std::mutex mutex;
  return mutex;
}

void WaitReleaseHandler::onHeadersComplete(
    std::unique_ptr<proxygen::HTTPMessage> msg) noexcept {
  VLOG(10) << "WaitReleaseHandler::onHeadersComplete";
  msg->dumpMessage(2);
  path_ = msg->getPath();
  auto idstr = msg->getQueryParam("id");

  if (msg->getMethod() != proxygen::HTTPMethod::GET ||
      idstr == proxygen::empty_string ||
      (path_ != "/wait" && path_ != "/release")) {
    sendErrorResponse("bad request\n");
    return;
  }

  auto id = folly::tryTo<uint>(idstr);
  if (!id.hasValue() || id.value() == 0) {
    sendErrorResponse("invalid id\n");
    return;
  }

  id_ = id.value();

  txn_->setIdleTimeout(std::chrono::milliseconds(120000));
  std::lock_guard<std::mutex> g(getMutex());
  auto& waitingHandlers = getWaitingHandlers();
  if (path_ == "/wait") {
    auto waitHandler = waitingHandlers.find(id_);
    if (waitHandler != waitingHandlers.end()) {
      sendErrorResponse("id already exists\n");
      return;
    }
    waitingHandlers.insert(std::make_pair(id_, this));
    sendOkResponse("waiting\n", false /* eom */);
  } else if (path_ == "/release") {
    auto waitHandler = waitingHandlers.find(id.value());
    if (waitHandler == waitingHandlers.end()) {
      sendErrorResponse("id does not exist\n");
      return;
    }
    waitHandler->second->release();
    waitingHandlers.erase(waitHandler);
    sendOkResponse("released\n", true /* eom */);
  }
}

void WaitReleaseHandler::maybeCleanup() {
  if (path_ == "/wait" && id_ != 0) {
    std::lock_guard<std::mutex> g(getMutex());
    auto& waitingHandlers = getWaitingHandlers();
    auto waitHandler = waitingHandlers.find(id_);
    if (waitHandler != waitingHandlers.end()) {
      waitingHandlers.erase(waitHandler);
    }
  }
}
// ServerPushHandler methods
//

class ServerPushHandler;

std::string gPushResponseBody;

ServerPushHandler::ServerPushHandler(const std::string& version)
    : BaseQuicHandler(version) {
  if (gPushResponseBody.empty()) {
    CHECK(folly::readFile(kPushFileName, gPushResponseBody))
        << "Failed to read push file=" << kPushFileName;
  }
}

void ServerPushHandler::onHeadersComplete(
    std::unique_ptr<proxygen::HTTPMessage> msg) noexcept {
  VLOG(10) << "ServerPushHandler::" << __func__;
  msg->dumpMessage(2);
  path_ = msg->getPath();

  if (msg->getMethod() != proxygen::HTTPMethod::GET) {
    LOG(ERROR) << "Method not supported";
    sendErrorResponse("bad request\n");
    return;
  }

  VLOG(2) << "Received GET request for " << msg->getPath() << " at: "
          << std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now().time_since_epoch()).count();

  std::vector<std::string> pathPieces;
  boost::split(pathPieces, msg->getPath(), boost::is_any_of("/"));
  int responseSize = 0;
  int numResponses = 1;

  if (pathPieces.size() > 2) {
    auto sizeFromPath = folly::tryTo<int>(pathPieces[2]);
    responseSize = sizeFromPath.value_or(0);
    if (responseSize != 0) {
      VLOG(2) << "Requested a response size of " << responseSize;
      gPushResponseBody = std::string(responseSize, 'a');
    }
  }

  if (pathPieces.size() > 3) {
    auto numResponsesFromPath = folly::tryTo<int>(pathPieces[3]);
    numResponses = numResponsesFromPath.value_or(1);
    VLOG(2) << "Requested a repeat count of " << numResponses;
  }

  for (int i = 0; i < numResponses; ++i) {
    VLOG(2) << "Sending push txn " << i << "/" << numResponses;

    // Create a URL for the pushed resource
    auto pushedResourceUrl = folly::to<std::string>(
        msg->getURL(), "/", "pushed", i);

    // Create a pushed transaction and handler
    auto pushedTxn = txn_->newPushedTransaction(&pushTxnHandler_);

    // Send a promise for the pushed resource
    sendPushPromise(pushedTxn, pushedResourceUrl);

    // Send the push response
    sendPushResponse(
        pushedTxn, pushedResourceUrl, gPushResponseBody, true /* eom */);

  }

  // Send the response to the original get request
  sendOkResponse("I AM THE REQUEST RESPONSE AND I AM RESPONSIBLE\n",
                 true /* eom */);
}

void ServerPushHandler::sendPushPromise(proxygen::HTTPTransaction* txn,
                                        const std::string& pushedResourceUrl) {
  VLOG(10) << "ServerPushHandler::" << __func__;
  proxygen::HTTPMessage promise;
  promise.setMethod("GET");
  promise.setURL(pushedResourceUrl);
  promise.setVersionString(version_);
  promise.setIsChunked(true);

  txn->sendHeaders(promise);

  VLOG(2) << "Sent push promise for " << pushedResourceUrl << " at: "
          << std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now().time_since_epoch()).count();
}

void ServerPushHandler::sendPushResponse(proxygen::HTTPTransaction* pushTxn,
                                         const std::string& pushedResourceUrl,
                                         const std::string& pushedResourceBody,
                                         bool eom) {
  VLOG(10) << "ServerPushHandler::" << __func__;
  proxygen::HTTPMessage resp;
  resp.setVersionString(version_);
  resp.setStatusCode(200);
  resp.setStatusMessage("OK");
  resp.setWantsKeepalive(true);
  resp.setIsChunked(true);

  pushTxn->sendHeaders(resp);

  std::string responseStr =
    "I AM THE PUSHED RESPONSE AND I AM NOT RESPONSIBLE: " + pushedResourceBody;
  pushTxn->sendBody(folly::IOBuf::copyBuffer(responseStr));

  VLOG(2) << "Sent push response for " << pushedResourceUrl << " at: "
          << std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now().time_since_epoch()).count();

  if (eom) {
    pushTxn->sendEOM();
    VLOG(2) << "Sent EOM for " << pushedResourceUrl << " at: "
            << std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
  }
}

void ServerPushHandler::sendErrorResponse(const std::string& body) {
  proxygen::HTTPMessage resp;
  resp.setVersionString(version_);
  resp.setStatusCode(400);
  resp.setStatusMessage("ERROR");
  resp.setWantsKeepalive(false);
  txn_->sendHeaders(resp);
  txn_->sendBody(folly::IOBuf::copyBuffer(body));
  txn_->sendEOM();
}

void ServerPushHandler::sendOkResponse(const std::string& body, bool eom) {
  VLOG(10) << "ServerPushHandler::" << __func__ << ": sending " << body.length()
           << " bytes";
  proxygen::HTTPMessage resp;
  resp.setVersionString(version_);
  resp.setStatusCode(200);
  resp.setStatusMessage("OK");
  resp.setWantsKeepalive(true);
  resp.setIsChunked(true);
  txn_->sendHeaders(resp);
  txn_->sendBody(folly::IOBuf::copyBuffer(body));
  if (eom) {
    txn_->sendEOM();
  }
}

void ServerPushHandler::onBody(
    std::unique_ptr<folly::IOBuf> /*chain*/) noexcept {
  VLOG(10) << "ServerPushHandler::" << __func__ << " - ignoring";
}

void ServerPushHandler::onEOM() noexcept {
  VLOG(10) << "ServerPushHandler::" << __func__ << " - ignoring";
}

void ServerPushHandler::onError(const proxygen::HTTPException& error) noexcept {
  VLOG(10) << "ServerPushHandler::onError error=" << error.what();
}
}} // namespace quic::samples
