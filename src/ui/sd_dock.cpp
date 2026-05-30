#include "ui/sd_dock.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <unordered_set>

#include "core/audio_util.hpp"  // source de verite du plancher (kDbFloor)

namespace sd::ui {

namespace {
// Une seule source de verite pour le plancher : sd::core::kDbFloor.
constexpr int kFloorDb = static_cast<int>(sd::core::kDbFloor);  // bas du vumetre
constexpr int kTickMs = 50;            // ~20 rafraichissements / seconde
constexpr const char* kSpeakColor = "#3FB950";
constexpr const char* kIdleColor = "#6E6E76";

// Niveau dB (double) -> valeur entiere [kFloorDb, 0] cohérente entre la barre et
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

    auto* header = new QLabel(QStringLiteral("StreamDirector — qui parle ? (lecture seule)"));
    header->setStyleSheet(QStringLiteral("font-weight:600;"));
    root->addWidget(header);

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

    auto* note = new QLabel(QStringLiteral("Lecture seule : ne pilote pas encore les scenes OBS."));
    note->setStyleSheet(QStringLiteral("color:#6E6E76; font-size:11px;"));
    root->addWidget(note);

    auto* refresh = new QPushButton(QStringLiteral("Rafraichir les sources"));
    connect(refresh, &QPushButton::clicked, this, [this]() { rebuildSources(); });
    root->addWidget(refresh);

    root->addStretch();

    rebuildSources();

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

void SdDock::rebuildSources() {
    // 1) Re-scanne les sources audio d'OBS (recree les volmeters).
    monitor_.start();
    const std::vector<std::string> names = monitor_.sourceNames();

    // 2) Construit une config auto : 1 "intervenant" par source audio (sans
    //    scenes en lecture seule — on veut juste detecter qui parle).
    sd::core::Config cfg;
    for (const auto& name : names) {
        sd::core::Speaker sp;
        sp.id = name;
        sp.name = name;
        sp.audioSource = name;
        cfg.speakers.push_back(sp);
    }
    director_ = std::make_unique<sd::core::Director>(cfg);

    // 3) Reconstruit les lignes d'UI. Suppression SYNCHRONE (delete) plutot que
    //    deleteLater : evite l'empilement visuel transitoire (anciennes lignes
    //    encore dans le layout pendant qu'on ajoute les nouvelles).
    for (auto& row : rows_) {
        delete row.line;  // detruit la ligne et ses enfants (bar/labels)
    }
    rows_.clear();
    shownOnAir_ = "\x01";  // force la maj du label "a l'antenne" au prochain tick

    emptyLabel_->setVisible(names.empty());

    for (const auto& name : names) {
        auto* line = new QWidget();
        auto* lay = new QHBoxLayout(line);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(8);

        auto* nameLabel = new QLabel(QString::fromStdString(name));
        nameLabel->setMinimumWidth(110);
        nameLabel->setMaximumWidth(160);

        auto* bar = new QProgressBar();
        bar->setRange(kFloorDb, 0);
        bar->setValue(kFloorDb);
        bar->setTextVisible(false);
        bar->setFixedHeight(10);

        auto* dbLabel = new QLabel(dbText(static_cast<int>(kFloorDb)));
        dbLabel->setMinimumWidth(52);
        dbLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto* stateLabel = new QLabel(QStringLiteral("silence"));
        stateLabel->setMinimumWidth(54);
        stateLabel->setStyleSheet(QString("color:%1;").arg(kIdleColor));

        lay->addWidget(nameLabel);
        lay->addWidget(bar, 1);
        lay->addWidget(dbLabel);
        lay->addWidget(stateLabel);

        rowsLayout_->addWidget(line);
        rows_.push_back(Row{name, line, bar, dbLabel, stateLabel});
    }
}

void SdDock::tick() {
    if (!director_) {
        return;
    }

    const double now = nowSeconds();
    const std::map<std::string, double> levels = monitor_.snapshot(now);
    director_->update(now, levels);

    // speakingIds() est trie ; on en fait un set pour un test d'appartenance O(1)
    // (une seule construction par tick au lieu d'un std::find par ligne).
    const std::vector<std::string> speaking = director_->speakingIds();
    const std::unordered_set<std::string> speakingSet(speaking.begin(), speaking.end());

    for (auto& row : rows_) {
        const auto it = levels.find(row.id);
        const double db = (it != levels.end()) ? it->second : static_cast<double>(kFloorDb);

        // On n'ecrit dans les widgets que si la valeur AFFICHEE change (le tick
        // tourne a 20 Hz ; eviter un re-parse CSS / relayout inutile par frame).
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

    const std::string onAir = speaking.empty() ? std::string{} : speaking.front();
    if (onAir != shownOnAir_) {
        shownOnAir_ = onAir;
        if (onAir.empty()) {
            onAirLabel_->setText(QStringLiteral("A l'antenne (calcule) : (personne)"));
        } else {
            onAirLabel_->setText(QStringLiteral("A l'antenne (calcule) : ") +
                                 QString::fromStdString(onAir));
        }
    }
}

}  // namespace sd::ui
