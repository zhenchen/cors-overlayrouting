#ifndef _JITTER_H_
#define _JITTER_H_

#include <list>

class RTPDataFrame {
 public:
  RTPDataFrame(int size = 2048);
  RTPDataFrame(const RTPDataFrame &frame);
  ~RTPDataFrame();

  uint8_t *GetPointer() const { return ptr_; }

  int GetSize() const { return size_; }
  void SetSize(int size) { size_ = size; }

  uint16_t GetSequenceNumber() const;
  uint32_t GetTimestamp() const;
  uint32_t GetSSRC() const;
  bool GetMarker() const { return (ptr_[1] & 0x80) != 0; }
 
  int Compare(const RTPDataFrame &frame) const;
  int Compare(uint16_t sequence_number) const;
  
  bool operator<(const RTPDataFrame &frame) const
    { return Compare(frame) < 0; }
  bool operator<(uint16_t sequence_number) const
    { return Compare(sequence_number) < 0; }

  bool operator<=(const RTPDataFrame &frame) const
    { return Compare(frame) <= 0; }
  bool operator<=(uint16_t sequence_number) const
    { return Compare(sequence_number) <= 0; }

  static const int RTP_HEADER_LEN = 12;

 private:
  uint8_t *ptr_;
  int size_;
};

class JitterBuffer {
 public:
  JitterBuffer();
  ~JitterBuffer();

  int Write(const RTPDataFrame &frame);
  RTPDataFrame *Read(uint32_t timestamp, int *finished);
  bool Empty() const
    { return buffer_.empty(); }
  int Size() const
    { return buffer_.size(); }
  
 private:
  uint32_t ConvertTimestamp(uint32_t timestamp);
  
  std::list<RTPDataFrame> buffer_;
  int recv_packets_;
  uint16_t sequence_deadline_;
  uint32_t timestamp_deadline_;
};

#endif
