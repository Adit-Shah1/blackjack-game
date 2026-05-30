#pragma once

#include <string>
#include <unordered_map>

namespace blackjack {

class AudioManager {
public:
    bool init();
    ~AudioManager();

    void playSFX(const std::string& key);
    void playAmbient(const std::string& key);
    void stopAmbient();

    void setMasterVolume(int vol);   // 0-100
    void setSFXVolume(int vol);      // 0-100
    void setAmbientVolume(int vol);  // 0-100

private:
    int m_masterVolume = 100;
    int m_sfxVolume = 100;
    int m_ambientVolume = 100;

    // Stored as void* to avoid SDL_mixer header in public header
    std::unordered_map<std::string, void*> m_sfx;    // Mix_Chunk*
    std::unordered_map<std::string, void*> m_music;  // Mix_Music*

    void applyVolumes();
};

}  // namespace blackjack
