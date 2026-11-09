qt_add_executable(msa_voice_worker src/main.cpp src/VoiceWorker.cpp include/msa/desktop/VoiceWorker.h)
target_compile_features(msa_voice_worker PRIVATE cxx_std_20)
target_include_directories(msa_voice_worker PRIVATE include)
target_link_libraries(msa_voice_worker PRIVATE msa_infra_voice msa_infra_vosk)
msa_apply_warnings(msa_voice_worker)
add_custom_command(TARGET msa_voice_worker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${msaVoskRuntimeFiles} "$<TARGET_FILE_DIR:msa_voice_worker>"
    VERBATIM
)
