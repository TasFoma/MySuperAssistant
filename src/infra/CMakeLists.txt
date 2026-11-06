add_library(msa_infra STATIC
    include/msa/infra/SqliteChatRepository.h
    include/msa/infra/SqliteDatabase.h
    include/msa/infra/SqliteEveningCheckInRepository.h
    include/msa/infra/SqliteEventWarningRepository.h
    include/msa/infra/SqliteGreetingRepository.h
    include/msa/infra/SqliteMigration.h
    include/msa/infra/SqliteSchema.h
    include/msa/infra/SqliteUserProfileRepository.h
    src/SqliteChatRepository.cpp
    src/SqliteDatabase.cpp
    src/SqliteEveningCheckInRepository.cpp
    src/SqliteEventWarningRepository.cpp
    src/SqliteGreetingRepository.cpp
    src/SqliteSchema.cpp
    src/SqliteUserProfileRepository.cpp
)

target_compile_features(msa_infra PUBLIC cxx_std_20)
target_include_directories(msa_infra PUBLIC include)
target_link_libraries(msa_infra PUBLIC msa_core Qt6::Sql)
msa_apply_warnings(msa_infra)
