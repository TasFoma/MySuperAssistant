#pragma once

#include <string>
#include <vector>

namespace msa::core
{
/// @brief Устойчивый непрозрачный ID и отображаемое имя аудиоустройства.
struct AudioDeviceChoice
{
    std::string identifier{};
    std::string name{};
    bool available{true};
};
/// @brief Пустой ID означает следование системному устройству по умолчанию.
struct AudioDevicePreferences
{
    std::string input{};
    std::string output{};
};
/// @brief Каталог и сохранённый выбор без открытия устройств.
struct AudioDevicesState
{
    std::vector<AudioDeviceChoice> inputs{};
    std::vector<AudioDeviceChoice> outputs{};
    AudioDevicePreferences preferences{};
    std::string error{};
};
} // namespace msa::core
