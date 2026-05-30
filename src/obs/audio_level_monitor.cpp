#include "obs/audio_level_monitor.hpp"

#include <obs.h>

#include <chrono>
#include <unordered_set>

namespace sd::obsbridge {

namespace {
// Plancher coherent avec le coeur (sd::core::kDbFloor).
constexpr float kFloorDb = -60.0f;
}  // namespace

AudioLevelMonitor::~AudioLevelMonitor() {
    stop();
}

double AudioLevelMonitor::monotonicNow() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

void AudioLevelMonitor::volmeterCallback(void* param, const float magnitude[],
                                         const float peak[], const float inputPeak[]) {
    (void)magnitude;
    (void)inputPeak;
    auto* meter = static_cast<SourceMeter*>(param);

    // On prend le pic le plus fort parmi les canaux (plus reactif a la parole).
    // peak[] est en dB (negatif, 0 = max) ; les canaux inactifs valent -inf dB,
    // donc le max les ignore naturellement. On part du plancher pour ne jamais
    // renvoyer une valeur hors contrat [-60, 0].
    float best = kFloorDb;
    for (int i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
        const float v = peak[i];
        // v == v ecarte les NaN (NaN > best est toujours faux, mais on est explicite).
        if (v == v && v > best) {
            best = v;
        }
    }
    meter->peakDb.store(best, std::memory_order_relaxed);
    meter->lastUpdate.store(monotonicNow(), std::memory_order_relaxed);
}

void AudioLevelMonitor::start() {
    stop();

    // 1) Collecter les noms des sources audio (obs_enum_sources = sources input).
    //    Deduplication : OBS peut exposer deux sources de meme nom ; on n'en
    //    garde qu'une (un nom = une cle dans la suite du pipeline).
    struct EnumCtx {
        std::vector<std::string> names;
        std::unordered_set<std::string> seen;
    };
    EnumCtx ctx;
    obs_enum_sources(
        [](void* param, obs_source_t* source) -> bool {
            const uint32_t flags = obs_source_get_output_flags(source);
            if (flags & OBS_SOURCE_AUDIO) {
                const char* name = obs_source_get_name(source);
                if (name && *name) {
                    auto* c = static_cast<EnumCtx*>(param);
                    if (c->seen.insert(name).second) {
                        c->names.emplace_back(name);
                    }
                }
            }
            return true;  // continuer l'enumeration
        },
        &ctx);

    // 2) Creer un volmeter par source et l'attacher.
    std::lock_guard<std::mutex> lock(metersMutex_);
    const double now = monotonicNow();
    for (const auto& name : ctx.names) {
        obs_source_t* source = obs_get_source_by_name(name.c_str());
        if (!source) {
            // Source disparue entre l'enumeration et l'attach : on l'ignore
            // (pas d'entree fantome figee dans le monitor).
            continue;
        }

        obs_volmeter_t* volmeter = obs_volmeter_create(OBS_FADER_LOG);
        if (!volmeter) {
            obs_source_release(source);
            continue;
        }

        auto meter = std::make_unique<SourceMeter>();
        meter->name = name;
        meter->volmeter = volmeter;
        meter->lastUpdate.store(now, std::memory_order_relaxed);

        // attach garde un pointeur brut + se connecte au signal "destroy" de la
        // source (pas une ref forte) : on peut donc relacher notre ref ensuite ;
        // si la source meurt, OBS detache automatiquement le volmeter.
        obs_volmeter_attach_source(volmeter, source);
        obs_source_release(source);

        obs_volmeter_add_callback(volmeter, &AudioLevelMonitor::volmeterCallback, meter.get());
        meters_.push_back(std::move(meter));
    }
}

void AudioLevelMonitor::stop() {
    std::lock_guard<std::mutex> lock(metersMutex_);
    for (auto& meter : meters_) {
        if (meter->volmeter) {
            // remove_callback d'abord : OBS le serialise avec l'invocation du
            // callback (meme callback_mutex), donc apres ce point plus aucun
            // callback n'ecrira dans peakDb -> destruction sure.
            obs_volmeter_remove_callback(meter->volmeter, &AudioLevelMonitor::volmeterCallback,
                                         meter.get());
            obs_volmeter_detach_source(meter->volmeter);
            obs_volmeter_destroy(meter->volmeter);
            meter->volmeter = nullptr;
        }
    }
    meters_.clear();
}

std::vector<std::string> AudioLevelMonitor::sourceNames() const {
    std::lock_guard<std::mutex> lock(metersMutex_);
    std::vector<std::string> names;
    names.reserve(meters_.size());
    for (const auto& meter : meters_) {
        names.push_back(meter->name);
    }
    return names;
}

std::map<std::string, double> AudioLevelMonitor::snapshot(double nowSeconds,
                                                          double staleSeconds) const {
    std::lock_guard<std::mutex> lock(metersMutex_);
    std::map<std::string, double> out;
    for (const auto& meter : meters_) {
        double db = static_cast<double>(meter->peakDb.load(std::memory_order_relaxed));

        // Retombee : si pas de mise a jour recente, la source est en pause/muette
        // -> on ramene au plancher au lieu de rester fige sur le dernier pic.
        const double last = meter->lastUpdate.load(std::memory_order_relaxed);
        if (nowSeconds - last > staleSeconds) {
            db = kFloorDb;
        }

        if (db < kFloorDb) {
            db = kFloorDb;
        } else if (db > 0.0) {
            db = 0.0;
        }
        out[meter->name] = db;
    }
    return out;
}

}  // namespace sd::obsbridge
