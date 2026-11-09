#include "msa/infra/VoskAudioSession.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QTimer>

#include <exception>
#include <stdexcept>

namespace msa::infra
{
namespace
{
constexpr int kSampleRate = 16000;
constexpr int kChannels = 1;
constexpr qsizetype kBytesPerSample = 2;
constexpr qint64 kQuantumBytes = 8000;
constexpr qint64 kMaximumBufferedBytes = kQuantumBytes * 4;
} // namespace
VoskAudioSession::VoskAudioSession(QObject* parent) : QObject{parent} {}
VoskAudioSession::~VoskAudioSession()
{
    stop();
}
bool VoskAudioSession::loaded() const
{
    return m_decoder != nullptr;
}
bool VoskAudioSession::recording() const
{
    return m_recording;
}
void VoskAudioSession::load(const QString& path)
{
    stop();
    m_decoder = std::make_unique<VoskDecoder>(path);
}
void VoskAudioSession::start()
{
    if (m_recording)
    {
        return;
    }
    try
    {
        if (!m_decoder)
        {
            throw std::runtime_error{"Модель ещё не загружена."};
        }
        const auto device = QMediaDevices::defaultAudioInput();
        if (device.isNull())
        {
            throw std::runtime_error{"Микрофон не найден."};
        }
        QAudioFormat format;
        format.setSampleRate(kSampleRate);
        format.setChannelCount(kChannels);
        format.setSampleFormat(QAudioFormat::Int16);
        if (!device.isFormatSupported(format))
        {
            throw std::runtime_error{"Микрофон не поддерживает PCM 16 кГц, mono, int16."};
        }
        m_decoder->begin();
        m_pending.clear();
        m_audio = new QAudioSource{device, format, this};
        auto* source = m_audio.data();
        source->setBufferSize(kMaximumBufferedBytes);
        connect(source, &QAudioSource::stateChanged, this,
                [this, source](QtAudio::State state)
                {
                    if (m_audio == source && m_recording && state == QtAudio::StoppedState &&
                        source->error() != QtAudio::NoError)
                    {
                        fail("Не удалось записать звук микрофона.");
                    }
                });
        m_recording = true;
        m_input = source->start();
        if (!m_input || !m_audio || source->error() != QtAudio::NoError)
        {
            throw std::runtime_error{"Не удалось открыть микрофон."};
        }
        connect(m_input, &QIODevice::readyRead, this, [this] { readAudio(); });
    }
    catch (const std::exception& error)
    {
        fail(QString::fromUtf8(error.what()));
    }
}
void VoskAudioSession::stop()
{
    m_recording = false;
    if (m_input)
    {
        disconnect(m_input, nullptr, this, nullptr);
    }
    m_input.clear();
    if (m_audio)
    {
        m_audio->stop();
        m_audio->deleteLater();
        m_audio.clear();
    }
    m_pending.clear();
}
void VoskAudioSession::finish()
{
    if (!m_recording)
    {
        return;
    }
    try
    {
        complete(m_decoder->finish());
    }
    catch (const std::exception& error)
    {
        fail(QString::fromUtf8(error.what()));
    }
}
void VoskAudioSession::readAudio()
{
    if (!m_recording || !m_input)
    {
        return;
    }
    try
    {
        if (m_input->bytesAvailable() > kMaximumBufferedBytes)
        {
            throw std::runtime_error{"Распознавание не успевает за записью. Повторите попытку."};
        }
        m_pending += m_input->read(kQuantumBytes);
        const auto length = m_pending.size() - m_pending.size() % kBytesPerSample;
        if (length > 0)
        {
            const auto pcm = m_pending.first(length);
            m_pending.remove(0, length);
            if (const auto text = m_decoder->accept(pcm))
            {
                complete(*text);
                return;
            }
        }
        if (m_recording && m_input && m_input->bytesAvailable() > 0)
        {
            QTimer::singleShot(0, this, [this] { readAudio(); });
        }
    }
    catch (const std::exception& error)
    {
        fail(QString::fromUtf8(error.what()));
    }
}
void VoskAudioSession::complete(const QString& text)
{
    stop();
    if (!text.isEmpty())
    {
        emit textReady(text);
    }
    emit completed();
}
void VoskAudioSession::fail(const QString& message)
{
    stop();
    emit failed(message);
}
} // namespace msa::infra
