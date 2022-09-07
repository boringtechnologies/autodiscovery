//******************************************************************************
//******************************************************************************

#include "grpc-roc.grpc.pb.h"

#include <iostream>
#include <thread>
#include <list>
#include <atomic>
#include <sstream>

#include <grpcpp/grpcpp.h>

namespace rpcroc
{

//******************************************************************************
//******************************************************************************
class Client
{
public:
    Client(const std::string & id, const std::string & group, std::shared_ptr<grpc::Channel> channel)
        : m_id(id)
        , m_group(group)
        , m_stub(rpcroc::RpcRoc::NewStub(channel))
    {}

    ~Client()
    {
        if (m_reader.joinable())
        {
            unsubscribe();
        }
    }

public:
    std::string id()    const { return m_id; }
    std::string group() const { return m_group; }

    void getPeerList();
    void subscribe(const uint32_t type = 1);
    void unsubscribe();
    void sendMessage(const std::string & to, const std::string & msg);

private:
    std::unique_ptr<rpcroc::RpcRoc::Stub> m_stub;

    std::string m_id;
    std::string m_group;

    std::thread m_reader;
};

//******************************************************************************
//******************************************************************************
void Client::getPeerList()
{
    std::cout << "getPeerList()" << std::endl;

    GetPeerList request;
    request.set_id(m_id);

    grpc::ClientContext context;

    std::unique_ptr<grpc::ClientReader<Peer> > reader(m_stub->getPeerList(&context, request));

    Peer peer;
    std::cout << "Received peer list:" << std::endl;
    while (reader->Read(&peer))
    {
        std::cout << "    <" << peer.id() << "@" << peer.group() << "> type "
                  << peer.type() << std::endl;
    }

    grpc::Status status = reader->Finish();
    if (!status.ok())
    {
        std::cout << "getPeerList finished with error "
                  << status.error_code() << ": " << status.error_message() << std::endl;
        return;
    }
}

//******************************************************************************
//******************************************************************************
void Client::subscribe(const uint32_t type)
{
    std::cout << "subscribe()" << std::endl;

    if (m_reader.joinable())
    {
        std::cout << "already subscribed" << std::endl;
        return;
    }

    m_reader = std::thread([this, type]()
    {
        Peer peer;
        peer.set_id(m_id);
        peer.set_group(m_group);
        peer.set_type(type == 2 ? WRITER :
                      type == 3 ? READER | WRITER :
                                  READER);

        grpc::ClientContext context;
        std::unique_ptr<grpc::ClientReader<Message> > reader(m_stub->subscribe(&context, peer));

        Message m;
        while (reader->Read(&m))
        {
            std::cout << "Message from " << m.from() << " --> " << m.msg() << std::endl;
        }

        grpc::Status status = reader->Finish();
        if (!status.ok())
        {
            std::cout << "subscribe error "
                      << status.error_code() << ": " << status.error_message() << std::endl;
        }
        std::cout << "subscribe finished" << std::endl;
    });
}

//******************************************************************************
//******************************************************************************
void Client::unsubscribe()
{
    std::cout << "unsubscribe()" << std::endl;

    if (!m_reader.joinable())
    {
        std::cout << "already unsubscribed" << std::endl;
        return;
    }

    Peer peer;
    peer.set_id(m_id);
    peer.set_group(m_group);
    peer.set_type(READER | WRITER);

    Empty resp;

    grpc::ClientContext context;
    grpc::Status status = m_stub->unsubscribe(&context, peer, &resp);
    if (!status.ok())
    {
        std::cout << status.error_code() << " : " << status.error_message()
                  << std::endl;
        return;
    }

    if (m_reader.joinable())
    {
        m_reader.join();
    }
}

//******************************************************************************
//******************************************************************************
void Client::sendMessage(const std::string & to, const std::string & msg)
{
    Message m;
    m.set_from(m_id);
    m.set_to(to);
    m.set_msg(msg);

    Empty resp;

    grpc::ClientContext context;
    grpc::Status status = m_stub->sendMessage(&context, m, &resp);
    if (!status.ok())
    {
        std::cout << status.error_code() << " : " << status.error_message()
                  << std::endl;
        return;
    }
}

} // namespace rpcroc

//******************************************************************************
//******************************************************************************
void printHelp()
{
    std::cout << "****************************************" << std::endl
              << "commands list:" << std::endl
              << "  exit" << std::endl
              << "  help" << std::endl
              << "  getPeerInfo" << std::endl
              << "  subscribe user_id|group_id|type (possible r, w, rw)" << std::endl
              << "  unsubscribe" << std::endl
              << "  sendMessage user_id|message" << std::endl
              << "****************************************" << std::endl;
}

//******************************************************************************
//******************************************************************************
std::vector<std::string> parseCommand(const std::string & cmd)
{
    std::istringstream ss(cmd);
    std::string item;
    std::vector<std::string> elems;
    while (std::getline(ss, item, '|'))
    {
        elems.push_back(item);
    }
    return elems;
}

//******************************************************************************
//******************************************************************************
int main(int argc, char * argv[])
{
    if (argc < 2)
    {
        std::cout << "try " << argv[0] << " ip:port <localhost:50051>" << std::endl;
        return 0;
    }

    std::string target = argv[1];

    std::shared_ptr<rpcroc::Client> client; // (new rpcroc::Client(cmds[1], cmds[2], grpc::CreateChannel(cmds[0], grpc::InsecureChannelCredentials())));;

    printHelp();

    while (true)
    {
        std::string command;
        std::cout << "> ";
        std::cin >> command;
        if (command == "exit")
        {
            break;
        }
        else if (command == "help")
        {
            printHelp();
            continue;
        }
        else if (command == "getPeerInfo")
        {
            if (!client)
            {
                client.reset(new rpcroc::Client("none", "none", grpc::CreateChannel(target, grpc::InsecureChannelCredentials())));
            }
            client->getPeerList();
            continue;
        }
        else if (command == "subscribe")
        {
            std::string params;
            std::cin >> params;
            std::vector<std::string> v = parseCommand(params);
            if (v.size() < 2)
            {
                std::cout << "wrong params, try help" << std::endl;
                continue;
            }

            uint32_t type = 1;
            if (v.size() > 2 && v[2].size() > 0)
            {
                type = v[2] == "r" ? 1 :
                       v[2] == "w" ? 2 :
                       v[2] == "rw" ? 3 : 0;
            }

            if (type == 0 || type > 3)
            {
                std::cout << "wrong type, try r, w or rw" << std::endl;
                continue;
            }

            client.reset(new rpcroc::Client(v[0], v[1], grpc::CreateChannel(target, grpc::InsecureChannelCredentials())));
            client->subscribe(type);
        }
        else if (command == "unsubscribe")
        {
            if (client)
            {
                // client->unsubscribe();
                client.reset();
            }
        }
        else if (command == "sendMessage")
        {
            if (!client)
            {
                std::cout << "not subscribed" << std::endl;
                continue;
            }

            std::string params;
            std::cin >> params;
            std::vector<std::string> v = parseCommand(params);
            if (v.size() != 2)
            {
                std::cout << "wrong params, try help" << std::endl;
                continue;
            }

            client->sendMessage(v[0], v[1]);
        }
        else
        {
            std::cout << "wrong, try help" << std::endl;
        }
    }

//    std::string target = "localhost:50051";
//    grpc::CreateChannel(target, grpc::InsecureChannelCredentials());

//    rpcroc::Client client("first", "blabla", grpc::CreateChannel(target, grpc::InsecureChannelCredentials()));


//    client.getPeerList();

//    sleep(1);

//    client.subscribe();

//    sleep(1);

//    client.getPeerList();

//    for (uint32_t i = 0; i < 4; ++i)
//    {
//        client.sendMessage("first", "simple test " + std::to_string(i));
//        sleep(2);
//    }

//    client.unsubscribe();

//    sleep(1);

//    client.getPeerList();

//    client.sendMessage("first", "unsubscribed test");
//    sleep(2);

//    std::cout << "Done!" << std::endl;

//    while (true)
//    {
//        sleep(5);
//    }

    return 0;
}
