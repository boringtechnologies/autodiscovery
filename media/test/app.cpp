#include <arpa/inet.h>
#include <sys/socket.h>

#include <cstdint>
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
    device (optional):\n"

static void write_callback(int sock, const std::string &addr,
                           const uint16_t port, const uint8_t *payload,
                           size_t len) {
    struct sockaddr_in server;

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(addr.c_str());

    int ret;
    if ((ret = sendto(sock, payload, len, 0, (struct sockaddr *)&server,
                      sizeof(server))) < 0) {
        std::cout << "Failed to send buf" << std::endl;
    }
    if (len != ret)
        std::cout << "(tried, send) " << len << "," << ret << " bytes"
                  << std::endl;
}

void sender_cli(media::Sender &sndr, int sock) {
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
                sndr.addReceiver(rcvr_id, [=](const uint8_t *data, size_t len) {
                    write_callback(sock, host, port, data, len);
                });
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

void receiver_thread(media::Receiver &rx, const std::string &addr,
                     uint16_t port) {
    int sock_desc;
    struct sockaddr_in server_addr, client_addr;
    uint8_t client_msg[2000] = {0};
    struct timeval read_timeout;
    socklen_t client_struct_length = sizeof(client_addr);

    sock_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (sock_desc < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(addr.c_str());

    if (bind(sock_desc, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
        0) {
        std::cerr << "Couldn't bind to the port\n" << std::endl;
        return;
    }
    while (true) {
        int ret = 0;
        if ((ret = recvfrom(sock_desc, client_msg, sizeof(client_msg), 0,
                            (struct sockaddr *)&client_addr,
                            &client_struct_length)) < 0) {
            std::cerr << "Couldnt receiver from client \n";
            continue;
        }

        // std::cout << "Received " << ret << " from "
        //           << inet_ntoa(client_addr.sin_addr) << ":"
        //           << ntohs(client_addr.sin_port) << std::endl;
        // std::cout << "-----------------------------------" << std::endl;
        // for (const auto &item : client_msg) {
        //     std::cout << std::hex << int(item);
        // }
        rx.push(client_msg, ret);
        // std::cout << "-----------------------------------" << std::endl;
    }
    close(sock_desc);
}

int main(int argc, char **argv) {
    media::ClockInfo clock;
    media::MediaType type = media::MediaType::Video;
    media::CaptureDevice dev;

    if (argc < 2) {
        std::cerr << "Usage:" << std::endl;
        std::cerr << argv[0] << APP_USAGE << std::endl;
        exit(-1);
    }

    if (argc > 2) {
        switch (argv[2][0]) {
            case 'v':
                type = media::MediaType::Video;
                break;

            case 'a':
                type = media::MediaType::Audio;
                break;

            default:
                std::cerr << "Unknown media type " << argv[2] << std::endl;
                std::exit(-1);
        }
    }

    if (argc == 4) {
        dev.name = std::string(argv[3]);
    }

    char mode = argv[1][0];
    if (mode == 's') {
        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        media::Sender sndr("sender", type, dev, clock);
        sndr.start();

        std::thread cli(sender_cli, std::ref(sndr), sock);
        cli.join();
        close(sock);

    } else if (mode == 'r') {
        uint16_t port;
        media::PlaybackDevice dev;
        media::ClockInfo clock;
        std::chrono::duration<int> delay;

        std::cout << "Using default address 0.0.0.0. please provide udp port "
                  << std::endl;
        std::cin >> port;
        std::string addr = "0.0.0.0";

        media::Receiver recvr("receiver", type, dev, clock, delay);

        recvr.start();
        std::thread th(receiver_thread, std::ref(recvr), addr, port);

        char key;
        do {
            std::cout << "Press 'q' to quit" << std::endl;
            std::cin >> key;
        } while (key != 'q');

        // TODO cancel 'th' thread gracefully
        recvr.stop();
    } else {
        std::cerr << "Invalid mode!!" << std::endl;
        std::cerr << "Usage:" << std::endl;
        std::cerr << argv[0] << APP_USAGE << std::endl;
    }

    std::cout << "exiting..";
}
