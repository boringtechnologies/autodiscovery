//******************************************************************************
//******************************************************************************

#ifndef JUICECLIENT_H
#define JUICECLIENT_H

#include <memory>

#include <juice/juice.h>

//******************************************************************************
//******************************************************************************
namespace rpcroc
{

typedef void (* on_state_changed_t)(juice_agent_t * agent, juice_state_t state, void * user_ptr);
typedef void (* on_candidate_t)(juice_agent_t * agent, const char * sdp, void * user_ptr);
typedef void (* on_gathering_done_t)(juice_agent_t * agent, void * user_ptr);
typedef void (* on_recv_t)(juice_agent_t * agent, const char * data, size_t size, void * user_ptr);

//******************************************************************************
//******************************************************************************
class JuiceClient
{
public:
    JuiceClient(on_state_changed_t onStateChangedHandler, on_candidate_t onCandidateHandler,
                on_gathering_done_t onGatheringDoneHandler, on_recv_t onReceiveHandler);
    ~JuiceClient();

public:
    void init();
    void connect(const std::string & sdpRemote);
    void addRemote(const std::string & sdpRemote);
    void sendMessage(const std::string & message);

    std::string sdp() const;

    juice_state_t state() const;

private:
    class impl;
    std::shared_ptr<impl> m_p;

    on_state_changed_t  m_onStateChangedHandler;
    on_candidate_t      m_onCandidateHandler;
    on_gathering_done_t m_onGatheringDoneHandler;
    on_recv_t           m_onReceiveHandler;
};

} // namespace rpcroc

#endif // JUICECLIENT_H
