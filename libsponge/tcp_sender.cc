#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _segments_flight()
    , _window_size(1) 
    , _retransmission_timeout(retx_timeout)
    , _nowtime(0) 
    , _consecutive_retransmissions(0) 
    , _syn_send(false) {}

uint64_t TCPSender::bytes_in_flight() const { 
    uint64_t res = 0;
    for (const pair<uint64_t, TCPSegment> &p : _segments_flight) 
        { res += p.second.length_in_sequence_space();}
    return res;
}

void TCPSender::fill_window() {
    // The TCPSender is asked to fill the window : it reads from its input ByteStream and
    // sends as many bytes as possible in the form of TCPSegments, as long as there are new
    // bytes to be read and space available in the window.
    if (!_syn_send) {
        TCPSegment seg;
        seg.header().seqno = WrappingInt32(_next_seqno + _isn.raw_value());
        seg.header().syn = true;
        _send_seg(seg);

        _syn_send = true;
    } else {
        if (_stream.buffer_empty()) return;
        // You’ll want to make sure that every TCPSegment you send fits fully inside the receiver’s
        // window. Make each individual TCPSegment as big as possible, but no bigger than the
        // value given by TCPConfig::MAX PAYLOAD SIZE (1452 bytes).

        // You can use the TCPSegment::length_in_sequence_space() method to count the total
        // number of sequence numbers occupied by a segment. Remember that the SYN and
        // FIN flags also occupy a sequence number each, which means that they occupy space in
        // the window

        // checkout the max bytes can be setted in the TCPSegment struct
        TCPSegment seg;
        seg.header().seqno = WrappingInt32(_next_seqno + _isn.raw_value());
        size_t header_sz = seg.length_in_sequence_space();
        // send_size
        size_t data_sz = std::min(_window_size - header_sz, TCPConfig::MAX_PAYLOAD_SIZE);
        if (data_sz != 0) {
            data_sz = std::min(data_sz, _stream.buffer_size());
            seg.payload() = _stream.peek_output(data_sz);
            _stream.pop_output(data_sz);
        }
        _send_seg(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // A segment is received from the receiver, conveying the new left (= ackno) and right (=
    // ackno + window size) edges of the window. The TCPSender should look through its
    // collection of outstanding segments and remove any that have now been fully acknowl-
    // edged (the ackno is greater than all of the sequence numbers in the segment). The
    // TCPSender should fill the window again if new space has opened up.

    // delete the _segment_queue that seqno is smaller than ackno
    uint64_t uw = unwrap(ackno, _isn, 0);
    uint64_t real_seqno = uw - 1; 
    while (!_segments_flight.empty() && uw != 0) {
        uint64_t first_seqno = unwrap(_segments_flight.begin()->second.header().seqno, _isn, 0x0);
        if (first_seqno > real_seqno) break;

        _segments_flight.pop_front();
    }

    // reset the window size...
    _window_size = window_size;

    // reset it to zero. 
    _consecutive_retransmissions = 0;
} 

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // Time has passed — a certain number of milliseconds since the last time this method
    // was called. The sender may need to retransmit an outstanding segment.
    _nowtime += ms_since_last_tick;

    // If tick is called and the retransmission timer has expired
    // (a) Retransmit the earliest (lowest sequence number) segment that hasn’t been fully
    // acknowledged by the TCP receiver. You’ll need to be storing the outstanding
    // segments in some internal data structure that makes it possible to do this.
    if (_segments_flight.empty()) return;


    // (b) If the window size is nonzero:
    //      i. Keep track of the number of consecutive retransmissions, and increment it
    //      because you just retransmitted something. Your TCPConnection will use this
    //      information to decide if the connection is hopeless (too many consecutive
    //      retransmissions in a row) and needs to be aborted.
    //      ii. Double the value of RTO. This is called “exponential backoff”—it slows down
    //      retransmissions on lousy networks to avoid further gumming up the works.
    uint64_t first_send_time = _segments_flight.front().first;
    if (_nowtime - first_send_time > _retransmission_timeout) {
        if (_window_size != 0) {
            // pop the segment, and resend it 
            TCPSegment seg = _segments_flight.front().second; 
            _segments_flight.pop_front();

            _segments_out.push(seg);
            _segments_flight.front().first = _nowtime;
            _retransmission_timeout <<= 1;
            ++_consecutive_retransmissions;
        }
    }

    // (c) Reset the retransmission timer and start it such that it expires after RTO millisec-
    // onds (taking into account that you may have just doubled the value of RTO!).
} 

unsigned int TCPSender::consecutive_retransmissions() const 
    { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    // The TCPSender should generate and send a TCPSegment
    // that has zero length in sequence space, and with the sequence number set correctly.
    // This is useful if the owner (the TCPConnection that you’re going to implement next
    // week) wants to send an empty ACK segment.
    TCPSegment seg;
    seg.header().seqno = WrappingInt32(_next_seqno + _isn.raw_value());
    _next_seqno += seg.length_in_sequence_space();
    // Note: a segment like this one, which occupies no sequence numbers, doesn’t need to be
    // kept track of as “outstanding” and won’t ever be retransmitted.
}

void TCPSender::_send_seg(const TCPSegment &seg) {
    // push to the queue... 
    _segments_out.push(seg);

    // setting the next_seq and the set for ack
    _segments_flight.push_back({_nowtime, seg});

    _next_seqno += seg.length_in_sequence_space();
}
