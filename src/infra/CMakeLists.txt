add_library(msa_infra STATIC
    include/msa/infra/TcpClient.h
    src/TcpClient.cpp
    include/msa/infra/TcpServer.h
    src/TcpServer.cpp
    include/msa/infra/ProtocolCodec.h
    include/msa/infra/ProtocolMessage.h
    include/msa/infra/ProtocolPayload.h
    src/ProtocolCodec.cpp
    src/ProtocolPayload.cpp
    include/msa/infra/SearxngSearchProvider.h
    src/SearxngSearchProvider.cpp
    include/msa/infra/SqliteGeocodingCacheRepository.h
    include/msa/infra/SqliteRouteHistoryRepository.h
    src/SqliteGeocodingCacheRepository.cpp
    src/SqliteRouteHistoryRepository.cpp
    include/msa/infra/HttpGetClient.h
    include/msa/infra/NominatimGeocoder.h
    include/msa/infra/OpenMeteoWeatherProvider.h
    include/msa/infra/OsrmRoutePlanner.h
    src/HttpGetClient.cpp
    src/NominatimGeocoder.cpp
    src/OpenMeteoWeatherProvider.cpp
    src/OsrmRoutePlanner.cpp
    include/msa/infra/LlamaClient.h
    include/msa/infra/SqliteChatRepository.h
    include/msa/infra/SqliteDatabase.h
    include/msa/infra/SqliteEveningCheckInRepository.h
    include/msa/infra/SqliteEventWarningRepository.h
    include/msa/infra/SqliteEventRepository.h
    include/msa/infra/SqliteGreetingRepository.h
    include/msa/infra/SqliteHabitRepository.h
    include/msa/infra/SqliteMigration.h
    include/msa/infra/SqliteMoodRepository.h
    include/msa/infra/SqliteReminderRepository.h
    include/msa/infra/SqliteSchema.h
    include/msa/infra/SqliteTaskRepository.h
    include/msa/infra/SqliteUserProfileRepository.h
    src/LlamaClient.cpp
    src/SqliteChatRepository.cpp
    src/SqliteDatabase.cpp
    src/SqliteEveningCheckInRepository.cpp
    src/SqliteEventWarningRepository.cpp
    src/SqliteEventRepository.cpp
    src/SqliteGreetingRepository.cpp
    src/SqliteHabitRepository.cpp
    src/SqliteMoodRepository.cpp
    src/SqliteReminderRepository.cpp
    src/SqliteSchema.cpp
    src/SqliteTaskRepository.cpp
    src/SqliteUserProfileRepository.cpp
)

target_compile_features(msa_infra PUBLIC cxx_std_20)
target_include_directories(msa_infra PUBLIC include)
target_link_libraries(msa_infra PUBLIC msa_core Qt6::Sql Qt6::Network)
msa_apply_warnings(msa_infra)
