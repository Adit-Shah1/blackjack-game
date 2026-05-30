#include <blackjack/audio.h>

#include <SDL_mixer.h>
#include <algorithm>
#include <iostream>

namespace blackjack {

bool AudioManager::init() {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "Mix_OpenAudio failed: " << Mix_GetError() << std::endl;
        return false;
    }
    return true;
}

AudioManager::~AudioManager() {
    stopAmbient();
    for (auto& [k, ptr] : m_sfx) {
        (void)k;
        if (ptr) {
            Mix_FreeChunk(static_cast<Mix_Chunk*>(ptr));
        }
    }
    for (auto& [k, ptr] : m_music) {
        (void)k;
        if (ptr) {
            Mix_FreeMusic(static_cast<Mix_Music*>(ptr));
        }
    }
    m_sfx.clear();
    m_music.clear();
    Mix_CloseAudio();
}

void AudioManager::playSFX(const std::string& key) {
    auto it = m_sfx.find(key);
    if (it == m_sfx.end() || !it->second) {
        std::string path = "assets/audio/" + key + ".wav";
        Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
        if (!chunk) {
            // Silently skip missing audio files
            return;
        }
        m_sfx[key] = chunk;
        it = m_sfx.find(key);
    }

    Mix_Chunk* chunk = static_cast<Mix_Chunk*>(it->second);
    int volume = (m_masterVolume * m_sfxVolume) / 100;
    Mix_VolumeChunk(chunk, (volume * MIX_MAX_VOLUME) / 100);
    Mix_PlayChannel(-1, chunk, 0);
}

void AudioManager::playAmbient(const std::string& key) {
    stopAmbient();
    auto it = m_music.find(key);
    if (it == m_music.end() || !it->second) {
        std::string path = "assets/audio/" + key + ".ogg";
        Mix_Music* music = Mix_LoadMUS(path.c_str());
        if (!music) {
            return;
        }
        m_music[key] = music;
        it = m_music.find(key);
    }

    Mix_Music* music = static_cast<Mix_Music*>(it->second);
    int volume = (m_masterVolume * m_ambientVolume) / 100;
    Mix_VolumeMusic((volume * MIX_MAX_VOLUME) / 100);
    Mix_PlayMusic(music, -1);
}

void AudioManager::stopAmbient() {
    if (Mix_PlayingMusic()) {
        Mix_HaltMusic();
    }
}

void AudioManager::setMasterVolume(int vol) {
    m_masterVolume = std::clamp(vol, 0, 100);
    applyVolumes();
}

void AudioManager::setSFXVolume(int vol) {
    m_sfxVolume = std::clamp(vol, 0, 100);
    applyVolumes();
}

void AudioManager::setAmbientVolume(int vol) {
    m_ambientVolume = std::clamp(vol, 0, 100);
    applyVolumes();
}

void AudioManager::applyVolumes() {
    int ambientVol = (m_masterVolume * m_ambientVolume) / 100;
    Mix_VolumeMusic((ambientVol * MIX_MAX_VOLUME) / 100);

    int sfxVol = (m_masterVolume * m_sfxVolume) / 100;
    for (auto& [k, ptr] : m_sfx) {
        (void)k;
        if (ptr) {
            Mix_VolumeChunk(static_cast<Mix_Chunk*>(ptr),
                            (sfxVol * MIX_MAX_VOLUME) / 100);
        }
    }
}

}  // namespace blackjack
