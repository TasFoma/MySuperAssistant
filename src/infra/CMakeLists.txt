add_library(msa_infra STATIC
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
