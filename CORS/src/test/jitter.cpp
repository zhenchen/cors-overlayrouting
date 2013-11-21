#include "jitter.h"
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
JitterBuffer::JitterBuffer() {
  recv_packets_ = 0;
  sequence_deadline_ = 0;
  timestamp_deadline_ = 0;
  buffer_.clear();
}

JitterBuffer::~JitterBuffer() {
  buffer_.clear();
}

int JitterBuffer::Write(const RTPDataFrame &frame) {
  //std::cout << "==========================================" << std::endl;
  //std::cout << "frame: " << frame.GetSequenceNumber()
  //  << " " << frame.GetTimestamp() << "("
  //  << ConvertTimestamp(frame.GetTimestamp()) << ")" << std::endl;
  //std::cout << "buffer: \n\tpackets_in = " << buffer_.size()
  //  << "\n\tsequence_deadline_ = " << sequence_deadline_
  //  << "\n\ttimestamp_deadline_ = " << timestamp_deadline_ << std::endl;
  //std::cout << "==========================================" << std::endl;

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
  //std::cout << "+++++++++++++++++++++++++++++++++++++++++" << std::endl;
  if (buffer_.empty()) {
    //std::cout << "empty buffer()" << std::endl;
    *finished = 1;
    return NULL;
  }

  //std::cout << "buffer_[0]: " << buffer_.front().GetSequenceNumber() << " " << buffer_.front().GetTimestamp() << std::endl;
  if (ConvertTimestamp(buffer_.front().GetTimestamp()) > timestamp) {
    // not yet time for reading buffer_.front()
    //std::cout << "not time for buffer_[0]" << std::endl;
    //std::cout << "+++++++++++++++++++++++++++++++++++++++++" << std::endl;
    *finished = 1;
    return NULL;
  }
  //std::cout << "take out buffer_[0]" << std::endl;
  //std::cout << "+++++++++++++++++++++++++++++++++++++++++" << std::endl;
  RTPDataFrame *pframe = new RTPDataFrame(buffer_.front());
  sequence_deadline_ = pframe->GetSequenceNumber() + 1;
  buffer_.pop_front();
  // check next frame's timestamp, determine whether finished
  if (buffer_.empty() || ConvertTimestamp(buffer_.front().GetTimestamp()) > timestamp)
    // reading a group of frames with same timestamp finished
    *finished = 1;
  else
    // not finished yet
    *finished = 0;

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
