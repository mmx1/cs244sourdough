#include <iostream>
#include <array>
#include <algorithm>

#include "controller.hh"
#include "timestamp.hh"

using namespace std;
#define EXPECTED_DELAY 200
#define WINDOW_FLOOR 5

/* Default constructor */
Controller::Controller( const bool debug )
  : debug_( debug ),
    the_window_size_(1),
    window_(),
    window_estimate_(10),
    stored_window(0),
    probe_conn_(false),
    recovery_(false),
    recovery_seqno_(0),
    min_delay_(EXPECTED_DELAY)

{}

/* Get current window size, in datagrams */
unsigned int Controller::window_size( void )
{
  /* Default: fixed window size of 100 outstanding datagrams */
  

  if ( debug_ ) {
    cerr << "At time " << timestamp_ms()
	 << " window size is " << the_window_size_ << endl;
  }

  return the_window_size_;
}

/* A datagram was sent */
void Controller::datagram_was_sent( const uint64_t sequence_number,
				    /* of the sent datagram */
				    const uint64_t __attribute__((unused)) send_timestamp )
                                    /* in milliseconds */
{
  /* Default: take no action */

  // if ( debug_ ) {
  //   cerr << "At time " << send_timestamp
	 // << " sent datagram " << sequence_number << endl;
  // }
  window_[sequence_number] = the_window_size_;
  if(!recovery_){
    recovery_seqno_ = sequence_number;
  }
}

/* An ack was received */
void Controller::ack_received( const uint64_t __attribute__((unused)) sequence_number_acked,
			       /* what sequence number was acknowledged */
			       const uint64_t send_timestamp_acked,
			       /* when the acknowledged datagram was sent (sender's clock) */
			       const uint64_t __attribute__((unused)) recv_timestamp_acked,
			       /* when the acknowledged datagram was received (receiver's clock)*/
			       const uint64_t timestamp_ack_received )
                               /* when the ack was received (by sender) */
{

  if (probe_conn_){
    recovery_ = true;
    the_window_size_ = stored_window;
    window_estimate_ = the_window_size_;
    stored_window = 0;
  }
  probe_conn_ = false;

  uint64_t delay = timestamp_ack_received - send_timestamp_acked;
  if(delay == 0 ) {
    delay = EXPECTED_DELAY;
  }
  min_delay_ = delay < min_delay_ ? delay : min_delay_;

  if (recovery_ ) {
    if(sequence_number_acked <= recovery_seqno_) {
      if (sequence_number_acked % the_window_size_ == 0) {
        the_window_size_ ++;
        window_estimate_ = the_window_size_;
      }
      return;
    }else{
      recovery_ = false;
    }
  }

  if (delay <= min_delay_ * 1.5) {
    if(debug_){
      cerr << "Min delay of " << min_delay_ << " after receiving delay " << delay << "incrementing "<< endl;
    }

    the_window_size_++;
    window_estimate_ = window_estimate_ < the_window_size_ ? the_window_size_ : window_estimate_;
  }else{
    if( the_window_size_ >= window_estimate_ ) { //if increasing
      window_estimate_ = window_[sequence_number_acked];
      if(window_estimate_ == 0){
        cerr << "Lookup failed" << endl;
        window_estimate_ = WINDOW_FLOOR;
      }

      the_window_size_ = window_estimate_ * .75; //rate limit recovery
      the_window_size_ = the_window_size_ > 0 ? the_window_size_ : WINDOW_FLOOR;

      if(debug_){
        cerr << "Min delay of " << min_delay_ << " after receiving delay " << delay << "rate recovery "<< endl;
      }

    }else {
      window_estimate_ *= .5;
      if(debug_){
        cerr << "Min delay of " << min_delay_ << " after receiving delay " << delay << "reset estimate "<< endl;
      }
    }
    window_estimate_ = window_estimate_ > 0 ? window_estimate_ : WINDOW_FLOOR;
  }
}

/* How long to wait (in milliseconds) if there are no acks
   before sending one more datagram */
unsigned int Controller::timeout_ms( void )
{
  float factor = probe_conn_ ? 1.2 : 6;
  return min_delay_ * factor; /* timeout of one second */
}

void Controller::timeout_occurred( void ){
  probe_conn_ = true;
  if (!stored_window){
    stored_window = the_window_size_ * 0.625;
    stored_window = stored_window > WINDOW_FLOOR ? stored_window : 1;  
  }
  //the_window_size_ = the_window_size_ > WINDOW_FLOOR ? the_window_size_ : WINDOW_FLOOR;
  the_window_size_ = 0;
  // uint64_t delay =  delays_.rbegin() != delays_.rend() ? delays_.rbegin()->first : 0;
  // cerr << "TIMEOUT OCCURRED at time " << timestamp_ms() << " with delay " << delay << "stored window " << stored_window << endl;
}