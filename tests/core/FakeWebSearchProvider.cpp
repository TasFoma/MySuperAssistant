#include "FakeWebSearchProvider.h"

namespace msa::core
{

ExternalServiceResult<std::vector<WebSearchResult>> FakeWebSearchProvider::requestSearch(const std::string& query)
{
    queries.push_back(query);
    if (onSearch)
    {
        onSearch();
    }

    return result;
}

} // namespace msa::core
