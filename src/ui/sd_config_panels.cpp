// StreamDirector — panneaux d'edition de configuration partages (implementation).
// Voir sd_config_panels.hpp. La logique est celle, validee, de l'assistant Run 5,
// rendue reutilisable (host fourni, Config par reference) pour servir aussi les
// parametres avances (Run 6) sans duplication.
#include "ui/sd_config_panels.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <memory>

#include "ui/sd_i18n.hpp"
#include "ui/sd_icons.hpp"
#include "ui/sd_style.hpp"
#include "ui/sd_theme.hpp"
#include "ui/sd_widgets.hpp"

namespace sd::ui {

namespace th = sd::ui::theme;
using namespace sd::ui::widgets;

namespace {
// Options deroulantes a partir d'une liste de noms.
QStringList toOptions(const std::vector<std::string>& names) {
    QStringList o;
    for (const auto& s : names) {
        o << QString::fromStdString(s);
    }
    return o;
}

// Libelle d'un intervenant (nom, ou "Intervenant N" si vide) — pour les onglets.
QString speakerLabelOf(const sd::core::Config& cfg, std::size_t i) {
    const std::string& n = cfg.speakers[i].name;
    return n.empty() ? i18n("Speakers.DefaultName").arg(static_cast<int>(i) + 1)
                     : QString::fromStdString(n);
}
}  // namespace

// Nettoie une config avant ecriture : retire les intervenants sans nom OU sans
// source, et les scenes vides ou de poids nul. Partage assistant + parametres
// avances. NOTE (dette tracee) : nettoyage SILENCIEUX ; validation a l'UI a faire.
sd::core::Config sanitizedConfig(const sd::core::Config& in) {
    sd::core::Config out = in;
    std::vector<sd::core::Speaker> kept;
    for (sd::core::Speaker s : out.speakers) {
        if (s.name.empty() || s.audioSource.empty()) {
            continue;  // intervenant incomplet -> non enregistre
        }
        std::vector<sd::core::SceneWeight> scenes;
        for (const auto& w : s.scenes) {
            if (!w.scene.empty() && w.weight > 0) {
                scenes.push_back(w);
            }
        }
        s.scenes = std::move(scenes);
        kept.push_back(std::move(s));
    }
    out.speakers = std::move(kept);
    return out;
}

ConfigPanels::ConfigPanels(sd::core::Config& cfg, std::vector<std::string> audioSources,
                           std::vector<std::string> scenes)
    : cfg_(cfg), audioSources_(std::move(audioSources)), scenes_(std::move(scenes)) {}

// --- helpers config ---------------------------------------------------------
sd::core::Speaker* ConfigPanels::findSpeaker(const std::string& id) {
    for (auto& s : cfg_.speakers) {
        if (s.id == id) {
            return &s;
        }
    }
    return nullptr;
}

std::string ConfigPanels::uniqueId() {
    std::string id;
    do {
        id = "spk-" + std::to_string(++idCounter_);
    } while (findSpeaker(id) != nullptr);
    return id;
}

void ConfigPanels::addBlankSpeaker() {
    cfg_.speakers.push_back({uniqueId(), "", "", {}});
}

void ConfigPanels::eraseSpeaker(const std::string& id) {
    cfg_.speakers.erase(std::remove_if(cfg_.speakers.begin(), cfg_.speakers.end(),
                                       [&](const sd::core::Speaker& s) { return s.id == id; }),
                        cfg_.speakers.end());
}

// === Panneau Intervenants ===================================================
void ConfigPanels::mountSpeakers(QVBoxLayout* host) {
    speakersHost_ = host;
    rebuildSpeakers();
}

void ConfigPanels::rebuildSpeakers() {
    if (!speakersHost_) {
        return;
    }
    clearLayout(speakersHost_);
    if (cfg_.speakers.empty()) {
        addBlankSpeaker();  // toujours au moins une carte a remplir
    }
    auto* list = new QWidget();
    auto* ll = new QVBoxLayout(list);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(10);
    for (std::size_t i = 0; i < cfg_.speakers.size(); ++i) {
        ll->addWidget(buildSpeakerCard(i));
    }
    speakersHost_->addWidget(list);

    auto* add = makeAddButton(i18n("Speakers.Add"), [this]() {
        addBlankSpeaker();
        rebuildSpeakers();
    });
    speakersHost_->addWidget(add);
    // Pas de addStretch ici : c'est l'appelant (assistant/settings) qui pousse le
    // contenu vers le haut au niveau de SA page -> evite un double stretch en conflit.
}

QWidget* ConfigPanels::buildSpeakerCard(std::size_t idx) {
    const std::string sid = cfg_.speakers[idx].id;
    auto* card = makeCard(QStringLiteral("spkEdit"));
    auto* lay = new QHBoxLayout(card);
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(10);

    auto* num = new QLabel(QString::number(static_cast<int>(idx) + 1));
    num->setFixedSize(30, 30);
    num->setAlignment(Qt::AlignCenter);
    num->setStyleSheet(QString("background:%1; border-radius:15px; color:%2;"
                               " font-size:13px; font-weight:600;")
                           .arg(th::kSurface3)
                           .arg(th::kTextSecondary));

    auto* fields = new QWidget();
    auto* fl = new QVBoxLayout(fields);
    fl->setContentsMargins(0, 0, 0, 0);
    fl->setSpacing(6);
    auto* name = new QLineEdit(QString::fromStdString(cfg_.speakers[idx].name));
    name->setPlaceholderText(i18n("Speakers.NamePlaceholder"));
    name->setStyleSheet(lineEditQss());
    QObject::connect(name, &QLineEdit::textChanged, name, [this, sid](const QString& t) {
        if (auto* s = findSpeaker(sid)) {
            s->name = t.toStdString();
        }
    });
    auto* src = new ComboField();
    src->setPlaceholder(i18n("Speakers.SourcePlaceholder"));
    src->setEmptyHint(i18n("Speakers.NoSource"));
    src->setOptions(toOptions(audioSources_), QString::fromStdString(cfg_.speakers[idx].audioSource));
    src->setOnChange([this, sid](const QString& v) {
        if (auto* s = findSpeaker(sid)) {
            s->audioSource = v.toStdString();
        }
    });
    fl->addWidget(withInfo(name, i18n("Tip.Speakers.Name")));
    fl->addWidget(withInfo(src, i18n("Tip.Speakers.Source")));

    auto* del = new QPushButton();
    del->setIcon(icon(Icon::Trash2, th::kTextTertiary, 15));
    del->setCursor(Qt::PointingHandCursor);
    del->setFlat(true);
    del->setStyleSheet(iconBtnQss());
    QObject::connect(del, &QPushButton::clicked, del, [this, sid]() {
        eraseSpeaker(sid);
        rebuildSpeakers();
    });

    lay->addWidget(num, 0, Qt::AlignTop);
    lay->addWidget(fields, 1);
    lay->addWidget(del, 0, Qt::AlignTop);
    return card;
}

// === Panneau Scenes (onglets par intervenant) ===============================
void ConfigPanels::mountCameras(QVBoxLayout* host) {
    camerasHost_ = host;
    rebuildCameras();
}

void ConfigPanels::rebuildCameras() {
    if (!camerasHost_) {
        return;
    }
    clearLayout(camerasHost_);
    sceneWeightSliders_.clear();
    sceneWeightBadges_.clear();

    if (cfg_.speakers.empty()) {
        camerasHost_->addWidget(makeHint(i18n("Cameras.Empty")));
        return;
    }
    if (selSpeaker_ >= static_cast<int>(cfg_.speakers.size())) {
        selSpeaker_ = 0;
    }

    // Onglets "dossier" + carte rattachee, dans un conteneur SANS gap.
    auto* tabWrap = new QWidget();
    auto* twl = new QVBoxLayout(tabWrap);
    twl->setContentsMargins(0, 0, 0, 0);
    twl->setSpacing(0);

    auto* tabs = new QWidget();
    auto* tl = new QHBoxLayout(tabs);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(4);
    for (std::size_t i = 0; i < cfg_.speakers.size(); ++i) {
        const bool active = static_cast<int>(i) == selSpeaker_;
        auto* tb = new ClickButton();
        tb->setMargins(16, 9);
        tb->setIconPix(icon(Icon::User, active ? th::kAccent : th::kTextTertiary, 14));
        tb->setLabel(speakerLabelOf(cfg_, i), active ? th::kAccent : th::kTextSecondary, 13,
                     active ? 700 : 600);
        tb->setBox(tabBoxQss(active));
        const int idx = static_cast<int>(i);
        tb->setOnClick([this, idx]() {
            selSpeaker_ = idx;
            rebuildCameras();
        });
        tl->addWidget(tb);
    }
    tl->addStretch();
    twl->addWidget(tabs);

    // Carte rattachee : coin haut-GAUCHE carre (sous le 1er onglet) facon dossier.
    auto* group = new QWidget();
    group->setObjectName(QStringLiteral("camGroup"));
    group->setStyleSheet(QString("#camGroup { background:%1; border:1px solid %2;"
                                 " border-top-left-radius:0px; border-top-right-radius:%3px;"
                                 " border-bottom-left-radius:%3px; border-bottom-right-radius:%3px; }")
                             .arg(th::kSurface2)
                             .arg(rgba(th::kBorder, 1.0))
                             .arg(th::kRadiusCard));
    auto* gl = new QVBoxLayout(group);
    gl->setContentsMargins(14, 14, 14, 14);
    gl->setSpacing(12);
    sd::core::Speaker& sp = cfg_.speakers[static_cast<std::size_t>(selSpeaker_)];
    if (sp.scenes.empty()) {
        gl->addWidget(makeHint(i18n("Cameras.NoScene")));
    }
    for (std::size_t s = 0; s < sp.scenes.size(); ++s) {
        gl->addWidget(buildSceneRow(selSpeaker_, s));
    }
    twl->addWidget(group);
    camerasHost_->addWidget(tabWrap);

    auto* add = makeAddButton(i18n("Cameras.Add"), [this]() {
        cfg_.speakers[static_cast<std::size_t>(selSpeaker_)].scenes.push_back({"", 50});
        rebuildCameras();
    });
    camerasHost_->addWidget(add);
    recomputeSceneWeights();
}

QWidget* ConfigPanels::buildSceneRow(int spk, std::size_t s) {
    auto* row = new QWidget();
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    auto* combo = new ComboField();
    combo->setPlaceholder(i18n("Cameras.ScenePlaceholder"));
    combo->setEmptyHint(i18n("Cameras.NoSceneAvail"));
    combo->setOptions(toOptions(scenes_),
                      QString::fromStdString(cfg_.speakers[static_cast<std::size_t>(spk)].scenes[s].scene));
    combo->setMinimumWidth(140);
    combo->setOnChange([this, spk, s](const QString& v) {
        cfg_.speakers[static_cast<std::size_t>(spk)].scenes[s].scene = v.toStdString();
    });

    auto* slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 100);
    slider->setValue(cfg_.speakers[static_cast<std::size_t>(spk)].scenes[s].weight);
    slider->setStyleSheet(accentSliderQss());
    slider->setCursor(Qt::PointingHandCursor);

    auto* val = new QLabel(QString::number(slider->value()));
    val->setMinimumWidth(28);
    val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    val->setStyleSheet(QString("color:%1; font-size:13px; font-weight:600;").arg(th::kTextPrimary));

    auto* badge = new QLabel();
    badge->setFixedWidth(44);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(badgeQss());

    auto* del = new QPushButton();
    del->setIcon(icon(Icon::Trash2, th::kTextTertiary, 15));
    del->setCursor(Qt::PointingHandCursor);
    del->setFlat(true);
    del->setStyleSheet(iconBtnQss());

    QObject::connect(slider, &QSlider::valueChanged, slider, [this, spk, s, val](int v) {
        cfg_.speakers[static_cast<std::size_t>(spk)].scenes[s].weight = v;
        val->setText(QString::number(v));
        recomputeSceneWeights();
    });
    QObject::connect(del, &QPushButton::clicked, del, [this, spk, s]() {
        auto& sc = cfg_.speakers[static_cast<std::size_t>(spk)].scenes;
        if (s < sc.size()) {
            sc.erase(sc.begin() + static_cast<long>(s));
        }
        rebuildCameras();
    });

    lay->addWidget(combo, 1);
    lay->addWidget(slider, 1);
    lay->addWidget(val);
    lay->addWidget(badge);
    lay->addWidget(makeInfoIcon(i18n("Tip.Cameras.Weight")));
    lay->addWidget(del);
    sceneWeightSliders_.push_back(slider);
    sceneWeightBadges_.push_back(badge);
    return row;
}

void ConfigPanels::recomputeSceneWeights() {
    int sum = 0;
    for (auto* s : sceneWeightSliders_) {
        sum += s->value();
    }
    for (std::size_t i = 0; i < sceneWeightSliders_.size(); ++i) {
        sceneWeightBadges_[i]->setText(QString::number(pctOf(sceneWeightSliders_[i]->value(), sum)) +
                                       QStringLiteral("%"));
    }
}

// === Panneau Plan large & priorites =========================================
void ConfigPanels::mountWide(QVBoxLayout* host) {
    clearLayout(host);

    // Scene plan large MISE EN AVANT (retour David : on ne voyait pas qu'il fallait
    // la choisir). Carte a lisere accent : icone + titre + champ + ligne d'aide.
    auto* sceneCard = new QWidget();
    sceneCard->setObjectName(QStringLiteral("wideSceneCard"));
    sceneCard->setStyleSheet(QString("#wideSceneCard { background:%1; border:1px solid %2;"
                                     " border-radius:%3px; }")
                                 .arg(rgba(th::kAccent, 0.08))
                                 .arg(rgba(th::kAccent, 0.5))
                                 .arg(th::kRadiusCard));
    auto* scLay = new QVBoxLayout(sceneCard);
    scLay->setContentsMargins(14, 12, 14, 12);
    scLay->setSpacing(8);
    auto* scHead = new QWidget();
    auto* scHeadLay = new QHBoxLayout(scHead);
    scHeadLay->setContentsMargins(0, 0, 0, 0);
    scHeadLay->setSpacing(8);
    auto* scIcon = new QLabel();
    scIcon->setPixmap(icon(Icon::LayoutGrid, th::kAccent, 16));
    auto* scTitle = new QLabel(i18n("Wide.SceneLabel"));
    scTitle->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(th::kAccent));
    scHeadLay->addWidget(scIcon);
    scHeadLay->addWidget(scTitle);
    scHeadLay->addStretch();
    scLay->addWidget(scHead);

    auto* wide = new ComboField();
    wide->setAllowEmpty(true, i18n("Wide.ScenePlaceholder"));
    wide->setPlaceholder(i18n("Wide.ScenePlaceholder"));
    wide->setEmptyHint(i18n("Cameras.NoSceneAvail"));
    wide->setOptions(toOptions(scenes_), QString::fromStdString(cfg_.wideShotScene));
    wide->setOnChange([this](const QString& v) { cfg_.wideShotScene = v.toStdString(); });
    // Aide via le ⓘ du champ (homogene avec les autres reglages) ; pas de ligne
    // d'aide visible separee, qui doublonnait le tooltip (retour David).
    scLay->addWidget(withInfo(wide, i18n("Tip.Wide.Scene")));
    host->addWidget(sceneCard);

    // Groupes de poids dans des vecteurs LOCAUX (recompute des % seulement, pas de
    // rebuild) -> pas besoin de membres. shared_ptr pour survivre a la portee.
    auto multRows = std::make_shared<std::vector<SliderRow*>>();
    auto silRows = std::make_shared<std::vector<SliderRow*>>();
    auto recompute = [](const std::shared_ptr<std::vector<SliderRow*>>& rows) {
        int sum = 0;
        for (auto* r : *rows) {
            sum += r->value();
        }
        for (auto* r : *rows) {
            r->setBadge(pctOf(r->value(), sum));
        }
    };

    auto* g1 = makeCard(QStringLiteral("wideMul"));
    auto* g1l = new QVBoxLayout(g1);
    g1l->setContentsMargins(14, 12, 14, 12);
    g1l->setSpacing(10);
    g1l->addWidget(makeGroupHeader(i18n("Wide.Multiple")));
    auto* r1 = new SliderRow(i18n("Wide.Loudest"), 0, 100, cfg_.whenMultiple.loudestSpeaker, nullptr,
                             true);
    r1->setOnChange([this, multRows, recompute](int v) {
        cfg_.whenMultiple.loudestSpeaker = v;
        recompute(multRows);
    });
    auto* r2 = new SliderRow(i18n("Wide.Current"), 0, 100, cfg_.whenMultiple.currentSpeaker, nullptr,
                             true);
    r2->setOnChange([this, multRows, recompute](int v) {
        cfg_.whenMultiple.currentSpeaker = v;
        recompute(multRows);
    });
    auto* r3 = new SliderRow(i18n("Wide.WideShot"), 0, 100, cfg_.whenMultiple.wideShot, nullptr, true);
    r3->setOnChange([this, multRows, recompute](int v) {
        cfg_.whenMultiple.wideShot = v;
        recompute(multRows);
    });
    r1->setInfo(i18n("Tip.Wide.Loudest"));
    r2->setInfo(i18n("Tip.Wide.Current"));
    r3->setInfo(i18n("Tip.Wide.WideShot"));
    *multRows = {r1, r2, r3};
    g1l->addWidget(r1);
    g1l->addWidget(r2);
    g1l->addWidget(r3);
    host->addWidget(g1);

    auto* g2 = makeCard(QStringLiteral("wideSil"));
    auto* g2l = new QVBoxLayout(g2);
    g2l->setContentsMargins(14, 12, 14, 12);
    g2l->setSpacing(10);
    g2l->addWidget(makeGroupHeader(i18n("Wide.Silence")));
    auto* s1 = new SliderRow(i18n("Wide.LastSpeaker"), 0, 100, cfg_.whenSilence.lastSpeaker, nullptr,
                             true);
    s1->setOnChange([this, silRows, recompute](int v) {
        cfg_.whenSilence.lastSpeaker = v;
        recompute(silRows);
    });
    auto* s2 = new SliderRow(i18n("Wide.WideShot"), 0, 100, cfg_.whenSilence.wideShot, nullptr, true);
    s2->setOnChange([this, silRows, recompute](int v) {
        cfg_.whenSilence.wideShot = v;
        recompute(silRows);
    });
    s1->setInfo(i18n("Tip.Wide.LastSpeaker"));
    s2->setInfo(i18n("Tip.Wide.WideShot"));
    *silRows = {s1, s2};
    g2l->addWidget(s1);
    g2l->addWidget(s2);
    host->addWidget(g2);

    recompute(multRows);
    recompute(silRows);
}

// === Panneau Rythme & sensibilite ===========================================
void ConfigPanels::mountRhythm(QVBoxLayout* host) {
    clearLayout(host);

    auto* timing = makeCard(QStringLiteral("rhyTiming"));
    auto* tlay = new QVBoxLayout(timing);
    tlay->setContentsMargins(14, 12, 14, 12);
    tlay->setSpacing(10);

    // Couplage min<=max : pointeur croise pour borner l'autre slider quand l'un
    // bouge (evite une config min>max insauvable). shared_ptr car les deux lambdas
    // se referencent mutuellement.
    auto minRowPtr = std::make_shared<SliderRow*>(nullptr);
    auto maxRowPtr = std::make_shared<SliderRow*>(nullptr);

    auto* minR = new SliderRow(i18n("Rhythm.MinShot"), 0, 30,
                               static_cast<int>(std::lround(cfg_.timing.minShotSeconds)), fmtSeconds,
                               false);
    minR->setOnChange([this, minRowPtr, maxRowPtr](int v) {
        cfg_.timing.minShotSeconds = v;
        if (*maxRowPtr && (*maxRowPtr)->value() < v) {
            (*maxRowPtr)->setValue(v);  // remonte le max au niveau du min
        }
    });
    auto* maxR = new SliderRow(i18n("Rhythm.MaxShot"), 0, 60,
                               static_cast<int>(std::lround(cfg_.timing.maxShotSeconds)), fmtSeconds,
                               false);
    maxR->setOnChange([this, minRowPtr, maxRowPtr](int v) {
        cfg_.timing.maxShotSeconds = v;
        if (*minRowPtr && (*minRowPtr)->value() > v) {
            (*minRowPtr)->setValue(v);  // redescend le min au niveau du max
        }
    });
    minR->setInfo(i18n("Tip.Rhythm.MinShot"));
    maxR->setInfo(i18n("Tip.Rhythm.MaxShot"));
    *minRowPtr = minR;
    *maxRowPtr = maxR;

    // Formateur special : a 0 (curseur tout a gauche), l'anti ping-pong est coupe
    // -> on affiche "Desactive" au lieu de "0 s" pour que ce soit explicite.
    auto* ppR = new SliderRow(
        i18n("Rhythm.PingPong"), 0, 30,
        static_cast<int>(std::lround(cfg_.timing.pingPongWindowSeconds)),
        [](int v) { return v == 0 ? i18n("Rhythm.PingPongOff") : fmtSeconds(v); }, false);
    ppR->setOnChange([this](int v) { cfg_.timing.pingPongWindowSeconds = v; });
    ppR->setInfo(i18n("Tip.Rhythm.PingPong"));
    tlay->addWidget(minR);
    tlay->addWidget(maxR);
    tlay->addWidget(ppR);
    host->addWidget(timing);

    host->addWidget(makeSectionLabel(i18n("Rhythm.AudioSection")));
    auto* audio = makeCard(QStringLiteral("rhyAudio"));
    auto* alay = new QVBoxLayout(audio);
    alay->setContentsMargins(14, 12, 14, 12);
    alay->setSpacing(10);
    int thr = static_cast<int>(std::lround(cfg_.audio.voiceThresholdDb));
    thr = std::max(-60, std::min(0, thr));
    auto* thrR = new SliderRow(i18n("Rhythm.Threshold"), -60, 0, thr, fmtDb, false);
    thrR->setOnChange([this](int v) { cfg_.audio.voiceThresholdDb = v; });
    thrR->setInfo(i18n("Tip.Rhythm.Threshold"));
    // Clamp d'AFFICHAGE seulement (comme le seuil) : on ne mute pas cfg_ en ouvrant le
    // panneau. L'invariant releaseFrames>=1 est garanti au chargement par
    // sd::core::fromJson (normalisation), pas par un effet de bord d'affichage.
    int rel = std::max(1, std::min(40, cfg_.audio.releaseFrames));
    auto* silR = new SliderRow(i18n("Rhythm.SilenceDelay"), 1, 40, rel, fmtSilence, false);
    silR->setOnChange([this](int v) { cfg_.audio.releaseFrames = v; });
    silR->setInfo(i18n("Tip.Rhythm.SilenceDelay"));
    alay->addWidget(thrR);
    alay->addWidget(silR);
    host->addWidget(audio);
}

}  // namespace sd::ui
