//******************************************************************************
//******************************************************************************

#include "grpc-roc.grpc.pb.h"

#include <iostream>
#include <thread>
#include <list>
#include <atomic>

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

public:
//    std::string callOne(const std::string & message);

    void getPeerList();
    void subscribe();
    void unsubscribe();
    void sendMessage(const std::string & to, const std::string & msg);

private:
    std::unique_ptr<rpcroc::RpcRoc::Stub> m_stub;

    std::string m_id;
    std::string m_group;

    std::atomic<bool> m_readerStopper;
    std::thread m_reader;

    std::thread m_messageLoop;

    std::list<Message> m_pendingMessages;
};

//******************************************************************************
//******************************************************************************
//std::string Client::callOne(const std::string & message)
//{
//    OneRequest request;
//    request.set_str(message);

//    OneReply reply;

//    grpc::ClientContext context;
//    grpc::Status status = m_stub->callOne(&context, request, &reply);
//    if (!status.ok())
//    {
//        std::cout << status.error_code() << " : " << status.error_message()
//                  << std::endl;
//        return "FAILED";
//    }

//    return reply.str();
//}

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
void Client::subscribe()
{
    std::cout << "subscribe()" << std::endl;

    if (m_reader.joinable())
    {
        std::cout << "already subscribed" << std::endl;
        return;
    }

    m_readerStopper = false;
    m_reader = std::thread([this]()
    {
        Peer peer;
        peer.set_id(m_id);
        peer.set_group(m_group);
        peer.set_type(READER | WRITER);

        grpc::ClientContext context;
        std::unique_ptr<grpc::ClientReader<Message> > reader(m_stub->subscribe(&context, peer));

        while (!m_readerStopper)
        {
            Message m;
            while (reader->Read(&m))
            {
                std::cout << "Message from " << m.from() << " --> " << m.msg() << std::endl;
            }

            sleep(1);
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

    if (m_reader.joinable())
    {
        m_readerStopper = true;
        m_reader.join();
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

//******************************************************************************
//******************************************************************************
//void Client::runMessagesLoop()
//{
//    std::cout << "runMessagesLoop" << std::endl;

//    // m_messageLoop = std::thread([this]()
//    {
//        std::cout << "runMessagesLoop in thread" << std::endl;

//        grpc::ClientContext context;

//        std::cout << "runMessagesLoop create stream" << std::endl;

//        Empty empty;
//        std::shared_ptr<grpc::ClientReader<Message> > stream(m_stub->getMessages(&context, empty));

////        std::cout << "runMessagesLoop run writer" << std::endl;

////        std::thread writer([stream, this]()
////        {
////            std::cout << "runMessagesLoop in writer" << std::endl;

////            for (uint32_t i = 0; i < 32; ++i)
////            {
////                while (m_pendingMessages.empty())
////                {
////                    sleep(1);
////                }

////                std::cout << "got message" << std::endl;

////                Message m = m_pendingMessages.front();
////                m_pendingMessages.pop_front();

////                stream->Write(m);
////            }
////            stream->WritesDone();
////        });

//        std::cout << "runMessagesLoop run reader" << std::endl;

//        std::thread reader([stream, this]()
//        {
//            std::cout << "runMessagesLoop in reader" << std::endl;

//            Message r;
//            while (stream->Read(&r))
//            {
//                std::cout << "Received message from " << r.from() << " to " << r.to() << " --> " << r.msg() << std::endl;
//            }

//            sleep(1);
//        });

//        std::cout << "runMessagesLoop join" << std::endl;

//        // writer.join();
//        reader.join();

//        std::cout << "runMessagesLoop finish" << std::endl;

//        grpc::Status status = stream->Finish();
//        if (!status.ok())
//        {
//            std::cout << status.error_code() << " : " << status.error_message()
//                      << std::endl;
//        }
//    } // );
//}

} // namespace rpcroc

//******************************************************************************
//******************************************************************************
int main()
{
    std::string target = "localhost:50051";
    grpc::CreateChannel(target, grpc::InsecureChannelCredentials());

    rpcroc::Client client("first", "blabla", grpc::CreateChannel(target, grpc::InsecureChannelCredentials()));


    client.getPeerList();

    client.subscribe();

    sleep(2);
    client.getPeerList();

    for (uint32_t i = 0; i < 8; ++i)
    {
        client.sendMessage("first", "simple test " + std::to_string(i));
        sleep(2);
    }

    client.unsubscribe();

    return 0;
}
