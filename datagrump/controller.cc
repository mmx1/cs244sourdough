#include <iostream>

#include "controller.hh"
#include "timestamp.hh"

using namespace std;

/* Default constructor */
Controller::Controller( const bool debug )
  : debug_( debug )
{}
unsigned int the_window_size = 13;
unsigned int part_of_window = 0;
unsigned int signal = 0;
/* Get current window size, in datagrams */
unsigned int Controller::window_size( void )
{
  /* Default: fixed window size of 100 outstanding datagrams */

  if ( debug_ ) {
    cerr << "At time " << timestamp_ms()
	 << " window size is " << the_window_size << endl;
  }

  return the_window_size;
}

/* A datagram was sent */
void Controller::datagram_was_sent( const uint64_t sequence_number,
				    /* of the sent datagram */
				    const uint64_t send_timestamp )
                                    /* in milliseconds */
{
  /* Default: take no action */

  if ( debug_ ) {
    cerr << "At time " << send_timestamp
	 << " sent datagram " << sequence_number << endl;
  }
}

unsigned int prev_RTT = 0;
unsigned int min_RTT = 0 - 1;
unsigned int overload_wnd = 0;
unsigned int old_window_size = 0;

/* An ack was received */
void Controller::ack_received( const uint64_t sequence_number_acked,
			       /* what sequence number was acknowledged */
			       const uint64_t send_timestamp_acked,
			       /* when the acknowledged datagram was sent (sender's clock) */
			       const uint64_t recv_timestamp_acked,
			       /* when the acknowledged datagram was received (receiver's clock)*/
			       const uint64_t timestamp_ack_received )
                               /* when the ack was received (by sender) */
{
  //cerr << sequence_number_acked << endl;
  //cerr << recv_timestamp_acked << endl;
  unsigned int curr_RTT = timestamp_ack_received - send_timestamp_acked;
  if (overload_wnd){
    overload_wnd--;
    //cerr << overload_wnd << endl;
    // if (!overload_wnd){
    //   the_window_size = old_window_size;
    // }
    prev_RTT = curr_RTT;
    return;
  }
    
  if(prev_RTT) {
    if (prev_RTT < curr_RTT) {
      if (curr_RTT > (min_RTT * 7)){
        //cerr << "*3 cut"<< endl;
        overload_wnd = the_window_size;
        //old_window_size = the_window_size / 2;
        the_window_size = 0;
        //part_of_window = 0;
      } else if (curr_RTT - prev_RTT > (min_RTT / 1.5)){
        //cerr << "/2 cut" << endl;
        overload_wnd = the_window_size;
        the_window_size = the_window_size / 2;
        //part_of_window = 0;
      } else if (curr_RTT - prev_RTT > (min_RTT / 2.5) || curr_RTT > (min_RTT*2)){
        the_window_size = (the_window_size == 0) ? 0 : the_window_size - 1;
        //cerr << "slow reduce" << endl;
        //part_of_window = 0;
      }else {
        part_of_window ++;
        if (part_of_window >= (the_window_size)){
          the_window_size++;
          part_of_window = 0;
        }
        //cerr << "slow grow" << endl;
      }
      
    } else {
      part_of_window +=3;
      if (part_of_window >= (the_window_size)){
        the_window_size++;
        part_of_window = 0;
      }
      //cerr << "fast grow" << endl;
    }
  } 
  prev_RTT = curr_RTT;
  if(curr_RTT < min_RTT){
    min_RTT = curr_RTT;
  }
  // cerr << "Min RTT: " << min_RTT << endl;
  // cerr << "window size: " << the_window_size << endl;
  if ( debug_ ) {
    cerr << "At time " << timestamp_ack_received
	 << " received ack for datagram " << sequence_number_acked
	 << " (send @ time " << send_timestamp_acked
	 << ", received @ time " << recv_timestamp_acked << " by receiver's clock)"
	 << endl;
  }
}

/* How long to wait (in milliseconds) if there are no acks
   before sending one more datagram */
unsigned int Controller::timeout_ms( void )
{
  return 300; /* timeout of one second */
}

void Controller::timeout_occurred (void)
{
  the_window_size = the_window_size / 2;
}