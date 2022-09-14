#include <sync.hpp>
#include <thread>
#include <iostream>

#define USER_OPTS "1. None\n2. Stop Sender\n3. Add Receiver\n4. Remove Receiver\n\n"

void ui(media::Sender sndr) {
    while (true) {
        int input;
        std::cout << "Please provide input"<< std::endl;
        std::cout << USER_OPTS << std::endl;
        std::cin >> input;
        switch (input) {
            case 2:
                sndr.stop();
                return;
            case 3:
            {
                std::string host;
                uint16_t port;
                std::string rcvr_id;
                std::cout << "provide host port rcvrid" << std::endl;
                std::cin >> host >> port >> rcvr_id;
                media::ReceiverInfo rcvr = { host, port };
                sndr.addReceiver(rcvr_id, rcvr);
            }
            break;
            case 4:
            {
                std::string rcvr_id;
                std::cout << "provide rcvrid" << std::endl;
                std::cin >> rcvr_id;
                sndr.removeReceiver(rcvr_id);
            }
            break;
            default:
             std::cout << "invalid command" << std::endl;
            break;
        }
    }   
    
}
void mloop (media::Sender sndr) {
    sndr.start();
}

int main() {
    media::CaptureDevice dev = { "/dev/video2"};
    media::ClockInfo clock;

    media::Sender sndr("first", media::MediaType::Video, dev, clock);
    std::thread loop (mloop, sndr);
    std::thread cli (ui, sndr);
 

    loop.join();
    std::cout << "loop joined" << std::endl;
    cli.join();
    std::cout << "exiting..";
}
