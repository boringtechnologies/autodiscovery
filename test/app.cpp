#include <cstdlib>
#include <sync.hpp>
#include <thread>
#include <iostream>

#define USER_OPTS "1. None\n2. Stop Sender\n3. Add Receiver\n4. Remove Receiver\n\n"

void ui(media::Sender &sndr) {
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

int main(int argc, char **argv) {
    media::ClockInfo clock;
    media::MediaType type = media::MediaType::Video;
    media::CaptureDevice dev = { argc > 2 ? argv[2] : "/dev/video0"};

    if (argc > 2) {
        switch (argv[1][0]) {
            case 'v':
                type = media::MediaType::Video;
                break;

            case 'a':
                type = media::MediaType::Audio;
                break;

            default:
                std::cerr << "Unknown media type " << argv[1] << std::endl;
                std::exit(-1);
        }
    }

    media::Sender sndr("first", media::MediaType::Video, dev, clock);
    sndr.start();

    std::thread cli(ui, std::ref(sndr));

    cli.join();
    std::cout << "exiting..";
}
