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
#include "core/profiles.hpp"
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

TEST_CASE("config : round-trip JSON complet (tous les champs)") {
    Config in = twoSpeakerConfig();
    // valeurs non-defaut sur TOUS les champs pour detecter une cle oubliee
    in.version = 7;
    in.audio = {-28.0, -55.0, 3, 11};  // voiceThr, volFloor, attack, release
    in.timing = {2.5, 9.0, 7.0};
    in.whenMultiple = {40, 35, 25};
    in.whenSilence = {70, 30};
    const Config out = fromJson(toJson(in));

    CHECK(out.version == 7);
    REQUIRE(out.speakers.size() == 2);
    CHECK(out.speakers[1].id == "B");
    CHECK(out.speakers[1].scenes[0].scene == "B_close");
    CHECK(out.speakers[1].scenes[0].weight == 100);
    CHECK(out.wideShotScene == "Plateau");

    CHECK(out.audio.voiceThresholdDb == doctest::Approx(-28.0));
    CHECK(out.audio.volumeFloorDb == doctest::Approx(-55.0));
    CHECK(out.audio.attackFrames == 3);
    CHECK(out.audio.releaseFrames == 11);

    CHECK(out.timing.minShotSeconds == doctest::Approx(2.5));
    CHECK(out.timing.maxShotSeconds == doctest::Approx(9.0));
    CHECK(out.timing.pingPongWindowSeconds == doctest::Approx(7.0));

    CHECK(out.whenMultiple.loudestSpeaker == 40);
    CHECK(out.whenMultiple.currentSpeaker == 35);
    CHECK(out.whenMultiple.wideShot == 25);
    CHECK(out.whenSilence.lastSpeaker == 70);
    CHECK(out.whenSilence.wideShot == 30);
}

TEST_CASE("config : JSON tolerant aux cles absentes") {
    const Config c = fromJson(R"({"version":1,"speakers":[]})");
    CHECK(c.speakers.empty());
    CHECK(c.audio.voiceThresholdDb == doctest::Approx(-35.0));  // defaut
    CHECK(c.whenMultiple.wideShot == 25);                       // defaut
}

TEST_CASE("config : JSON invalide leve une exception") {
    CHECK_THROWS(fromJson("{ ceci n'est pas du json"));
}

TEST_CASE("config : fromJson normalise les invariants (max>=min, frames>=1)") {
    // Un config.json edite a la main / d'une version anterieure peut violer les
    // invariants ; fromJson les retablit a la lecture (source unique de verite).
    const Config c = fromJson(R"({
        "version":1,
        "timing":{"minShotSeconds":20.0,"maxShotSeconds":8.0,"pingPongWindowSeconds":10.0},
        "audio":{"attackFrames":0,"releaseFrames":0}
    })");
    // max etait < min -> remonte au niveau du min (pas de re-cut court-circuitant le verrou).
    CHECK(c.timing.maxShotSeconds == doctest::Approx(20.0));
    CHECK(c.timing.minShotSeconds == doctest::Approx(20.0));
    // frames a 0 -> au moins 1 (sinon l'hysteresis ne se declenche jamais).
    CHECK(c.audio.attackFrames == 1);
    CHECK(c.audio.releaseFrames == 1);
}

TEST_CASE("config : fromJson laisse intact un timing deja valide") {
    const Config c = fromJson(R"({
        "version":1,
        "timing":{"minShotSeconds":3.0,"maxShotSeconds":12.0}
    })");
    CHECK(c.timing.minShotSeconds == doctest::Approx(3.0));
    CHECK(c.timing.maxShotSeconds == doctest::Approx(12.0));
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

TEST_CASE("director : contexte B — choix 'plan large' quand plusieurs parlent") {
    // B a une seule scene (B_close, 100). On installe d'abord un plan courant
    // (A_close via A seul), puis on fait parler A+B : opts = loudest/current/wide.
    // rng=0.999 -> derniere option = 'wide' (45+30+25 -> 0.999*100=99.9 dans [75,100[).
    Director dir(twoSpeakerConfig(), seq({0.999}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}});  // A_close affiche (hold)
    // attendre la fin du verrou (minShot=3s) pour autoriser une bascule
    for (int i = 0; i < 8; ++i) {
        dir.update(0.2 + i * 0.05, {{"A", mulToDb(0.6)}, {"B", mulToDb(0.9)}});
    }
    Decision d = dir.update(4.0, {{"A", mulToDb(0.6)}, {"B", mulToDb(0.9)}});
    CHECK(d.context == Context::Multiple);
    CHECK(d.scene == "Plateau");   // bascule sur le plan large
    CHECK(d.owner.empty());
}

TEST_CASE("director : contexte C (silence) — last 80 / wide 20, pondere") {
    // Quelqu'un a deja parle (lastSpeaker=A), puis silence : opts = last(80)/wide(20).
    // rng=0.0 -> 'last' (premiere option) -> scene du pool de A (A_close).
    Director dir(twoSpeakerConfig(), seq({0.0}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}});  // A devient lastSpeaker
    // silence prolonge (relachement 8 frames + depasser minShot)
    Decision d;
    for (int i = 0; i < 80; ++i) {
        d = dir.update(0.2 + i * 0.1, {{"A", kDbFloor}, {"B", kDbFloor}});
    }
    CHECK(d.context == Context::Silence);
    CHECK(d.owner == "A");          // 'last' tire -> on garde le dernier locuteur
    CHECK(d.scene == "A_close");
}

TEST_CASE("director : contexte C (silence) — rng force le plan large") {
    // lastSpeaker=A puis silence ; rng=0.99 -> part 'wide' (last80/wide20).
    Director dir(twoSpeakerConfig(), seq({0.99}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}});
    Decision d;
    for (int i = 0; i < 80; ++i) {
        d = dir.update(0.2 + i * 0.1, {{"A", kDbFloor}, {"B", kDbFloor}});
    }
    CHECK(d.context == Context::Silence);
    CHECK(d.scene == "Plateau");
    CHECK(d.owner.empty());
}

TEST_CASE("director : rafraichissement au temps-max (variete des plans)") {
    // A a 2 scenes (A_close 90 / A_wide 10). RNG CONTROLABLE : on fixe la valeur
    // retournee selon la phase, pour ne pas dependre du nombre exact de tirages
    // consommes pendant la montee en parole (test robuste).
    auto rngState = std::make_shared<double>(0.0);  // 0.0 -> A_close
    Director::Rng rng = [rngState]() { return *rngState; };

    Config c = twoSpeakerConfig();
    c.wideShotScene = "";       // pas de plan large : isole le pool de A
    c.timing.maxShotSeconds = 12.0;
    Director dir(c, rng);

    dir.update(0.0, {{"A", mulToDb(0.5)}});           // frame 1 : montee (pas encore "parle")
    Decision d = dir.update(0.1, {{"A", mulToDb(0.5)}});  // frame 2 : A parle -> tirage 0.0 -> A_close
    REQUIRE(dir.currentScene() == "A_close");
    CHECK(d.owner == "A");

    // A continue de parler ; avant maxShot -> aucun changement (anti-nervosite)
    d = dir.update(5.0, {{"A", mulToDb(0.5)}});
    CHECK(d.scene == "A_close");
    CHECK(d.switched == false);

    // on bascule le RNG vers A_wide, puis on depasse maxShot -> rafraichissement
    *rngState = 0.95;  // 0.95 -> A_wide (cumul 90/10)
    d = dir.update(13.0, {{"A", mulToDb(0.5)}});
    CHECK(d.owner == "A");
    CHECK(d.scene == "A_wide");
    CHECK(d.switched == true);
}

TEST_CASE("director : silence prolonge — variete au temps-max (re-tirage du choix)") {
    // Regression (observe en reel par David) : en silence, le choix last/wide
    // etait tire UNE fois puis fige -> on ne repassait jamais au plan large, meme
    // apres 30 s. Au temps-max, le choix doit etre RE-TIRE.
    Config c = twoSpeakerConfig();           // wideShotScene = "Plateau", maxShot 12s
    auto rngState = std::make_shared<double>(0.0);  // 1er tirage silence -> 'last'
    Director::Rng rng = [rngState]() { return *rngState; };
    Director dir(c, rng);

    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}});  // A parle -> A_close, lastSwitch=0.1
    REQUIRE(dir.currentScene() == "A_close");

    // Silence : relachement (8 frames) puis le choix 'last' garde A_close.
    Decision d;
    for (int i = 0; i < 10; ++i) {
        d = dir.update(1.0 + i * 0.1, {{"A", kDbFloor}, {"B", kDbFloor}});
    }
    REQUIRE(d.context == Context::Silence);
    CHECK(dir.currentScene() == "A_close");  // 'last' -> dernier locuteur, fige

    // On bascule le RNG vers 'wide' et on depasse le temps-max : le choix doit
    // etre re-tire -> passage au plan large (ce qui ne se produisait jamais avant).
    *rngState = 0.99;
    d = dir.update(20.0, {{"A", kDbFloor}, {"B", kDbFloor}});
    CHECK(d.context == Context::Silence);
    CHECK(d.scene == "Plateau");
    CHECK(d.switched == true);
}

TEST_CASE("director : single ne re-tire pas a chaque tick (anti-scintillement)") {
    // A a 2 scenes ; le RNG renverrait des scenes differentes a chaque tick s'il
    // etait appele. On verifie que la scene NE change PAS tant qu'on reste sous
    // maxShot et que A parle en continu.
    Config c = twoSpeakerConfig();
    c.timing.maxShotSeconds = 100.0;
    Director dir(c, seq({0.0, 0.95, 0.95, 0.95}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}});
    const std::string first = dir.currentScene();
    for (int i = 0; i < 20; ++i) {
        Decision d = dir.update(1.0 + i * 0.5, {{"A", mulToDb(0.5)}});
        CHECK(d.scene == first);       // pas de scintillement
        CHECK(d.switched == false);
    }
}

TEST_CASE("director : fallback plan large quand le locuteur n'a aucune scene") {
    // Cause racine : un locuteur sans scene ne doit pas laisser l'ecran vide.
    Config c;
    c.wideShotScene = "Plateau";
    Speaker a;
    a.id = "A";
    a.audioSource = "srcA";
    // pool VIDE volontairement
    c.speakers = {a};
    Director dir(c, seq({0.0}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    Decision d = dir.update(0.1, {{"A", mulToDb(0.5)}});
    CHECK(d.context == Context::Single);
    CHECK(d.scene == "Plateau");   // fallback : on montre le plan large
    CHECK(d.owner.empty());
}

TEST_CASE("director : sans scene ni plan large, rien n'est affiche (pas de crash)") {
    Config c;  // pas de wideShotScene, speaker sans scene
    Speaker a;
    a.id = "A";
    a.audioSource = "srcA";
    c.speakers = {a};
    Director dir(c, seq({0.0}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    Decision d = dir.update(0.1, {{"A", mulToDb(0.5)}});
    CHECK(d.scene.empty());        // rien de jouable -> on ne force pas de scene
    CHECK(d.switched == false);
}

TEST_CASE("director : forceScene puis reprise auto apres le verrou") {
    Director dir(twoSpeakerConfig(), seq({0.0}));
    dir.forceScene(1.0, "Plateau");          // plan force (hold)
    // pendant le verrou (minShot=3s), A parle mais on reste sur Plateau
    dir.update(1.1, {{"A", mulToDb(0.5)}});
    Decision d = dir.update(1.2, {{"A", mulToDb(0.5)}});
    CHECK(d.scene == "Plateau");
    // apres le verrou, A parle toujours -> l'auto reprend la main
    d = dir.update(5.0, {{"A", mulToDb(0.5)}});
    CHECK(d.owner == "A");
    CHECK(d.scene == "A_close");
    CHECK(d.switched == true);
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

TEST_CASE("director : seuil par intervenant (slider) surpasse le seuil global") {
    Config c = twoSpeakerConfig();
    c.audio.voiceThresholdDb = -35.0;
    c.audio.attackFrames = 1;  // declenchement immediat pour le test
    Director dir(c, seq({0.0}));

    // Par defaut : seuil global -35 dB. Un niveau de -20 dB > -35 -> A parle.
    CHECK(dir.speakerThresholdDb("A") == doctest::Approx(-35.0));
    CHECK(dir.update(0.0, {{"A", -20.0}}).context == Context::Single);

    // On REMONTE le seuil de A a -10 dB (slider) : -20 dB < -10 -> A ne parle plus.
    dir.setSpeakerThreshold("A", -10.0);
    CHECK(dir.speakerThresholdDb("A") == doctest::Approx(-10.0));
    // relachement (releaseFrames=8) puis verif silence
    Decision d;
    for (int i = 0; i < 10; ++i) {
        d = dir.update(1.0 + i * 0.1, {{"A", -20.0}});
    }
    CHECK(d.context == Context::Silence);

    // B n'est pas affecte par l'override de A (seuil global -35) -> B parle a -20.
    CHECK(dir.speakerThresholdDb("B") == doctest::Approx(-35.0));
}

TEST_CASE("director : override de seuil inconnu ignore + reset par setConfig") {
    Config c = twoSpeakerConfig();
    Director dir(c, seq({0.0}));
    dir.setSpeakerThreshold("ZZZ", -10.0);              // inconnu : ignore
    CHECK(dir.speakerThresholdDb("ZZZ") == doctest::Approx(c.audio.voiceThresholdDb));
    dir.setSpeakerThreshold("A", -12.0);
    CHECK(dir.speakerThresholdDb("A") == doctest::Approx(-12.0));
    dir.setConfig(c);                                    // recharge : la config reprend la main
    CHECK(dir.speakerThresholdDb("A") == doctest::Approx(c.audio.voiceThresholdDb));
}

TEST_CASE("director : forceSpeaker tire une scene du pool de l'intervenant") {
    Director dir(twoSpeakerConfig(), seq({0.0}));  // 0.0 -> 1er du pool de A = A_close
    Decision d = dir.forceSpeaker(1.0, "A");
    CHECK(d.switched == true);
    CHECK(d.owner == "A");
    CHECK(d.scene == "A_close");
    CHECK(dir.currentScene() == "A_close");
}

TEST_CASE("director : forceSpeaker respecte le pool pondere") {
    // Pool A = A_close(90)/A_wide(10), total 100. rng=0.95 -> tranche [90,100) = A_wide.
    Director dir(twoSpeakerConfig(), seq({0.95}));
    Decision d = dir.forceSpeaker(1.0, "A");
    CHECK(d.scene == "A_wide");
}

TEST_CASE("director : forceSpeaker sur intervenant inconnu ne change rien") {
    Director dir(twoSpeakerConfig(), seq({0.0}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}});  // A_close affiche
    Decision d = dir.forceSpeaker(1.0, "ZZZ");
    CHECK(d.switched == false);
    CHECK(dir.currentScene() == "A_close");
}

TEST_CASE("director : forceSpeaker sans scene dans le pool ne change rien") {
    Config c = twoSpeakerConfig();
    c.speakers[1].scenes.clear();  // B n'a plus de scene
    Director dir(c, seq({0.0}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}});  // A_close affiche
    Decision d = dir.forceSpeaker(1.0, "B");
    CHECK(d.switched == false);
    CHECK(dir.currentScene() == "A_close");
}

TEST_CASE("director : contexte B 'current' apres plan force sans owner ne part pas en plan large") {
    // Regression (revue Run 3) : apres forceScene(scene, owner=""), le plan courant
    // n'a pas d'owner et n'est PAS le plan large. Si le tirage Multiple donne
    // 'current' (rng dans [45,75[ -> 0.5), on ne doit PAS basculer sur le plan
    // large ; on retombe sur le plus fort (choix sur), jamais sur "Plateau".
    Director dir(twoSpeakerConfig(), seq({0.5}));
    dir.forceScene(0.0, "A_close", "");  // plan courant sans owner, != wideShotScene
    // Laisser tomber le verrou (minShot 3s) puis faire parler A+B.
    for (int i = 0; i < 8; ++i) {
        dir.update(0.2 + i * 0.05, {{"A", mulToDb(0.6)}, {"B", mulToDb(0.9)}});
    }
    Decision d = dir.update(4.0, {{"A", mulToDb(0.6)}, {"B", mulToDb(0.9)}});
    CHECK(d.context == Context::Multiple);
    CHECK(d.scene != "Plateau");   // surtout PAS le plan large par erreur
    CHECK(d.owner == "B");         // retombe sur le plus fort
}

TEST_CASE("director : forceSpeaker pose un hold (verrou temps-mini)") {
    Director dir(twoSpeakerConfig(), seq({0.0}));
    dir.forceSpeaker(1.0, "B");  // -> B_close (hold), lastSwitch = 1.0
    CHECK(dir.currentScene() == "B_close");
    // A parle juste apres : le verrou (minShot 3s) tient le plan force.
    dir.update(1.5, {{"A", mulToDb(0.9)}});
    dir.update(1.6, {{"A", mulToDb(0.9)}});  // A "parle" (attaque 2 frames)
    CHECK(dir.currentScene() == "B_close");
    // Une fois le verrou ecoule, A prend l'antenne.
    Decision d = dir.update(4.5, {{"A", mulToDb(0.9)}});
    CHECK(d.scene == "A_close");
    CHECK(dir.currentScene() == "A_close");
}

// ===========================================================================
// Catalogue de profils (modele pur)

TEST_CASE("profiles : round-trip JSON du catalogue") {
    ProfileIndex idx;
    idx.activeId = 2;
    idx.nextId = 4;
    idx.profiles = {{2, "4 invites + moi"}, {1, "3 invites"}, {3, "Duo"}};
    const ProfileIndex back = profileIndexFromJson(profileIndexToJson(idx));
    CHECK(back.activeId == 2);
    CHECK(back.nextId == 4);
    REQUIRE(back.profiles.size() == 3);
    CHECK(back.profiles[0].id == 2);
    CHECK(back.profiles[0].name == "4 invites + moi");
    CHECK(back.profiles[2].name == "Duo");  // l'ordre d'affichage est preserve
}

TEST_CASE("profiles : fromJson normalise nextId et activeId") {
    // nextId trop bas + activeId pointant un profil absent -> corriges.
    const std::string js = R"({
        "activeId": 99, "nextId": 1,
        "profiles": [ {"id": 5, "name": "A"}, {"id": 8, "name": "B"} ]
    })";
    const ProfileIndex idx = profileIndexFromJson(js);
    CHECK(idx.nextId == 9);    // au-dessus du plus grand id (8)
    CHECK(idx.activeId == 5);  // 99 invalide -> premier profil
}

TEST_CASE("profiles : addProfile attribue des ids croissants sans reutilisation") {
    ProfileIndex idx;
    const int a = addProfile(idx, "A");
    const int b = addProfile(idx, "B");
    CHECK(a == 1);
    CHECK(b == 2);
    CHECK(idx.nextId == 3);
    // suppression de B puis ajout : l'id de B n'est PAS recycle.
    idx.activeId = a;  // a est actif, b est supprimable
    CHECK(removeProfile(idx, b) == true);
    const int c = addProfile(idx, "C");
    CHECK(c == 3);  // pas 2
}

TEST_CASE("profiles : noms rendus uniques") {
    ProfileIndex idx;
    addProfile(idx, "Plateau");
    const int dup = addProfile(idx, "Plateau");
    CHECK(idx.profiles[1].name == "Plateau (2)");
    CHECK(nameExists(idx, "Plateau"));
    // renommer une entree avec son propre nom reste autorise (exceptId).
    CHECK(renameProfile(idx, dup, "Plateau (2)") == true);
    CHECK(idx.profiles[1].name == "Plateau (2)");
    // renommer vers un nom pris -> suffixe.
    CHECK(renameProfile(idx, dup, "Plateau") == true);
    CHECK(idx.profiles[1].name == "Plateau (2)");
}

TEST_CASE("profiles : removeProfile protege l'actif et le dernier") {
    ProfileIndex idx;
    const int a = addProfile(idx, "A");
    const int b = addProfile(idx, "B");
    idx.activeId = a;
    CHECK(removeProfile(idx, a) == false);    // a est actif
    CHECK(removeProfile(idx, 999) == false);  // id absent
    CHECK(removeProfile(idx, b) == true);
    CHECK(removeProfile(idx, a) == false);  // a est le dernier restant
    REQUIRE(idx.profiles.size() == 1);
}

TEST_CASE("profiles : setActiveProfile valide l'id") {
    ProfileIndex idx;
    const int a = addProfile(idx, "A");
    const int b = addProfile(idx, "B");
    CHECK(setActiveProfile(idx, b) == true);
    CHECK(idx.activeId == b);
    CHECK(setActiveProfile(idx, 999) == false);
    CHECK(idx.activeId == b);  // inchange
    CHECK(setActiveProfile(idx, a) == true);
}

// --- Pilier B (Run 8) : seuil par intervenant persiste dans le profil ---

TEST_CASE("config : thresholdDb par intervenant survit a un aller-retour JSON") {
    Config c;
    Speaker a;
    a.id = "A";
    a.name = "Alice";
    a.audioSource = "srcA";
    a.thresholdDb = -28.0;  // seuil propre regle
    Speaker b;
    b.id = "B";
    b.name = "Bob";
    b.audioSource = "srcB";  // pas de seuil propre -> doit rester absent
    c.speakers = {a, b};

    const std::string js = toJson(c);
    const Config back = fromJson(js);
    REQUIRE(back.speakers.size() == 2);
    CHECK(back.speakers[0].thresholdDb.has_value());
    CHECK(back.speakers[0].thresholdDb.value() == doctest::Approx(-28.0));
    CHECK_FALSE(back.speakers[1].thresholdDb.has_value());
}

TEST_CASE("config : un intervenant sans seuil propre n'ecrit pas la cle thresholdDb") {
    Config c;
    Speaker a;
    a.id = "A";
    a.audioSource = "srcA";  // pas de thresholdDb
    c.speakers = {a};
    const std::string js = toJson(c);
    CHECK(js.find("thresholdDb") == std::string::npos);
}

TEST_CASE("director : setConfig seme l'override de seuil depuis le profil") {
    Config c = twoSpeakerConfig();    // A et B, seuil global par defaut (-35)
    c.audio.voiceThresholdDb = -35.0;
    c.speakers[0].thresholdDb = -28.0;  // A a un seuil propre ; B non
    Director dir(c);
    // A : seuil propre seme depuis le profil ; B : retombe sur le global.
    CHECK(dir.speakerThresholdDb("A") == doctest::Approx(-28.0));
    CHECK(dir.speakerThresholdDb("B") == doctest::Approx(-35.0));
    // Un intervenant inconnu retombe aussi sur le global (pas d'override orphelin).
    CHECK(dir.speakerThresholdDb("inconnu") == doctest::Approx(-35.0));
}

// --- Anti ping-pong (Run 12) : amortir la navette en contexte multiple ---

TEST_CASE("director : l'anti ping-pong amortit la navette (contexte multiple)") {
    // Deux personnes se chevauchent (toujours au-dessus du seuil) ; le plus fort
    // alterne. rng=0 => le tirage choisit toujours "le plus fort". On verifie que,
    // fenetre active, le retour vers une personne qu'on vient de quitter est amorti.
    auto run = [](double window) -> std::string {
        Config c = twoSpeakerConfig();
        c.timing.minShotSeconds = 3.0;
        c.timing.pingPongWindowSeconds = window;
        Director dir(c, seq({0.0}));
        const double hi = mulToDb(0.9);  // ~ -0.9 dB (le plus fort)
        const double lo = mulToDb(0.5);  // ~ -6 dB
        // Phase 1 : A le plus fort -> on finit par montrer A.
        dir.update(0.0, {{"A", hi}, {"B", lo}});
        dir.update(0.1, {{"A", hi}, {"B", lo}});
        // Phase 2 : verrou ecoule, B devient le plus fort -> bascule A -> B.
        dir.update(3.2, {{"A", lo}, {"B", hi}});
        dir.update(3.3, {{"A", lo}, {"B", hi}});
        // Phase 3 : verrou ecoule, A redevient le plus fort -> tentative de RETOUR.
        dir.update(6.4, {{"A", hi}, {"B", lo}});
        const Decision d = dir.update(6.5, {{"A", hi}, {"B", lo}});
        return d.owner;
    };
    CHECK(run(12.0) == "B");  // fenetre active : on RESTE sur B (navette amortie)
    CHECK(run(0.0) == "A");   // anti ping-pong off : on suit le plus fort -> retour A
}

TEST_CASE("director : l'anti ping-pong relache APRES la fenetre (vraie borne de temps)") {
    // Prouve que c'est la FENETRE qui borne le maintien, pas le temps-max (regression :
    // si l'override etait fige dans la memoisation, on resterait colle jusqu'au temps-max).
    Config c = twoSpeakerConfig();
    c.timing.minShotSeconds = 3.0;
    c.timing.maxShotSeconds = 30.0;  // grand expres : ne doit PAS gouverner le relachement
    c.timing.pingPongWindowSeconds = 5.0;
    Director dir(c, seq({0.0}));
    const double hi = mulToDb(0.9);
    const double lo = mulToDb(0.5);
    dir.update(0.0, {{"A", hi}, {"B", lo}});
    dir.update(0.1, {{"A", hi}, {"B", lo}});  // -> A
    dir.update(3.2, {{"A", lo}, {"B", hi}});
    dir.update(3.3, {{"A", lo}, {"B", hi}});  // -> B (on a quitte A a 3.2)
    // A redevient le plus fort, encore DANS la fenetre (3.2 + 5 = 8.2) -> on reste B.
    CHECK(dir.update(6.0, {{"A", hi}, {"B", lo}}).owner == "B");
    // Bien APRES la fenetre (et avant le temps-max=30) -> on bascule enfin sur A.
    dir.update(9.0, {{"A", hi}, {"B", lo}});
    CHECK(dir.update(9.1, {{"A", hi}, {"B", lo}}).owner == "A");
}
