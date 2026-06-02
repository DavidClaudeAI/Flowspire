// StreamDirector — tests du magasin de profils (ProfileStore) hors OBS.
// Exerce la LOGIQUE de persistance sur un FakeFileStore en memoire : migration,
// atomicite des bascules, recuperation .bak, reconstruction par scan. Le main
// doctest est fourni par test_core.cpp (meme executable sd_tests).
#include <doctest/doctest.h>

#include <string>

#include "core/config.hpp"
#include "core/profiles.hpp"
#include "obs/profiles_store.hpp"
#include "fake_file_store.hpp"

using namespace sd::core;
using namespace sd::profiles;

namespace {
// Config minimale avec un seuil global reconnaissable (marqueur de comparaison).
Config cfgWithThreshold(double db) {
    Config c;
    c.audio.voiceThresholdDb = db;
    return c;
}
} // namespace

TEST_CASE("profiles_store : premier lancement cree le profil 1 (defauts), pas de config.json") {
    sdtest::FakeFileStore fs;
    ProfileStore store(fs);
    const ActiveConfigResult r = store.loadActiveConfig("Defaut");
    REQUIRE(r.ok);
    CHECK(r.activeId == 1);
    CHECK(r.config.speakers.empty());
    CHECK(r.config.audio.voiceThresholdDb == doctest::Approx(-35.0)); // defaut code en dur
    CHECK(fs.hasRaw("profiles/index.json"));
    CHECK(fs.hasRaw("profiles/1.json"));
    CHECK_FALSE(fs.hasRaw("config.json")); // config.json n'est plus jamais ecrit
}

TEST_CASE("profiles_store : migration recupere l'ancien config.json") {
    sdtest::FakeFileStore fs;
    fs.setRaw("config.json", toJson(cfgWithThreshold(-22.0)));
    ProfileStore store(fs);
    const ActiveConfigResult r = store.loadActiveConfig("Defaut");
    REQUIRE(r.ok);
    CHECK(r.activeId == 1);
    CHECK(r.config.audio.voiceThresholdDb == doctest::Approx(-22.0));
    CHECK(fs.hasRaw("profiles/1.json"));
}

TEST_CASE("profiles_store : setActive = une seule ecriture (index), pas de config.json") {
    sdtest::FakeFileStore fs;
    ProfileStore store(fs);
    store.loadActiveConfig("Defaut"); // profil 1
    const StoreResult c2 = store.createProfile("B", cfgWithThreshold(-10.0), /*makeActive=*/false);
    REQUIRE(c2.ok);
    fs.writeCalls = 0;
    const StoreResult sa = store.setActive(c2.id);
    REQUIRE(sa.ok);
    CHECK(fs.writeCalls == 1); // SEUL index.json est ecrit (aucune copie de contenu)
    CHECK_FALSE(fs.hasRaw("config.json"));
    // Le moteur lit desormais le profil 2 directement.
    const ActiveConfigResult r = store.loadActiveConfig("Defaut");
    CHECK(r.activeId == c2.id);
    CHECK(r.config.audio.voiceThresholdDb == doctest::Approx(-10.0));
}

TEST_CASE("profiles_store : saveActive ecrit le profil actif (jamais config.json)") {
    sdtest::FakeFileStore fs;
    ProfileStore store(fs);
    store.loadActiveConfig("Defaut"); // profil 1 actif
    const StoreResult sv = store.saveActive(cfgWithThreshold(-18.0));
    REQUIRE(sv.ok);
    const ConfigResult pc = store.loadProfile(1);
    REQUIRE(pc.ok);
    CHECK(pc.config.audio.voiceThresholdDb == doctest::Approx(-18.0));
    CHECK_FALSE(fs.hasRaw("config.json"));
}

TEST_CASE("profiles_store : coupure au commit de createProfile laisse l'index intact") {
    sdtest::FakeFileStore fs;
    ProfileStore store(fs);
    store.loadActiveConfig("Defaut");   // profil 1 actif
    fs.failPathContains = "index.json"; // le commit (index) va echouer
    const StoreResult c = store.createProfile("B", cfgWithThreshold(-5.0), /*makeActive=*/true);
    CHECK_FALSE(c.ok);
    CHECK(fs.hasRaw("profiles/2.json")); // contenu deja ecrit : orphelin inoffensif
    fs.failPathContains.clear();
    // L'index n'a pas bouge : toujours 1 profil, actif = 1 (pas de bascule partielle).
    const ListResult l = store.loadList("Defaut");
    REQUIRE(l.ok);
    CHECK(l.index.profiles.size() == 1);
    CHECK(l.index.activeId == 1);
}

TEST_CASE("profiles_store : un profil corrompu est recupere depuis son .bak") {
    sdtest::FakeFileStore fs;
    ProfileStore store(fs);
    store.loadActiveConfig("Defaut");                         // profiles/1.json = defauts (pas encore de .bak)
    store.saveActive(cfgWithThreshold(-30.0));                // profiles/1.json = -30 ; .bak = defauts
    store.saveActive(cfgWithThreshold(-12.0));                // profiles/1.json = -12 ; .bak = -30
    fs.setRaw("profiles/1.json", "{ ceci n'est pas du JSON"); // corruption du fichier courant
    const ConfigResult pc = store.loadProfile(1);
    REQUIRE(pc.ok);
    CHECK(pc.config.audio.voiceThresholdDb == doctest::Approx(-30.0)); // valeur du .bak
    // LECTURE SEULE : la recuperation ne reecrit pas le fichier courant -> le .bak
    // reste INTACT (filet preserve) et le fichier courant reste tel quel. Une 2e
    // lecture repasse donc encore par le .bak et redonne la meme valeur.
    CHECK(fs.getRaw("profiles/1.json") == "{ ceci n'est pas du JSON"); // non reecrit
    const Config bak = fromJson(fs.getRaw("profiles/1.json.bak"));
    CHECK(bak.audio.voiceThresholdDb == doctest::Approx(-30.0)); // .bak non empoisonne
    const ConfigResult again = store.loadProfile(1);
    REQUIRE(again.ok);
    CHECK(again.config.audio.voiceThresholdDb == doctest::Approx(-30.0));
}

TEST_CASE("profiles_store : index perdu -> reconstruction par scan du dossier") {
    sdtest::FakeFileStore fs;
    fs.setRaw("profiles/2.json", toJson(cfgWithThreshold(-20.0)));
    fs.setRaw("profiles/5.json", toJson(cfgWithThreshold(-40.0)));
    ProfileStore store(fs);
    const ListResult l = store.loadList("Recup");
    REQUIRE(l.ok);
    REQUIRE(l.index.profiles.size() == 2);
    CHECK(l.index.profiles[0].id == 2);
    CHECK(l.index.profiles[1].id == 5);
    CHECK(l.index.activeId == 2);            // plus petit id
    CHECK(l.index.nextId == 6);              // plus grand id + 1 (jamais de reutilisation)
    CHECK(fs.hasRaw("profiles/index.json")); // index reconstruit et ecrit
}

TEST_CASE("profiles_store : index corrompu recupere depuis son .bak sans perte de profil") {
    sdtest::FakeFileStore fs;
    ProfileStore store(fs);
    store.loadActiveConfig("Defaut");                                                             // index v1 {p1}
    const StoreResult c = store.createProfile("B", cfgWithThreshold(-7.0), /*makeActive=*/false); // index v2, .bak=v1
    REQUIRE(c.ok);
    const std::string profile2 = fs.getRaw("profiles/2.json");
    fs.setRaw("profiles/index.json", "{ corrompu"); // corruption de l'index courant
    const ListResult l = store.loadList("Defaut");
    REQUIRE(l.ok);
    CHECK(l.index.profiles.size() == 1);             // recupere depuis index.bak (v1)
    CHECK(fs.getRaw("profiles/2.json") == profile2); // contenu du profil 2 intact
}

TEST_CASE("profiles_store : recup index .bak perime ne reattribue pas l'id d'un orphelin") {
    sdtest::FakeFileStore fs;
    ProfileStore store(fs);
    store.loadActiveConfig("Defaut"); // index v1 {1}, nextId=2
    const StoreResult b = store.createProfile("B", cfgWithThreshold(-7.0), /*makeActive=*/false); // v2 {1,2}; .bak=v1
    REQUIRE(b.ok);
    REQUIRE(b.id == 2);
    const std::string profile2 = fs.getRaw("profiles/2.json");
    fs.setRaw("profiles/index.json", "{ corrompu"); // corruption -> recup depuis .bak v1 (nextId=2)
    const ListResult l = store.loadList("Defaut");
    REQUIRE(l.ok);
    // nextId a ete rehausse au-dessus de profiles/2.json present : une nouvelle creation
    // NE reattribue PAS l'id 2 (sinon elle ecraserait l'orphelin encore sur disque).
    const StoreResult c = store.createProfile("C", cfgWithThreshold(-3.0), /*makeActive=*/false);
    REQUIRE(c.ok);
    CHECK(c.id != 2);
    CHECK(c.id >= 3);
    CHECK(fs.getRaw("profiles/2.json") == profile2); // contenu de B preserve
}

TEST_CASE("profiles_store : le scan ignore les noms de fichier non canoniques") {
    sdtest::FakeFileStore fs;
    fs.setRaw("profiles/2.json", toJson(cfgWithThreshold(-20.0)));
    fs.setRaw("profiles/07.json", toJson(cfgWithThreshold(-40.0))); // zero de tete : non canonique
    fs.setRaw("profiles/x.json", toJson(cfgWithThreshold(-50.0)));  // non numerique
    ProfileStore store(fs);
    const ListResult l = store.loadList("Recup");
    REQUIRE(l.ok);
    // Seul "2.json" est canonique -> un seul profil reconstruit, pas d'entree fantome
    // id=7 dont profiles/7.json n'existe pas (qui ferait echouer loadProfile).
    REQUIRE(l.index.profiles.size() == 1);
    CHECK(l.index.profiles[0].id == 2);
}
