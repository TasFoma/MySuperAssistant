add_library(msa_infra_voice STATIC
    include/msa/infra/SpeechEngineFactory.h
    include/msa/infra/QtSpeechSynthesizer.h
    src/QtSpeechSynthesizer.cpp
    include/msa/infra/ProcessSpeechRecognizer.h
    include/msa/infra/VoiceProtocol.h
    include/msa/infra/JsonVoicePreferences.h
    src/ProcessSpeechRecognizer.cpp
    src/VoiceProtocol.cpp
    src/JsonVoicePreferences.cpp
)
if(WIN32)
    target_sources(msa_infra_voice PRIVATE ../platform/windows/src/WindowsSpeechEngine.cpp)
else()
    target_sources(msa_infra_voice PRIVATE src/DefaultSpeechEngine.cpp)
endif()
target_compile_features(msa_infra_voice PUBLIC cxx_std_20)
target_include_directories(msa_infra_voice PUBLIC include)
target_link_libraries(msa_infra_voice PUBLIC msa_core Qt6::Network Qt6::TextToSpeech Qt6::Multimedia)
msa_apply_warnings(msa_infra_voice)

if(TARGET msa_vosk)
    add_library(msa_infra_vosk STATIC
        include/msa/infra/VoskDecoder.h src/VoskDecoder.cpp
        include/msa/infra/VoskAudioSession.h src/VoskAudioSession.cpp)
    target_compile_features(msa_infra_vosk PUBLIC cxx_std_20)
    target_include_directories(msa_infra_vosk PUBLIC include)
    target_link_libraries(msa_infra_vosk PUBLIC msa_vosk Qt6::Multimedia)
    msa_apply_warnings(msa_infra_vosk)
endif()
