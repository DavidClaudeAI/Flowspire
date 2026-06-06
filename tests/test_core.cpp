// Flowspire — tests unitaires du coeur (sans OBS).
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
#include "core/rhythm_style.hpp"
#include "core/speaker_detector.hpp"
#include "core/threshold_calibration.hpp"
#include "core/version.hpp"
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
    // Poids, relachement ET reaction au silence FIXES ici (independants des defauts livres) :
    // ces tests verifient la LOGIQUE de tirage/hysteresis, pas la config par defaut du produit.
    // Les defauts livres ont change (profil "Cyp Live") -> sans ce verrouillage, les tests
    // B/C/silence casseraient.
    c.audio.attackFrames = 2; // EPINGLE (sinon depend du defaut struct -> timing des tests fragile)
    c.audio.releaseFrames = 8;
    c.whenMultiple = {30, 25}; // {rester, plan large} (plus de "le plus fort")
    c.whenSilence = {80, 20};
    c.timing.silenceReactionSeconds = 0.0; // reaction immediate (la grace a ses propres tests)
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
} // namespace

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
    const double loud = mulToDb(0.5); // ~ -6 dB
    CHECK(d.update(loud) == false);   // 1 frame : pas encore
    CHECK(d.update(loud) == true);    // 2 frames : parle
}

TEST_CASE("detecteur : relachement sur 8 frames (anti-respiration)") {
    SpeakerDetector d(-35.0, 2, 8);
    const double loud = mulToDb(0.5);
    d.update(loud);
    d.update(loud);
    REQUIRE(d.speaking());
    for (int i = 0; i < 7; ++i) {
        CHECK(d.update(kDbFloor) == true); // tient encore
    }
    CHECK(d.update(kDbFloor) == false); // 8e : ne parle plus
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
    in.audio = {-28.0, -55.0, 3, 11}; // voiceThr, volFloor, attack, release
    in.timing = {2.5, 9.0, 7.0, 4.0}; // min, max, pingPong, silenceReaction
    in.whenMultiple = {40, 35};       // {rester, plan large}
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
    CHECK(out.timing.silenceReactionSeconds == doctest::Approx(4.0));

    CHECK(out.whenMultiple.currentSpeaker == 40);
    CHECK(out.whenMultiple.wideShot == 35);
    CHECK(out.whenSilence.lastSpeaker == 70);
    CHECK(out.whenSilence.wideShot == 30);
}

TEST_CASE("config : JSON tolerant aux cles absentes") {
    const Config c = fromJson(R"({"version":1,"speakers":[]})");
    CHECK(c.speakers.empty());
    CHECK(c.audio.voiceThresholdDb == doctest::Approx(-35.0)); // defaut
    CHECK(c.whenMultiple.wideShot == 100);                     // defaut (tune "Cyp Live")
}

TEST_CASE("config : retrocompat - ancienne cle whenMultiple.loudestSpeaker ignoree sans erreur") {
    // Un profil ecrit par v0.5.0 contient "loudestSpeaker" (option "le plus fort" retiree). Il
    // doit se charger SANS erreur, la cle etant simplement ignoree, currentSpeaker/wideShot
    // preserves, et styleName a vide (=Perso) faute de cle.
    const Config c =
        fromJson(R"({"version":1,"whenMultiple":{"loudestSpeaker":45,"currentSpeaker":30,"wideShot":25}})");
    CHECK(c.whenMultiple.currentSpeaker == 30);
    CHECK(c.whenMultiple.wideShot == 25);
    CHECK(c.styleName.empty());
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
    dir.update(0.1, {{"A", mulToDb(0.5)}}); // -> A_close a t=0.1
    REQUIRE(dir.currentScene() == "A_close");
    // B se met a parler tres vite : attaque 2 frames, mais on est dans le verrou (<3s)
    dir.update(0.2, {{"B", mulToDb(0.9)}});
    Decision d = dir.update(0.3, {{"B", mulToDb(0.9)}});
    CHECK(d.scene == "A_close"); // verrou : pas de bascule
    CHECK(d.switched == false);
}

TEST_CASE("director : apres le verrou, bascule sur le nouveau locuteur") {
    Director dir(twoSpeakerConfig(), seq({0.0}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}}); // A_close a t=0.1
    // A se tait (8 frames), B parle, le temps depasse minShot (3s)
    for (int i = 0; i < 8; ++i) {
        dir.update(0.2 + i * 0.05, {{"A", kDbFloor}, {"B", mulToDb(0.9)}});
    }
    Decision d = dir.update(4.0, {{"B", mulToDb(0.9)}});
    CHECK(d.scene == "B_close");
    CHECK(d.owner == "B");
    CHECK(d.switched == true);
}

TEST_CASE("director : contexte B (plusieurs) — 'rester' garde le plan du locuteur courant (rng=0)") {
    // On etablit d'abord un plan courant (A seul -> A_close), puis A+B parlent. opts =
    // {rester(30)/plan large(25)} ; rng=0 -> 'rester' -> on GARDE A (plus de "le plus fort").
    Director dir(twoSpeakerConfig(), seq({0.0}));
    dir.update(0.0, {{"A", mulToDb(0.8)}});
    dir.update(0.1, {{"A", mulToDb(0.8)}}); // A_close (Single A)
    for (int i = 0; i < 8; ++i) {           // depasser le verrou mini avant d'autoriser une bascule
        dir.update(0.2 + i * 0.05, {{"A", mulToDb(0.8)}, {"B", mulToDb(0.7)}});
    }
    Decision d = dir.update(4.0, {{"A", mulToDb(0.8)}, {"B", mulToDb(0.7)}});
    CHECK(d.context == Context::Multiple);
    CHECK(d.owner == "A"); // on reste sur le locuteur courant, pas de bascule "au plus fort"
    CHECK(d.scene == "A_close");
}

TEST_CASE("director : multi — plan large a 0, un locuteur qui se tait NE force PAS le plan large") {
    // David : plan large a 0 = "jamais de plan large". Quand le locuteur sur lequel on reste se
    // tait (mais 2+ parlent encore), on RESPECTE ce 0 -> on garde son plan (le vrai changement
    // se fera quand quelqu'un reprend SEUL la parole = contexte Single). On ne va au plan large
    // que si son poids est > 0. (NB : la cle suit le locuteur -> il y a bien re-tirage quand il
    // se tait, mais ce re-tirage respecte le poids 0.)
    Config c;
    c.wideShotScene = "Plateau";
    c.timing.minShotSeconds = 3.0;
    c.timing.maxShotSeconds = 30.0; // grand : ne doit PAS gouverner la decision
    c.whenMultiple = {100, 0};      // "rester" seul possible (plan large interdit)
    c.audio.attackFrames = 2;       // EPINGLE (timing deterministe, independant du defaut struct)
    c.audio.releaseFrames = 2;
    Speaker a;
    a.id = "A";
    a.name = "A";
    a.audioSource = "sA";
    a.scenes = {{"A_close", 100}};
    Speaker b;
    b.id = "B";
    b.name = "B";
    b.audioSource = "sB";
    b.scenes = {{"B_close", 100}};
    Speaker cc;
    cc.id = "C";
    cc.name = "C";
    cc.audioSource = "sC";
    cc.scenes = {{"C_close", 100}};
    c.speakers = {a, b, cc};
    Director dir(c, seq({0.0}));
    const double hi = mulToDb(0.9);
    dir.update(0.0, {{"A", hi}, {"B", kDbFloor}, {"C", kDbFloor}});
    dir.update(0.1, {{"A", hi}, {"B", kDbFloor}, {"C", kDbFloor}}); // -> A_close (Single A)
    for (int i = 0; i < 6; ++i) {
        dir.update(0.2 + i * 0.1, {{"A", hi}, {"B", hi}, {"C", kDbFloor}}); // A+B multi -> on reste sur A
    }
    REQUIRE(dir.currentScene() == "A_close");
    // A SE TAIT ; B et C parlent (toujours multi). Plan large interdit (poids 0) -> on RESTE
    // sur A (on ne force pas le plan large, on ne saute pas "au plus fort").
    Decision d;
    for (int i = 0; i < 10; ++i) {
        d = dir.update(4.0 + i * 0.1, {{"A", kDbFloor}, {"B", hi}, {"C", hi}});
    }
    CHECK(d.context == Context::Multiple);
    CHECK(d.owner == "A"); // plan large a 0 respecte : on garde le plan courant
    CHECK(d.scene == "A_close");
}

TEST_CASE("director : anti ping-pong NE se declenche PAS sur la pause d'un orateur seul") {
    // Regression (revue) : l'anti ping-pong ("retour au plan large") vise la navette A<->B.
    // Un orateur SEUL qui marque une pause (-> plan large au silence) puis reprend ne doit PAS
    // rester bloque sur le plan large : ce n'est pas une navette. Corrige en n'armant la navette
    // que lorsqu'on quitte un locuteur POUR UN AUTRE LOCUTEUR (pas pour le plan large).
    Config c;
    c.wideShotScene = "Plateau";
    c.timing.minShotSeconds = 1.0;
    c.timing.maxShotSeconds = 30.0;
    c.timing.pingPongWindowSeconds = 5.0;  // anti ping-pong arme
    c.timing.silenceReactionSeconds = 0.0; // reaction au silence immediate (isole l'effet)
    c.whenSilence = {0, 100};              // silence -> plan large a coup sur
    c.audio.attackFrames = 2;
    c.audio.releaseFrames = 2;
    Speaker a;
    a.id = "A";
    a.name = "A";
    a.audioSource = "sA";
    a.scenes = {{"A_close", 100}};
    c.speakers = {a};
    Director dir(c, seq({0.0}));
    const double hi = mulToDb(0.9);
    dir.update(0.0, {{"A", hi}});
    dir.update(0.1, {{"A", hi}}); // A seul -> A_close
    REQUIRE(dir.currentScene() == "A_close");
    // A se tait : apres le verrou mini, on bascule au plan large (silence).
    for (int i = 0; i < 30 && dir.currentScene() != "Plateau"; ++i) {
        dir.update(1.2 + i * 0.1, {{"A", kDbFloor}});
    }
    REQUIRE(dir.currentScene() == "Plateau");
    // A REPREND la parole DANS la fenetre (5 s) -> on doit RECOUPER SUR A (ce n'etait pas une navette).
    Decision d;
    double t = 5.0;
    for (int i = 0; i < 20; ++i) {
        d = dir.update(t, {{"A", hi}});
        t += 0.1;
    }
    CHECK(d.owner == "A");
    CHECK(d.scene == "A_close");
}

TEST_CASE("config : un style applique (tempo + poids plan large) survit a un aller-retour JSON") {
    Config c = twoSpeakerConfig();
    applyRhythmStyle(c, builtinRhythmStyles()[2]); // Speed {65,10}/{30,70}, pingPong 5, mini 2
    const Config back = fromJson(toJson(c));
    CHECK(back.styleName == "Speed");
    CHECK(back.timing.minShotSeconds == doctest::Approx(2.0));
    CHECK(back.timing.pingPongWindowSeconds == doctest::Approx(5.0));
    CHECK(back.whenMultiple.currentSpeaker == 65);
    CHECK(back.whenMultiple.wideShot == 10);
    CHECK(back.whenSilence.lastSpeaker == 30);
    CHECK(back.whenSilence.wideShot == 70);
}

TEST_CASE("rhythm style : bibliotheque globale - round-trip JSON + tolerance") {
    Config speedCfg;
    applyRhythmStyle(speedCfg, builtinRhythmStyles()[2]); // Speed
    std::vector<RhythmStyle> lib;
    lib.push_back(styleFromConfig(speedCfg, "Mon debat")); // capture du reglage courant
    RhythmStyle posed;
    posed.name = "Talk pose";
    posed.minShotSeconds = 7.0;
    posed.maxShotSeconds = 14.0;
    posed.whenMultiple = {0, 100};
    posed.whenSilence = {0, 100};
    lib.push_back(posed);

    const auto round = rhythmStyleLibraryFromJson(rhythmStyleLibraryToJson(lib));
    REQUIRE(round.size() == 2);
    CHECK(round[0].name == "Mon debat");
    CHECK(round[0].whenMultiple.currentSpeaker == 65); // hérité de Speed
    CHECK(round[0].pingPongWindowSeconds == doctest::Approx(5.0));
    CHECK(round[1].name == "Talk pose");
    CHECK(round[1].maxShotSeconds == doctest::Approx(14.0));
    CHECK(round[1].whenSilence.wideShot == 100);

    // Tolerance : entree sans nom ignoree ; cles absentes -> defauts ; JSON minimal -> vide.
    const auto partial = rhythmStyleLibraryFromJson(R"({"styles":[{"name":""},{"name":"X"}]})");
    REQUIRE(partial.size() == 1);
    CHECK(partial[0].name == "X");
    CHECK(partial[0].minShotSeconds == doctest::Approx(3.0)); // defaut RhythmStyle
    CHECK(rhythmStyleLibraryFromJson(R"({})").empty());
}

TEST_CASE("rhythm style : makeUniqueStyleName evite les doublons ET les noms livres") {
    std::vector<RhythmStyle> lib;
    RhythmStyle a;
    a.name = "Debat";
    lib.push_back(a);
    CHECK(makeUniqueStyleName(lib, "Nouveau") == "Nouveau"); // libre
    CHECK(makeUniqueStyleName(lib, "Debat") == "Debat (2)"); // deja dans la lib
    CHECK(makeUniqueStyleName(lib, "Cool") == "Cool (2)");   // nom livre (built-in) -> evite
    CHECK(styleNameExists(lib, "Debat"));
    CHECK_FALSE(styleNameExists(lib, "Absent"));
}

TEST_CASE("director : contexte B — choix 'plan large' quand plusieurs parlent") {
    // B a une seule scene (B_close, 100). On installe d'abord un plan courant
    // (A_close via A seul), puis on fait parler A+B : opts = {rester(30)/plan large(25)}.
    // rng=0.999 -> derniere option = 'wide' (plan large).
    Director dir(twoSpeakerConfig(), seq({0.999}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}}); // A_close affiche (hold)
    // attendre la fin du verrou (minShot=3s) pour autoriser une bascule
    for (int i = 0; i < 8; ++i) {
        dir.update(0.2 + i * 0.05, {{"A", mulToDb(0.6)}, {"B", mulToDb(0.9)}});
    }
    Decision d = dir.update(4.0, {{"A", mulToDb(0.6)}, {"B", mulToDb(0.9)}});
    CHECK(d.context == Context::Multiple);
    CHECK(d.scene == "Plateau"); // bascule sur le plan large
    CHECK(d.owner.empty());
}

TEST_CASE("director : contexte C (silence) — last 80 / wide 20, pondere") {
    // Quelqu'un a deja parle (lastSpeaker=A), puis silence : opts = last(80)/wide(20).
    // rng=0.0 -> 'last' (premiere option) -> scene du pool de A (A_close).
    Director dir(twoSpeakerConfig(), seq({0.0}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}}); // A devient lastSpeaker
    // silence prolonge (relachement 8 frames + depasser minShot)
    Decision d;
    for (int i = 0; i < 80; ++i) {
        d = dir.update(0.2 + i * 0.1, {{"A", kDbFloor}, {"B", kDbFloor}});
    }
    CHECK(d.context == Context::Silence);
    CHECK(d.owner == "A"); // 'last' tire -> on garde le dernier locuteur
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
    auto rngState = std::make_shared<double>(0.0); // 0.0 -> A_close
    Director::Rng rng = [rngState]() {
        return *rngState;
    };

    Config c = twoSpeakerConfig();
    c.wideShotScene = ""; // pas de plan large : isole le pool de A
    c.timing.maxShotSeconds = 12.0;
    Director dir(c, rng);

    dir.update(0.0, {{"A", mulToDb(0.5)}});              // frame 1 : montee (pas encore "parle")
    Decision d = dir.update(0.1, {{"A", mulToDb(0.5)}}); // frame 2 : A parle -> tirage 0.0 -> A_close
    REQUIRE(dir.currentScene() == "A_close");
    CHECK(d.owner == "A");

    // A continue de parler ; avant maxShot -> aucun changement (anti-nervosite)
    d = dir.update(5.0, {{"A", mulToDb(0.5)}});
    CHECK(d.scene == "A_close");
    CHECK(d.switched == false);

    // on bascule le RNG vers A_wide, puis on depasse maxShot -> rafraichissement
    *rngState = 0.95; // 0.95 -> A_wide (cumul 90/10)
    d = dir.update(13.0, {{"A", mulToDb(0.5)}});
    CHECK(d.owner == "A");
    CHECK(d.scene == "A_wide");
    CHECK(d.switched == true);
}

TEST_CASE("director : silence prolonge — variete au temps-max (re-tirage du choix)") {
    // Regression (observe en reel par David) : en silence, le choix last/wide
    // etait tire UNE fois puis fige -> on ne repassait jamais au plan large, meme
    // apres 30 s. Au temps-max, le choix doit etre RE-TIRE.
    Config c = twoSpeakerConfig();                 // wideShotScene = "Plateau", maxShot 12s
    auto rngState = std::make_shared<double>(0.0); // 1er tirage silence -> 'last'
    Director::Rng rng = [rngState]() {
        return *rngState;
    };
    Director dir(c, rng);

    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}}); // A parle -> A_close, lastSwitch=0.1
    REQUIRE(dir.currentScene() == "A_close");

    // Silence : relachement (8 frames) puis le choix 'last' garde A_close.
    Decision d;
    for (int i = 0; i < 10; ++i) {
        d = dir.update(1.0 + i * 0.1, {{"A", kDbFloor}, {"B", kDbFloor}});
    }
    REQUIRE(d.context == Context::Silence);
    CHECK(dir.currentScene() == "A_close"); // 'last' -> dernier locuteur, fige

    // On bascule le RNG vers 'wide' et on depasse le temps-max : le choix doit
    // etre re-tire -> passage au plan large (ce qui ne se produisait jamais avant).
    *rngState = 0.99;
    d = dir.update(20.0, {{"A", kDbFloor}, {"B", kDbFloor}});
    CHECK(d.context == Context::Silence);
    CHECK(d.scene == "Plateau");
    CHECK(d.switched == true);
}

TEST_CASE("director : le plan large issu d'un silence respecte le temps mini (anti-flash)") {
    // Demande David : avant, le plan large de silence n'etait PAS un "hold" -> une
    // reprise de parole le remplacait IMMEDIATEMENT (flash). Desormais TOUT plan tient
    // le temps-mini. On verifie qu'apres etre passe sur le plan large en silence, une
    // reprise de parole AVANT minShot ne fait pas basculer, et qu'APRES minShot si.
    auto rngState = std::make_shared<double>(0.0);
    Director::Rng rng = [rngState]() {
        return *rngState;
    };
    Config c = twoSpeakerConfig(); // minShot=3, maxShot=12, wide=Plateau
    Director dir(c, rng);

    // A parle brievement -> A_close (hold, lastSwitch=0.1).
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}});
    REQUIRE(dir.currentScene() == "A_close");

    // Silence + rng force 'wide' : le verrou tient A_close jusqu'au temps-mini, puis on
    // bascule sur le plan large. On capture l'instant exact de cette bascule.
    *rngState = 0.99; // 'wide' (silence : last 80 / wide 20)
    double wideAt = -1.0;
    double t = 0.2;
    for (int i = 0; i < 80 && wideAt < 0.0; ++i) {
        Decision d = dir.update(t, {{"A", kDbFloor}, {"B", kDbFloor}});
        if (d.switched && d.scene == "Plateau") {
            wideAt = t;
        }
        t += 0.1;
    }
    REQUIRE(wideAt > 0.0);
    REQUIRE(dir.currentScene() == "Plateau");

    // A reparle TOUT DE SUITE apres le passage au plan large (< minShot) -> on RESTE sur
    // le plan large (anti-flash : il montre tout le monde, on ne rate rien).
    dir.update(wideAt + 0.1, {{"A", mulToDb(0.9)}});              // A monte
    Decision d = dir.update(wideAt + 0.2, {{"A", mulToDb(0.9)}}); // A "parle" (attaque 2 frames)
    CHECK(d.scene == "Plateau");

    // Apres le temps-mini, A prend l'antenne (rng=0.0 -> A_close).
    *rngState = 0.0;
    d = dir.update(wideAt + 3.5, {{"A", mulToDb(0.9)}});
    CHECK(d.owner == "A");
    CHECK(d.scene == "A_close");
}

TEST_CASE("director : le plan large succedant a une scene FORCEE tient le temps mini (anti-flash)") {
    // Regression attrapee au /code-review : un critere base sur currentOwner_ laissait le
    // plan large succedant a une scene FORCEE sans proprietaire (forceScene owner="")
    // NON verrouille -> flash. Le critere `!init` le couvre : des qu'un plan a ete affiche,
    // le plan large qui lui succede tient le temps mini.
    auto rngState = std::make_shared<double>(0.99); // silence sans lastSpeaker -> 'wide'
    Director::Rng rng = [rngState]() {
        return *rngState;
    };
    Director dir(twoSpeakerConfig(), rng); // minShot=3, wide=Plateau

    // Scene forcee SANS proprietaire (ex. scene hors-regie "comptee comme un plan").
    dir.forceScene(0.0, "Intro", "");
    REQUIRE(dir.currentScene() == "Intro");

    // Silence : on bascule sur le plan large apres expiration du verrou de la scene forcee.
    Decision d;
    double wideAt = -1.0;
    double t = 0.1;
    for (int i = 0; i < 80 && wideAt < 0.0; ++i) {
        d = dir.update(t, {{"A", kDbFloor}, {"B", kDbFloor}});
        if (d.switched && d.scene == "Plateau") {
            wideAt = t;
        }
        t += 0.1;
    }
    REQUIRE(wideAt > 0.0);

    // Quelqu'un parle juste apres -> le plan large tient le temps mini (pas de flash).
    dir.update(wideAt + 0.1, {{"A", mulToDb(0.9)}});
    d = dir.update(wideAt + 0.2, {{"A", mulToDb(0.9)}});
    CHECK(d.scene == "Plateau");
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
        CHECK(d.scene == first); // pas de scintillement
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
    CHECK(d.scene == "Plateau"); // fallback : on montre le plan large
    CHECK(d.owner.empty());
}

TEST_CASE("director : sans scene ni plan large, rien n'est affiche (pas de crash)") {
    Config c; // pas de wideShotScene, speaker sans scene
    Speaker a;
    a.id = "A";
    a.audioSource = "srcA";
    c.speakers = {a};
    Director dir(c, seq({0.0}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    Decision d = dir.update(0.1, {{"A", mulToDb(0.5)}});
    CHECK(d.scene.empty()); // rien de jouable -> on ne force pas de scene
    CHECK(d.switched == false);
}

TEST_CASE("director : forceScene puis reprise auto apres le verrou") {
    Director dir(twoSpeakerConfig(), seq({0.0}));
    dir.forceScene(1.0, "Plateau"); // plan force (hold)
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
    c.audio.attackFrames = 1; // declenchement immediat pour le test
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
    dir.setSpeakerThreshold("ZZZ", -10.0); // inconnu : ignore
    CHECK(dir.speakerThresholdDb("ZZZ") == doctest::Approx(c.audio.voiceThresholdDb));
    dir.setSpeakerThreshold("A", -12.0);
    CHECK(dir.speakerThresholdDb("A") == doctest::Approx(-12.0));
    dir.setConfig(c); // recharge : la config reprend la main
    CHECK(dir.speakerThresholdDb("A") == doctest::Approx(c.audio.voiceThresholdDb));
}

TEST_CASE("director : forceSpeaker tire une scene du pool de l'intervenant") {
    Director dir(twoSpeakerConfig(), seq({0.0})); // 0.0 -> 1er du pool de A = A_close
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
    dir.update(0.1, {{"A", mulToDb(0.5)}}); // A_close affiche
    Decision d = dir.forceSpeaker(1.0, "ZZZ");
    CHECK(d.switched == false);
    CHECK(dir.currentScene() == "A_close");
}

TEST_CASE("director : forceSpeaker sans scene dans le pool ne change rien") {
    Config c = twoSpeakerConfig();
    c.speakers[1].scenes.clear(); // B n'a plus de scene
    Director dir(c, seq({0.0}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}}); // A_close affiche
    Decision d = dir.forceSpeaker(1.0, "B");
    CHECK(d.switched == false);
    CHECK(dir.currentScene() == "A_close");
}

TEST_CASE("director : contexte B 'rester' apres plan force sans owner -> plan large (plus de repli 'le plus fort')") {
    // Apres forceScene(scene, owner=""), le plan courant n'a pas d'owner. Si le tirage
    // Multiple donne 'rester' (rng=0.5 sur {rester(30)/large(25)}), on ne peut pas "rester"
    // comme owner -> on se recule sur le plan large. NB : avant le retrait du "plus fort",
    // ce cas retombait sur le plus fort ; ce repli a disparu (le volume ne decide plus).
    Director dir(twoSpeakerConfig(), seq({0.5}));
    dir.forceScene(0.0, "A_close", ""); // plan courant sans owner, != wideShotScene
    // Laisser tomber le verrou (minShot 3s) puis faire parler A+B.
    for (int i = 0; i < 8; ++i) {
        dir.update(0.2 + i * 0.05, {{"A", mulToDb(0.6)}, {"B", mulToDb(0.9)}});
    }
    Decision d = dir.update(4.0, {{"A", mulToDb(0.6)}, {"B", mulToDb(0.9)}});
    CHECK(d.context == Context::Multiple);
    CHECK(d.scene == "Plateau"); // on se recule sur le plan large
    CHECK(d.owner.empty());
}

TEST_CASE("director : forceSpeaker pose un hold (verrou temps-mini)") {
    Director dir(twoSpeakerConfig(), seq({0.0}));
    dir.forceSpeaker(1.0, "B"); // -> B_close (hold), lastSwitch = 1.0
    CHECK(dir.currentScene() == "B_close");
    // A parle juste apres : le verrou (minShot 3s) tient le plan force.
    dir.update(1.5, {{"A", mulToDb(0.9)}});
    dir.update(1.6, {{"A", mulToDb(0.9)}}); // A "parle" (attaque 2 frames)
    CHECK(dir.currentScene() == "B_close");
    // Une fois le verrou ecoule, A prend l'antenne.
    Decision d = dir.update(4.5, {{"A", mulToDb(0.9)}});
    CHECK(d.scene == "A_close");
    CHECK(dir.currentScene() == "A_close");
}

TEST_CASE("director : sceneInProgram reconnait pool, plan large et hors-regie") {
    Director dir(twoSpeakerConfig()); // wide=Plateau ; A:{A_close,A_wide} ; B:{B_close}
    std::string owner = "sentinelle";
    // Scene d'un intervenant -> owner renseigne.
    CHECK(dir.sceneInProgram("A_close", owner));
    CHECK(owner == "A");
    owner = "sentinelle";
    CHECK(dir.sceneInProgram("B_close", owner));
    CHECK(owner == "B");
    // Plan large -> owner vide.
    owner = "sentinelle";
    CHECK(dir.sceneInProgram("Plateau", owner));
    CHECK(owner.empty());
    // Hors regie -> false.
    owner = "sentinelle";
    CHECK_FALSE(dir.sceneInProgram("Intro", owner));
    // Scene vide -> false (jamais consideree dans la regie).
    CHECK_FALSE(dir.sceneInProgram("", owner));
}

TEST_CASE("director : sceneInProgram — le plan large prime sur un pool homonyme") {
    Config c = twoSpeakerConfig();
    c.speakers[0].scenes = {{"Plateau", 50}, {"A_close", 50}}; // A a aussi le plan large
    Director dir(c);
    std::string owner = "sentinelle";
    CHECK(dir.sceneInProgram("Plateau", owner));
    CHECK(owner.empty()); // reconnu comme plan large, pas comme scene de A
}

TEST_CASE("director : sceneInProgram — un intervenant a id vide n'est jamais proprietaire") {
    // Invariant : owner vide <=> plan large. Un id vide (config corrompue) ne doit pas
    // faire passer une scene d'intervenant pour le plan large.
    Config c;
    c.wideShotScene = ""; // pas de plan large -> isole le cas
    Speaker bad;
    bad.id = ""; // corrompu
    bad.audioSource = "src";
    bad.scenes = {{"X_close", 100}};
    c.speakers = {bad};
    Director dir(c);
    std::string owner = "sentinelle";
    CHECK_FALSE(dir.sceneInProgram("X_close", owner)); // pas reconnu comme dans la regie
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
    CHECK(back.profiles[2].name == "Duo"); // l'ordre d'affichage est preserve
}

TEST_CASE("profiles : fromJson normalise nextId et activeId") {
    // nextId trop bas + activeId pointant un profil absent -> corriges.
    const std::string js = R"({
        "activeId": 99, "nextId": 1,
        "profiles": [ {"id": 5, "name": "A"}, {"id": 8, "name": "B"} ]
    })";
    const ProfileIndex idx = profileIndexFromJson(js);
    CHECK(idx.nextId == 9);   // au-dessus du plus grand id (8)
    CHECK(idx.activeId == 5); // 99 invalide -> premier profil
}

TEST_CASE("profiles : addProfile attribue des ids croissants sans reutilisation") {
    ProfileIndex idx;
    const int a = addProfile(idx, "A");
    const int b = addProfile(idx, "B");
    CHECK(a == 1);
    CHECK(b == 2);
    CHECK(idx.nextId == 3);
    // suppression de B puis ajout : l'id de B n'est PAS recycle.
    idx.activeId = a; // a est actif, b est supprimable
    CHECK(removeProfile(idx, b) == true);
    const int c = addProfile(idx, "C");
    CHECK(c == 3); // pas 2
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
    CHECK(removeProfile(idx, a) == false);   // a est actif
    CHECK(removeProfile(idx, 999) == false); // id absent
    CHECK(removeProfile(idx, b) == true);
    CHECK(removeProfile(idx, a) == false); // a est le dernier restant
    REQUIRE(idx.profiles.size() == 1);
}

TEST_CASE("profiles : setActiveProfile valide l'id") {
    ProfileIndex idx;
    const int a = addProfile(idx, "A");
    const int b = addProfile(idx, "B");
    CHECK(setActiveProfile(idx, b) == true);
    CHECK(idx.activeId == b);
    CHECK(setActiveProfile(idx, 999) == false);
    CHECK(idx.activeId == b); // inchange
    CHECK(setActiveProfile(idx, a) == true);
}

// --- Pilier B (Run 8) : seuil par intervenant persiste dans le profil ---

TEST_CASE("config : thresholdDb par intervenant survit a un aller-retour JSON") {
    Config c;
    Speaker a;
    a.id = "A";
    a.name = "Alice";
    a.audioSource = "srcA";
    a.thresholdDb = -28.0; // seuil propre regle
    Speaker b;
    b.id = "B";
    b.name = "Bob";
    b.audioSource = "srcB"; // pas de seuil propre -> doit rester absent
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
    a.audioSource = "srcA"; // pas de thresholdDb
    c.speakers = {a};
    const std::string js = toJson(c);
    CHECK(js.find("thresholdDb") == std::string::npos);
}

TEST_CASE("director : setConfig seme l'override de seuil depuis le profil") {
    Config c = twoSpeakerConfig(); // A et B, seuil global par defaut (-35)
    c.audio.voiceThresholdDb = -35.0;
    c.speakers[0].thresholdDb = -28.0; // A a un seuil propre ; B non
    Director dir(c);
    // A : seuil propre seme depuis le profil ; B : retombe sur le global.
    CHECK(dir.speakerThresholdDb("A") == doctest::Approx(-28.0));
    CHECK(dir.speakerThresholdDb("B") == doctest::Approx(-35.0));
    // Un intervenant inconnu retombe aussi sur le global (pas d'override orphelin).
    CHECK(dir.speakerThresholdDb("inconnu") == doctest::Approx(-35.0));
}

// --- Delai avant reaction au silence (grace de silence) ---

TEST_CASE("director : grace de silence — tient le locuteur puis bascule au plan large") {
    Config c = twoSpeakerConfig();
    c.timing.silenceReactionSeconds = 2.0; // grace de 2 s
    // rng=0.85 : contexte A -> A_close (pool 90/10) ; silence -> wide (whenSilence 80/20).
    Director dir(c, seq({0.85}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}});
    dir.update(4.0, {{"A", mulToDb(0.5)}}); // A_close, lastSwitch=0.1, temps-mini (3 s) ecoule
    REQUIRE(dir.currentScene() == "A_close");

    // A se tait : on avance jusqu'a ce que le silence soit confirme (relachement detecteur).
    double tSilence = -1.0, t = 4.1;
    for (int i = 0; i < 20 && tSilence < 0.0; ++i) {
        Decision d = dir.update(t, {{"A", kDbFloor}, {"B", kDbFloor}});
        if (d.context == Context::Silence) {
            tSilence = t;
        }
        t += 0.1;
    }
    REQUIRE(tSilence > 0.0);
    // Pendant la grace (silence < 2 s) : on RESTE sur A_close.
    CHECK(dir.update(tSilence + 1.0, {{"A", kDbFloor}, {"B", kDbFloor}}).scene == "A_close");
    // Au-dela de la grace : on bascule sur le plan large.
    CHECK(dir.update(tSilence + 2.5, {{"A", kDbFloor}, {"B", kDbFloor}}).scene == "Plateau");
}

TEST_CASE("director : reprise pendant la grace de silence -> on n'a jamais quitte le locuteur") {
    Config c = twoSpeakerConfig();
    c.timing.silenceReactionSeconds = 2.0;
    Director dir(c, seq({0.85}));
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}});
    dir.update(4.0, {{"A", mulToDb(0.5)}});
    REQUIRE(dir.currentScene() == "A_close");

    double tSilence = -1.0, t = 4.1;
    for (int i = 0; i < 20 && tSilence < 0.0; ++i) {
        Decision d = dir.update(t, {{"A", kDbFloor}, {"B", kDbFloor}});
        if (d.context == Context::Silence) {
            tSilence = t;
        }
        t += 0.1;
    }
    REQUIRE(tSilence > 0.0);
    CHECK(dir.update(tSilence + 1.0, {{"A", kDbFloor}, {"B", kDbFloor}}).scene == "A_close"); // grace

    // A REPREND la parole avant la fin de la grace (attaque 2 frames) -> on n'a jamais quitte.
    dir.update(tSilence + 1.2, {{"A", mulToDb(0.9)}});
    Decision d = dir.update(tSilence + 1.3, {{"A", mulToDb(0.9)}});
    CHECK(d.context == Context::Single);
    CHECK(d.scene == "A_close");
    CHECK(d.switched == false); // pas de bascule : reprise instantanee
}

TEST_CASE("director : un nouveau locuteur pendant la grace de silence bascule normalement") {
    Config c = twoSpeakerConfig();
    c.timing.silenceReactionSeconds = 2.0;
    Director dir(c, seq({0.0})); // contexte A/B : 1ere scene du pool
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}});
    dir.update(4.0, {{"A", mulToDb(0.5)}}); // A_close, temps-mini ecoule
    REQUIRE(dir.currentScene() == "A_close");

    double tSilence = -1.0, t = 4.1;
    for (int i = 0; i < 20 && tSilence < 0.0; ++i) {
        Decision d = dir.update(t, {{"A", kDbFloor}, {"B", kDbFloor}});
        if (d.context == Context::Silence) {
            tSilence = t;
        }
        t += 0.1;
    }
    REQUIRE(tSilence > 0.0);

    // Pendant la grace, B prend la parole -> bascule IMMEDIATE (la grace n'agit que sur le silence).
    dir.update(tSilence + 0.5, {{"B", mulToDb(0.9)}});
    Decision d = dir.update(tSilence + 0.6, {{"B", mulToDb(0.9)}});
    CHECK(d.context == Context::Single);
    CHECK(d.owner == "B");
    CHECK(d.scene == "B_close");
    CHECK(d.switched == true);
}

TEST_CASE("director : grace de silence + temps-mini — blanc AVANT le temps-mini tient le locuteur") {
    // Cas d'origine de David : un plan vient d'etre pris, un blanc survient AVANT le
    // temps-mini. La grace ET le verrou temps-mini doivent tous deux retenir le locuteur ;
    // on ne bascule au plan large qu'une fois les DEUX ecoules.
    Config c = twoSpeakerConfig(); // minShot = 3 s, wide = "Plateau"
    c.timing.silenceReactionSeconds = 1.0;
    Director dir(c, seq({0.85})); // A_close en contexte A ; wide en silence
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}}); // A_close, lastSwitch = 0.1
    REQUIRE(dir.currentScene() == "A_close");

    // A se tait TRES VITE (le blanc commence bien avant les 3 s de temps-mini).
    double tSilence = -1.0, t = 0.2;
    for (int i = 0; i < 20 && tSilence < 0.0; ++i) {
        Decision d = dir.update(t, {{"A", kDbFloor}, {"B", kDbFloor}});
        if (d.context == Context::Silence) {
            tSilence = t;
        }
        t += 0.1;
    }
    REQUIRE(tSilence > 0.0); // ~0.9 s (relachement 8 frames)

    // t=2 : grace (1 s) ecoulee, MAIS le temps-mini (lastSwitch 0.1 + 3 s) tient encore.
    CHECK(dir.update(2.0, {{"A", kDbFloor}, {"B", kDbFloor}}).scene == "A_close");
    // t=3.5 : grace ecoulee ET temps-mini ecoule -> on bascule enfin au plan large.
    CHECK(dir.update(3.5, {{"A", kDbFloor}, {"B", kDbFloor}}).scene == "Plateau");
}

TEST_CASE("director : grace de silence SANS plan large — reste sur le dernier locuteur") {
    Config c = twoSpeakerConfig();
    c.wideShotScene = ""; // pas de plan large
    c.timing.silenceReactionSeconds = 1.0;
    Director dir(c, seq({0.0})); // A_close (1er du pool)
    dir.update(0.0, {{"A", mulToDb(0.5)}});
    dir.update(0.1, {{"A", mulToDb(0.5)}});
    dir.update(4.0, {{"A", mulToDb(0.5)}}); // A_close, temps-mini ecoule
    REQUIRE(dir.currentScene() == "A_close");

    double tSilence = -1.0, t = 4.1;
    for (int i = 0; i < 20 && tSilence < 0.0; ++i) {
        Decision d = dir.update(t, {{"A", kDbFloor}, {"B", kDbFloor}});
        if (d.context == Context::Silence) {
            tSilence = t;
        }
        t += 0.1;
    }
    REQUIRE(tSilence > 0.0);

    CHECK(dir.update(tSilence + 0.5, {{"A", kDbFloor}, {"B", kDbFloor}}).scene == "A_close"); // grace
    // Grace ecoulee : sans plan large, la decision de silence reste sur le dernier locuteur
    // (jamais d'ecran vide).
    Decision d = dir.update(tSilence + 2.0, {{"A", kDbFloor}, {"B", kDbFloor}});
    CHECK(d.context == Context::Silence);
    CHECK(d.scene == "A_close");
}

// --- Anti ping-pong reaffecte : "retour au plan large sur echange rapide" ---
// A et B se relaient comme SEULS locuteurs (navette). Quand on s'apprete a recouper sur
// quelqu'un qu'on vient de quitter (dans la fenetre), on se recule sur le plan large.

namespace {
// Joue une navette A-seul -> B-seul -> A-revient, et renvoie la decision quand A revient.
// `wide` vide => config SANS plan large (test de degradation gracieuse).
Decision runBounce(double window, const std::string& wide) {
    Config c = twoSpeakerConfig();
    c.wideShotScene = wide;
    c.timing.minShotSeconds = 3.0;
    c.timing.maxShotSeconds = 30.0;
    c.timing.pingPongWindowSeconds = window;
    c.audio.releaseFrames = 2; // relachement court -> transitions Single nettes
    Director dir(c, seq({0.0}));
    const double hi = mulToDb(0.9);
    auto solo = [&](double t0, const std::string& who, int n) {
        for (int i = 0; i < n; ++i) {
            std::map<std::string, double> lv = {{"A", kDbFloor}, {"B", kDbFloor}};
            lv[who] = hi;
            dir.update(t0 + i * 0.1, lv);
        }
    };
    solo(0.0, "A", 6); // A seul -> A_close (verrou s'arme)
    solo(3.5, "B", 6); // apres le verrou : B seul -> cut B (on quitte A)
    solo(7.0, "A", 5); // A revient (le verrou de B est ecoule) -> retour amorti ?
    return dir.update(7.6, {{"A", hi}, {"B", kDbFloor}});
}
} // namespace

TEST_CASE("director : anti ping-pong = retour au plan large sur navette (echange rapide)") {
    // Fenetre active + plan large -> on s'est recule sur le plan large.
    const Decision withWide = runBounce(12.0, "Plateau");
    CHECK(withWide.scene == "Plateau");
    CHECK(withWide.owner.empty());
    // Fenetre off -> on suit la parole : retour sur A.
    CHECK(runBounce(0.0, "Plateau").owner == "A");
    // SANS plan large -> nulle part ou se reculer : on suit la parole (retour A), le
    // temps-mini gere (degradation gracieuse, conforme au principe de perimetre).
    CHECK(runBounce(12.0, "").owner == "A");
}

TEST_CASE("director : anti ping-pong relache APRES la fenetre -> on repart sur le locuteur") {
    // Prouve que c'est la FENETRE qui borne le retour au plan large, pas le temps-max.
    Config c = twoSpeakerConfig();
    c.timing.minShotSeconds = 3.0;
    c.timing.maxShotSeconds = 30.0; // grand expres : ne doit PAS gouverner le relachement
    c.timing.pingPongWindowSeconds = 5.0;
    c.audio.releaseFrames = 2;
    Director dir(c, seq({0.0}));
    const double hi = mulToDb(0.9);
    auto feedA = [&](double from, double to) {
        for (double t = from; t <= to + 1e-9; t += 0.1) {
            dir.update(t, {{"A", hi}, {"B", kDbFloor}});
        }
    };
    auto soloB = [&](double from, double to) {
        for (double t = from; t <= to + 1e-9; t += 0.1) {
            dir.update(t, {{"A", kDbFloor}, {"B", hi}});
        }
    };
    feedA(0.0, 0.5); // -> A
    soloB(3.5, 4.0); // -> B (on quitte A ~3.6)
    feedA(7.0, 7.5); // A revient DANS la fenetre (3.6 + 5 = 8.6) -> recule sur le plan large
    CHECK(dir.currentScene() == "Plateau");
    // A continue ; la fenetre est depassee (8.6) ET le verrou du plan large ecoule -> on repart sur A.
    feedA(7.6, 11.0);
    CHECK(dir.update(11.1, {{"A", hi}, {"B", kDbFloor}}).owner == "A");
}

// --- Versions semantiques (systeme de mise a jour) ------------------------------

TEST_CASE("version : parseSemVer accepte X.Y.Z (prefixe 'v' et espaces toleres)") {
    auto a = parseSemVer("1.2.3");
    REQUIRE(a.has_value());
    CHECK(a->major == 1);
    CHECK(a->minor == 2);
    CHECK(a->patch == 3);
    CHECK(parseSemVer("v0.1.0").has_value());
    CHECK(parseSemVer("V2.0.0").has_value());
    CHECK(parseSemVer("  0.10.4 ").value() == SemVer{0, 10, 4});
    CHECK(parseSemVer("10.20.30").value() == SemVer{10, 20, 30});
}

TEST_CASE("version : parseSemVer rejette les formats invalides") {
    CHECK_FALSE(parseSemVer("1.2").has_value());       // pas assez de segments
    CHECK_FALSE(parseSemVer("1.2.3.4").has_value());   // trop de segments
    CHECK_FALSE(parseSemVer("1.2.3-rc1").has_value()); // pre-release : volontairement nul
    CHECK_FALSE(parseSemVer("1.2.x").has_value());     // segment non numerique
    CHECK_FALSE(parseSemVer("1..2").has_value());      // segment vide
    CHECK_FALSE(parseSemVer("1.2.").has_value());      // dernier segment vide
    CHECK_FALSE(parseSemVer("").has_value());
    CHECK_FALSE(parseSemVer("   ").has_value());
    CHECK_FALSE(parseSemVer("abc").has_value());
}

TEST_CASE("version : SemVer ordre total (major > minor > patch)") {
    CHECK(SemVer{0, 1, 0} < SemVer{0, 2, 0});
    CHECK(SemVer{0, 9, 9} < SemVer{1, 0, 0});
    CHECK(SemVer{1, 2, 3} < SemVer{1, 2, 4});
    CHECK_FALSE(SemVer{1, 0, 0} < SemVer{1, 0, 0});
    CHECK(SemVer{1, 0, 0} == SemVer{1, 0, 0});
}

TEST_CASE("version : isNewerVersion ne notifie que pour une version stable superieure") {
    CHECK(isNewerVersion("0.2.0", "0.1.0"));
    CHECK(isNewerVersion("1.0.0", "0.9.9"));
    CHECK(isNewerVersion("0.1.1", "0.1.0"));
    CHECK_FALSE(isNewerVersion("0.1.0", "0.1.0")); // identique
    CHECK_FALSE(isNewerVersion("0.1.0", "0.2.0")); // plus ancienne
    // Tolerance : si un cote ne parse pas (reseau douteux, pre-release) -> aucune notif.
    CHECK_FALSE(isNewerVersion("garbage", "0.1.0"));
    CHECK_FALSE(isNewerVersion("0.2.0", ""));
    CHECK_FALSE(isNewerVersion("0.2.0-rc1", "0.1.0"));
}

// --- Styles de realisation (presets de rythme) -----------------------------
TEST_CASE("rhythm style : les 3 built-ins, ordre et valeurs") {
    const auto styles = builtinRhythmStyles();
    REQUIRE(styles.size() == 3);
    CHECK(styles[0].name == "Chill");
    CHECK(styles[1].name == "Cool");
    CHECK(styles[2].name == "Speed");

    // "Cool" reprend EXACTEMENT les defauts livres -> un Config par defaut == style Cool.
    const Config def;
    const RhythmStyle& cool = styles[1];
    CHECK(cool.minShotSeconds == doctest::Approx(def.timing.minShotSeconds));
    CHECK(cool.maxShotSeconds == doctest::Approx(def.timing.maxShotSeconds));
    CHECK(cool.silenceReactionSeconds == doctest::Approx(def.timing.silenceReactionSeconds));
    CHECK(cool.pingPongWindowSeconds == doctest::Approx(def.timing.pingPongWindowSeconds));

    // Speed est le seul a armer l'anti ping-pong ; Chill/Cool le laissent a 0.
    CHECK(styles[0].pingPongWindowSeconds == doctest::Approx(0.0));
    CHECK(styles[2].pingPongWindowSeconds > 0.0);
    // Du plus pose au plus vif : temps maxi strictement decroissant.
    CHECK(styles[0].maxShotSeconds > styles[1].maxShotSeconds);
    CHECK(styles[1].maxShotSeconds > styles[2].maxShotSeconds);

    // Invariants de TOUT built-in : maxi >= mini (sinon fromJson remonterait le maxi et le
    // plan affiche divergerait du preset) et tous les delais >= 0. Garde-fou si un futur
    // edit des valeurs casse un de ces invariants.
    for (const auto& s : styles) {
        CHECK(s.maxShotSeconds >= s.minShotSeconds);
        CHECK(s.minShotSeconds >= 0.0);
        CHECK(s.silenceReactionSeconds >= 0.0);
        CHECK(s.pingPongWindowSeconds >= 0.0);
        CHECK(s.whenMultiple.currentSpeaker >= 0);
        CHECK(s.whenMultiple.wideShot >= 0);
        CHECK(s.whenSilence.lastSpeaker >= 0);
        CHECK(s.whenSilence.wideShot >= 0);
    }
    // La politique plan large fait partie du temperament : Speed reste bien plus "serre"
    // quand 2+ parlent (moins de plan large, plus "rester") que Cool.
    CHECK(styles[2].whenMultiple.wideShot < styles[1].whenMultiple.wideShot); // Speed < Cool en large
    CHECK(styles[2].whenMultiple.currentSpeaker >
          styles[1].whenMultiple.currentSpeaker); // Speed reste + sur le locuteur
}

TEST_CASE("rhythm style : appliquer un style a 0 DESARME l'anti ping-pong (pas seulement laisse)") {
    Config c;
    c.timing.pingPongWindowSeconds = 12.0; // anti ping-pong arme au prealable
    const RhythmStyle chill = builtinRhythmStyles()[0];
    REQUIRE(chill.pingPongWindowSeconds == doctest::Approx(0.0));
    applyRhythmStyle(c, chill);
    // La valeur DOIT etre ecrasee a 0 (et pas conservee) : un style "calme" coupe bien
    // l'anti ping-pong herite d'un style precedent.
    CHECK(c.timing.pingPongWindowSeconds == doctest::Approx(0.0));
    CHECK(c.styleName == "Chill");
}

TEST_CASE("rhythm style : applyRhythmStyle copie le tempo ET la politique plan large, marque le style") {
    Config c;
    // Hors perimetre du style -> doivent rester intacts (calibration) :
    c.audio.voiceThresholdDb = -42.0;
    c.audio.attackFrames = 3;
    c.audio.releaseFrames = 9;
    // Politique plan large de depart, differente du style applique :
    c.whenMultiple = {20, 70};
    c.whenSilence = {30, 70};

    const RhythmStyle speed = builtinRhythmStyles()[2];
    applyRhythmStyle(c, speed);

    CHECK(c.styleName == "Speed");
    // Tempo copie.
    CHECK(c.timing.minShotSeconds == doctest::Approx(speed.minShotSeconds));
    CHECK(c.timing.maxShotSeconds == doctest::Approx(speed.maxShotSeconds));
    CHECK(c.timing.silenceReactionSeconds == doctest::Approx(speed.silenceReactionSeconds));
    CHECK(c.timing.pingPongWindowSeconds == doctest::Approx(speed.pingPongWindowSeconds));
    // Politique plan large copiee (fait partie du temperament depuis le recentrage).
    CHECK(c.whenMultiple.currentSpeaker == speed.whenMultiple.currentSpeaker);
    CHECK(c.whenMultiple.wideShot == speed.whenMultiple.wideShot);
    CHECK(c.whenSilence.lastSpeaker == speed.whenSilence.lastSpeaker);
    CHECK(c.whenSilence.wideShot == speed.whenSilence.wideShot);
    // Seuil + attaque/relache INTACTS (calibration, hors style).
    CHECK(c.audio.voiceThresholdDb == doctest::Approx(-42.0));
    CHECK(c.audio.attackFrames == 3);
    CHECK(c.audio.releaseFrames == 9);
}

TEST_CASE("config : styleName survit a un aller-retour JSON et est retrocompatible") {
    Config c;
    c.styleName = "Chill";
    const Config back = fromJson(toJson(c));
    CHECK(back.styleName == "Chill");

    // Cle absente (profil anterieur a la feature) -> vide = "Perso".
    const Config legacy = fromJson(R"({"version":1})");
    CHECK(legacy.styleName.empty());
}

// ---------------------------------------------------------------------------
// Calibration automatique du seuil (ThresholdCalibrator) — #3
// ---------------------------------------------------------------------------

TEST_CASE("Calibration : micro gate (gros ecart) -> seuil bas mais dans le creux") {
    ThresholdCalibrator cal;
    for (int i = 0; i < 70; ++i) {
        cal.addSample(-60.0); // gate ferme : quasi-silence
    }
    for (int i = 0; i < 40; ++i) {
        cal.addSample(-15.0); // voix franche
    }
    REQUIRE(cal.done());
    const double t = cal.thresholdDb();
    CHECK(t > kDbFloor); // jamais au plancher absolu
    CHECK(t < -15.0);    // toujours SOUS la voix
    CHECK(t < -37.5);    // placement proportionnel (fraction < 0.5) -> sous le milieu
}

TEST_CASE("Calibration : micro ouvert bruyant (petit ecart) -> seuil au-dessus du bruit") {
    ThresholdCalibrator cal;
    for (int i = 0; i < 70; ++i) {
        cal.addSample(-42.0); // bruit de piece
    }
    for (int i = 0; i < 40; ++i) {
        cal.addSample(-18.0); // voix
    }
    REQUIRE(cal.done());
    const double t = cal.thresholdDb();
    CHECK(t > -42.0); // au-dessus du bruit de fond
    CHECK(t < -18.0); // sous la voix
}

TEST_CASE("Calibration : ne parle pas (que du silence) -> non cale, rien a ecrire") {
    ThresholdCalibrator cal;
    for (int i = 0; i < 200; ++i) {
        cal.addSample(-55.0);
    }
    CHECK_FALSE(cal.done()); // aucun ecart voix/silence -> on ne touche pas au seuil
}

TEST_CASE("Calibration : ecart trop faible -> non cale") {
    ThresholdCalibrator cal;
    for (int i = 0; i < 70; ++i) {
        cal.addSample(-40.0);
    }
    for (int i = 0; i < 40; ++i) {
        cal.addSample(-34.0); // ecart 6 dB < minGap (12)
    }
    CHECK_FALSE(cal.done());
}

TEST_CASE("Calibration : fige apres avoir cale (gel, pas de derive)") {
    ThresholdCalibrator cal;
    for (int i = 0; i < 70; ++i) {
        cal.addSample(-60.0);
    }
    for (int i = 0; i < 40; ++i) {
        cal.addSample(-15.0);
    }
    REQUIRE(cal.done());
    const double t = cal.thresholdDb();
    for (int i = 0; i < 100; ++i) {
        cal.addSample(0.0); // bruit max apres coup -> doit etre ignore (no-op)
    }
    CHECK(cal.done());
    CHECK(cal.thresholdDb() == doctest::Approx(t));
}

TEST_CASE("Calibration : finalizeNow conclut tot si l'ecart est franc") {
    ThresholdCalibrator cal;
    for (int i = 0; i < 50; ++i) {
        cal.addSample(-60.0);
    }
    for (int i = 0; i < 30; ++i) {
        cal.addSample(-15.0); // 80 echantillons < minFrames (100) -> pas d'auto-conclusion
    }
    CHECK_FALSE(cal.done());
    CHECK(cal.finalizeNow()); // bouton "Terminer" : assez d'ecart -> cale
    CHECK(cal.done());
    const double t = cal.thresholdDb();
    CHECK(t > kDbFloor);
    CHECK(t < -15.0);
}

TEST_CASE("Calibration : finalizeNow sans ecart suffisant -> echoue, reste non cale") {
    ThresholdCalibrator cal;
    for (int i = 0; i < 60; ++i) {
        cal.addSample(-50.0);
    }
    CHECK_FALSE(cal.finalizeNow());
    CHECK_FALSE(cal.done());
}
