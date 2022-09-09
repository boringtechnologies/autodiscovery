//******************************************************************************
//******************************************************************************

#include "grpcclient.h"

#include <iostream>

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

    std::shared_ptr<rpcroc::GRpcClient> client; // (new rpcroc::Client(cmds[1], cmds[2], grpc::CreateChannel(cmds[0], grpc::InsecureChannelCredentials())));;

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
                client.reset(new rpcroc::GRpcClient("none", "none", grpc::CreateChannel(target, grpc::InsecureChannelCredentials())));
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

            client.reset(new rpcroc::GRpcClient(v[0], v[1], grpc::CreateChannel(target, grpc::InsecureChannelCredentials())));
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

    return 0;
}
