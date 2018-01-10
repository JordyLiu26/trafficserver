/** @file
 *
 *  A brief file description
 *
 *  @section license License
 *
 *  Licensed to the Apache Software Foundation (ASF) under one
 *  or more contributor license agreements.  See the NOTICE file
 *  distributed with this work for additional information
 *  regarding copyright ownership.  The ASF licenses this file
 *  to you under the Apache License, Version 2.0 (the
 *  "License"); you may not use this file except in compliance
 *  with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "QUICFrameDispatcher.h"
#include "QUICDebugNames.h"

static constexpr char tag[] = "quic_frame_handler";

//
// Frame Dispatcher
//

void
QUICFrameDispatcher::add_handler(QUICFrameHandler *handler)
{
  for (QUICFrameType t : handler->interests()) {
    this->_handlers[static_cast<uint8_t>(t)].push_back(handler);
  }
}

QUICErrorUPtr
QUICFrameDispatcher::receive_frames(const uint8_t *payload, uint16_t size, bool &should_send_ack)
{
  std::shared_ptr<const QUICFrame> frame(nullptr);
  uint16_t cursor     = 0;
  should_send_ack     = false;
  QUICErrorUPtr error = QUICErrorUPtr(new QUICNoError());

  while (cursor < size) {
    frame = this->_frame_factory.fast_create(payload + cursor, size - cursor);
    if (frame == nullptr) {
      Debug(tag, "Failed to create a frame (%u bytes skipped)", size - cursor);
      break;
    }
    cursor += frame->size();

    QUICFrameType type = frame->type();

    // TODO: check debug build
    if (type != QUICFrameType::PADDING) {
      Debug(tag, "Received %s frame, size %zu", QUICDebugNames::frame_type(frame->type()), frame->size());
    }

    should_send_ack |= (type != QUICFrameType::PADDING && type != QUICFrameType::ACK);

    std::vector<QUICFrameHandler *> handlers = this->_handlers[static_cast<uint8_t>(type)];
    for (auto h : handlers) {
      error = h->handle_frame(frame);
      // TODO: is there any case to continue this loop even if error?
      if (error->cls != QUICErrorClass::NONE) {
        return error;
      }
    }
  }

  return error;
}