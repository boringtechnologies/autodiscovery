//******************************************************************************
//******************************************************************************

#include "grpcclient.h"

//******************************************************************************
//******************************************************************************
namespace rpcroc
{

//******************************************************************************
//******************************************************************************
void GRpcClient::getPeerList()
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
void GRpcClient::subscribe(const uint32_t type)
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
void GRpcClient::unsubscribe()
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
void GRpcClient::sendMessage(const std::string & to, const std::string & msg)
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
