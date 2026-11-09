include(FetchContent)

# DLL распознавания используется только отдельным процессом VoiceWorker.
FetchContent_Declare(msa_vosk_binary
    URL https://github.com/alphacep/vosk-api/releases/download/v0.3.45/vosk-win64-0.3.45.zip
    URL_HASH SHA256=f1dcc9cca460630f81ea8f71794f69c80bed6556d2a4e6237b5785e1d2dff34b
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(msa_vosk_binary)

add_library(msa_vosk SHARED IMPORTED GLOBAL)
set_target_properties(msa_vosk PROPERTIES
    IMPORTED_LOCATION "${msa_vosk_binary_SOURCE_DIR}/libvosk.dll"
    IMPORTED_IMPLIB "${msa_vosk_binary_SOURCE_DIR}/libvosk.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${msa_vosk_binary_SOURCE_DIR}"
)

set(msaVoskRuntimeFiles
    "${msa_vosk_binary_SOURCE_DIR}/libvosk.dll"
    "${msa_vosk_binary_SOURCE_DIR}/libgcc_s_seh-1.dll"
    "${msa_vosk_binary_SOURCE_DIR}/libstdc++-6.dll"
    "${msa_vosk_binary_SOURCE_DIR}/libwinpthread-1.dll"
)
