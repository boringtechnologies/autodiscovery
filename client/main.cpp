//******************************************************************************
//******************************************************************************

#include "grpcclient.h"
#include "juiceclient.h"

#include <iostream>

//******************************************************************************
//******************************************************************************
std::shared_ptr<rpcroc::GRpcClient>  grclient;

typedef std::shared_ptr<rpcroc::JuiceClient> jcclient_ptr_t;
std::map<std::string, jcclient_ptr_t> jcclients;

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
    // std::cout << "onStateChanged " << state << " " << juice_state_to_string(state) << std::endl;
    std::cout << "P2P connection <" << state << "> " << juice_state_to_string(state) << std::endl;
}

//******************************************************************************
//******************************************************************************
void onCandidate(juice_agent_t * agent, const char * sdp, void * user_ptr)
{
//    std::cout << "onCandidate " << sdp << std::endl;

    rpcroc::JuiceClient * caller = reinterpret_cast<rpcroc::JuiceClient *>(user_ptr);

//    if (grclient)
//    {
//        grclient->sendMessage(caller->id(), sdp);
//    }
}

//******************************************************************************
//******************************************************************************
void onGatheringDone(juice_agent_t * agent, void * user_ptr)
{
//    std::cout << "onGatheringDone" << std::endl;
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
jcclient_ptr_t getJccClient(const std::string & id)
{
    if (jcclients.count(id) == 0)
    {
        // std::cout << "message to unknown " << to << std::endl;
        return jcclient_ptr_t();
    }

    return jcclients[id];
}

//******************************************************************************
//******************************************************************************
void onGrpcMessage(const std::string & from, const std::string & to, const std::string & msg)
{
    // std::cout << "Message from " << from << " --> " << msg << std::endl;

    // TODO move juice messages to another instance
    if (msg.rfind("JP2PI", 0) == 0)
    {
        // juice init connection, sdp
        jcclient_ptr_t ptr = getJccClient(from);
        if (ptr)
        {
            std::cout << "juice already initialized " << from << std::endl;
            return;
        }

        ptr.reset(new rpcroc::JuiceClient(from, &onStateChanged, &onCandidate, &onGatheringDone, &onRecv));
        ptr->init(msg.substr(6));

        if (!grclient->p2pAccept(from, ptr->sdp()))
        {
            std::cout << "p2p not accepted " << from << std::endl;
            return;
        }

        jcclients[from] = ptr;
    }
    else if (msg.rfind("JP2PA", 0) == 0)
    {
        // juice connection accepted, sdp
        jcclient_ptr_t ptr = getJccClient(from);
        if (!ptr)
        {
            std::cout << "juice not initialized " << from << std::endl;
            return;
        }

        ptr->connect(msg.substr(6));
    }
    else if (msg.rfind("JP2PC", 0) == 0)
    {
        // juice candidate
        jcclient_ptr_t ptr = getJccClient(from);
        if (!ptr)
        {
            std::cout << "juice not initialized " << from << std::endl;
            return;
        }
        ptr->addRemote(msg.substr(6));
    }
    else if (msg.rfind("JP2PD", 0) == 0)
    {
        // juice disconnect
        std::cout << "juice disconnectd, " << msg.substr(6) << std::endl;
        jcclients.erase(from);
    }
    else
    {
        // simple message
        std::cout << "msg from <" << from << "> " << msg << std::endl;
    }
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
//              << "  sendMessage user_id|message" << std::endl
              << "  unsubscribe" << std::endl
              << "  juiceConnect user_id" << std::endl
              << "  juiceState user_id" << std::endl
              << "  juiceSend user_id|message" << std::endl
              << "  juiceDisconnect user_id" << std::endl
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
                grclient.reset(new rpcroc::GRpcClient("none", "none", grpc::CreateChannel(target, grpc::InsecureChannelCredentials())));
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

//            jccpending.reset(new rpcroc::JuiceClient(v[0], &onStateChanged, &onCandidate, &onGatheringDone, &onRecv));
//            jccpending->init();

            grclient.reset(new rpcroc::GRpcClient(v[0], group, grpc::CreateChannel(target, grpc::InsecureChannelCredentials())));
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
//            rpcroc::Peer p = grclient->getPeer(v[0]);
//            if (p.id().empty())
//            {
//                std::cout << "peer not found, try get list" << std::endl;
//                continue;
//            }

//            if (jcclients.count(p.id()))
//            {
//                std::cout << "already connected" << std::endl;
//                continue;
//            }

            jcclient_ptr_t ptr(new rpcroc::JuiceClient(v[0], &onStateChanged, &onCandidate, &onGatheringDone, &onRecv));
            ptr->init();

            if (!grclient->p2pInit(v[0], ptr->sdp()))
            {
                std::cout << "p2p init error" << std::endl;
                continue;
            }

            jcclients[v[0]] = ptr;
        }
        else if (command == "juiceState")
        {
            std::string params;
            std::cin >> params;
            std::vector<std::string> v = parseCommand(params);
            if (v.size() < 1)
            {
                std::cout << "wrong params, try help" << std::endl;
                continue;
            }

            if (!jcclients.count(v[0]))
            {
                std::cout << "not found" << std::endl;
                continue;
            }

            juice_state_t s = jcclients[v[0]]->state();
            std::cout << s << " " << juice_state_to_string(s) << std::endl;
        }
        else if (command == "juiceSend")
        {
            std::string params;
            std::cin >> params;
            std::vector<std::string> v = parseCommand(params);
            if (v.size() < 2)
            {
                std::cout << "wrong params, try help" << std::endl;
                continue;
            }

            if (!jcclients.count(v[0]))
            {
                std::cout << "not connected" << std::endl;
                continue;
            }

            jcclients[v[0]]->sendMessage(v[1]);
        }
        else if (command == "juiceDisconnect")
        {
            std::string params;
            std::cin >> params;
            std::vector<std::string> v = parseCommand(params);
            if (v.size() < 1)
            {
                std::cout << "wrong params, try help" << std::endl;
                continue;
            }

            if (!jcclients.count(v[0]))
            {
                std::cout << "not connected" << std::endl;
                continue;
            }

            if (!grclient->p2pDisconnect(v[0], "user request"))
            {
                std::cout << "p2p init error" << std::endl;
                continue;
            }

            jcclients[v[0]]->disconnect();
            jcclients.erase(v[0]);
        }
        else if (false && command == "sendMessage")
        {
            if (!grclient)
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

            grclient->sendMessage(v[0], v[1]);
        }
        else
        {
            std::cout << "wrong, try help" << std::endl;
        }
    }

    for (const std::pair<std::string, jcclient_ptr_t> & it : jcclients)
    {
        if (!grclient->p2pDisconnect(it.first, "destroyed"))
        {
            std::cout << "p2p disconnect error" << std::endl;
            continue;
        }

        it.second->disconnect();
    }

    return 0;
}
