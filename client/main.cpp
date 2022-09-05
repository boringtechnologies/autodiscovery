//******************************************************************************
//******************************************************************************

#include "grpc-roc.grpc.pb.h"

#include <iostream>

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

private:
    std::unique_ptr<rpcroc::RpcRoc::Stub> m_stub;

    std::string m_id;
    std::string m_group;
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
    std::cout << "Done!" << std::endl;
}

//******************************************************************************
//******************************************************************************
void Client::subscribe()
{
    Peer peer;
    peer.set_id(m_id);
    peer.set_group(m_group);
    peer.set_type(READER | WRITER);

    Empty resp;

    grpc::ClientContext context;
    grpc::Status status = m_stub->subscribe(&context, peer, &resp);
    if (!status.ok())
    {
        std::cout << status.error_code() << " : " << status.error_message()
                  << std::endl;
        return;
    }
}

//******************************************************************************
//******************************************************************************
void Client::unsubscribe()
{
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

} // namespace rpcroc

//******************************************************************************
//******************************************************************************
int main()
{
    std::string target = "localhost:50051";
    grpc::CreateChannel(target, grpc::InsecureChannelCredentials());

    rpcroc::Client client("first", "blabla", grpc::CreateChannel(target, grpc::InsecureChannelCredentials()));

//    std::string reply = client.callOne("blabla");
//    std::cout << reply << std::endl;

    client.unsubscribe();
    client.getPeerList();
    client.subscribe();
    client.subscribe();
    client.getPeerList();

    return 0;
}
