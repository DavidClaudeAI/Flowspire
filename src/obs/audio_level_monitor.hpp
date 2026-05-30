// StreamDirector — pont OBS : lecture des niveaux audio internes.
// Couche "obs" : seul endroit qui parle a l'API OBS pour l'audio. Encapsule un
// obs_volmeter par source audio. Le callback volmeter est invoque sur un THREAD
// AUDIO ; on stocke le dernier pic (dB) + son horodatage dans des std::atomic
// pour que le thread UI puisse les lire sans verrou. Ce sont les memes niveaux
// que ceux affiches par OBS.
#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Forward-declaration : on evite d'inclure <obs.h> dans ce header.
struct obs_volmeter;
typedef struct obs_volmeter obs_volmeter_t;

namespace sd::obsbridge {

class AudioLevelMonitor {
public:
    AudioLevelMonitor() = default;
    ~AudioLevelMonitor();

    AudioLevelMonitor(const AudioLevelMonitor&) = delete;
    AudioLevelMonitor& operator=(const AudioLevelMonitor&) = delete;

    // Enumere les sources audio d'OBS et cree un volmeter attache a chacune.
    // Idempotent : un appel relache d'abord l'etat precedent (stop()).
    void start();

    // Detache et detruit tous les volmeters. Sur : remove_callback est appele
    // avant destruction, donc plus aucun callback n'ecrit apres stop()
    // (OBS serialise callback et remove_callback via le meme mutex interne).
    void stop();

    // Noms des sources audio decouvertes (dans l'ordre d'enumeration OBS).
    std::vector<std::string> sourceNames() const;

    // Dernier niveau (dB, borne [-60, 0]) par source, avec RETOMBEE temporelle :
    // si une source n'a pas recu de mise a jour depuis `staleSeconds`, son niveau
    // est ramene au plancher (une source en pause/muette cesse d'emettre des
    // callbacks et resterait sinon figee a son dernier pic -> faux "parle").
    // `nowSeconds` = horloge monotone (meme source de temps que l'appelant).
    std::map<std::string, double> snapshot(double nowSeconds, double staleSeconds = 0.3) const;

private:
    struct SourceMeter {
        std::string name;
        obs_volmeter_t* volmeter = nullptr;
        std::atomic<float> peakDb{-60.0f};
        std::atomic<double> lastUpdate{0.0};  // horodatage monotone du dernier callback
    };

    // Callback OBS (thread audio). `param` = le SourceMeter concerne.
    static void volmeterCallback(void* param, const float magnitude[],
                                 const float peak[], const float inputPeak[]);

    // Horloge monotone partagee callback (thread audio) <-> snapshot (thread UI).
    static double monotonicNow();

    // Protege meters_ : start()/stop() (mutation) vs snapshot()/sourceNames()
    // (lecture). Les atomics internes restent lisibles sans ce verrou, mais le
    // vector lui-meme (taille/stockage) doit etre protege contre un re-scan
    // concurrent. Mutable pour permettre le lock dans les methodes const.
    mutable std::mutex metersMutex_;
    std::vector<std::unique_ptr<SourceMeter>> meters_;
};

}  // namespace sd::obsbridge
