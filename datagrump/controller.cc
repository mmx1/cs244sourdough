#include <iostream>
#include <array>
#include <algorithm>

#include "controller.hh"
#include "timestamp.hh"

using namespace std;
#define INTERVAL_TIME 200000
#define DELAY_FLOOR 100

typedef std::pair<uint64_t, uint64_t> delay_pair;
/* Default constructor */
Controller::Controller( const bool debug )
  : debug_( debug ),
    the_window_size_(5),
    delays_(),
    window_(),
    window_estimate_(10),
    inflight_(),
    arrivals_()
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
void Controller::datagram_was_sent( uint64_t sequence_number,
				    /* of the sent datagram */
				    const uint64_t send_timestamp )
                                    /* in milliseconds */
{
  /* Default: take no action */

  // if ( debug_ ) {
  //   cerr << "At time " << send_timestamp
	 // << " sent datagram " << sequence_number << endl;
  // }
  window_[send_timestamp] = the_window_size_;
  inflight_[sequence_number]++;
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

  //delete ack'ed from inflight 
  auto if_it = inflight_.find(sequence_number_acked);
  if (if_it != inflight_.end() ) {
    inflight_.erase( if_it );
  }

  //add arrival to queue
  auto arr_it = std::lower_bound(arrivals_.begin(), arrivals_.end(), recv_timestamp_acked);
  arrivals_.insert(arr_it, recv_timestamp_acked);
  //trim to last 300 ms
  for (auto it = arrivals_.begin(); it != arrivals_.end(); it++ ) {
    if ( *it + 300 < timestamp_ack_received ) {
      arrivals_.erase(it);
    }else{
      break;
    }
  }
  

  uint64_t delay = timestamp_ack_received - send_timestamp_acked;
  if(delay == 0 ) {
    delay = 100;
  }
  delays_.push_back(delay_pair(delay, timestamp_ack_received));

  if (timestamp_ack_received > INTERVAL_TIME) {
    while( delays_.front().second <  timestamp_ack_received - INTERVAL_TIME ) {
      delays_.pop_front();
    }
  }

  uint64_t min_delay = delay;
  for (auto pr : delays_) {
    min_delay = pr.first < min_delay ? pr.first : min_delay;
  }

  //if low delay or not enough sample size to estimate bandwidth
  
    //window_estimate_ = window_estimate_ < the_window_size_ ? the_window_size_ : window_estimate_;
    // cerr << "At time " << timestamp_ms()
    //       << " window size increment " << the_window_size_ << endl;
  if (delay <= min_delay * 1.25) {

    // cerr << "At time " << timestamp_ms() << " slow start increment to " << the_window_size_  << endl;

    the_window_size_++;

  }else{
    if( timestamp_ack_received > 300 ) {
      unsigned int longCount = arrivals_.size();
      unsigned int shortCount = 0;
      auto arr_rit = arrivals_.rbegin();
      while(arr_rit != arrivals_.rend() && *arr_rit + 150 > timestamp_ack_received ){
        shortCount++;
        arr_rit++;
      }
      // cerr << "At time " << timestamp_ms()
      //       << " long count " << longCount - shortCount 
      //       << " short count " << shortCount << endl;
      //if decreasing, accounting for rounding errors
      if ( shortCount * 2 < longCount) {
        unsigned int firstSample = longCount - shortCount ;
        // firstSample, shortCount, compute the next in the series
        // by if condition, firstSample > longcount
        int nextSample = shortCount - (firstSample - shortCount) * .5; //or 2shortcount - firstSample
        
        //the_window_size_ = shortCount;

        the_window_size_ = nextSample > 0 ? nextSample : 1; //2.25 / .237
        
        // cerr << "At time " << timestamp_ms() << " Set window size to " << the_window_size_ << endl;
      }
    } else if (the_window_size_ > 1) {
      // cerr << "At time " << timestamp_ms() << "delay-based decrease" << the_window_size_ << endl;
      the_window_size_--;
      //  the_window_size_ *= .5;
    }
  } 

}

/* How long to wait (in milliseconds) if there are no acks
   before sending one more datagram */
unsigned int Controller::timeout_ms( void )
{
  return 1000; /* timeout of one second */
}
