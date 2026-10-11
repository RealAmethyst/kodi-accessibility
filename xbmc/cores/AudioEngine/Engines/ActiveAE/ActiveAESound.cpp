/*
 *  Copyright (C) 2010-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ActiveAESound.h"

#include "ActiveAE.h"
#include "ServiceBroker.h"
#include "cores/AudioEngine/AEResampleFactory.h"
#include "utils/JobManager.h"
#include "cores/AudioEngine/Interfaces/AESound.h"
#include "filesystem/File.h"
#include "utils/log.h"

extern "C" {
#include <libavutil/avutil.h>
}

using namespace ActiveAE;
using namespace XFILE;

CActiveAESound::CActiveAESound(const std::string& filename, CActiveAE* ae)
  : IAESound(filename), m_filename(filename)
{
  m_orig_sound = NULL;
  m_dst_sound = NULL;
  m_pFile = NULL;
  m_isSeekPossible = false;
  m_fileSize = 0;
  m_isConverted = false;
  m_activeAE = ae;
}

CActiveAESound::~CActiveAESound()
{
  Finish();
}

void CActiveAESound::Play()
{
  m_activeAE->PlaySound(this);

}

void CActiveAESound::Stop()
{
  m_activeAE->StopSound(this);
}

bool CActiveAESound::IsPlaying()
{
  //! @todo implement
  return false;
}

uint8_t** CActiveAESound::InitSound(bool orig, SampleConfig config, int nb_samples)
{
  std::shared_ptr<CSoundPacket>* info;
  if (orig)
    info = &m_orig_sound;
  else
    info = &m_dst_sound;

  *info = std::make_shared<CSoundPacket>(config, nb_samples);

  (*info)->nb_samples = 0;
  m_isConverted = false;
  return (*info)->data;
}

bool CActiveAESound::StoreSound(bool orig, uint8_t **buffer, int samples, int linesize)
{
  std::shared_ptr<CSoundPacket>* info;
  if (orig)
    info = &m_orig_sound;
  else
    info = &m_dst_sound;

  if ((*info)->nb_samples + samples > (*info)->max_nb_samples)
  {
    CLog::Log(LOGERROR, "CActiveAESound::StoreSound - exceeded max samples");
    return false;
  }

  int bytes_to_copy = samples * (*info)->bytes_per_sample * (*info)->config.channels;
  bytes_to_copy /= (*info)->planes;
  int start = (*info)->nb_samples * (*info)->bytes_per_sample * (*info)->config.channels;
  start /= (*info)->planes;

  for (int i=0; i<(*info)->planes; i++)
  {
    memcpy((*info)->data[i]+start, buffer[i], bytes_to_copy);
  }
  (*info)->nb_samples += samples;

  return true;
}

CSoundPacket *CActiveAESound::GetSound(bool orig)
{
  if (orig)
    return m_orig_sound.get();
  else
    return m_dst_sound.get();
}

bool CActiveAESound::Convert(const SampleConfig& config,
                             const CAEChannelInfo& channels,
                             AEQuality quality)
{
  for (auto& conversion : m_conversions)
  {
    const auto& previous = conversion.config;
    if (previous.fmt != config.fmt || previous.channel_layout != config.channel_layout ||
        previous.channels != config.channels || previous.sample_rate != config.sample_rate ||
        previous.bits_per_sample != config.bits_per_sample ||
        previous.dither_bits != config.dither_bits || conversion.channels != channels ||
        conversion.quality != quality)
      continue;

    if (conversion.failed)
      return false;
    if (conversion.result.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
      return false;
    try
    {
      m_dst_sound = conversion.result.get();
    }
    catch (const std::exception& error)
    {
      CLog::Log(LOGERROR, "GUI sound conversion failed: {}", error.what());
      conversion.failed = true;
      return false;
    }
    if (!m_dst_sound)
    {
      CLog::Log(LOGERROR, "GUI sound conversion returned no audio");
      conversion.failed = true;
    }
    m_isConverted = m_dst_sound != nullptr;
    return m_isConverted;
  }

  // Jobs own their immutable input and result, never the sound or engine. A
  // sound may be unloaded or its output format changed while work is running.
  auto task = std::make_shared<std::packaged_task<std::shared_ptr<CSoundPacket>()>>(
      [source = m_orig_sound, config, channels, quality]() mutable -> std::shared_ptr<CSoundPacket>
      {
        CAEChannelInfo remap = channels;
        auto resampler = CAEResampleFactory::Create(AERESAMPLEFACTORY_QUICK_RESAMPLE);
        if (!resampler->Init(config, source->config, false, true, M_SQRT1_2,
                             remap.Count() ? &remap : nullptr, quality, false))
          return {};
        const int frames = resampler->CalcDstSampleCount(
            source->nb_samples, config.sample_rate, source->config.sample_rate);
        auto result = std::make_shared<CSoundPacket>(config, frames);
        if (!result->data)
          return {};
        result->nb_samples = resampler->Resample(result->data, frames, source->data,
                                                 source->nb_samples, 1.0);
        if (result->nb_samples <= 0)
          return {};
        return result;
      });
  // Two formats cover the usual idle/music rate change without unbounded PCM
  // retention. packaged_task futures do not join the worker when destroyed.
  if (m_conversions.size() == 2)
    m_conversions.pop_front();
  m_conversions.push_back({config, channels, quality, task->get_future().share()});
  CServiceBroker::GetJobManager()->Submit([task]() { (*task)(); }, CJob::PRIORITY_HIGH);
  return false;
}

bool CActiveAESound::Prepare()
{
  unsigned int flags = READ_TRUNCATED | READ_CHUNKED;
  m_pFile = new CFile();

  if (!m_pFile->Open(m_filename, flags))
  {
    delete m_pFile;
    m_pFile = NULL;
    return false;
  }
  m_isSeekPossible = m_pFile->IoControl(IOCTRL_SEEK_POSSIBLE, NULL) != 0;
  m_fileSize = m_pFile->GetLength();
  return true;
}

void CActiveAESound::Finish()
{
  delete m_pFile;
  m_pFile = NULL;
}

int CActiveAESound::GetChunkSize()
{
  return m_pFile->GetChunkSize();
}

int CActiveAESound::Read(void *h, uint8_t* buf, int size)
{
  CFile *pFile = static_cast<CActiveAESound*>(h)->m_pFile;
  int len = pFile->Read(buf, size);
  if (len == 0)
    return AVERROR_EOF;
  else
    return len;
}

int64_t CActiveAESound::Seek(void *h, int64_t pos, int whence)
{
  CFile* pFile = static_cast<CActiveAESound*>(h)->m_pFile;
  if(whence == AVSEEK_SIZE)
    return pFile->GetLength();
  else
    return pFile->Seek(pos, whence & ~AVSEEK_FORCE);
}
