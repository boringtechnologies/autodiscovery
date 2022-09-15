#include <cstdlib>
#include <iostream>
#include <sync.hpp>
#include <thread>

#define USER_OPTS "1. Add Receiver\n2. Remove Receiver\n3. Stop Sender\n\n"

#define APP_USAGE \
    " <mode> <options> <device>\n\
    mode:\n\
    s - Sender\n\
    r - Receiver\n\n\
    type (optional):\n\
    a - Audio\n\
    v - Video\n\
    (default: v)\n\n\
    device (optional):\n\
    (default: /dev/video0)\n"

void ui(media::Sender &sndr) {
    while (true) {
        int input;
        std::cout << "Please provide input" << std::endl;
        std::cout << USER_OPTS << std::endl;
        std::cin >> input;
        switch (input) {
            case 1: {
                std::string host;
                uint16_t port;
                std::string rcvr_id;
                std::cout << "provide host port rcvrid" << std::endl;
                std::cin >> host >> port >> rcvr_id;
                media::ReceiverInfo rcvr = {host, port};
                sndr.addReceiver(rcvr_id, rcvr);
            } break;
            case 2: {
                std::string rcvr_id;
                std::cout << "provide rcvrid" << std::endl;
                std::cin >> rcvr_id;
                sndr.removeReceiver(rcvr_id);
            } break;
            case 3:
                sndr.stop();
                return;
            default:
                std::cout << "invalid command" << std::endl;
                continue;
        }
    }
}

int main(int argc, char **argv) {
    media::ClockInfo clock;
    media::MediaType type = media::MediaType::Video;
    media::CaptureDevice dev = {argc > 3 ? argv[3] : "/dev/video0"};
    if (argc < 2) {
        std::cerr << "Usage:" << std::endl;
        std::cerr << argv[0] << APP_USAGE << std::endl;
        exit(-1);
    }
    char mode = argv[1][0];
    if (argc > 3) {
        switch (argv[2][0]) {
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
    if (mode == 's') {
        media::Sender sndr("sender", type, dev, clock);
        sndr.start();

        std::thread cli(ui, std::ref(sndr));
        cli.join();
    } else if (mode == 'r') {
        media::ReceiverInfo rcvr_info;
        media::PlaybackDevice dev;
        media::ClockInfo clock;
        std::chrono::duration<int> delay;

        std::cout << "Using default address 0.0.0.0. please provide udp port "
                  << std::endl;
        std::cin >> rcvr_info.port;

        media::Receiver recvr("receiver", type, rcvr_info, dev, clock, delay);
        recvr.start();

        char key;
        do {
            std::cout << "Press 'q' to quit" << std::endl;
            std::cin >> key;
        } while (key != 'q');

        recvr.stop();
    }
    std::cout << "exiting..";
}
