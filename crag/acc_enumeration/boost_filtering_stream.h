//
// Created by dpantele on 5/25/16.
//

#ifndef ACC_BOOSTFILTERINGSTREAM_H
#define ACC_BOOSTFILTERINGSTREAM_H

#include <memory>
#include <boost/iostreams/filtering_streambuf.hpp>

// movable version of boost::filtering_stream

namespace detail {

struct FilteringIn {
  typedef std::istream Stream;
  typedef boost::iostreams::filtering_istreambuf Buf;
};

struct FilteringOut {
  typedef std::ostream Stream;
  typedef boost::iostreams::filtering_ostreambuf Buf;
};

}


template<typename Direction>
class BoostFilteringStream : public Direction::Stream
{
  std::unique_ptr<typename Direction::Buf> buf_;
 public:
  BoostFilteringStream()
      : Direction::Stream(nullptr)
      , buf_(std::make_unique<typename Direction::Buf>()) {
    Direction::Stream::init(buf_.get());
  }

  BoostFilteringStream(BoostFilteringStream&& other)
      : Direction::Stream(std::move(other))
      , buf_(std::move(other.buf_))
  {
    Direction::Stream::set_rdbuf(buf_.get());
  }

  BoostFilteringStream& operator=(BoostFilteringStream&& other)
  {
    Direction::Stream::operator=(std::move(other));
    buf_ = std::move(other.buf_);
    Direction::Stream::set_rdbuf(buf_.get());

    return *this;
  }


  const std::type_info& component_type(int n) const {
    return buf_->component_type(n);
  }

  template<typename T>
  T* component(int n) const {
    return buf_->template component<T>(n);
  }

  template<typename T>
  void push(const T& t) {
    return buf_->push(t);
  }

  template<typename StreamOrStreambuf>
  void push(StreamOrStreambuf& t) {
    return buf_->push(t);
  }

  void pop() {
    return buf_->pop();
  }

  bool empty() const {
    return buf_->empty();
  }

  typename Direction::Buf::size_type size() const {
    return buf_->size();
  }

  void reset() {
    return buf_->reset();
  }

  bool is_complete() const {
    return buf_->is_complete();
  }

  bool auto_close() const {
    return buf_->auto_close();
  }

  void set_auto_close(bool close) {
    return buf_->set_auto_close(close);
  }
};

typedef BoostFilteringStream<detail::FilteringIn> BoostFilteringIStream;
typedef BoostFilteringStream<detail::FilteringOut> BoostFilteringOStream;

#endif //ACC_BOOSTFILTERINGSTREAM_H
