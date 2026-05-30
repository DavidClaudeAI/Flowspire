// StreamDirector — tests unitaires du coeur (sans OBS).
// Lancer : ctest --output-on-failure  (ou directement l'executable sd_tests).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <map>
#include <string>
#include <vector>

#include "core/audio_util.hpp"
#include "core/config.hpp"
#include "core/director.hpp"
#include "core/speaker_detector.hpp"
#include "core/weighted_pick.hpp"

using namespace sd::core;

namespace {
// RNG deterministe rejouant une sequence fixe (puis se repete).
Director::Rng seq(std::vector<double> values) {
    auto idx = std::make_shared<size_t>(0);
    auto vals = std::make_shared<std::vector<double>>(std::move(values));
    return [idx, vals]() -> double {
        if (vals->empty()) {
            return 0.0;
        }
        const double v = (*vals)[*idx % vals->size()];
        ++(*idx);
        return v;
    };
}

Config twoSpeakerConfig() {
    Config c;
    c.wideShotScene = "Plateau";
    c.timing.minShotSeconds = 3.0;
    c.timing.maxShotSeconds = 12.0;
    Speaker a;
    a.id = "A";
    a.name = "Alice";
    a.audioSource = "srcA";
    a.scenes = {{"A_close", 90}, {"A_wide", 10}};
    Speaker b;
    b.id = "B";
    b.name = "Bob";
    b.audioSource = "srcB";
    b.scenes = {{"B_close", 100}};
    c.speakers = {a, b};
    return c;
}
}  // namespace

TEST_CASE("conversion mul -> dB") {
    CHECK(mulToDb(0.0) == doctest::Approx(kDbFloor));
    CHECK(mulToDb(1.0) == doctest::Approx(0.0));
    CHECK(mulToDb(0.5) == doctest::Approx(-6.0206).epsilon(0.001));
    CHECK(mulToDb(0.00001) == doctest::Approx(kDbFloor));
}

TEST_CASE("levelsToDb prend le peak max des canaux") {
    std::vector<std::vector<double>> frame = {{0.2, 0.25, 0.3}, {0.4, 0.5, 0.55}};
    CHECK(levelsToDb(frame) == doctest::Approx(-6.0206).epsilon(0.001));
    CHECK(levelsToDb({}) == doctest::Approx(kDbFloor));
}

TEST_CASE("detecteur : attaque sur 2 frames") {
    SpeakerDetector d(-35.0, 2, 8);
    const double loud = mulToDb(0.5);  // ~ -6 dB
    CHECK(d.update(loud) == false);    // 1 frame : pas encore
    CHECK(d.update(loud) == true);     // 2 frames : parle
}

TEST_CASE("detecteur : relachement sur 8 frames (anti-respiration)") {
    SpeakerDetector d(-35.0, 2, 8);
    const double loud = mulToDb(0.5);
    d.update(loud);
    d.update(loud);
    REQUIRE(d.speaking());
    for (int i = 0; i < 7; ++i) {
        CHECK(d.update(kDbFloor) == true);  // tient encore
    }
    CHECK(d.update(kDbFloor) == false);  // 8e : ne parle plus
}

TEST_CASE("detecteur : un pic isole ne declenche pas") {
    SpeakerDetector d(-35.0, 2, 8);
    CHECK(d.update(mulToDb(0.9)) == false);
    CHECK(d.update(kDbFloor) == false);
    CHECK(d.speaking() == false);
}

TEST_CASE("weightedPick respecte les bornes et ignore les poids nuls") {
    std::vector<std::pair<std::string, int>> opts = {{"x", 0}, {"y", 10}};
    CHECK(*weightedPick(opts, 0.0) == "y");
    CHECK(*weightedPick(opts, 0.99) == "y");
    std::vector<std::pair<std::string, int>> empty;
    CHECK(weightedPick(empty, 0.5) == nullptr);
}

TEST_CASE("weightedPick : distribution conforme aux poids") {
    // 90 / 10 : r<0.9 -> a, r>=0.9 -> b
    std::vector<std::pair<std::string, int>> opts = {{"a", 90}, {"b", 10}};
    CHECK(*weightedPick(opts, 0.0) == "a");
    CHECK(*weightedPick(opts, 0.89) == "a");
    CHECK(*weightedPick(opts, 0.90) == "b");
    CHECK(*weightedPick(opts, 0.95) == "b");
}

TEST_CASE("config : round-trip JSON") {
    const Config in = twoSpeakerConfig();
    const std::string text = toJson(in);
    const Config out = fromJson(text);
    REQUIRE(out.speakers.size() == 2);
    CHECK(out.speakers[0].id == "A");
    CHECK(out.speakers[0].scenes.size() == 2);
    CHECK(out.speakers[0].scenes[0].scene == "A_close");
    CHECK(out.speakers[0].scenes[0].weight == 90);
    CHECK(out.wideShotScene == "Plateau");
    CHECK(out.whenMultiple.loudestSpeaker == 45);
    CHECK(out.whenSilence.lastSpeaker == 80);
    CHECK(out.timing.minShotSeconds == doctest::Approx(3.0));
}

TEST_CASE("config : JSON tolerant aux cles absentes") {
    const Config c = fromJson(R"({"version":1,"speakers":[]})");
    CHECK(c.speakers.empty());
    CHECK(c.audio.voiceThresholdDb == doctest::Approx(-35.0));  // defaut
    CHECK(c.whenMultiple.wideShot == 25);                       // defaut
}

TEST_CASE("director : contexte A bascule sur la scene de celui qui parle") {
    // rng=0.0 -> tire la 1ere scene du pool (A_close, poids 90)
    Director dir(twoSpeakerConfig(), seq({0.0}));
    // attaque = 2 frames pour passer "parle"
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    Decision d = dir.update(0.1, {{"A", mulToDb(0.5)}});
    CHECK(d.context == Context::Single);
    CHECK(d.scene == "A_close");
    CHECK(d.owner == "A");
    CHECK(d.switched == true);
}

TEST_CASE("director : verrou temps-mini empeche un recoupe trop rapide") {
    Director dir(twoSpeakerConfig(), seq({0.0}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}});  // -> A_close a t=0.1
    REQUIRE(dir.currentScene() == "A_close");
    // B se met a parler tres vite : attaque 2 frames, mais on est dans le verrou (<3s)
    dir.update(0.2, {{"B", mulToDb(0.9)}});
    Decision d = dir.update(0.3, {{"B", mulToDb(0.9)}});
    CHECK(d.scene == "A_close");      // verrou : pas de bascule
    CHECK(d.switched == false);
}

TEST_CASE("director : apres le verrou, bascule sur le nouveau locuteur") {
    Director dir(twoSpeakerConfig(), seq({0.0}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}});  // A_close a t=0.1
    // A se tait (8 frames), B parle, le temps depasse minShot (3s)
    for (int i = 0; i < 8; ++i) {
        dir.update(0.2 + i * 0.05, {{"A", kDbFloor}, {"B", mulToDb(0.9)}});
    }
    Decision d = dir.update(4.0, {{"B", mulToDb(0.9)}});
    CHECK(d.scene == "B_close");
    CHECK(d.owner == "B");
    CHECK(d.switched == true);
}

TEST_CASE("director : contexte B (plusieurs) — le plus fort gagne avec rng=0") {
    // opts B = loudest(45)/current(30)/wide(25) ; rng=0 -> 'loudest'
    // puis tirage scene du plus fort. On rend B plus fort que A.
    Director dir(twoSpeakerConfig(), seq({0.0}));
    // amorcage : faire parler les deux (attaque 2 frames)
    dir.update(0.0, {{"A", mulToDb(0.4)}, {"B", mulToDb(0.8)}});
    Decision d = dir.update(0.1, {{"A", mulToDb(0.4)}, {"B", mulToDb(0.8)}});
    CHECK(d.context == Context::Multiple);
    CHECK(d.owner == "B");        // B plus fort
    CHECK(d.scene == "B_close");
}

TEST_CASE("director : contexte C (silence) — rng force le plan large") {
    Config c = twoSpeakerConfig();
    Director dir(c, seq({0.99}));  // 0.99 -> dans la part 'wide' (last80/wide20)
    // Personne ne parle des le depart : contexte Silence.
    Decision d = dir.update(0.0, {{"A", kDbFloor}, {"B", kDbFloor}});
    CHECK(d.context == Context::Silence);
    CHECK(d.scene == "Plateau");
    CHECK(d.owner.empty());
}

TEST_CASE("director : auto desactive ne bascule jamais") {
    Director dir(twoSpeakerConfig(), seq({0.0}));
    dir.setAutoEnabled(false);
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    Decision d = dir.update(0.1, {{"A", mulToDb(0.5)}});
    CHECK(d.scene.empty());
    CHECK(d.switched == false);
}

TEST_CASE("director : forceScene applique la scene et arme le verrou") {
    Director dir(twoSpeakerConfig(), seq({0.0}));
    Decision f = dir.forceScene(1.0, "Plateau");
    CHECK(f.scene == "Plateau");
    CHECK(dir.currentScene() == "Plateau");
    // juste apres, A parle : verrou actif -> pas de bascule
    dir.update(1.1, {{"A", mulToDb(0.5)}});
    Decision d = dir.update(1.2, {{"A", mulToDb(0.5)}});
    CHECK(d.scene == "Plateau");
}

TEST_CASE("director : memoire des derniers locuteurs (anti ping-pong)") {
    Director dir(twoSpeakerConfig(), seq({0.0}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}});
    dir.update(5.0, {{"B", mulToDb(0.9)}});
    dir.update(5.1, {{"B", mulToDb(0.9)}});
    const auto recent = dir.recentSpeakers(5.2);
    REQUIRE(recent.size() == 2);
    CHECK(recent[0] == "B");  // le plus recent d'abord
    CHECK(recent[1] == "A");
    // au-dela de la fenetre (12s), l'historique s'oublie
    CHECK(dir.recentSpeakers(20.0).empty());
}
