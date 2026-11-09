add_library(msa_infra_audio STATIC
    include/msa/infra/AudioDevices.h
    include/msa/infra/JsonAudioDevicePreferences.h
    src/AudioDevices.cpp
    src/JsonAudioDevicePreferences.cpp
    include/msa/infra/JsonMusicPreferences.h
    include/msa/infra/LocalMusicPlayer.h
    include/msa/infra/detail/MusicLibraryScanner.h
    src/JsonMusicPreferences.cpp
    src/LocalMusicPlayer.cpp
    src/MusicLibraryScanner.cpp
)
target_compile_features(msa_infra_audio PUBLIC cxx_std_20)
target_include_directories(msa_infra_audio PUBLIC include)
target_link_libraries(msa_infra_audio PUBLIC msa_core Qt6::Multimedia)
msa_apply_warnings(msa_infra_audio)
