#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity) 
    : _m_data(),
      _m_capacity(capacity),
      _m_bytes_w(0),
      _m_bytes_r(0),
      _m_eof(false),
      _m_sz(0),
      _m_writable(true)
    {}

size_t ByteStream::write(const string &data) {
    if (input_ended())
        return data.size();

    std::size_t bw; 
    for (bw = 0; bw < data.size(); ++bw) {
        if (data[bw] == '\0' ||  _m_sz == _m_capacity)
            break;
        _m_data.push_front(data[bw]);
        if (data[bw] != '\0') ++_m_sz;
    }
    _m_bytes_w += bw;
    return bw;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    list<char>::const_iterator it = _m_data.cend();
    list<char>::const_iterator start = _m_data.cbegin();
    string res;
    for (size_t i = 0; i < len; ++i) {
        --it;
        res.push_back(*it);
        if (it == start)
            break;
    }
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    if (len >= _m_sz) {
        if (_m_data.front() == '\0')
            _m_eof = true;
        _m_bytes_r += _m_sz; 
        _m_data.clear();
        _m_sz = 0;
    } else { 
        for (size_t i = 0; i < len; ++i) {
            if (_m_data.back() == '\0') {
                _m_eof = true;
            } else {
                --_m_sz;
            }
            ++_m_bytes_r;
            _m_data.pop_back();
        }
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res = peek_output(len);
    pop_output(len);
    return res;
}

void ByteStream::end_input() { 
    _m_writable = false;
    if (buffer_empty()) 
        _m_eof = true;
    _m_data.push_front('\0'); 
}

bool ByteStream::input_ended() const { return !_m_writable; }

size_t ByteStream::buffer_size() const { return _m_sz; }

bool ByteStream::buffer_empty() const { return _m_sz == 0; }

bool ByteStream::eof() const { return _m_eof; }

size_t ByteStream::bytes_written() const { return _m_bytes_w; }

size_t ByteStream::bytes_read() const { return _m_bytes_r; }

size_t ByteStream::remaining_capacity() const { return _m_capacity - _m_sz; }
