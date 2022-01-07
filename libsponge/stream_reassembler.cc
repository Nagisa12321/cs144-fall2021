#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`
#include <iostream>
#include <algorithm>
using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) 
    : _output(capacity), 
      _capacity(capacity),
      _deq(_capacity, INT32_MIN),
      _first_idx(0),
      _unassembled(0),
      _eof_idx(-1)
      {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t real_index = index;
    // fix _deq
    size_t sz = _deq.size();
    for (size_t i = 0; i < _capacity - _output.buffer_size() - sz; ++i)
        _deq.push_back(INT32_MIN);

    string real_string = data;
    if (real_index <= _first_idx) {
        if (data.size() <= _first_idx - real_index) {
            if (eof) _output.end_input();
            return;
        }
        real_string = real_string.substr(_first_idx - real_index);
        real_index = _first_idx;
    }
    if (eof) { _eof_idx = index + data.size() - 1; }

    // copy the realstring to _deq
    real_string = real_string.substr(0, min(_first_idx + _capacity - _output.buffer_size() - real_index, real_string.size()));
    for (size_t i = 0; i < real_string.size(); ++i) {
        if (_deq[real_index - _first_idx + i] == INT32_MIN)
            { ++_unassembled; }
    }
    copy_n(real_string.begin(), real_string.size(), _deq.begin() + real_index - _first_idx);

    // push the string to the stream. 
    string push_stream;
    bool end_output = false;
    while (!_deq.empty() && _deq.front() != INT32_MIN) {
        push_stream.push_back(static_cast<char>(_deq.front()));
        _deq.pop_front();
        if (_first_idx == _eof_idx)
            { end_output = true; }

        ++_first_idx;
        --_unassembled;
    }
    if (!push_stream.empty())
        _output.write(push_stream);
    if (end_output)
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled; }

bool StreamReassembler::empty() const { return _unassembled == 0; }

string StreamReassembler::_real_string(const string &data, size_t index) const {
    string real_string = data;
    if (index <= _first_idx) {
        if (data.size() <= _first_idx - index) { return ""; }
        real_string = real_string.substr(_first_idx - index);
    } else if (index + real_string.size() >= _capacity)
        real_string = real_string.substr(0, _capacity - index);
    return real_string;
}