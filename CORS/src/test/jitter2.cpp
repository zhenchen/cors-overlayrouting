#include "jitter2.h"
#include <iostream>

/////////////////////////////////////////////////////////
// RTPDataFrame
//
RTPDataFrame::RTPDataFrame(int size)
  : size_(size) {
  ptr_ = new uint8_t[size];
  memset(ptr_, 0, size * sizeof(uint8_t));
  memset(ptr_, 0x80, 1);
}

RTPDataFrame::RTPDataFrame(const RTPDataFrame &frame) {
  size_ = frame.GetSize();
  ptr_ = new uint8_t[size_];
  memcpy(ptr_, frame.GetPointer(), size_);
}

RTPDataFrame::~RTPDataFrame() {
  delete[] ptr_;
}

uint16_t RTPDataFrame::GetSequenceNumber() const {
  return (ptr_[2] << 8) + ptr_[3];
}

uint32_t RTPDataFrame::GetTimestamp() const {
  return (ptr_[4] << 24) + (ptr_[5] << 16) + (ptr_[6] << 8) + ptr_[7];
}

uint32_t RTPDataFrame::GetSSRC() const {
  return (ptr_[8] << 24) + (ptr_[9] << 16) + (ptr_[10] << 8) + ptr_[11];
}

int RTPDataFrame::Compare(const RTPDataFrame &frame) const {
  return this->GetSequenceNumber() - frame.GetSequenceNumber();
}

int RTPDataFrame::Compare(uint16_t sequence_number) const {
  return this->GetSequenceNumber() - sequence_number;
}

////////////////////////////////////////////////////////
// JitterBuffer
//
JitterBuffer::JitterBuffer(int min_length) 
  : min_length_(min_length) {
  recv_packets_ = 0;
  sequence_deadline_ = 0;
  timestamp_deadline_ = 0;
  buffer_.clear();
  bench_.clear();
}

JitterBuffer::~JitterBuffer() {
  buffer_.clear();
  bench_.clear();
}

int JitterBuffer::Write(const RTPDataFrame &frame, int path) {
  std::cout << "==========================================" << std::endl;
  if (path == 0) {
    std::cout << "direct frame: ";
  } else {
    std::cout << "Indirect frame: ";
  }
  std::cout << frame.GetSequenceNumber()
    << " " << frame.GetTimestamp() << "("
    << ConvertTimestamp(frame.GetTimestamp()) << ")" << std::endl;
  std::cout << "buffer: \n\tpackets_in = " << buffer_.size()
    << "\n\tsequence_deadline_ = " << sequence_deadline_ << std::endl;
  std::cout << "==========================================" << std::endl;

  if (path != 0) { // from indirect path
    uint16_t seq = frame.GetSequenceNumber();
    if (seq < sequence_deadline_)
      return 0;
    bench_.insert(std::pair<uint16_t, RTPDataFrame>(seq, frame));
    return 1;
  }

  // the following handles packets from direct path
  if (recv_packets_ == 0) {
    buffer_.push_back(frame);
    sequence_deadline_ = frame.GetSequenceNumber();
    recv_packets_++;
    return 1;
  }
  
  // a too-late-to-enqueue frame 
  if (frame < sequence_deadline_ ||
      ConvertTimestamp(frame.GetTimestamp()) < timestamp_deadline_)
    return 0;

  std::list<RTPDataFrame>::iterator it = --buffer_.end();
  for (; it != --buffer_.begin(); --it) {
    if (*it <= frame)
      break;
  }
  if (it == --buffer_.begin() || *it < frame) {
    buffer_.insert(++it, frame);
    recv_packets_++;
    return 1;
  } else {
    // a repeated frame
    return 0;
  }
}

RTPDataFrame *JitterBuffer::Read(uint32_t timestamp, int *finished) {
  // timestamp_deadline_ = timestamp + 1;
  
  // timestamp == 0xffffffff means clearing all packets in buffer & bench
  // despite what their timestamps are
  if (timestamp == 0xffffffff && !bench_.empty()) {
    while (!bench_.empty()) {
      RTPDataFrame frame(bench_.begin()->second);
      if (frame.GetSequenceNumber() >= sequence_deadline_) {
        std::list<RTPDataFrame>::iterator it = --buffer_.end();
        for (; it != --buffer_.begin(); --it) {
          if (*it <= frame)
            break;
        }
        if (it == --buffer_.begin() || *it < frame) {
          buffer_.insert(++it, frame);
          recv_packets_++;
        }
      }
      bench_.erase(bench_.begin());
    }
  }

  std::cout << "+++++++++++++++++++++++++++++++++++++++++" << std::endl;
  if (buffer_.empty()) {
    std::cout << "empty buffer()" << std::endl;
    *finished = 1;
    return NULL;
  }

  std::cout << "sequence_deadline: " << sequence_deadline_ << std::endl;
  std::cout << "buffer_head: " << buffer_.front().GetSequenceNumber() 
    << " " << buffer_.front().GetTimestamp() 
    << "(" << ConvertTimestamp(buffer_.front().GetTimestamp()) << ")"
    << std::endl;
  std::cout << "buffer_tail: " << buffer_.back().GetSequenceNumber()
    << " " << buffer_.back().GetTimestamp()
    << "(" << ConvertTimestamp(buffer_.back().GetTimestamp()) << ")"
    << std::endl;
  
  RTPDataFrame *pframe = NULL;
  uint32_t last_timestamp = ConvertTimestamp(buffer_.back().GetTimestamp());

  if (buffer_.front().GetSequenceNumber() == sequence_deadline_) {
    bench_.erase(buffer_.front().GetSequenceNumber());
    if (timestamp == 0xffffffff || ConvertTimestamp(buffer_.front().GetTimestamp()) + min_length_ <= last_timestamp) {
      pframe = new RTPDataFrame(buffer_.front());
      sequence_deadline_ = pframe->GetSequenceNumber() + 1;
      buffer_.pop_front();
    } else {
      *finished = 1;
      return NULL;
    }
  } else {
    std::map<uint16_t, RTPDataFrame>::iterator it = 
      bench_.find(sequence_deadline_);
    if (it != bench_.end()) {
      // expected packet in bench (received from indirect path)
      if (timestamp == 0xffffffff || ConvertTimestamp(it->second.GetTimestamp()) + min_length_ <= last_timestamp) {
        pframe = new RTPDataFrame(it->second);
        recv_packets_++;
        sequence_deadline_ = pframe->GetSequenceNumber() + 1;
        bench_.erase(it);
      } else {
        *finished = 1;
        return NULL;
      }
    } else {
      // expected packet is lost
      // look for the packet with smallest sequence in buffer & bench
      uint16_t i = sequence_deadline_ + 1;
      bool flag = false;
      for (; i < buffer_.front().GetSequenceNumber(); i++) {
        if ((it = bench_.find(i)) != bench_.end()) {
          flag = true;
          break;
        }
      }
      // i is the smallest sequence number
      if (!flag) {
        // i == buffer_.front().GetSequence()
        bench_.erase(i);
        if (timestamp == 0xffffffff || ConvertTimestamp(buffer_.front().GetTimestamp()) + min_length_ <= last_timestamp) {
          // it's time for playing this packet
          sequence_deadline_ = i + 1;
          pframe = new RTPDataFrame(buffer_.front());
          buffer_.pop_front();
        } else {
          *finished = 1;
          return NULL;
        }
      } else {
        if (timestamp == 0xffffffff || ConvertTimestamp(it->second.GetTimestamp()) + min_length_ <= last_timestamp) {
          sequence_deadline_ = i + 1;
          pframe = new RTPDataFrame(it->second);
          bench_.erase(it);
        } else {
          *finished = 1;
          return NULL;
        }
      }
    }
  }
  
  // check next frame's timestamp, determine whether finished
  if (buffer_.empty()) {
    // reading a group of frames with same timestamp finished
    *finished = 1;
  } else {
    // look for the packet with smallest sequence in buffer & bench
    std::map<uint16_t, RTPDataFrame>::iterator it;
    bool flag = false;
    uint16_t i = sequence_deadline_;
    for (; i < buffer_.front().GetSequenceNumber(); i++) {
      if ((it = bench_.find(i)) != bench_.end()) {
        flag = true;
        break;
      }
    }
    if (!flag) {
      bench_.erase(i);
      if (timestamp == 0xffffffff || ConvertTimestamp(buffer_.front().GetTimestamp()) + min_length_ <= last_timestamp) {
        // not finished yet
        *finished = 0;
      } else {
        *finished = 1;
      }
    } else {
      if (timestamp == 0xffffffff || ConvertTimestamp(it->second.GetTimestamp()) + min_length_ <= last_timestamp) {
        *finished = 0;
      } else {
        *finished = 1;
      }
      // move this packet from bench to buffer
      // make it faster in the next calling of Read()
      buffer_.push_front(it->second);
      recv_packets_++;
      bench_.erase(it);
    }
  }
  return pframe;
}

uint32_t JitterBuffer::ConvertTimestamp(uint32_t timestamp) {
  if (timestamp == 0)
    return 0;

  if (timestamp % 3000 == 0)
    return timestamp / 1000 - 2;
  else
    return timestamp / 1000 + 1;
}
