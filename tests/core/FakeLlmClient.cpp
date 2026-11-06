#include "FakeLlmClient.h"

#include <utility>

namespace msa::core {

FakeLlmClient::FakeLlmClient(LlmResult result)
    : m_result{std::move(result)}
{
}

const std::vector<LlmRequest>& FakeLlmClient::requests() const
{
    return m_requests;
}

LlmResult FakeLlmClient::requestCompletion(const LlmRequest& request)
{
    m_requests.push_back(request);

    return m_result;
}

}  // namespace msa::core

