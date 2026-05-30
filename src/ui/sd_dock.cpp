#include "ui/sd_dock.hpp"

#include <QCheckBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <unordered_set>

#include "core/audio_util.hpp"   // source de verite du plancher (kDbFloor)
#include "obs/config_loader.hpp"  // chargement du config.json

namespace sd::ui {

namespace {
// Une seule source de verite pour le plancher : sd::core::kDbFloor.
constexpr int kFloorDb = static_cast<int>(sd::core::kDbFloor);  // bas du vumetre
constexpr int kTickMs = 50;            // ~20 rafraichissements / seconde
constexpr const char* kSpeakColor = "#3FB950";
constexpr const char* kIdleColor = "#6E6E76";

// Niveau dB (double) -> valeur entiere [kFloorDb, 0] coherente entre la barre et
// le texte (meme arrondi des deux cotes : lround, pas troncature).
int dbToInt(double db) {
    int v = static_cast<int>(std::lround(db));
    if (v < kFloorDb) {
        v = kFloorDb;
    } else if (v > 0) {
        v = 0;
    }
    return v;
}

QString dbText(int db) {
    if (db <= kFloorDb) {
        return QStringLiteral("-inf");
    }
    return QString::number(db) + QStringLiteral(" dB");
}

// Type d'evenement custom alloue dynamiquement par Qt (plage utilisateur).
// registerEventType() est thread-safe et appele une fois a l'init.
const QEvent::Type kActionEventType = static_cast<QEvent::Type>(QEvent::registerEventType());

// Evenement portant une action a executer sur le thread UI du dock.
class ActionEvent : public QEvent {
public:
    explicit ActionEvent(std::function<void()> fn)
        : QEvent(kActionEventType), fn_(std::move(fn)) {}
    void run() {
        if (fn_) {
            fn_();
        }
    }

private:
    std::function<void()> fn_;
};
}  // namespace

double SdDock::nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

SdDock::SdDock(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("StreamDirectorDock"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto* header = new QLabel(QStringLiteral("StreamDirector — realisation automatique"));
    header->setStyleSheet(QStringLiteral("font-weight:600;"));
    root->addWidget(header);

    // --- Controles : pilotage auto + plan large ---
    auto* controls = new QHBoxLayout();
    controls->setSpacing(8);
    autoCheck_ = new QCheckBox(QStringLiteral("Pilotage automatique"));
    autoCheck_->setToolTip(
        QStringLiteral("Quand c'est coche, StreamDirector change vos scenes OBS selon qui parle."));
    connect(autoCheck_, &QCheckBox::toggled, this, [this](bool on) { setAuto(on); });
    wideButton_ = new QPushButton(QStringLiteral("Plan large"));
    wideButton_->setToolTip(QStringLiteral("Forcer le plan large (maintenu le temps-mini)."));
    connect(wideButton_, &QPushButton::clicked, this, [this]() { forceWide(); });
    controls->addWidget(autoCheck_);
    controls->addWidget(wideButton_);
    controls->addStretch();
    root->addLayout(controls);

    modeLabel_ = new QLabel();
    modeLabel_->setWordWrap(true);
    root->addWidget(modeLabel_);

    rowsLayout_ = new QVBoxLayout();
    rowsLayout_->setSpacing(6);
    root->addLayout(rowsLayout_);

    emptyLabel_ = new QLabel(
        QStringLiteral("Aucune source audio detectee. Ajoute des sources dans OBS puis Rafraichir."));
    emptyLabel_->setWordWrap(true);
    emptyLabel_->setStyleSheet(QStringLiteral("color:#9C9CA3;"));
    rowsLayout_->addWidget(emptyLabel_);

    auto* divider = new QFrame();
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral("color:#3A3A42;"));
    root->addWidget(divider);

    onAirLabel_ = new QLabel();
    onAirLabel_->setStyleSheet(QStringLiteral("font-weight:600;"));
    root->addWidget(onAirLabel_);

    configLabel_ = new QLabel();
    configLabel_->setWordWrap(true);
    configLabel_->setStyleSheet(QStringLiteral("color:#6E6E76; font-size:11px;"));
    configLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(configLabel_);

    auto* refresh = new QPushButton(QStringLiteral("Rafraichir (sources + config)"));
    connect(refresh, &QPushButton::clicked, this, [this]() { reload(); });
    root->addWidget(refresh);

    root->addStretch();

    reload();

    timer_ = new QTimer(this);
    timer_->setInterval(kTickMs);
    connect(timer_, &QTimer::timeout, this, [this]() { tick(); });
    timer_->start();
}

SdDock::~SdDock() {
    if (timer_) {
        timer_->stop();
    }
    monitor_.stop();  // remove_callback : plus aucun callback audio apres ce point
}

void SdDock::reload() {
    // 1) Re-scanne les sources audio d'OBS (recree les volmeters).
    monitor_.start();
    const std::vector<std::string> names = monitor_.sourceNames();

    // 2) Tente de charger une config JSON. Si valide -> mode "pilotage reel" ;
    //    sinon -> mode "lecture seule" (1 intervenant par source, sans scenes).
    const sd::obsbridge::ConfigLoadResult loaded = sd::obsbridge::loadConfig();

    sd::core::Config cfg;
    displaySpeakers_.clear();
    if (loaded.parsed && !loaded.config.speakers.empty()) {
        configMode_ = true;
        cfg = loaded.config;
        for (const auto& sp : cfg.speakers) {
            displaySpeakers_.push_back({sp.id, sp.name, sp.audioSource});
        }
    } else {
        configMode_ = false;
        for (const auto& name : names) {
            sd::core::Speaker sp;
            sp.id = name;
            sp.name = name;
            sp.audioSource = name;
            cfg.speakers.push_back(sp);
            displaySpeakers_.push_back({name, name, name});
        }
    }

    director_ = std::make_unique<sd::core::Director>(cfg);
    // GARDE-FOU (frontiere de surete reelle) : le dock est le SEUL endroit qui
    // bascule les scenes OBS (applyDecision/force*). Le pilotage auto demarre a
    // OFF (membre autoEnabled_ = false) et n'est arme qu'ici, de facon synchrone
    // sur le thread UI juste apres la construction : aucun tick ne peut s'intercaler
    // entre la creation du Director et ce setAutoEnabled, donc aucune bascule
    // involontaire possible. Hors mode config, l'auto est en plus force a OFF.
    if (!configMode_) {
        autoEnabled_ = false;
    }
    director_->setAutoEnabled(autoEnabled_);

    // 3) Reflete l'etat dans les controles.
    {
        const QSignalBlocker blocker(autoCheck_);
        autoCheck_->setChecked(autoEnabled_);
    }
    autoCheck_->setEnabled(configMode_);
    wideButton_->setEnabled(configMode_ && !cfg.wideShotScene.empty());

    // 4) Libelle de config (chemin exact + etat) — selectionnable pour copie.
    const QString path = QString::fromStdString(loaded.path);
    if (loaded.parsed) {
        configLabel_->setText(
            QStringLiteral("Config chargee ✓ — %1 intervenant(s)\n%2")
                .arg(static_cast<int>(loaded.config.speakers.size()))
                .arg(path));
    } else if (loaded.fileFound) {
        configLabel_->setText(QStringLiteral("Config INVALIDE — %1\n%2")
                                  .arg(QString::fromStdString(loaded.error))
                                  .arg(path));
    } else {
        configLabel_->setText(
            QStringLiteral("Aucune config (mode lecture seule). Placez config.json ici :\n%1")
                .arg(path));
    }

    // 5) Reconstruit les lignes d'UI (suppression SYNCHRONE : evite l'empilement
    //    visuel transitoire pendant qu'on ajoute les nouvelles).
    for (auto& row : rows_) {
        delete row.line;  // detruit la ligne et ses enfants (bar/labels/bouton)
    }
    rows_.clear();
    shownOnAir_ = "\x01";  // force la maj du label "a l'antenne" au prochain tick

    emptyLabel_->setVisible(displaySpeakers_.empty());

    for (size_t i = 0; i < displaySpeakers_.size(); ++i) {
        const DisplaySpeaker& ds = displaySpeakers_[i];

        auto* line = new QWidget();
        auto* lay = new QHBoxLayout(line);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(8);

        auto* nameLabel = new QLabel(QString::fromStdString(ds.name));
        nameLabel->setMinimumWidth(110);
        nameLabel->setMaximumWidth(160);

        auto* bar = new QProgressBar();
        bar->setRange(kFloorDb, 0);
        bar->setValue(kFloorDb);
        bar->setTextVisible(false);
        bar->setFixedHeight(10);

        auto* dbLabel = new QLabel(dbText(kFloorDb));
        dbLabel->setMinimumWidth(52);
        dbLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto* stateLabel = new QLabel(QStringLiteral("silence"));
        stateLabel->setMinimumWidth(54);
        stateLabel->setStyleSheet(QString("color:%1;").arg(kIdleColor));

        lay->addWidget(nameLabel);
        lay->addWidget(bar, 1);
        lay->addWidget(dbLabel);
        lay->addWidget(stateLabel);

        // Bouton "Forcer" : seulement en mode config (sinon aucune scene a montrer).
        if (configMode_) {
            auto* forceBtn = new QPushButton(QStringLiteral("Forcer"));
            forceBtn->setToolTip(QStringLiteral("Forcer le plan de cet intervenant."));
            const int index = static_cast<int>(i);
            connect(forceBtn, &QPushButton::clicked, this,
                    [this, index]() { forceSpeakerByIndex(index); });
            lay->addWidget(forceBtn);
        }

        rowsLayout_->addWidget(line);
        rows_.push_back(Row{ds.id, ds.audioSource, line, bar, dbLabel, stateLabel});
    }

    updateModeLabel();
}

void SdDock::updateModeLabel() {
    if (!modeLabel_) {
        return;
    }
    if (!configMode_) {
        modeLabel_->setText(QStringLiteral(
            "Mode lecture seule : aucune config. Ajoutez un config.json pour piloter les scenes."));
        modeLabel_->setStyleSheet(QStringLiteral("color:#9C9CA3;"));
        return;
    }
    if (autoEnabled_) {
        modeLabel_->setText(
            QStringLiteral("PILOTAGE AUTO actif — StreamDirector change vos scenes selon qui parle."));
        modeLabel_->setStyleSheet(QStringLiteral("color:#3FB950; font-weight:600;"));
    } else {
        modeLabel_->setText(QStringLiteral(
            "Pilotage en pause — cochez « Pilotage automatique » pour lancer la realisation."));
        modeLabel_->setStyleSheet(QStringLiteral("color:#D29922; font-weight:600;"));
    }
}

void SdDock::setAuto(bool on) {
    // GARDE-FOU : pas d'activation du pilotage auto sans config valide chargee.
    // Sinon (mode lecture seule) l'etat afficherait "PILOTAGE AUTO actif" alors
    // qu'aucune scene n'est pilotable -> etat trompeur. La desactivation (off)
    // reste toujours possible. Couvre aussi la voie hotkey (toggleAuto).
    if (on && !configMode_) {
        return;
    }
    autoEnabled_ = on;
    if (director_) {
        director_->setAutoEnabled(on);
    }
    if (autoCheck_) {
        const QSignalBlocker blocker(autoCheck_);
        autoCheck_->setChecked(on);
    }
    updateModeLabel();
}

void SdDock::toggleAuto() {
    setAuto(!autoEnabled_);
}

void SdDock::runOnUiThread(std::function<void()> fn) {
    // postEvent est explicitement thread-safe (Qt en prend possession et le
    // supprime apres traitement, ou a la destruction de l'objet cible).
    QCoreApplication::postEvent(this, new ActionEvent(std::move(fn)));
}

void SdDock::customEvent(QEvent* event) {
    if (event && event->type() == kActionEventType) {
        static_cast<ActionEvent*>(event)->run();
        return;
    }
    QWidget::customEvent(event);
}

void SdDock::applyDecision(const sd::core::Decision& decision, const std::string& currentOnAir) {
    // On ne pilote OBS que si le pilotage auto est actif (garde-fou). Les forçages
    // manuels (forceWide/forceSpeakerByIndex) appliquent leur scene eux-memes.
    if (!autoEnabled_ || decision.scene.empty()) {
        return;
    }
    // Le Director ne lit JAMAIS la scene reelle d'OBS : sa seule verite est sa
    // cible interne (decision.scene). On compare donc ici a la scene REELLEMENT a
    // l'antenne. Si elles different — soit le realisateur vient de changer d'avis
    // (decision.switched), soit l'utilisateur a change de scene A LA MAIN dans OBS
    // pendant l'auto — on (re)applique pour reprendre la main. switchTo est un
    // no-op si la scene est deja active, donc aucun spam quand tout est synchrone.
    if (decision.scene != currentOnAir) {
        sceneSwitcher_.switchTo(decision.scene);
    }
}

void SdDock::forceWide() {
    if (!director_) {
        return;
    }
    const std::string wide = director_->config().wideShotScene;
    if (wide.empty()) {
        return;  // pas de plan large defini : rien a forcer.
    }
    const sd::core::Decision decision = director_->forceScene(nowSeconds(), wide, "");
    // Un forcage doit toujours s'appliquer a OBS (meme si le cerveau croyait deja
    // y etre : la scene a pu etre changee a la main). switchTo ignore si deja active.
    sceneSwitcher_.switchTo(decision.scene.empty() ? wide : decision.scene);
}

void SdDock::forceSpeakerByIndex(int index) {
    if (!director_ || index < 0 || index >= static_cast<int>(displaySpeakers_.size())) {
        return;
    }
    const sd::core::Decision decision =
        director_->forceSpeaker(nowSeconds(), displaySpeakers_[static_cast<size_t>(index)].id);
    if (!decision.scene.empty()) {
        sceneSwitcher_.switchTo(decision.scene);
    }
}

void SdDock::tick() {
    if (!director_) {
        return;
    }

    const double now = nowSeconds();
    // snapshot() est indexe par NOM DE SOURCE ; le Director attend les niveaux
    // par ID d'intervenant -> on remappe via audioSource (en mode config, id peut
    // differer de la source ; en mode auto, id == audioSource).
    const std::map<std::string, double> levelsBySource = monitor_.snapshot(now);
    std::map<std::string, double> levelsById;
    for (const auto& ds : displaySpeakers_) {
        const auto it = levelsBySource.find(ds.audioSource);
        levelsById[ds.id] = (it != levelsBySource.end()) ? it->second : static_cast<double>(kFloorDb);
    }

    const sd::core::Decision decision = director_->update(now, levelsById);

    // Scene REELLEMENT a l'antenne dans OBS, lue UNE seule fois par tick : sert a
    // la fois au pilotage (detection de derive / reprise de main) et au libelle.
    const std::string onAir = sceneSwitcher_.currentProgramScene();
    applyDecision(decision, onAir);

    // speakingIds() est trie ; on en fait un set pour un test d'appartenance O(1).
    const std::vector<std::string> speaking = director_->speakingIds();
    const std::unordered_set<std::string> speakingSet(speaking.begin(), speaking.end());

    for (auto& row : rows_) {
        const auto it = levelsBySource.find(row.audioSource);
        const double db = (it != levelsBySource.end()) ? it->second : static_cast<double>(kFloorDb);

        // On n'ecrit dans les widgets que si la valeur AFFICHEE change (20 Hz).
        const int dbInt = dbToInt(db);
        if (dbInt != row.shownDb) {
            row.shownDb = dbInt;
            row.bar->setValue(dbInt);
            row.dbLabel->setText(dbText(dbInt));
        }

        const int speakingNow = speakingSet.count(row.id) ? 1 : 0;
        if (speakingNow != row.shownSpeaking) {
            row.shownSpeaking = speakingNow;
            row.stateLabel->setText(speakingNow ? QStringLiteral("PARLE")
                                                : QStringLiteral("silence"));
            row.stateLabel->setStyleSheet(
                QString("color:%1;").arg(speakingNow ? kSpeakColor : kIdleColor));
        }
    }

    // Libelle "scene a l'antenne" : reutilise `onAir` deja lu en debut de tick
    // (apres un eventuel switchTo, le libelle se corrige au tick suivant ; a 20 Hz
    // l'ecart est imperceptible et on evite un 2e appel a l'API OBS).
    if (onAir != shownOnAir_) {
        shownOnAir_ = onAir;
        if (onAir.empty()) {
            onAirLabel_->setText(QStringLiteral("Scene a l'antenne : (indisponible)"));
        } else {
            onAirLabel_->setText(QStringLiteral("Scene a l'antenne : ") +
                                 QString::fromStdString(onAir));
        }
    }
}

}  // namespace sd::ui
