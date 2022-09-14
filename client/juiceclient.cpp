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

    impl(JuiceClient * owner)
        : m_o(owner)
    {
    }

    JuiceClient       * m_o;

    juice_agent_t     * m_agent;
    juice_config_t      m_config;
    juice_turn_server_t m_turnServer;
};

//******************************************************************************
//******************************************************************************
JuiceClient::JuiceClient(on_state_changed_t onStateChangedHandler, on_candidate_t onCandidateHandler,
                         on_gathering_done_t onGatheringDoneHandler, on_recv_t onReceiveHandler)
    : m_p(new impl(this))
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
    juice_destroy(m_p->m_agent);
}

//******************************************************************************
//******************************************************************************
void JuiceClient::init()
{
    memset(&m_p->m_config, 0, sizeof(juice_config_t));

    m_p->m_config.stun_server_host = "62.210.189.244";
    m_p->m_config.stun_server_port = 3478;

    memset(&m_p->m_turnServer, 0, sizeof(juice_turn_server_t));

    m_p->m_turnServer.host     = "62.210.189.244";
    m_p->m_turnServer.port     = 3478;
    m_p->m_turnServer.username = "testUser";
    m_p->m_turnServer.password = "testPassword";

    m_p->m_config.turn_servers       = &m_p->m_turnServer;
    m_p->m_config.turn_servers_count = 1;

    m_p->m_config.cb_state_changed  = m_onStateChangedHandler;
    m_p->m_config.cb_candidate      = m_onCandidateHandler;
    m_p->m_config.cb_gathering_done = m_onGatheringDoneHandler;
    m_p->m_config.cb_recv           = m_onReceiveHandler;
    m_p->m_config.user_ptr          = this;

    m_p->m_agent = juice_create(&m_p->m_config);
}

//******************************************************************************
//******************************************************************************
void JuiceClient::connect(const std::string & sdpRemote)
{
    juice_set_remote_description(m_p->m_agent, sdpRemote.c_str());
    juice_gather_candidates(m_p->m_agent);
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
