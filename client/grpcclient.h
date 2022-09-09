//******************************************************************************
//******************************************************************************

#ifndef GRPCCLIENT_H
#define GRPCCLIENT_H

//******************************************************************************
//******************************************************************************

#include "grpc-roc.grpc.pb.h"

#include <thread>
#include <list>
#include <atomic>
#include <sstream>

#include <grpcpp/grpcpp.h>

//******************************************************************************
//******************************************************************************
namespace rpcroc
{

//******************************************************************************
//******************************************************************************
class GRpcClient
{
public:
    GRpcClient(const std::string & id, const std::string & group, std::shared_ptr<grpc::Channel> channel)
        : m_id(id)
        , m_group(group)
        , m_stub(rpcroc::RpcRoc::NewStub(channel))
    {}

    ~GRpcClient()
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

} // namespace rpcroc

#endif // GRPCCLIENT_H
