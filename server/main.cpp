//******************************************************************************
//******************************************************************************

//#include "absl/flags/flag.h"
//#include "absl/flags/parse.h"
//#include "absl/strings/str_cat.h"

#include "grpc-roc.grpc.pb.h"

#include <iostream>
#include <vector>
#include <map>

#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/xds_server_builder.h>

//******************************************************************************
//******************************************************************************
// ABSL_FLAG(int32_t, port, 50051, "Server port for service.");

namespace rpcroc
{

//******************************************************************************
//******************************************************************************
class Service : public rpcroc::RpcRoc::Service
{
    grpc::Status getPeerList(grpc::ServerContext * context,
                             const rpcroc::GetPeerList * request,
                             grpc::ServerWriter<rpcroc::Peer> * writer) override;

    grpc::Status subscribe(grpc::ServerContext * context,
                           const rpcroc::Peer * request,
                           rpcroc::Empty * reply) override;

    grpc::Status unsubscribe(grpc::ServerContext * context,
                             const rpcroc::Peer * request,
                             rpcroc::Empty * reply) override;

private:
    std::map<std::string, rpcroc::Peer> m_peers;

};

//******************************************************************************
//******************************************************************************
grpc::Status Service::getPeerList(grpc::ServerContext * context,
                                  const rpcroc::GetPeerList * request,
                                  grpc::ServerWriter<rpcroc::Peer> * writer)
{
    std::cout << "getPeerList called from " << request->id() << std::endl;
    for (const std::pair<std::string, rpcroc::Peer> & p : m_peers)
    {
        writer->Write(p.second);
    }
    return grpc::Status::OK;
}

//******************************************************************************
//******************************************************************************
grpc::Status Service::subscribe(grpc::ServerContext * context,
                                const rpcroc::Peer * request,
                                rpcroc::Empty * reply)
{
    std::cout << "subscribe called from " << request->id() << std::endl;
    if (m_peers.find(request->id()) != m_peers.end())
    {
        return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, "User exists");
    }
    m_peers[request->id()] = *request;
    return grpc::Status::OK;
}

//******************************************************************************
//******************************************************************************
grpc::Status Service::unsubscribe(grpc::ServerContext * context,
                                  const rpcroc::Peer * request,
                                  rpcroc::Empty * reply)
{
    std::cout << "unsubscribe called from " << request->id() << std::endl;
    if (m_peers.find(request->id()) == m_peers.end())
    {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "User not found");
    }
    m_peers.erase(request->id());
    return grpc::Status::OK;
}

} // namespace rpcroc

//******************************************************************************
//******************************************************************************
int main()
{
    uint32_t port = 50051; // absl::GetFlag(FLAGS_port);

    // grpc::XdsServerBuilder xds_builder;

    rpcroc::Service service;

    grpc::ServerBuilder builder;
    builder.RegisterService(&service);

    builder.AddListeningPort("0.0.0.0:" + std::to_string(port),
                             grpc::InsecureServerCredentials());

    std::cout << "Server started on 0.0.0.0:" << port << std::endl;

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    server->Wait();

    return 0;
}
