#include <sync.hpp>

int main() {
    synch::CaptureDevice dev = { "/dev/video0"};
    synch::ClockInfo clock;
    synch::ReceiverInfo rcvr = { "0.0.0.0", 5000 };
    synch::Sender sndr("first", synch::MediaType::Video, dev, clock);
    sndr.addReceiver("firstpipe", rcvr);
    sndr.start(); // TODO : blocks on the mainloop, move it a CLI sort of thread 
}