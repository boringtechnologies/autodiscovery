//******************************************************************************
//******************************************************************************

#include "juiceclient.h"

#include <cstring>
#include <iostream>

#include <juice/juice.h>

//******************************************************************************
//******************************************************************************
namespace rpcroc
{

//******************************************************************************
//******************************************************************************
class JuiceClient::impl
{
    friend class JuiceClient;

    impl(JuiceClient * owner, const std::string & id)
        : m_o(owner)
        , m_id(id)
    {
    }

    JuiceClient       * m_o     = nullptr;
    std::string         m_id;
    juice_agent_t     * m_agent = nullptr;
    juice_config_t      m_config;
    juice_turn_server_t m_turnServer;
};

//******************************************************************************
//******************************************************************************
JuiceClient::JuiceClient(const std::string & id,
                         on_state_changed_t onStateChangedHandler, on_candidate_t onCandidateHandler,
                         on_gathering_done_t onGatheringDoneHandler, on_recv_t onReceiveHandler)
    : m_p(new impl(this, id))
    , m_onStateChangedHandler(onStateChangedHandler)
    , m_onCandidateHandler(onCandidateHandler)
    , m_onGatheringDoneHandler(onGatheringDoneHandler)
    , m_onReceiveHandler(onReceiveHandler)
{

}

//******************************************************************************
//******************************************************************************
JuiceClient::~JuiceClient()
{
    disconnect();
}

//******************************************************************************
//******************************************************************************
bool JuiceClient::init()
{
    memset(&m_p->m_config, 0, sizeof(juice_config_t));

    m_p->m_config.stun_server_host = "62.210.189.244";
    m_p->m_config.stun_server_port = 3478;

    memset(&m_p->m_turnServer, 0, sizeof(juice_turn_server_t));

    m_p->m_turnServer.host     = "62.210.189.244";
    m_p->m_turnServer.port     = 3478;
    m_p->m_turnServer.username = "testUser";
    m_p->m_turnServer.password = "testPassword";

//    m_p->m_config.turn_servers       = &m_p->m_turnServer;
//    m_p->m_config.turn_servers_count = 1;

    m_p->m_config.cb_state_changed  = m_onStateChangedHandler;
    m_p->m_config.cb_candidate      = m_onCandidateHandler;
    m_p->m_config.cb_gathering_done = m_onGatheringDoneHandler;
    m_p->m_config.cb_recv           = m_onReceiveHandler;
    m_p->m_config.user_ptr          = this;

    m_p->m_agent = juice_create(&m_p->m_config);
    return (m_p->m_agent != nullptr);
}

//******************************************************************************
//******************************************************************************
bool JuiceClient::init(const std::string & sdp)
{
    return (init() &&
            juice_set_remote_description(m_p->m_agent, sdp.c_str()) == JUICE_ERR_SUCCESS &&
            juice_gather_candidates(m_p->m_agent) == JUICE_ERR_SUCCESS);
}

//******************************************************************************
//******************************************************************************
bool JuiceClient::connect(const std::string & sdpRemote)
{
    if (!m_p->m_agent)
    {
        std::cout << "need init" << std::endl;
        return false;
    }
    return (juice_set_remote_description(m_p->m_agent, sdpRemote.c_str()) == JUICE_ERR_SUCCESS &&
            juice_gather_candidates(m_p->m_agent) == JUICE_ERR_SUCCESS);
}

//******************************************************************************
//******************************************************************************
void JuiceClient::disconnect()
{
    if (!m_p->m_agent)
    {
        return;
    }
    juice_destroy(m_p->m_agent);
    m_p->m_agent = nullptr;
}

//******************************************************************************
//******************************************************************************
void JuiceClient::addRemote(const std::string & sdpRemote)
{
    juice_add_remote_candidate(m_p->m_agent, sdpRemote.c_str());
}

//******************************************************************************
//******************************************************************************
void JuiceClient::sendMessage(const std::string & message)
{
    juice_send(m_p->m_agent, message.c_str(), message.size());
}

//******************************************************************************
//******************************************************************************
void JuiceClient::sendMessage(const uint8_t * data, const uint32_t size)
{
    juice_send(m_p->m_agent, reinterpret_cast<const char *>(data), size);
}

//******************************************************************************
//******************************************************************************
std::string JuiceClient::id() const
{
    return m_p->m_id;
}

//******************************************************************************
//******************************************************************************
std::string JuiceClient::sdp() const
{
    char sdp[JUICE_MAX_SDP_STRING_LEN];
    juice_get_local_description(m_p->m_agent, sdp, JUICE_MAX_SDP_STRING_LEN);
    // std::cout << "Local description " << sdp << std::endl;
    return sdp;
}

//******************************************************************************
//******************************************************************************
juice_state_t JuiceClient::state() const
{
    if (m_p->m_agent)
    {
        return juice_get_state(m_p->m_agent);
    }

    return JUICE_STATE_FAILED;
}

} // namespace rpcroc
