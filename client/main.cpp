//******************************************************************************
//******************************************************************************

#include "grpcclient.h"
#include "juiceclient.h"

#include <iostream>

//******************************************************************************
//******************************************************************************
std::shared_ptr<rpcroc::JuiceClient> jcclient;
std::shared_ptr<rpcroc::GRpcClient>  grclient;

std::string connectToUID;

//******************************************************************************
//******************************************************************************
//const char * stateString(const juice_state_t state)
//{
//    switch (state)
//    {
//        case JUICE_STATE_DISCONNECTED: return "JUICE_STATE_DISCONNECTED";
//        case JUICE_STATE_GATHERING:    return "JUICE_STATE_GATHERING";
//        case JUICE_STATE_CONNECTING:   return "JUICE_STATE_CONNECTING";
//        case JUICE_STATE_CONNECTED:    return "JUICE_STATE_CONNECTED";
//        case JUICE_STATE_COMPLETED:    return "JUICE_STATE_COMPLETED";
//        case JUICE_STATE_FAILED:       return "JUICE_STATE_FAILED";
//    }
//    return "UNDEFINED";
//}

//******************************************************************************
//******************************************************************************
void onStateChanged(juice_agent_t * agent, juice_state_t state, void * user_ptr)
{
    std::cout << "onStateChanged " << state << " " << juice_state_to_string(state) << std::endl;
}

//******************************************************************************
//******************************************************************************
void onCandidate(juice_agent_t * agent, const char * sdp, void * user_ptr)
{
    std::cout << "onCandidate " << sdp << std::endl;

    if (grclient)
    {
        grclient->sendMessage(connectToUID, sdp);
    }
}

//******************************************************************************
//******************************************************************************
void onGatheringDone(juice_agent_t * agent, void * user_ptr)
{
    std::cout << "onGatheringDone" << std::endl;
}

//******************************************************************************
//******************************************************************************
void onRecv(juice_agent_t * agent, const char * data, size_t size, void * user_ptr)
{
    std::cout << "onRecv " << size << " bytes" << std::endl;
    std::cout << "       " << std::string(data, size) << std::endl;
}

//******************************************************************************
//******************************************************************************
void onGrpcMessage(const std::string & from, const std::string & msg)
{
    // std::cout << "Message from " << from << " --> " << msg << std::endl;

    jcclient->addRemote(msg);
}

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
              << "  juiceConnect user_id" << std::endl
              << "  juiceState" << std::endl
              << "  juiceSend message" << std::endl
//              << "  sendMessage user_id|message" << std::endl
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

    jcclient.reset(new rpcroc::JuiceClient(&onStateChanged, &onCandidate, &onGatheringDone, &onRecv));
    jcclient->init();

    std::string target = argv[1];

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
            if (grclient)
            {
                grclient->getPeerList();
            }
            else
            {
                grclient.reset(new rpcroc::GRpcClient("none", "none", jcclient->sdp(), grpc::CreateChannel(target, grpc::InsecureChannelCredentials())));
                grclient->getPeerList();
                grclient.reset();
            }
            continue;
        }
        else if (command == "subscribe")
        {
            std::string params;
            std::cin >> params;
            std::vector<std::string> v = parseCommand(params);
            if (v.size() < 1)
            {
                std::cout << "wrong params, try help" << std::endl;
                continue;
            }

            std::string group = "group";
            if (v.size() >= 2)
            {
                group = v[1];
            }

            uint32_t type = 3;
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

            grclient.reset(new rpcroc::GRpcClient(v[0], group, jcclient->sdp(), grpc::CreateChannel(target, grpc::InsecureChannelCredentials())));
            grclient->subscribe(type, onGrpcMessage);
        }
        else if (command == "unsubscribe")
        {
            if (grclient)
            {
                // grclient->unsubscribe();
                grclient.reset();
            }
        }
        else if (command == "juiceConnect")
        {
            std::string params;
            std::cin >> params;
            std::vector<std::string> v = parseCommand(params);
            if (v.size() < 1)
            {
                std::cout << "wrong params, try help" << std::endl;
                continue;
            }

            if (!grclient)
            {
                std::cout << "not subscribed" << std::endl;
                continue;
            }
            rpcroc::Peer p = grclient->getPeer(v[0]);
            if (p.id().empty())
            {
                std::cout << "peer not found, try get list" << std::endl;
                continue;
            }

            connectToUID = p.id();
            jcclient->connect(p.sdp());
        }
        else if (command == "juiceState")
        {
            juice_state_t s = jcclient->state();
            std::cout << s << " " << juice_state_to_string(s) << std::endl;
        }
        else if (command == "juiceSend")
        {
            std::string params;
            std::cin >> params;
            std::vector<std::string> v = parseCommand(params);
            if (v.size() < 1)
            {
                std::cout << "wrong params, try help" << std::endl;
                continue;
            }

            if (!jcclient)
            {
                std::cout << "not connected" << std::endl;
                continue;
            }

            jcclient->sendMessage(v[0]);
        }
//        else if (command == "sendMessage")
//        {
//            if (!grclient)
//            {
//                std::cout << "not subscribed" << std::endl;
//                continue;
//            }

//            std::string params;
//            std::cin >> params;
//            std::vector<std::string> v = parseCommand(params);
//            if (v.size() != 2)
//            {
//                std::cout << "wrong params, try help" << std::endl;
//                continue;
//            }

//            grclient->sendMessage(v[0], v[1]);
//        }
        else
        {
            std::cout << "wrong, try help" << std::endl;
        }
    }

    return 0;
}
