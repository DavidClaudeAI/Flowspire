// Flowspire — fenetre Parametres avances (implementation, Run 6 + Run 7).
// Voir sd_settings.hpp. Fenetre modale frameless a sidebar gauche (maquette `QbX5t`).
//
// Run 7 : "plus de pop-up sauf l'assistant" (decision David). Profils et Soutenir
// sont desormais de vrais PANNEAUX (sd_profiles_panel / sd_support). L'edition
// (Intervenants/Cameras/Plan large/Rythme) ecrit dans le PROFIL ACTIF via
// sd::profiles::saveActive (le fichier <actif>.json = source de verite lue par le
// moteur). Modele "tout-sur-Enregistrer" pour l'edition ; les actions de profils
// (charger/nouveau/...) sont, elles, IMMEDIATES, avec garde anti-perte si des
// reglages d'edition n'ont pas ete enregistres.
#include "ui/sd_settings.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QString>
#include <QVBoxLayout>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "obs/obs_inventory.hpp"
#include "obs/profiles_store.hpp"
#include "ui/sd_config_panels.hpp"
#include "ui/sd_i18n.hpp"
#include "ui/sd_icons.hpp"
#include "ui/sd_prefs.hpp"
#include "ui/sd_profiles_panel.hpp"
#include "ui/sd_style.hpp"
#include "ui/sd_support.hpp"
#include "ui/sd_theme.hpp"
#include "ui/sd_widgets.hpp"

namespace sd::ui {

namespace th = sd::ui::theme;
using namespace sd::ui::widgets;

struct SdSettings::Impl {
    SdSettings* q = nullptr;

    // `working`/`savedBaseline` AVANT `panels` : panels detient une reference sur
    // working et doit etre detruit en premier (ordre de destruction inverse).
    sd::core::Config working;       // config en cours d'edition (profil actif)
    sd::core::Config savedBaseline; // dernier etat enregistre -> detection "modifie"
    std::vector<std::string> audioSources;
    std::vector<std::string> scenes;
    std::unique_ptr<ConfigPanels> panels;
    std::unique_ptr<ProfilesPanel> profilesPanel;

    bool saved = false;           // le profil actif a change -> le dock recharge
    QString pendingAssistantName; // "Nouveau > Avec l'assistant" -> nom a creer via l'assistant

    // Preferences GLOBALES (onglet Parametres generaux) : chargees a l'ouverture,
    // modifiees + persistees IMMEDIATEMENT a chaque changement (hors modele "Enregistrer").
    GlobalPrefs generalPrefs;

    QWidget* root = nullptr;
    QWidget* header = nullptr;
    QVBoxLayout* navLay = nullptr;
    QLabel* contentTitle = nullptr;
    QVBoxLayout* contentLay = nullptr;
    QPushButton* resetBtn = nullptr;
    int selPanel = SdSettings::TabSpeakers;

    bool dragging = false;
    QPoint dragOffset;
    bool centered = false;

    // Le bouton "Reinitialiser aux defauts" ne concerne que les REGLAGES FINS
    // (Rythme + Plan large) -> visible uniquement la (retour David Run 6).
    static bool panelHasDefaults(int p) { return p == SdSettings::TabWide || p == SdSettings::TabRhythm; }

    QString panelTitle(int p) const;
    QWidget* makeNavItem(Icon ic, const QString& label, bool active, std::function<void()> onClick,
                         const char* tint = nullptr);
    void rebuildNav();
    void showPanel(int p);
    void resetToDefaults();
    void mountGeneral(QVBoxLayout* host); // panneau "Parametres generaux" (persistance immediate)

    // --- profils ---
    bool dirty() const;
    bool maybeSaveBeforeSwitch(); // garde anti-perte ; false => annuler le changement
    void reloadWorkingFromDisk(); // recharge working depuis le profil actif + recree panels
    void switchActive(int id, bool goToEdit);
    void newProfile(bool withAssistant);
    QString promptName(const QString& initial);

    bool persist(); // garde anti-perte + ecriture (profil actif) ; true si ecrit
    void finish();
};

QString SdSettings::Impl::panelTitle(int p) const {
    switch (p) {
    case SdSettings::TabSpeakers:
        return i18n("Settings.Nav.Speakers");
    case SdSettings::TabCameras:
        return i18n("Settings.Nav.Cameras");
    case SdSettings::TabWide:
        return i18n("Settings.Nav.Wide");
    case SdSettings::TabRhythm:
        return i18n("Settings.Nav.Rhythm");
    case SdSettings::TabProfiles:
        return i18n("Settings.Nav.Profiles");
    case SdSettings::TabSupport:
        return i18n("Settings.Nav.Support");
    case SdSettings::TabGeneral:
        return i18n("Settings.Nav.General");
    default:
        return QString();
    }
}

QWidget* SdSettings::Impl::makeNavItem(Icon ic, const QString& label, bool active, std::function<void()> onClick,
                                       const char* tint) {
    auto* item = new ClickButton();
    item->setMargins(10, 9);
    item->setFillLeft();
    const char* fg = tint ? tint : (active ? th::kAccent : th::kTextSecondary);
    item->setIconPix(icon(ic, fg, 15));
    item->setLabel(label, fg, 12, active ? 700 : 600);
    const QString base = QStringLiteral("#clickbtn { background:%1; border-radius:6px; }");
    item->setBox(base.arg(active ? rgba(th::kAccent, 0.15) : QStringLiteral("transparent")),
                 base.arg(active ? rgba(th::kAccent, 0.15) : rgba("#FFFFFF", 0.05)));
    item->setOnClick(std::move(onClick));
    return item;
}

void SdSettings::Impl::rebuildNav() {
    clearLayout(navLay);
    const struct {
        Icon ic;
        int panel;
    } items[] = {
        {Icon::Users, SdSettings::TabSpeakers},    {Icon::Video, SdSettings::TabCameras},
        {Icon::LayoutGrid, SdSettings::TabWide},   {Icon::Clock, SdSettings::TabRhythm},
        {Icon::Bookmark, SdSettings::TabProfiles},
    };
    for (const auto& it : items) {
        const int p = it.panel;
        navLay->addWidget(makeNavItem(it.ic, panelTitle(p), selPanel == p, [this, p]() {
            selPanel = p;
            rebuildNav();
            showPanel(p);
        }));
    }
    // Separateur, puis les reglages "meta" (hors config de realisation) : Parametres
    // generaux (reglages d'app, persistance immediate) + Soutenir le projet. Tous deux
    // sont de vrais PANNEAUX (plus de pop-up).
    auto* sep = new QFrame();
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(th::kBorder));
    navLay->addWidget(sep);
    navLay->addWidget(makeNavItem(Icon::SlidersHorizontal, i18n("Settings.Nav.General"),
                                  selPanel == SdSettings::TabGeneral, [this]() {
                                      selPanel = SdSettings::TabGeneral;
                                      rebuildNav();
                                      showPanel(SdSettings::TabGeneral);
                                  }));
    navLay->addWidget(makeNavItem(
        Icon::Heart, i18n("Settings.Nav.Support"), selPanel == SdSettings::TabSupport,
        [this]() {
            selPanel = SdSettings::TabSupport;
            rebuildNav();
            showPanel(SdSettings::TabSupport);
        },
        th::kDanger));
    navLay->addStretch();
}

void SdSettings::Impl::showPanel(int p) {
    clearLayout(contentLay);
    contentTitle->setText(panelTitle(p));

    auto* hostW = new QWidget();
    auto* host = new QVBoxLayout(hostW);
    host->setContentsMargins(0, 0, 0, 0);
    host->setSpacing(p == SdSettings::TabSpeakers ? 10 : (p == SdSettings::TabProfiles ? 12 : 14));

    switch (p) {
    case SdSettings::TabSpeakers:
        panels->mountSpeakers(host);
        break;
    case SdSettings::TabCameras:
        panels->mountCameras(host);
        break;
    case SdSettings::TabWide:
        panels->mountWide(host);
        break;
    case SdSettings::TabRhythm:
        panels->mountRhythm(host);
        break;
    case SdSettings::TabProfiles:
        profilesPanel->mount(host);
        break;
    case SdSettings::TabSupport:
        mountSupport(host);
        break;
    case SdSettings::TabGeneral:
        mountGeneral(host);
        break;
    default:
        break;
    }
    contentLay->addWidget(hostW);
    contentLay->addStretch();

    if (resetBtn) {
        resetBtn->setVisible(panelHasDefaults(p));
    }
}

void SdSettings::Impl::resetToDefaults() {
    // Reinitialise les REGLAGES FINS de la page courante (Plan large / Rythme) aux
    // defauts. Modele "tout-sur-Enregistrer" : on remet a l'ecran, on n'ecrit pas ici.
    const sd::core::Config def;
    if (selPanel == SdSettings::TabRhythm) {
        working.timing = def.timing;
        working.audio = def.audio;
        // styleName est COUPLE a timing (cf. rhythm_style) : le remettre au defaut aussi,
        // sinon la pastille de style resterait allumee sur un style dont les curseurs ne
        // refletent plus les valeurs (incoherence d'affichage + nom perime ecrit au profil).
        working.styleName = def.styleName; // "" = Perso (reglage manuel)
    } else if (selPanel == SdSettings::TabWide) {
        working.whenMultiple = def.whenMultiple;
        working.whenSilence = def.whenSilence;
    } else {
        return;
    }
    showPanel(selPanel);
}

void SdSettings::Impl::mountGeneral(QVBoxLayout* host) {
    // Reglages d'APPLICATION (globaux, hors profil) appliques IMMEDIATEMENT (pas de
    // modele "Enregistrer") : on lit/ecrit generalPrefs (prefs.json via sd_prefs) a
    // chaque changement. Pas de bouton de validation propre a ce panneau.

    // --- Section : Mises a jour ---
    host->addWidget(makeSectionLabel(i18n("Settings.General.UpdatesSection")));
    auto* updCard = makeCard(QStringLiteral("genUpdCard"));
    auto* updLay = new QVBoxLayout(updCard);
    updLay->setContentsMargins(14, 12, 14, 12);
    updLay->setSpacing(6);
    auto* updCheck = new QCheckBox(i18n("Update.CheckOnStartup"));
    updCheck->setChecked(generalPrefs.checkUpdatesOnStartup);
    updCheck->setCursor(Qt::PointingHandCursor);
    updCheck->setStyleSheet(checkBoxQss());
    QObject::connect(updCheck, &QCheckBox::toggled, q, [this](bool on) {
        generalPrefs.checkUpdatesOnStartup = on;
        savePrefs(generalPrefs); // persistance immediate
    });
    updLay->addWidget(updCheck);
    updLay->addWidget(makeHint(i18n("Settings.General.UpdatesHint")));
    host->addWidget(updCard);

    // --- Section : Realisation (scene hors regie) ---
    host->addWidget(makeSectionLabel(i18n("Settings.General.DirectingSection")));
    auto* offCard = makeCard(QStringLiteral("genOffCard"));
    auto* offLay = new QVBoxLayout(offCard);
    offLay->setContentsMargins(14, 12, 14, 12);
    offLay->setSpacing(10);

    // Intitule + aide contextuelle (immediat / tous profils / scenes concernees). Le
    // libelle peut etre long -> wordWrap (pas de troncature) et ⓘ aligne en haut.
    auto* head = new QWidget();
    auto* headLay = new QHBoxLayout(head);
    headLay->setContentsMargins(0, 0, 0, 0);
    headLay->setSpacing(6);
    auto* offHeader = makeGroupHeader(i18n("Settings.General.OffScene.Label"));
    offHeader->setWordWrap(true);
    headLay->addWidget(offHeader, 1);
    headLay->addWidget(makeInfoIcon(i18n("Settings.General.OffScene.Info")), 0, Qt::AlignTop);
    offLay->addWidget(head);

    // Deux options exclusives (radios). Defaut produit = Pause (le plus sur). Le groupe
    // est parente a offCard -> detruit avec le panneau au prochain remontage (pas
    // d'accumulation de groupes vides au fil des navigations).
    auto* group = new QButtonGroup(offCard);
    auto* optShot = new QRadioButton(i18n("Settings.General.OffScene.AsShot"));
    auto* optPause = new QRadioButton(i18n("Settings.General.OffScene.Pause"));
    for (QRadioButton* r : {optShot, optPause}) {
        r->setCursor(Qt::PointingHandCursor);
        r->setStyleSheet(radioQss());
        group->addButton(r);
        offLay->addWidget(r);
    }
    optShot->setChecked(generalPrefs.offSceneBehavior == OffSceneBehavior::ForceAsShot);
    optPause->setChecked(generalPrefs.offSceneBehavior == OffSceneBehavior::PauseAuto);
    // On persiste sur la transition "devient coche" de chaque option (toggled(true)).
    QObject::connect(optShot, &QRadioButton::toggled, q, [this](bool on) {
        if (!on) {
            return;
        }
        generalPrefs.offSceneBehavior = OffSceneBehavior::ForceAsShot;
        savePrefs(generalPrefs);
    });
    QObject::connect(optPause, &QRadioButton::toggled, q, [this](bool on) {
        if (!on) {
            return;
        }
        generalPrefs.offSceneBehavior = OffSceneBehavior::PauseAuto;
        savePrefs(generalPrefs);
    });
    host->addWidget(offCard);

    // --- Section : Connexion externe (Stream Deck / regie) ---
    // Remontee du statut ON/OFF de la regie vers une appli externe (Bitfocus Companion) :
    // Flowspire POST une variable personnalisee. Persistance immediate (savePrefs) ; le
    // dock relit ces prefs et pousse l'etat a la fermeture des Parametres + periodiquement.
    host->addWidget(makeSectionLabel(i18n("Settings.General.ExternalSection")));
    auto* extCard = makeCard(QStringLiteral("genExtCard"));
    auto* extLay = new QVBoxLayout(extCard);
    extLay->setContentsMargins(14, 12, 14, 12);
    extLay->setSpacing(10);

    auto* extCheck = new QCheckBox(i18n("Settings.General.External.Enable"));
    extCheck->setChecked(generalPrefs.statusPushEnabled);
    extCheck->setCursor(Qt::PointingHandCursor);
    extCheck->setStyleSheet(checkBoxQss());
    QObject::connect(extCheck, &QCheckBox::toggled, q, [this](bool on) {
        generalPrefs.statusPushEnabled = on;
        savePrefs(generalPrefs); // persistance immediate (modele de l'onglet)
    });
    extLay->addWidget(extCheck);

    // Ligne "Adresse IP" : label a largeur fixe + champ extensible.
    auto* hostRow = new QWidget();
    auto* hostRowLay = new QHBoxLayout(hostRow);
    hostRowLay->setContentsMargins(0, 0, 0, 0);
    hostRowLay->setSpacing(8);
    auto* hostLabel = new QLabel(i18n("Settings.General.External.Host"));
    hostLabel->setStyleSheet(QString("color:%1; font-size:12px;").arg(th::kTextSecondary));
    hostLabel->setFixedWidth(70);
    auto* hostEdit = new QLineEdit(QString::fromStdString(generalPrefs.statusPushHost));
    hostEdit->setStyleSheet(lineEditQss());
    QObject::connect(hostEdit, &QLineEdit::editingFinished, q, [this, hostEdit]() {
        generalPrefs.statusPushHost = hostEdit->text().trimmed().toStdString();
        savePrefs(generalPrefs);
    });
    hostRowLay->addWidget(hostLabel);
    hostRowLay->addWidget(hostEdit, 1);
    extLay->addWidget(hostRow);

    // Ligne "Port" : saisie restreinte aux entiers 1..65535 (validator).
    auto* portRow = new QWidget();
    auto* portRowLay = new QHBoxLayout(portRow);
    portRowLay->setContentsMargins(0, 0, 0, 0);
    portRowLay->setSpacing(8);
    auto* portLabel = new QLabel(i18n("Settings.General.External.Port"));
    portLabel->setStyleSheet(QString("color:%1; font-size:12px;").arg(th::kTextSecondary));
    portLabel->setFixedWidth(70);
    auto* portEdit = new QLineEdit(QString::number(generalPrefs.statusPushPort));
    portEdit->setStyleSheet(lineEditQss());
    portEdit->setValidator(new QIntValidator(1, 65535, portEdit));
    QObject::connect(portEdit, &QLineEdit::editingFinished, q, [this, portEdit]() {
        const int p = portEdit->text().toInt();
        if (p >= 1 && p <= 65535) {
            generalPrefs.statusPushPort = p;
            savePrefs(generalPrefs);
        } else {
            // Saisie vide / hors plage -> on restaure l'affichage sur la valeur reellement
            // persistee : le champ ne doit pas "mentir" sur l'etat enregistre.
            portEdit->setText(QString::number(generalPrefs.statusPushPort));
        }
    });
    portRowLay->addWidget(portLabel);
    portRowLay->addWidget(portEdit, 1);
    extLay->addWidget(portRow);

    extLay->addWidget(makeHint(i18n("Settings.General.External.Hint")));
    host->addWidget(extCard);
}

bool SdSettings::Impl::dirty() const {
    // Comparaison robuste : serialisation des configs nettoyees. Egales => aucun
    // changement non enregistre (pas de faux positif a l'ouverture).
    return sd::core::toJson(sanitizedConfig(working)) != sd::core::toJson(sanitizedConfig(savedBaseline));
}

bool SdSettings::Impl::persist() {
    // GARDE ANTI-PERTE : jamais d'ecriture sans intervenant valide (cf. Run 6).
    int valid = 0;
    for (const auto& s : working.speakers) {
        if (!s.name.empty() && !s.audioSource.empty()) {
            ++valid;
        }
    }
    if (valid == 0) {
        QMessageBox::warning(q, i18n("Settings.Title"), i18n("Summary.Error.NoSpeaker"));
        return false;
    }
    // Ecrit dans le PROFIL ACTIF : le fichier <actif>.json (source de verite lue par
    // le moteur). saveActive s'appuie sur le catalogue (garanti des l'ouverture).
    const sd::profiles::StoreResult res = sd::profiles::saveActive(sanitizedConfig(working));
    if (!res.ok) {
        QMessageBox::warning(q, i18n("Settings.Title"),
                             i18n("Summary.Error.Save").arg(QString::fromStdString(res.error)));
        return false;
    }
    savedBaseline = working; // l'etat enregistre devient la nouvelle reference
    saved = true;
    return true;
}

bool SdSettings::Impl::maybeSaveBeforeSwitch() {
    if (!dirty()) {
        return true;
    }
    const QMessageBox::StandardButton btn =
        QMessageBox::question(q, i18n("Profiles.Unsaved.Title"), i18n("Profiles.Unsaved.Body"),
                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (btn == QMessageBox::Cancel) {
        return false;
    }
    if (btn == QMessageBox::Save) {
        return persist(); // si l'enregistrement echoue (0 intervenant) -> on annule
    }
    return true; // Discard : on bascule sans enregistrer
}

void SdSettings::Impl::reloadWorkingFromDisk() {
    // On vide d'abord le panneau monte (ses widgets referencent l'ancien `panels`
    // via leurs lambdas de rebuild) AVANT de recreer panels -> pas de pointeur
    // pendant. clearLayout differe la destruction (hide + deleteLater).
    clearLayout(contentLay);
    const sd::profiles::ActiveConfigResult loaded =
        sd::profiles::loadActiveConfig(i18n("Profiles.DefaultName").toStdString());
    working = loaded.ok ? loaded.config : sd::core::Config{};
    savedBaseline = working;
    panels = std::make_unique<ConfigPanels>(working, audioSources, scenes);
}

void SdSettings::Impl::switchActive(int id, bool goToEdit) {
    if (!maybeSaveBeforeSwitch()) {
        return;
    }
    const sd::profiles::StoreResult res = sd::profiles::setActive(id);
    if (!res.ok) {
        QMessageBox::warning(q, i18n("Settings.Title"),
                             i18n("Profiles.Error.Generic").arg(QString::fromStdString(res.error)));
        return;
    }
    saved = true; // le profil actif a change -> le dock rechargera a la fermeture
    reloadWorkingFromDisk();
    if (goToEdit) {
        selPanel = SdSettings::TabSpeakers;
    }
    rebuildNav();
    showPanel(selPanel);
}

void SdSettings::Impl::newProfile(bool withAssistant) {
    if (!maybeSaveBeforeSwitch()) {
        return;
    }
    const QString name = promptName(i18n("Profiles.NewBlankName"));
    if (name.isEmpty()) {
        return;
    }
    if (withAssistant) {
        // On NE cree PAS le profil ici : on memorise juste le nom et on ferme. Le dock
        // ouvre alors l'assistant en mode creation, qui creera + activera le profil A
        // LA FIN seulement. -> rien n'est ecrit avant validation (le dock n'affiche
        // pas un profil vide "actif" pendant le remplissage). Retour David.
        pendingAssistantName = name;
        q->accept();
        return;
    }
    // Sans assistant : profil VIERGE cree + actif tout de suite, edite dans les onglets
    // de CETTE fenetre (l'edition vise le profil actif). Decision David : "nouveau" = vide.
    const sd::profiles::StoreResult res =
        sd::profiles::createProfile(name.toStdString(), sd::core::Config{}, /*makeActive=*/true);
    if (!res.ok) {
        QMessageBox::warning(q, i18n("Settings.Title"),
                             i18n("Profiles.Error.Generic").arg(QString::fromStdString(res.error)));
        return;
    }
    saved = true;
    reloadWorkingFromDisk();
    selPanel = SdSettings::TabSpeakers;
    rebuildNav();
    showPanel(selPanel);
}

QString SdSettings::Impl::promptName(const QString& initial) {
    bool ok = false;
    const QString name = QInputDialog::getText(q, i18n("Profiles.NamePrompt.Title"), i18n("Profiles.NamePrompt.Body"),
                                               QLineEdit::Normal, initial, &ok);
    return ok ? name.trimmed() : QString();
}

void SdSettings::Impl::finish() {
    if (persist()) {
        q->accept();
    }
}

// ===========================================================================
SdSettings::SdSettings(QWidget* parent, Tab initialTab) : QDialog(parent), d_(new Impl) {
    d_->q = this;
    d_->selPanel = initialTab;
    d_->generalPrefs = loadPrefs(); // etat initial de l'onglet Parametres generaux
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setWindowTitle(QStringLiteral("Flowspire"));

    d_->audioSources = sd::obsbridge::audioSourceNames();
    d_->scenes = sd::obsbridge::sceneNames();
    // Charge la config du profil ACTIF (la source de verite lue par le moteur).
    // loadActiveConfig garantit au passage l'existence du catalogue (migration de
    // l'existant en profil n.1 au premier usage).
    const sd::profiles::ActiveConfigResult loaded =
        sd::profiles::loadActiveConfig(i18n("Profiles.DefaultName").toStdString());
    if (loaded.ok) {
        d_->working = loaded.config;
    }
    d_->savedBaseline = d_->working;
    d_->panels = std::make_unique<ConfigPanels>(d_->working, d_->audioSources, d_->scenes);

    // Panneau Profils : actions de structure internes, actions d'edition/fenetre
    // deleguees (garde anti-perte + rechargement + changement d'onglet vivent ici).
    ProfilesPanel::Callbacks cbs;
    cbs.onLoad = [this](int id) {
        d_->switchActive(id, /*goToEdit=*/false);
    };
    cbs.onEdit = [this](int id) {
        d_->switchActive(id, /*goToEdit=*/true);
    };
    cbs.onNewWithAssistant = [this]() {
        d_->newProfile(/*withAssistant=*/true);
    };
    cbs.onNewBlank = [this]() {
        d_->newProfile(/*withAssistant=*/false);
    };
    d_->profilesPanel = std::make_unique<ProfilesPanel>(this, std::move(cbs));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 18, 24, 30);

    d_->root = new QWidget(this);
    d_->root->setObjectName(QStringLiteral("settingsRoot"));
    d_->root->setFixedWidth(680);
    d_->root->setStyleSheet(QString("#settingsRoot { background:%1; border:1px solid %2;"
                                    " border-radius:12px; }")
                                .arg(th::kSurface1)
                                .arg(th::kBorder));
    auto* shadow = new QGraphicsDropShadowEffect(d_->root);
    shadow->setBlurRadius(50);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 18);
    d_->root->setGraphicsEffect(shadow);
    outer->addWidget(d_->root);

    d_->root->setMinimumHeight(520);

    auto* rootLay = new QVBoxLayout(d_->root);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    // --- En-tete ---
    d_->header = new QWidget();
    d_->header->setObjectName(QStringLiteral("setHeader"));
    d_->header->setStyleSheet(QString("#setHeader { background:%1; border-top-left-radius:12px;"
                                      " border-top-right-radius:12px; }")
                                  .arg(th::kSurfaceBar));
    auto* hLay = new QHBoxLayout(d_->header);
    hLay->setContentsMargins(20, 16, 20, 16);
    hLay->setSpacing(8);
    auto* hIcon = new QLabel();
    hIcon->setPixmap(icon(Icon::Settings, th::kAccent, 18));
    auto* hTitle = new QLabel(i18n("Settings.Title"));
    hTitle->setStyleSheet(QString("color:%1; font-size:14px; font-weight:700;").arg(th::kTextPrimary));
    auto* close = new QPushButton();
    close->setIcon(icon(Icon::X, th::kTextTertiary, 16));
    close->setCursor(Qt::PointingHandCursor);
    close->setFlat(true);
    close->setStyleSheet(iconBtnQss());
    close->setToolTip(i18n("Assistant.Close"));
    connect(close, &QPushButton::clicked, this, [this]() { reject(); });
    hLay->addWidget(hIcon);
    hLay->addWidget(hTitle);
    hLay->addStretch();
    hLay->addWidget(close);
    rootLay->addWidget(d_->header);

    auto* sepTop = new QFrame();
    sepTop->setFixedHeight(1);
    sepTop->setStyleSheet(QString("background:%1;").arg(th::kBorder));
    rootLay->addWidget(sepTop);

    // --- Corps : sidebar + contenu ---
    auto* body = new QWidget();
    auto* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);

    auto* nav = new QWidget();
    nav->setObjectName(QStringLiteral("setNav"));
    nav->setFixedWidth(200);
    nav->setStyleSheet(QString("#setNav { background:%1; }").arg(th::kBg));
    d_->navLay = new QVBoxLayout(nav);
    d_->navLay->setContentsMargins(12, 12, 12, 12);
    d_->navLay->setSpacing(4);
    bodyLay->addWidget(nav);

    auto* contentOuter = new QWidget();
    auto* contentOuterLay = new QVBoxLayout(contentOuter);
    contentOuterLay->setContentsMargins(20, 18, 20, 18);
    contentOuterLay->setSpacing(14);
    d_->contentTitle = new QLabel();
    d_->contentTitle->setStyleSheet(QString("color:%1; font-size:15px; font-weight:700;").arg(th::kTextPrimary));
    contentOuterLay->addWidget(d_->contentTitle);

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background:transparent; border:none; }"));
    auto* scrollContent = new QWidget();
    scrollContent->setStyleSheet(QStringLiteral("background:transparent;"));
    d_->contentLay = new QVBoxLayout(scrollContent);
    d_->contentLay->setContentsMargins(0, 0, 0, 0);
    d_->contentLay->setSpacing(14);
    scroll->setWidget(scrollContent);
    contentOuterLay->addWidget(scroll, 1);
    bodyLay->addWidget(contentOuter, 1);

    rootLay->addWidget(body, 1);

    // --- Pied : Reinitialiser (gauche) + Enregistrer et fermer (droite) ---
    auto* sepBot = new QFrame();
    sepBot->setFixedHeight(1);
    sepBot->setStyleSheet(QString("background:%1;").arg(th::kBorder));
    rootLay->addWidget(sepBot);
    auto* foot = new QWidget();
    foot->setObjectName(QStringLiteral("setFooter"));
    foot->setStyleSheet(QString("#setFooter { background:%1; border-bottom-left-radius:12px;"
                                " border-bottom-right-radius:12px; }")
                            .arg(th::kSurfaceBar));
    auto* fLay = new QHBoxLayout(foot);
    fLay->setContentsMargins(20, 14, 20, 14);
    fLay->setSpacing(12);
    d_->resetBtn = new QPushButton(i18n("Settings.Reset"));
    d_->resetBtn->setIcon(icon(Icon::RotateCcw, th::kTextSecondary, 13));
    d_->resetBtn->setCursor(Qt::PointingHandCursor);
    d_->resetBtn->setStyleSheet(secondaryBtnQss());
    connect(d_->resetBtn, &QPushButton::clicked, this, [this]() { d_->resetToDefaults(); });
    auto* done = new QPushButton(i18n("Settings.Done"));
    done->setCursor(Qt::PointingHandCursor);
    done->setStyleSheet(primaryBtnQss());
    connect(done, &QPushButton::clicked, this, [this]() { d_->finish(); });
    fLay->addWidget(d_->resetBtn);
    fLay->addStretch();
    fLay->addWidget(done);
    rootLay->addWidget(foot);

    d_->rebuildNav();
    d_->showPanel(d_->selPanel);
}

SdSettings::~SdSettings() = default;

bool SdSettings::savedConfig() const {
    return d_->saved;
}

QString SdSettings::pendingAssistantName() const {
    return d_->pendingAssistantName;
}

void SdSettings::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        for (QWidget* w = childAt(event->position().toPoint()); w; w = w->parentWidget()) {
            if (w == d_->header) {
                d_->dragging = true;
                d_->dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
                break;
            }
        }
    }
    QDialog::mousePressEvent(event);
}

void SdSettings::mouseMoveEvent(QMouseEvent* event) {
    if (d_->dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - d_->dragOffset);
    }
    QDialog::mouseMoveEvent(event);
}

void SdSettings::mouseReleaseEvent(QMouseEvent* event) {
    d_->dragging = false;
    QDialog::mouseReleaseEvent(event);
}

void SdSettings::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (!d_->centered) {
        if (QWidget* p = parentWidget()) {
            const QRect pg = p->geometry();
            move(pg.center().x() - width() / 2, pg.center().y() - height() / 2);
        }
        d_->centered = true;
    }
}

} // namespace sd::ui
