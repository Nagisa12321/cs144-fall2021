#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    bool accepted = false;
    // •
    // Set the Initial Sequence Number if necessary. The sequence number of the first-
    // arriving segment that has the SYN flag set is the initial sequence number. You’ll want
    // to keep track of that in order to keep converting between 32-bit wrapped seqnos/acknos
    // and their absolute equivalents. (Note that the SYN flag is just one flag in the header.
    // The same segment could also carry data and could even have the FIN flag set.)
    if (seg.header().syn && !_isn_setted) {
        _isn_setted = true;
        _isn = seg.header().seqno;
        accepted = true;
    }
    // •
    // Push any data, or end-of-stream marker, to the StreamReassembler. If the
    // FIN flag is set in a TCPSegment’s header, that means that the last byte of the payload
    // is the last byte of the entire stream. Remember that the StreamReassembler expects
    // stream indexes starting at zero; you will have to unwrap the seqnos to produce these.
    // 
    if (seg.payload().size() != 0 && _isn_setted) {
        uint64_t checkpoint = _reassembler.stream_out().bytes_read();
        uint64_t tmp = unwrap(seg.header().seqno, _isn, checkpoint);
        // A byte with invalid stream index should be ignored
        if (tmp == 0) return; 
        uint64_t index = tmp - 1;
        string data = seg.payload().copy();
        _reassembler.push_substring(data, index, false);
        accepted = true;
    } 
    if (seg.header().fin && _isn_setted) {
        _reassembler.stream_out().end_input();
        accepted = true;
    }

    if (accepted)
        _seq_len += seg.length_in_sequence_space() - seg.payload().size();
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (!_isn_setted) return optional<WrappingInt32>();
    else return optional<WrappingInt32>(_reassembler.stream_out().bytes_written() + _isn.raw_value() + _seq_len);
}

size_t TCPReceiver::window_size() const 
    { return _capacity - _reassembler.stream_out().buffer_size(); }
