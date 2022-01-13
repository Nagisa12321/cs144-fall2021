#include "tcp_connection.hh"

#include <iostream>
#include <cassert>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const 
    { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const 
    { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const 
    { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const 
    { return _nowtime - _last_received_time; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // set the last received time
    _last_received_time = _nowtime;

    // if the rst (reset) flag is set, sets both the inbound and outbound streams to the error
    // state and kills the connection permanently. Otherwise it. . .
    if (seg.header().rst) 
        { _kill_connection(); }

    // • gives the segment to the TCPReceiver so it can inspect the fields it cares about on
    // incoming segments: seqno, syn , payload, and fin .
    _receiver.segment_received(seg);

    // • if the ack flag is set, tells the TCPSender about the fields it cares about on incoming
    // segments: ackno and window size.
    if (seg.header().ack) 
        { _sender.ack_received(seg.header().ackno, seg.header().win); }

    // • if the incoming segment occupied any sequence numbers, the TCPConnection makes
    // sure that at least one segment is sent in reply, to reflect an update in the ackno and
    // window size.
    if (seg.length_in_sequence_space() != 0) {
        _sender.send_empty_segment();
        TCPSegment &recv_seg = _sender.segments_out().front();
        
        // I think the ackno() should have value... 
        // get the front from the queue and set this shit...
        assert(_receiver.ackno().has_value());
        recv_seg.header().ackno = _receiver.ackno().value();
        recv_seg.header().win = _receiver.window_size();

        // And then send to internet. 
        _send_segments_to_internet();
    }

    // • There is one extra special case that you will have to handle in the TCPConnection’s
    // segment received() method: responding to a “keep-alive” segment. The peer may
    // choose to send a segment with an invalid sequence number to see if your TCP imple-
    // mentation is still alive (and if so, what your current window is). Your TCPConnection
    // should reply to these “keep-alives” even though they do not occupy any sequence
    // numbers. Code to implement this can look like this:
    // ** For keepalive **
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0)
         and seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    } 
}

bool TCPConnection::active() const 
    { return _connection_killed; }

size_t TCPConnection::write(const string &data) {
    // • any time the TCPSender has pushed a segment onto its outgoing queue, having set the
    // fields it’s responsible for on outgoing segments: (seqno, syn , payload, and fin ).
    size_t written = _sender.stream_in().write(data);
    _sender.fill_window();

    // Just send data to internet... 
    _send_segments_to_internet();

    return written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    // • tell the TCPSender about the passage of time.
    _sender.tick(ms_since_last_tick);

    // • abort the connection, and send a reset segment to the peer (an empty segment with
    // the rst flag set), if the number of consecutive retransmissions is more than an upper
    // limit TCPConfig::MAX RETX ATTEMPTS.
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) { 
        // this is abort the connection... 
        // TODO: need 4 handshake...?
        _kill_connection(); 

        // now send the reset to peer... 
        _sender.send_empty_segment();
        _sender.segments_out().front().header().rst = true;
    }

    // • end the connection cleanly if necessary (please see Section 5).
}

void TCPConnection::end_input_stream() 
    { _sender.stream_in().end_input(); }

void TCPConnection::connect() {
    // Initiate a connection by sending a SYN segment.
    _sender.send_empty_segment();
    _sender.segments_out().front().header().syn = true;

    // After setting, send it to internet 
    _send_segments_to_internet();
} 

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            // TODO: need 4 handshake...?
            _kill_connection(); 

            // now send the reset to peer... 
            _sender.send_empty_segment();
            _sender.segments_out().front().header().rst = true;

            // just send to internet. 
            _send_segments_to_internet();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_kill_connection() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();

    // This is how to kill .... 
    _connection_killed = true;
}

void TCPConnection::_send_segments_to_internet() {
    // Just write all the shit to the _segments_out ? 
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().back();
        _sender.segments_out().pop();

        // • Before sending the segment, the TCPConnection will ask the TCPReceiver for the fields
        // it’s responsible for on outgoing segments: ackno and window size. If there is an ackno,
        // it will set the ack flag and the fields in the TCPSegment.
        if (_receiver.ackno().has_value()) {
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
    }
}