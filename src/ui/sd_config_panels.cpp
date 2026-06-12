// Flowspire — panneaux d'edition de configuration partages (implementation).
// Voir sd_config_panels.hpp. La logique est celle, validee, de l'assistant Run 5,
// rendue reutilisable (host fourni, Config par reference) pour servir aussi les
// parametres avances (Run 6) sans duplication.
#include "ui/sd_config_panels.hpp"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "core/rhythm_style.hpp"
#include "obs/style_store.hpp"
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
    return n.empty() ? i18n("Speakers.DefaultName").arg(static_cast<int>(i) + 1) : QString::fromStdString(n);
}
} // namespace

// Nettoie une config avant ecriture : retire les intervenants sans nom OU sans
// source, et les scenes vides ou de poids nul. Partage assistant + parametres
// avances. NOTE (dette tracee) : nettoyage SILENCIEUX ; validation a l'UI a faire.
sd::core::Config sanitizedConfig(const sd::core::Config& in) {
    sd::core::Config out = in;
    std::vector<sd::core::Speaker> kept;
    for (sd::core::Speaker s : out.speakers) {
        if (s.name.empty() || s.audioSource.empty()) {
            continue; // intervenant incomplet -> non enregistre
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
    : cfg_(cfg),
      audioSources_(std::move(audioSources)),
      scenes_(std::move(scenes)) {}

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
        addBlankSpeaker(); // toujours au moins une carte a remplir
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
        tb->setLabel(speakerLabelOf(cfg_, i), active ? th::kAccent : th::kTextSecondary, 13, active ? 700 : 600);
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

// === Panneau Realisation : style + tempo + plan large + sensibilite =========
void ConfigPanels::mountRhythm(QVBoxLayout* host, RhythmLayout layout) {
    clearLayout(host);
    const bool advanced = (layout == RhythmLayout::Advanced);
    const bool assistant = !advanced;

    // Scene de plan large (SETUP : depend des cameras, INDEPENDANTE du style). Construite
    // tot pour la placer differemment selon la disposition, SANS index absolu : en ASSISTANT
    // elle est mise EN AVANT tout en haut (+ conseil incitatif) ; en AVANCE elle garde sa
    // place a plat (ajoutee plus bas, avec la politique plan large). Chaque disposition
    // ajoute ses widgets dans l'ordre voulu -> pas de host->insertWidget(index) fragile.
    auto* sceneCard = makeCard(QStringLiteral("rhyWideScene"));
    {
        auto* scLay = new QVBoxLayout(sceneCard);
        scLay->setContentsMargins(14, 12, 14, 12);
        scLay->setSpacing(8);
        scLay->addWidget(makeGroupHeader(i18n("Wide.SceneLabel")));
        auto* wideCombo = new ComboField();
        wideCombo->setAllowEmpty(true, i18n("Wide.ScenePlaceholder"));
        wideCombo->setPlaceholder(i18n("Wide.ScenePlaceholder"));
        wideCombo->setEmptyHint(i18n("Cameras.NoSceneAvail"));
        wideCombo->setOptions(toOptions(scenes_), QString::fromStdString(cfg_.wideShotScene));
        wideCombo->setOnChange([this](const QString& v) { cfg_.wideShotScene = v.toStdString(); });
        scLay->addWidget(withInfo(wideCombo, i18n("Tip.Wide.Scene")));
    }
    if (assistant) {
        host->addWidget(sceneCard); // plan large mis en avant, tout en haut
        host->addWidget(makeAccentTip(i18n("Realisation.WideTip")));
    }

    // Pointeurs croises vers les 4 curseurs de rythme : servent (a) au couplage min<=max
    // (min et max se bornent mutuellement) et (b) au selecteur de style pour faire GLISSER
    // les curseurs. shared_ptr (holders) car les lambdas (refresh/onPick) sont definies
    // AVANT les sliders : elles capturent ces holders, peuples apres la creation des rows.
    auto minRowPtr = std::make_shared<SliderRow*>(nullptr);
    auto maxRowPtr = std::make_shared<SliderRow*>(nullptr);
    auto ppRowPtr = std::make_shared<SliderRow*>(nullptr);
    auto silRowPtr = std::make_shared<SliderRow*>(nullptr);
    auto repRowPtr = std::make_shared<SliderRow*>(nullptr); // curseur repetition-max
    auto repHintPtr = std::make_shared<QLabel*>(nullptr);   // sous-texte "≈ N s sur une personne"
    // Holders des 4 curseurs de "politique plan large" (eux aussi pilotes par le style, avec
    // badges %). Construits dans les DEUX dispositions (la politique plan large est partout).
    auto mCurRowPtr = std::make_shared<SliderRow*>(nullptr);
    auto mWideRowPtr = std::make_shared<SliderRow*>(nullptr);
    auto sLastRowPtr = std::make_shared<SliderRow*>(nullptr);
    auto sWideRowPtr = std::make_shared<SliderRow*>(nullptr);
    // Recompute des badges % (part de chaque poids dans la somme du groupe). Gardes par les
    // holders -> no-op tant que les curseurs n'existent pas (ou en mode assistant).
    auto recomputeMulti = [mCurRowPtr, mWideRowPtr]() {
        const int sum = (*mCurRowPtr ? (*mCurRowPtr)->value() : 0) + (*mWideRowPtr ? (*mWideRowPtr)->value() : 0);
        if (*mCurRowPtr) {
            (*mCurRowPtr)->setBadge(pctOf((*mCurRowPtr)->value(), sum));
        }
        if (*mWideRowPtr) {
            (*mWideRowPtr)->setBadge(pctOf((*mWideRowPtr)->value(), sum));
        }
    };
    auto recomputeSil = [sLastRowPtr, sWideRowPtr]() {
        const int sum = (*sLastRowPtr ? (*sLastRowPtr)->value() : 0) + (*sWideRowPtr ? (*sWideRowPtr)->value() : 0);
        if (*sLastRowPtr) {
            (*sLastRowPtr)->setBadge(pctOf((*sLastRowPtr)->value(), sum));
        }
        if (*sWideRowPtr) {
            (*sWideRowPtr)->setBadge(pctOf((*sWideRowPtr)->value(), sum));
        }
    };
    // Sous-texte du curseur repetition-max : "≈ (temps maxi x repetition) s sur une personne avant
    // respiration". Recalcule a chaque changement du temps-maxi OU de la repetition (les deux jouent
    // sur le produit). Cache quand la repetition vaut 0 (desactivee) -> pas de duree trompeuse.
    // Garde par holders -> no-op tant que les widgets n'existent pas.
    auto updateRepHint = [maxRowPtr, repRowPtr, repHintPtr]() {
        if (!*repHintPtr) {
            return;
        }
        const int reps = *repRowPtr ? (*repRowPtr)->value() : 0;
        const int maxS = *maxRowPtr ? (*maxRowPtr)->value() : 0;
        (*repHintPtr)->setVisible(reps > 0);
        if (reps > 0) {
            (*repHintPtr)->setText(i18n("Rhythm.MaxRepeatsHint").arg(reps * maxS));
        }
    };

    // === Selecteur de style de realisation (styles livres + Perso) ============
    // Un style = un bundle nomme de parametres de RYTHME (cf. core/rhythm_style). Le
    // choisir fait glisser les 4 curseurs ci-dessous ; toucher un curseur repasse en
    // "Perso". `applying` neutralise ce basculement pendant l'application programmee.
    auto applying = std::make_shared<bool>(false);
    auto chips = std::make_shared<std::vector<std::pair<std::string, ClickButton*>>>(); // nom -> chip ("" = Perso)
    // Bibliotheque GLOBALE des styles PERSO (chargee seulement en reglages avances). Les
    // actions enregistrer/renommer/supprimer la modifient puis re-montent le panneau.
    auto libStyles = std::make_shared<std::vector<sd::core::RhythmStyle>>();
    // Picker "Mes styles" present dans les DEUX dispositions -> on charge toujours la bibliotheque.
    *libStyles = sd::styles::loadLibrary().styles;
    auto userComboPtr = std::make_shared<ComboField*>(nullptr); // menu "Mes styles" (persos)
    auto manageRowPtr = std::make_shared<QWidget*>(nullptr);    // ligne renommer/supprimer

    auto paintChip = [](ClickButton* b, const QString& label, bool selected) {
        const QString base = QStringLiteral("#clickbtn { background:%1; border:1px solid %2; border-radius:8px; }");
        if (selected) {
            const QString box = base.arg(rgba(th::kAccent, 0.15)).arg(rgba(th::kAccent, 0.7));
            b->setBox(box, box); // etat actif stable (pas de variation au survol)
            b->setLabel(label, th::kAccent, 13, 700);
        } else {
            b->setBox(base.arg(th::kSurface3).arg(th::kBorder), base.arg(th::kSurface3).arg(rgba(th::kAccent, 0.5)));
            b->setLabel(label, th::kTextSecondary, 13, 600);
        }
    };
    // Repeint l'etat du selecteur selon le style actif (cfg_.styleName) :
    //  - pastille d'un style livre allumee si elle correspond ;
    //  - "Perso" allumee si AUCUN style (livre OU perso) n'est actif ;
    //  - le menu "Mes styles" pointe sur le style perso actif (sinon "—") ;
    //  - la ligne renommer/supprimer n'apparait que sur un style perso.
    auto refresh = [this, chips, paintChip, libStyles, userComboPtr, manageRowPtr]() {
        const bool userActive = !cfg_.styleName.empty() && sd::core::styleNameExists(*libStyles, cfg_.styleName);
        bool builtinActive = false;
        for (const auto& pr : *chips) {
            if (!pr.first.empty() && pr.first == cfg_.styleName) {
                builtinActive = true;
            }
        }
        for (const auto& pr : *chips) {
            const bool isPerso = pr.first.empty();
            const bool selected = isPerso ? (!builtinActive && !userActive) : (pr.first == cfg_.styleName);
            const QString label = isPerso ? i18n("Rhythm.StyleCustom") : QString::fromStdString(pr.first);
            paintChip(pr.second, label, selected);
        }
        if (*userComboPtr) {
            (*userComboPtr)->setValue(userActive ? QString::fromStdString(cfg_.styleName) : QString());
        }
        if (*manageRowPtr) {
            (*manageRowPtr)->setVisible(userActive);
        }
    };
    // Applique un style : copie ses valeurs (coeur) PUIS fait glisser les curseurs. Le
    // garde `applying` empeche ces setValue de repasser le style en "Perso".
    auto onPick = [this, applying, refresh, minRowPtr, maxRowPtr, ppRowPtr, silRowPtr, repRowPtr, mCurRowPtr,
                   mWideRowPtr, sLastRowPtr, sWideRowPtr, recomputeMulti, recomputeSil,
                   updateRepHint](const sd::core::RhythmStyle& st) {
        *applying = true;
        sd::core::applyRhythmStyle(cfg_, st);
        if (*minRowPtr) {
            (*minRowPtr)->setValue(static_cast<int>(std::lround(st.minShotSeconds)));
        }
        if (*maxRowPtr) {
            (*maxRowPtr)->setValue(static_cast<int>(std::lround(st.maxShotSeconds)));
        }
        if (*ppRowPtr) {
            (*ppRowPtr)->setValue(static_cast<int>(std::lround(st.pingPongWindowSeconds)));
        }
        if (*silRowPtr) {
            (*silRowPtr)->setValue(static_cast<int>(std::lround(st.silenceReactionSeconds * 2.0))); // demi-secondes
        }
        if (*repRowPtr) {
            (*repRowPtr)->setValue(st.maxPlanRepeats);
        }
        // Politique plan large (presente dans les deux dispositions ; holders toujours peuples).
        if (*mCurRowPtr) {
            (*mCurRowPtr)->setValue(st.whenMultiple.currentSpeaker);
        }
        if (*mWideRowPtr) {
            (*mWideRowPtr)->setValue(st.whenMultiple.wideShot);
        }
        if (*sLastRowPtr) {
            (*sLastRowPtr)->setValue(st.whenSilence.lastSpeaker);
        }
        if (*sWideRowPtr) {
            (*sWideRowPtr)->setValue(st.whenSilence.wideShot);
        }
        *applying = false;
        recomputeMulti();
        recomputeSil();
        updateRepHint();
        refresh();
    };

    auto* styleCard = makeCard(QStringLiteral("rhyStyle"));
    auto* slay = new QVBoxLayout(styleCard);
    slay->setContentsMargins(14, 12, 14, 12);
    slay->setSpacing(10);
    slay->addWidget(makeGroupHeader(i18n("Rhythm.StyleSection")));
    auto* chipRow = new QWidget();
    // FlowLayout : les pastilles s'enroulent sur plusieurs lignes si la fenetre est etroite (5
    // styles + "Perso" ne tiennent plus sur une seule ligne) -> plus de debordement horizontal.
    // Size policy heightForWidth : le QVBoxLayout parent reserve la hauteur des lignes enroulees.
    QSizePolicy chipPol = chipRow->sizePolicy();
    chipPol.setHeightForWidth(true);
    chipRow->setSizePolicy(chipPol);
    auto* chipLay = new FlowLayout(chipRow, 0, 8, 8);
    // Styles livres : noms propres affiches tels quels (non traduits, comme "Flowspire").
    for (const auto& st : sd::core::builtinRhythmStyles()) {
        auto* chip = new ClickButton();
        chip->setOnClick([onPick, st]() { onPick(st); });
        chips->push_back({st.name, chip});
        chipLay->addWidget(chip);
    }
    // "Perso" : indicateur d'etat (reglage manuel), pas un bouton d'action -> aucun
    // onClick, curseur fleche pour le distinguer des styles cliquables.
    auto* persoChip = new ClickButton();
    persoChip->setCursor(Qt::ArrowCursor);
    chips->push_back({std::string{}, persoChip});
    chipLay->addWidget(persoChip);
    slay->addWidget(chipRow);

    // === "Mes styles" : selection d'un style perso enregistre (menu deroulant) ===
    // Separation claire (decidee avec David) : pastilles = styles FOURNIS ; menu = TES styles.
    // Le PICKER est present dans les deux dispositions ; la GESTION (enregistrer / renommer /
    // supprimer) reste reservee aux reglages avances -> l'assistant ne fait que CHOISIR.
    {
        slay->addWidget(makeSectionLabel(i18n("Rhythm.StyleMineSection")));
        auto* mineRow = new QWidget();
        auto* mineLay = new QHBoxLayout(mineRow);
        mineLay->setContentsMargins(0, 0, 0, 0);
        mineLay->setSpacing(8);
        auto* userCombo = new ComboField();
        // PAS de setAllowEmpty : on ne propose pas d'entree "—" selectionnable (un no-op qui
        // desynchroniserait l'affichage du menu de l'etat reel). Le placeholder "— selectionner —"
        // s'affiche quand aucun style perso n'est actif ; pour quitter un style on en choisit un
        // autre, on touche un curseur (-> Perso) ou on clique un fourni.
        userCombo->setPlaceholder(i18n("Rhythm.StyleSelectNone"));
        userCombo->setEmptyHint(i18n("Rhythm.StyleNoneSaved"));
        QStringList userNames;
        for (const auto& s : *libStyles) {
            userNames << QString::fromStdString(s.name);
        }
        userCombo->setOptions(userNames, QString());
        userCombo->setOnChange([this, libStyles, onPick](const QString& v) {
            const std::string name = v.toStdString();
            for (const auto& s : *libStyles) {
                if (s.name == name) {
                    onPick(s); // applique le style perso (glisse les curseurs + marque actif)
                    return;
                }
            }
        });
        *userComboPtr = userCombo;
        mineLay->addWidget(userCombo, 1);
        slay->addWidget(mineRow);

        // Gestion des styles perso : reglages AVANCES uniquement (l'assistant ne CHOISIT que).
        if (advanced) {
            // Petit bouton secondaire (fond surface, liseré accent au survol).
            auto smallBtn = [](const QString& text) {
                auto* b = new ClickButton();
                b->setMargins(11, 8);
                const QString base =
                    QStringLiteral("#clickbtn { background:%1; border:1px solid %2; border-radius:8px; }");
                b->setBox(base.arg(th::kSurface3).arg(th::kBorder),
                          base.arg(th::kSurface3).arg(rgba(th::kAccent, 0.5)));
                b->setLabel(text, th::kTextSecondary, 13, 600);
                return b;
            };

            // "Enregistrer sous..." : capture le reglage courant comme nouveau style nomme.
            auto* saveBtn = smallBtn(i18n("Rhythm.StyleSaveAs"));
            saveBtn->setOnClick([this, styleCard, host, libStyles, layout]() {
                bool ok = false;
                const QString name = QInputDialog::getText(styleCard, i18n("Rhythm.StyleSaveAsTitle"),
                                                           i18n("Rhythm.StyleSaveAsPrompt"), QLineEdit::Normal,
                                                           QString(), &ok);
                const std::string trimmed = name.trimmed().toStdString();
                if (!ok || trimmed.empty()) {
                    return;
                }
                const std::string unique = sd::core::makeUniqueStyleName(*libStyles, trimmed);
                libStyles->push_back(sd::core::styleFromConfig(cfg_, unique));
                sd::styles::saveLibrary(*libStyles);
                cfg_.styleName = unique;   // on est desormais "sur" ce style enregistre
                mountRhythm(host, layout); // re-monte : le nouveau style apparait + selectionne
            });
            mineLay->addWidget(saveBtn);

            // Ligne renommer / supprimer (affichee seulement quand un style PERSO est actif).
            auto* manageRow = new QWidget();
            auto* manageLay = new QHBoxLayout(manageRow);
            manageLay->setContentsMargins(0, 0, 0, 0);
            manageLay->setSpacing(8);
            auto* renameBtn = smallBtn(i18n("Rhythm.StyleRename"));
            renameBtn->setOnClick([this, styleCard, host, libStyles, layout]() {
                const std::string cur = cfg_.styleName;
                if (cur.empty() || !sd::core::styleNameExists(*libStyles, cur)) {
                    return;
                }
                bool ok = false;
                const QString name = QInputDialog::getText(styleCard, i18n("Rhythm.StyleRenameTitle"),
                                                           i18n("Rhythm.StyleSaveAsPrompt"), QLineEdit::Normal,
                                                           QString::fromStdString(cur), &ok);
                const std::string trimmed = name.trimmed().toStdString();
                if (!ok || trimmed.empty() || trimmed == cur) {
                    return;
                }
                std::vector<sd::core::RhythmStyle> others; // unicite EXCLUANT l'entree renommee
                for (const auto& s : *libStyles) {
                    if (s.name != cur) {
                        others.push_back(s);
                    }
                }
                const std::string unique = sd::core::makeUniqueStyleName(others, trimmed);
                for (auto& s : *libStyles) {
                    if (s.name == cur) {
                        s.name = unique;
                    }
                }
                sd::styles::saveLibrary(*libStyles);
                cfg_.styleName = unique;
                mountRhythm(host, layout);
            });
            auto* deleteBtn = smallBtn(i18n("Rhythm.StyleDelete"));
            deleteBtn->setOnClick([this, styleCard, host, libStyles, layout]() {
                const std::string cur = cfg_.styleName;
                if (cur.empty() || !sd::core::styleNameExists(*libStyles, cur)) {
                    return;
                }
                const QMessageBox::StandardButton b =
                    QMessageBox::question(styleCard, i18n("Rhythm.StyleDeleteTitle"),
                                          i18n("Rhythm.StyleDeleteConfirm").arg(QString::fromStdString(cur)));
                if (b != QMessageBox::Yes) {
                    return;
                }
                libStyles->erase(std::remove_if(libStyles->begin(), libStyles->end(),
                                                [&](const sd::core::RhythmStyle& s) { return s.name == cur; }),
                                 libStyles->end());
                sd::styles::saveLibrary(*libStyles);
                cfg_.styleName.clear(); // le style actif n'existe plus -> Perso (valeurs gardees)
                mountRhythm(host, layout);
            });
            manageLay->addWidget(renameBtn);
            manageLay->addWidget(deleteBtn);
            manageLay->addStretch();
            *manageRowPtr = manageRow;
            slay->addWidget(manageRow);
        }
    }

    slay->addWidget(makeHint(i18n("Rhythm.StyleHint")));
    host->addWidget(styleCard);
    refresh(); // etat initial du selecteur (chips + menu + ligne renommer/supprimer)

    // En mode ASSISTANT, tout ce qui suit (tempo, poids plan large, sensibilite) va dans un
    // TIROIR REPLIABLE ; seuls la scene de plan large + le style restent visibles. En mode
    // AVANCE, advTarget == host -> tout reste a plat (disposition d'origine inchangee).
    QVBoxLayout* advTarget = host;
    CollapsibleSection* drawer = nullptr;
    if (assistant) {
        drawer = new CollapsibleSection(i18n("Realisation.Advanced"), i18n("Realisation.AdvancedNote"));
        host->addWidget(drawer);
        advTarget = drawer->body();
    }

    // === Curseurs de rythme (pilotes par le style, ou ajustes a la main = "Perso") ===
    auto* timing = makeCard(QStringLiteral("rhyTiming"));
    auto* tlay = new QVBoxLayout(timing);
    tlay->setContentsMargins(14, 12, 14, 12);
    tlay->setSpacing(10);

    auto* minR = new SliderRow(i18n("Rhythm.MinShot"), 0, 30, static_cast<int>(std::lround(cfg_.timing.minShotSeconds)),
                               fmtSeconds, false);
    minR->setOnChange([this, maxRowPtr, applying, refresh](int v) {
        cfg_.timing.minShotSeconds = v;
        if (*maxRowPtr && (*maxRowPtr)->value() < v) {
            (*maxRowPtr)->setValue(v); // remonte le max au niveau du min
        }
        if (!*applying) {
            cfg_.styleName.clear(); // ajustement manuel -> on quitte le style livre
            refresh();
        }
    });
    auto* maxR = new SliderRow(i18n("Rhythm.MaxShot"), 0, 60, static_cast<int>(std::lround(cfg_.timing.maxShotSeconds)),
                               fmtSeconds, false);
    maxR->setOnChange([this, minRowPtr, applying, refresh, updateRepHint](int v) {
        cfg_.timing.maxShotSeconds = v;
        if (*minRowPtr && (*minRowPtr)->value() > v) {
            (*minRowPtr)->setValue(v); // redescend le min au niveau du max
        }
        updateRepHint(); // le produit temps-maxi x repetition change -> sous-texte recalcule
        if (!*applying) {
            cfg_.styleName.clear();
            refresh();
        }
    });
    minR->setInfo(i18n("Tip.Rhythm.MinShot"));
    maxR->setInfo(i18n("Tip.Rhythm.MaxShot"));

    // Formateur special : a 0 (curseur tout a gauche), l'anti ping-pong est coupe
    // -> on affiche "Desactive" au lieu de "0 s" pour que ce soit explicite.
    auto* ppR = new SliderRow(
        i18n("Rhythm.PingPong"), 0, 30, static_cast<int>(std::lround(cfg_.timing.pingPongWindowSeconds)),
        [](int v) { return v == 0 ? i18n("Rhythm.PingPongOff") : fmtSeconds(v); }, false);
    ppR->setOnChange([this, applying, refresh](int v) {
        cfg_.timing.pingPongWindowSeconds = v;
        if (!*applying) {
            cfg_.styleName.clear();
            refresh();
        }
    });
    ppR->setInfo(i18n("Tip.Rhythm.PingPong"));

    // Delai avant reaction au silence : pas de 0,5 s -> curseur en DEMI-SECONDES (0..10 =>
    // 0..5 s). A 0 (tout a gauche), reaction immediate -> "Immediat" (comme l'anti
    // ping-pong pour son "Desactive"). Formateur : "1 s", "1.5 s", "2 s"... ('g' coupe les
    // zeros inutiles ; separateur '.' du C locale, coherent avec fmtSilence).
    auto* silReactR = new SliderRow(
        i18n("Rhythm.SilenceReaction"), 0, 10,
        std::max(0, std::min(10, static_cast<int>(std::lround(cfg_.timing.silenceReactionSeconds * 2.0)))),
        [](int v) {
            return v == 0 ? i18n("Rhythm.SilenceReactionImmediate")
                          : QString::number(v * 0.5, 'g', 3) + QStringLiteral(" s");
        },
        false);
    silReactR->setOnChange([this, applying, refresh](int v) {
        cfg_.timing.silenceReactionSeconds = v * 0.5;
        if (!*applying) {
            cfg_.styleName.clear();
            refresh();
        }
    });
    silReactR->setInfo(i18n("Tip.Rhythm.SilenceReaction"));

    // Repetition-max : nombre de fois max sur un MEME plan avant respiration (variete forcee). ×N
    // (0 = desactive). Entier direct (pas de demi-pas). Le sous-texte donne le temps approx tenu sur
    // une personne (= temps maxi x repetition) -> recalcule aussi quand on bouge le temps-maxi.
    auto* repR = new SliderRow(
        i18n("Rhythm.MaxRepeats"), 0, 10, std::max(0, std::min(10, cfg_.timing.maxPlanRepeats)),
        [](int v) { return v == 0 ? i18n("Rhythm.MaxRepeatsOff") : i18n("Rhythm.MaxRepeatsValue").arg(v); }, false);
    repR->setOnChange([this, applying, refresh, updateRepHint](int v) {
        cfg_.timing.maxPlanRepeats = v;
        updateRepHint();
        if (!*applying) {
            cfg_.styleName.clear();
            refresh();
        }
    });
    repR->setInfo(i18n("Tip.Rhythm.MaxRepeats"));
    auto* repHint = makeHint(QString()); // texte pose par updateRepHint (cache si repetition = 0)

    *minRowPtr = minR;
    *maxRowPtr = maxR;
    *ppRowPtr = ppR;
    *silRowPtr = silReactR;
    *repRowPtr = repR;
    *repHintPtr = repHint;
    updateRepHint(); // etat initial du sous-texte

    tlay->addWidget(minR);
    tlay->addWidget(maxR);
    tlay->addWidget(repR);
    tlay->addWidget(repHint);
    tlay->addWidget(ppR);
    tlay->addWidget(silReactR);
    advTarget->addWidget(timing);

    // === Politique plan large : "quand 2+ parlent" / "quand silence" (poids PILOTES PAR LE
    //     STYLE). La scene de groupe (SETUP) a deja ete construite plus haut. ===============
    {
        // En AVANCE, la scene de plan large prend sa place ICI (apres le tempo, ordre
        // d'origine) ; en ASSISTANT elle a deja ete mise en avant tout en haut.
        if (advanced) {
            advTarget->addWidget(sceneCard);
        }

        // -- Quand 2+ parlent : { rester / plan large } (pilote par le style) --
        auto* g1 = makeCard(QStringLiteral("rhyMul"));
        auto* g1l = new QVBoxLayout(g1);
        g1l->setContentsMargins(14, 12, 14, 12);
        g1l->setSpacing(10);
        g1l->addWidget(makeGroupHeader(i18n("Wide.Multiple")));
        auto* mCurR = new SliderRow(i18n("Wide.Current"), 0, 100, cfg_.whenMultiple.currentSpeaker, nullptr, true);
        mCurR->setOnChange([this, recomputeMulti, applying, refresh](int v) {
            cfg_.whenMultiple.currentSpeaker = v;
            recomputeMulti();
            if (!*applying) {
                cfg_.styleName.clear();
                refresh();
            }
        });
        auto* mWideR = new SliderRow(i18n("Wide.WideShot"), 0, 100, cfg_.whenMultiple.wideShot, nullptr, true);
        mWideR->setOnChange([this, recomputeMulti, applying, refresh](int v) {
            cfg_.whenMultiple.wideShot = v;
            recomputeMulti();
            if (!*applying) {
                cfg_.styleName.clear();
                refresh();
            }
        });
        mCurR->setInfo(i18n("Tip.Wide.Current"));
        mWideR->setInfo(i18n("Tip.Wide.WideShot"));
        *mCurRowPtr = mCurR;
        *mWideRowPtr = mWideR;
        g1l->addWidget(mCurR);
        g1l->addWidget(mWideR);
        advTarget->addWidget(g1);

        // -- Quand personne ne parle : { dernier locuteur / plan large } (pilote par le style) --
        auto* g2 = makeCard(QStringLiteral("rhySil"));
        auto* g2l = new QVBoxLayout(g2);
        g2l->setContentsMargins(14, 12, 14, 12);
        g2l->setSpacing(10);
        g2l->addWidget(makeGroupHeader(i18n("Wide.Silence")));
        auto* sLastR = new SliderRow(i18n("Wide.LastSpeaker"), 0, 100, cfg_.whenSilence.lastSpeaker, nullptr, true);
        sLastR->setOnChange([this, recomputeSil, applying, refresh](int v) {
            cfg_.whenSilence.lastSpeaker = v;
            recomputeSil();
            if (!*applying) {
                cfg_.styleName.clear();
                refresh();
            }
        });
        auto* sWideR = new SliderRow(i18n("Wide.WideShot"), 0, 100, cfg_.whenSilence.wideShot, nullptr, true);
        sWideR->setOnChange([this, recomputeSil, applying, refresh](int v) {
            cfg_.whenSilence.wideShot = v;
            recomputeSil();
            if (!*applying) {
                cfg_.styleName.clear();
                refresh();
            }
        });
        sLastR->setInfo(i18n("Tip.Wide.LastSpeaker"));
        sWideR->setInfo(i18n("Tip.Wide.WideShot"));
        *sLastRowPtr = sLastR;
        *sWideRowPtr = sWideR;
        g2l->addWidget(sLastR);
        g2l->addWidget(sWideR);
        advTarget->addWidget(g2);

        recomputeMulti(); // badges initiaux
        recomputeSil();
    }

    advTarget->addWidget(makeSectionLabel(i18n("Rhythm.AudioSection")));
    auto* audio = makeCard(QStringLiteral("rhyAudio"));
    auto* alay = new QVBoxLayout(audio);
    alay->setContentsMargins(14, 12, 14, 12);
    alay->setSpacing(10);
    int thr = static_cast<int>(std::lround(cfg_.audio.voiceThresholdDb));
    thr = std::max(-60, std::min(0, thr));
    auto* thrR = new SliderRow(i18n("Rhythm.Threshold"), -60, 0, thr, fmtDb, false);
    thrR->setOnChange([this](int v) { cfg_.audio.voiceThresholdDb = v; });
    thrR->setInfo(i18n("Tip.Rhythm.Threshold"));
    // Delai d'attaque (attackFrames ; pendant du delai de silence cote DEBUT de parole,
    // mais reglage distinct : defaut 2 vs 8) : duree de voix continue avant de confirmer
    // qu'une personne PARLE (anti faux declenchement sur un bruit bref).
    // Clamp d'AFFICHAGE seulement (l'invariant attackFrames>=1 est garanti par fromJson).
    int att = std::max(1, std::min(20, cfg_.audio.attackFrames));
    auto* attR = new SliderRow(i18n("Rhythm.Attack"), 1, 20, att, fmtSilence, false);
    attR->setOnChange([this](int v) { cfg_.audio.attackFrames = v; });
    attR->setInfo(i18n("Tip.Rhythm.Attack"));
    // Clamp d'AFFICHAGE seulement (comme le seuil) : on ne mute pas cfg_ en ouvrant le
    // panneau. L'invariant releaseFrames>=1 est garanti au chargement par
    // sd::core::fromJson (normalisation), pas par un effet de bord d'affichage.
    int rel = std::max(1, std::min(40, cfg_.audio.releaseFrames));
    auto* silR = new SliderRow(i18n("Rhythm.SilenceDelay"), 1, 40, rel, fmtSilence, false);
    silR->setOnChange([this](int v) { cfg_.audio.releaseFrames = v; });
    silR->setInfo(i18n("Tip.Rhythm.SilenceDelay"));
    alay->addWidget(thrR);
    alay->addWidget(attR);
    alay->addWidget(silR);
    advTarget->addWidget(audio);
}

} // namespace sd::ui
