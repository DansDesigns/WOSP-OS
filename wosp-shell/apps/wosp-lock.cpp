/*
Run:
./wosp-lock
./wosp-lock --boot
./wosp-lock --auth
*/

#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QDateTime>
#include <QTime>
#include <QPainter>
#include <QMouseEvent>
#include <QScreen>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QGraphicsOpacityEffect>
#include <QFontMetrics>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QtMath>
#include <QCryptographicHash>
#include <QPushButton>
#include <QGridLayout>
#include <QRandomGenerator>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>

#include <signal.h>
#include <unistd.h>
#include <pwd.h>
#include <functional>

// ─────────────────────────────────────────────
// Lock mode
// ─────────────────────────────────────────────
enum class LockMode {
    SESSION,
    BOOT,
    AUTH
};

static LockMode g_lockMode = LockMode::SESSION;

// ─────────────────────────────────────────────
// Best-effort signal hardening
// ─────────────────────────────────────────────
static void ignoreSignal(int) {}
static void installSignalHardening() {
    signal(SIGINT,  ignoreSignal);
    signal(SIGTERM, ignoreSignal);
    signal(SIGHUP,  ignoreSignal);
    signal(SIGQUIT, ignoreSignal);
}

// ─────────────────────────────────────────────
// HOME resolver
// ─────────────────────────────────────────────
static QString realHomePath() {
    QByteArray homeEnv = qgetenv("HOME");
    if (!homeEnv.isEmpty()) {
        QString h = QString::fromUtf8(homeEnv).trimmed();
        if (!h.isEmpty() && QFileInfo(h).isDir())
            return h;
    }

    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        QString h = QString::fromUtf8(pw->pw_dir).trimmed();
        if (!h.isEmpty() && QFileInfo(h).isDir())
            return h;
    }

    return QDir::homePath();
}

// ─────────────────────────────────────────────
// Utility
// ─────────────────────────────────────────────
static QString readFirstLine(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QTextStream in(&f);
    return in.readLine().trimmed();
}

static QString sha256(const QString &input) {
    return QString(QCryptographicHash::hash(
        input.toUtf8(), QCryptographicHash::Sha256).toHex());
}

// ─────────────────────────────────────────────
// Lockscreen Page
// ─────────────────────────────────────────────
class LockscreenPage : public QWidget {
public:
    explicit LockscreenPage(QWidget *parent = nullptr)
        : QWidget(parent),
          wifiActive(false),
          btActive(false),
          batteryPercent(-1),
          sliderOffset(0.0),
          sliding(false),
          slidingBack(false)
    {
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_ShowWithoutActivating, false);

        // IMPORTANT PERF CHANGE:
        // Defer heavy I/O (wallpaper/icons/sys probing) until after first paint.
        // This makes lockscreen appear instantly.
        QTimer::singleShot(0, this, [this](){
            loadWallpaper();
            loadIcons();
            // In AUTH mode we should never be showing LockscreenPage anyway,
            // but guard it regardless:
            if (g_lockMode != LockMode::AUTH) {
                updateStatus();
            }
            update();
        });

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(40,40,40,40);
        mainLayout->setSpacing(15);

        // TOP ROW
        QHBoxLayout *topRow = new QHBoxLayout();
        topRow->setSpacing(20);

        wifiLabel = new QLabel(this);
        wifiLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        batteryLabel = new QLabel("Battery: --%", this);
        batteryLabel->setAlignment(Qt::AlignCenter);
        batteryLabel->setStyleSheet("color:white;");

        btLabel = new QLabel(this);
        btLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        wifiLabel->setStyleSheet("color:grey;");
        btLabel->setStyleSheet("color:grey;");

        wifiEffect = new QGraphicsOpacityEffect(this);
        btEffect   = new QGraphicsOpacityEffect(this);
        wifiLabel->setGraphicsEffect(wifiEffect);
        btLabel->setGraphicsEffect(btEffect);

        topRow->addWidget(wifiLabel);
        topRow->addStretch(1);
        topRow->addWidget(batteryLabel);
        topRow->addStretch(1);
        topRow->addWidget(btLabel);

        mainLayout->addLayout(topRow);
        mainLayout->addStretch(1);

        // CLOCK
        timeLabel = new QLabel("--:--", this);
        timeLabel->setAlignment(Qt::AlignCenter);
        timeLabel->setStyleSheet("color:white;");
        QFont clockFont("Comfortaa");
        timeLabel->setFont(clockFont);
        mainLayout->addWidget(timeLabel, 0, Qt::AlignCenter);

        mainLayout->addStretch(4);

        // SLIDE TEXT
        slideTextLabel = new QLabel("Slide up to unlock", this);
        slideTextLabel->setAlignment(Qt::AlignCenter | Qt::AlignBottom);
        slideTextLabel->setStyleSheet("color:white;");
        QFont labelFont("Comfortaa");
        slideTextLabel->setFont(labelFont);
        slideTextLabel->setContentsMargins(0, 20, 0, 0);
        mainLayout->addWidget(slideTextLabel);

        // Timers
        QTimer *clockTimer = new QTimer(this);
        connect(clockTimer, &QTimer::timeout, this, &LockscreenPage::updateClock);
        clockTimer->start(1000);
        updateClock();

        QTimer *statusTimer = new QTimer(this);
        connect(statusTimer, &QTimer::timeout, this, &LockscreenPage::updateStatus);
        statusTimer->start(5000);
        // PERF: do not call updateStatus() synchronously in ctor; deferred above.

        slideBackTimer = new QTimer(this);
        slideBackTimer->setInterval(16);
        connect(slideBackTimer, &QTimer::timeout, this, &LockscreenPage::onSlideBackStep);

        adjustScaling();
    }

    void setOnUnlockRequested(const std::function<void()> &cb) { onUnlockRequested = cb; }

    void activateInputGrab() {
        setFocus(Qt::OtherFocusReason);
        grabKeyboard();
        grabMouse();
    }
    void deactivateInputGrab() {
        releaseMouse();
        releaseKeyboard();
    }

protected:
    void showEvent(QShowEvent *e) override {
        QWidget::showEvent(e);
        activateInputGrab();
    }

    void closeEvent(QCloseEvent *ev) override { ev->ignore(); }
    void keyPressEvent(QKeyEvent *e) override { e->accept(); }
    void keyReleaseEvent(QKeyEvent *e) override { e->accept(); }

    void paintEvent(QPaintEvent *ev) override {
        Q_UNUSED(ev);
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform,true);

        p.fillRect(rect(), Qt::black);

        // PERF: draw cached scaled wallpaper (no rescale per frame)
        if (!wallpaperScaled.isNull()) {
            QPoint c = rect().center() - QPoint(wallpaperScaled.width()/2, wallpaperScaled.height()/2);
            p.drawPixmap(c, wallpaperScaled);
        } else if (!wallpaper.isNull()) {
            // Fallback: if scaling cache hasn't populated yet, draw raw
            QPixmap scaled = wallpaper.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            QPoint c = rect().center() - QPoint(scaled.width()/2, scaled.height()/2);
            p.drawPixmap(c, scaled);
        }

        p.fillRect(rect(), QColor(0,0,0,80));

        if (slideTextLabel) {
            QRect tg = slideTextLabel->geometry();

            int baseY = tg.top() - int(50 * scaleFactor);
            int minY = int(height() * 0.35);
            int maxY = int(height() * 0.90);
            if (baseY < minY) baseY = minY;
            if (baseY > maxY) baseY = maxY;

            int arrowY = baseY + int(sliderOffset);
            int cx = width()/2;

            if (sliderIconAvailable && !sliderIcon.isNull()) {
                int desiredHeight = int((height() / 18.0) * scaleFactor);
                QPixmap sliderScaled2 = sliderIcon.scaledToHeight(desiredHeight, Qt::SmoothTransformation);

                int x = cx - sliderScaled2.width() / 2;
                int y = arrowY - sliderScaled2.height() / 2;

                p.drawPixmap(x, y, sliderScaled2);
            } else {
                QFont f = font();
                f.setPointSize(int(42 * scaleFactor));
                p.setFont(f);
                p.setPen(Qt::white);
                QRect r(cx-40, arrowY-30, 80, 60);
                p.drawText(r, Qt::AlignTop, "🔒");
            }
        }
    }

    void resizeEvent(QResizeEvent *) override {
        adjustScaling();
        // PERF: rescale wallpaper once per resize
        rebuildWallpaperCache();
    }

    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() != Qt::LeftButton) return;

        int cx = width()/2;
        QRect tg = slideTextLabel->geometry();

        int baseY = tg.top() - int(50 * scaleFactor);
        int minY = int(height() * 0.35);
        int maxY = int(height() * 0.90);
        if (baseY < minY) baseY = minY;
        if (baseY > maxY) baseY = maxY;

        int arrowY = baseY + int(sliderOffset);

        int rad = int(60 * scaleFactor);
        QRect handle(cx-rad, arrowY-rad, rad*2, rad*2);

        if (handle.contains(e->pos())) {
            sliding = true;
            slidingBack = false;
            slideBackTimer->stop();
            lastPos = e->pos();
        }
    }

    void mouseMoveEvent(QMouseEvent *e) override {
        if (!sliding) return;

        int dy = e->pos().y() - lastPos.y();
        lastPos = e->pos();

        sliderOffset += dy;
        double maxUp = -height()*0.3;
        if (sliderOffset < maxUp) sliderOffset = maxUp;
        if (sliderOffset > 0) sliderOffset = 0;

        update();
    }

    void mouseReleaseEvent(QMouseEvent *e) override {
        Q_UNUSED(e);
        if (!sliding) return;
        sliding = false;

        if (-sliderOffset > height()*0.2) {
            deactivateInputGrab();
            if (onUnlockRequested) onUnlockRequested();
        } else {
            startSlideBack();
        }
    }

private:
    QLabel *wifiLabel = nullptr;
    QLabel *btLabel = nullptr;
    QLabel *batteryLabel = nullptr;
    QLabel *timeLabel = nullptr;
    QLabel *slideTextLabel = nullptr;

    QGraphicsOpacityEffect *wifiEffect = nullptr;
    QGraphicsOpacityEffect *btEffect = nullptr;

    QPixmap wallpaper;
    QPixmap wallpaperScaled; // PERF: cached scaled version
    QPixmap wifiIcon;
    QPixmap btIcon;
    QPixmap sliderIcon;
    bool sliderIconAvailable = false;

    bool wifiActive;
    bool btActive;
    int batteryPercent;

    double sliderOffset;
    bool sliding;
    bool slidingBack;
    QPoint lastPos;
    QTimer *slideBackTimer = nullptr;

    qreal scaleFactor = 1.0;
    std::function<void()> onUnlockRequested;

    void rebuildWallpaperCache() {
        if (wallpaper.isNull() || width() <= 0 || height() <= 0) {
            wallpaperScaled = QPixmap();
            return;
        }
        wallpaperScaled = wallpaper.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }

    void loadWallpaper() {
        QString home = realHomePath();
        QString cfg = home + "/.config/osm-paper.conf";
        QString path;

        if (QFile::exists(cfg)) {
            QFile f(cfg);
            if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
                QTextStream in(&f);
                while (!in.atEnd()) {
                    QString line = in.readLine().trimmed();
                    if (line.startsWith("wallpaper=")) {
                        path = line.section('=',1).trimmed();
                        break;
                    }
                }
            }
        }

        if (!path.isEmpty() && QFile::exists(path))
            wallpaper.load(path);
        else
            wallpaper = QPixmap();

        rebuildWallpaperCache();
    }

    void loadIcons() {
        QString home = realHomePath();
        QString dir = home + "/.config/wosp/images/";

        QString wp = dir + "wifi.png";
        if (QFile::exists(wp)) wifiIcon.load(wp);

        QString bp = dir + "bt.png";
        if (QFile::exists(bp)) btIcon.load(bp);

        QString sp = dir + "slider.png";
        sliderIconAvailable = QFile::exists(sp);
        if (sliderIconAvailable) sliderIcon.load(sp);
    }

    void adjustScaling() {
        scaleFactor = this->devicePixelRatio();

        int H = height();
        if (H <= 0) H = 800;

        QFont f = timeLabel->font();
        f.setPointSize(int((H/12) * scaleFactor));
        timeLabel->setFont(f);

        QFont f2 = batteryLabel->font();
        f2.setPointSize(int((H/60) * scaleFactor));
        batteryLabel->setFont(f2);

        int iconH = QFontMetrics(f2).height();

        if (!wifiIcon.isNull())
            wifiLabel->setPixmap(wifiIcon.scaledToHeight(iconH, Qt::SmoothTransformation));
        else
            wifiLabel->setText("WiFi");

        if (!btIcon.isNull())
            btLabel->setPixmap(btIcon.scaledToHeight(iconH, Qt::SmoothTransformation));
        else
            btLabel->setText("BT");

        QFont f3 = slideTextLabel->font();
        f3.setPointSize(int((H/70) * scaleFactor));
        slideTextLabel->setFont(f3);

        update();
    }

    void updateClock() {
        timeLabel->setText(QTime::currentTime().toString("HH:mm"));
    }

    void updateStatus() {
        // PERF: do not probe /sys for AUTH prompts (and this page shouldn't exist in AUTH anyway)
        if (g_lockMode == LockMode::AUTH) return;

        wifiActive = detectWifiActive();
        btActive   = detectBtActive();
        batteryPercent = readBattery();

        if (batteryPercent >= 0)
            batteryLabel->setText(QString("🔋%1%").arg(batteryPercent));
        else
            batteryLabel->setText("🔋--%");

        wifiEffect->setOpacity(wifiActive ? 1.0 : 0.3);
        btEffect->setOpacity(btActive ? 1.0 : 0.3);

        update();
    }

    bool detectWifiActive() {
        QDir d("/sys/class/net");
        if (!d.exists()) return false;
        QStringList devs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &iface : devs) {
            QString w = d.absoluteFilePath(iface + "/wireless");
            if (!QDir(w).exists()) continue;
            QString op = readFirstLine(d.absoluteFilePath(iface + "/operstate"));
            if (op == "up") return true;
        }
        return false;
    }

    bool detectBtActive() {
        QDir d("/sys/class/bluetooth");
        return d.exists() && !d.entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty();
    }

    int readBattery() {
        QDir d("/sys/class/power_supply");
        if (!d.exists()) return -1;
        QStringList ps = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        QString name;
        for (const QString &n : ps) {
            if (n.toUpper().startsWith("BAT")) { name=n; break; }
        }
        if (name.isEmpty()) {
            for (const QString &n : ps) {
                QString t = readFirstLine(d.absoluteFilePath(n+"/type")).toLower();
                if (t=="battery") { name=n; break; }
            }
        }
        if (name.isEmpty()) return -1;
        QString cap = readFirstLine(d.absoluteFilePath(name+"/capacity"));
        bool ok=false;
        int v = cap.toInt(&ok);
        return ok ? v : -1;
    }

    void startSlideBack() {
        slidingBack = true;
        slideBackTimer->start();
    }

    void onSlideBackStep() {
        if (!slidingBack) {
            slideBackTimer->stop();
            return;
        }
        sliderOffset += (12 * scaleFactor);
        if (sliderOffset >= 0) {
            sliderOffset = 0;
            slidingBack = false;
            slideBackTimer->stop();
        }
        update();
    }
};

// ─────────────────────────────────────────────
// Auth Page (UNCHANGED from your latest)
// ─────────────────────────────────────────────
struct ShapeItem {
    QString shape;
    QColor color;
    QRect rect;
};

class AuthPage : public QWidget {
public:
    explicit AuthPage(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setStyleSheet("background-color:#000000; color:white;");
        setFocusPolicy(Qt::StrongFocus);

        titleLabel = new QLabel(this);
        titleLabel->setStyleSheet("font-size:28px; color:white;");
        titleLabel->setAlignment(Qt::AlignCenter);

        securityToggle = new QPushButton("Enhanced Security Mode", this);
        securityToggle->setFlat(true);
        connect(securityToggle, &QPushButton::clicked, this, [this]() {
            if (!firstRun || enhancedLocked) return;
            enhancedSecurity = !enhancedSecurity;
            updateSecurityToggleStyle();
            saveConfigEnhancedOnly();
            if (pinModeActive) {
                buildPinPad();
                pinInput.clear();
                update();
            } else {
                update();
            }
        });

        loadConfig();

        if (firstRun) {
            securityToggle->show();
            securityToggle->setEnabled(true);
        } else {
            if (enhancedSecurity) {
                securityToggle->show();
                securityToggle->setEnabled(false);
            } else {
                securityToggle->hide();
            }
        }

        if (firstRun) {
            titleLabel->setText("Please select your combination of shapes");
        } else {
            titleLabel->setText("Enter your combination to Unlock..");
        }

        generateGrid();
        updateSecurityToggleStyle();
    }

    void setOnAuthenticated(const std::function<void()> &cb) { onAuthenticated = cb; }

    void activateInputGrab() {
        setFocus(Qt::OtherFocusReason);
        grabKeyboard();
        // IMPORTANT: do NOT grabMouse() here, or PIN buttons won't receive clicks.
    }

    void deactivateInputGrab() {
        releaseKeyboard();
    }

protected:
    void showEvent(QShowEvent *e) override {
        QWidget::showEvent(e);
        activateInputGrab();
    }

    void closeEvent(QCloseEvent *e) override { e->ignore(); }

    void resizeEvent(QResizeEvent *) override {
        securityToggle->setGeometry(10, 5, 220, 30);

        int gridTop = height() * 0.5;
        int desiredY = gridTop - 250;
        if (desiredY < 40) desiredY = 40;

        titleLabel->setGeometry(0, desiredY, width(), 40);

        if (pinWidget && pinWidget->isVisible())
            positionPinPad();
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#000000"));

        if (!pinModeActive) {
            for (const auto &s : shapes) {
                p.setBrush(s.color);
                p.setPen(QPen(Qt::white, 2));
                int x = s.rect.x(), y = s.rect.y(), w = s.rect.width(), h = s.rect.height();
                int cx = x + w / 2, cy = y + h / 2;

                if (s.shape == "circle")
                    p.drawEllipse(QRect(x, y, w, h));
                else if (s.shape == "square")
                    p.drawRect(QRect(x, y, w, h));
                else if (s.shape == "triangle") {
                    QPolygonF tri;
                    tri << QPointF(cx, y)
                        << QPointF(x, y + h)
                        << QPointF(x + w, y + h);
                    p.drawPolygon(tri);
                } else if (s.shape == "pentagon") {
                    QPolygonF pent;
                    for (int k = 0; k < 5; k++) {
                        double ang = qDegreesToRadians(72.0 * k - 90.0);
                        pent << QPointF(cx + w / 2 * qCos(ang),
                                        cy + h / 2 * qSin(ang));
                    }
                    p.drawPolygon(pent);
                }
            }
        }

        int neededDots = enhancedSecurity ? 5 : 4;
        int dotSize = 14, spacing = 20;
        int total = (dotSize + spacing) * neededDots - spacing;
        int startX = (width() - total) / 2;

        int gridTop = height() * 0.5;
        int y = gridTop - 70;
        if (y < 80) y = 80;

        if (pinModeActive) {
            for (int i = 0; i < neededDots; i++) {
                bool filled = (i < pinInput.size());
                p.setBrush(filled ? Qt::white : Qt::gray);
                p.setPen(Qt::NoPen);
                p.drawEllipse(startX + i * (dotSize + spacing), y, dotSize, dotSize);
            }
        } else {
            for (int i = 0; i < neededDots; i++) {
                bool filled = (i < currentSeq.size());
                p.setBrush(filled ? Qt::white : Qt::gray);
                p.setPen(Qt::NoPen);
                p.drawEllipse(startX + i * (dotSize + spacing), y, dotSize, dotSize);
            }
        }
    }

    void mousePressEvent(QMouseEvent *ev) override {
        if (securityToggle->isVisible() && securityToggle->geometry().contains(ev->pos())) {
            QWidget::mousePressEvent(ev);
            return;
        }

        if (pinModeActive) {
            QWidget::mousePressEvent(ev);
            return;
        }

        for (const auto &s : shapes) {
            if (s.rect.contains(ev->pos())) {
                QString key = s.shape + "-" + colorName(s.color);
                QString h = sha256(key);
                currentSeq.append(h);
                update();

                int reqShapes = requiredShapeCount();

                if (firstRun) {
                    if (currentSeq.size() == reqShapes) {
                        enhancedLocked = true;
                        securityToggle->setEnabled(false);
                        saveConfigEnhancedOnly();

                        if (!confirmingPattern) {
                            firstPatternSeq = currentSeq;
                            currentSeq.clear();
                            confirmingPattern = true;
                            titleLabel->setText("Confirm your pattern");
                            generateGrid();
                            update();
                            return;
                        } else {
                            if (currentSeq == firstPatternSeq) {
                                confirmingPattern = false;
                                startPinSetupMode();
                                return;
                            } else {
                                currentSeq.clear();
                                firstPatternSeq.clear();
                                confirmingPattern = false;
                                titleLabel->setText("Patterns did not match");
                                generateGrid();
                                update();
                                return;
                            }
                        }
                    }
                } else {
                    if (currentSeq.size() == reqShapes)
                        verifyPattern();
                }
                break;
            }
        }
    }

    void keyPressEvent(QKeyEvent *e) override {
        if ((e->key() == Qt::Key_F4 && (e->modifiers() & Qt::AltModifier)) ||
            ((e->key() == Qt::Key_W || e->key() == Qt::Key_Q) &&
             (e->modifiers() & (Qt::MetaModifier | Qt::ControlModifier))) ||
            (e->key() == Qt::Key_Tab && (e->modifiers() & Qt::AltModifier)) ||
            (e->key() == Qt::Key_Escape) ||
            ((e->key() == Qt::Key_Delete) &&
             (e->modifiers() & Qt::ControlModifier) && (e->modifiers() & Qt::AltModifier)))
        {
            e->ignore();
            return;
        }

        if (!pinModeActive) {
            e->ignore();
            return;
        }

        bool isEnhanced = enhancedSecurity;
        int req = requiredPinLength();
        int key = e->key();
        QString text = e->text();

        if (key == Qt::Key_Backspace) {
            if (!pinInput.isEmpty()) {
                pinInput.chop(1);
                update();
            }
            return;
        }

        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            submitPinEntry();
            return;
        }

        if (key >= Qt::Key_0 && key <= Qt::Key_9) {
            if (pinInput.size() < req) {
                pinInput.append(QString(QChar('0' + (key - Qt::Key_0))));
                update();
                if (pinInput.size() == req)
                    submitPinEntry();
            }
            return;
        }

        if (isEnhanced && (text == "!" || text == "?" || text == "<" || text == ">")) {
            if (pinInput.size() < req) {
                pinInput.append(text);
                update();
                if (pinInput.size() == req)
                    submitPinEntry();
            }
            return;
        }

        e->ignore();
    }

    void keyReleaseEvent(QKeyEvent *e) override { e->ignore(); }

private:
    QVector<ShapeItem> shapes;
    QVector<QString> patternHash;
    QString passwordHash;
    bool enhancedSecurity = false;

    QVector<QString> currentSeq;
    int attemptCount = 0;

    bool pinModeActive = false;
    bool pinSetupMode = false;
    bool pinSetupConfirm = false;
    QString pinInput;
    QString pinSetupFirst;

    QLabel *titleLabel = nullptr;
    QPushButton *securityToggle = nullptr;
    QWidget *pinWidget = nullptr;

    bool firstRun = false;
    bool enhancedLocked = false;

    bool confirmingPattern = false;
    QVector<QString> firstPatternSeq;

    std::function<void()> onAuthenticated;

    void loadConfig() {
        QString home = realHomePath();
        QString cfg = home + "/.config/wosp/.osm_lockdata";
        QFile file(cfg);
        if (!file.exists()) {
            firstRun = true;
            enhancedSecurity = false;
            return;
        }

        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.startsWith("pattern=")) {
                    QStringList list = line.mid(8).split(",");
                    patternHash = QVector<QString>::fromList(list);
                } else if (line.startsWith("password=")) {
                    passwordHash = line.mid(9);
                } else if (line.startsWith("enhanced=")) {
                    enhancedSecurity = (line.mid(9).trimmed() == "1");
                }
            }
            file.close();
            firstRun = false;
        }
    }

    void saveConfig() {
        QString home = realHomePath();
        QString path = home + "/.config/wosp/.osm_lockdata";
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            QStringList plist = QStringList::fromVector(patternHash);
            out << "pattern=" << plist.join(",") << "\n";
            out << "password=" << passwordHash << "\n";
            out << "enhanced=" << (enhancedSecurity ? "1" : "0") << "\n";
            f.close();
        }
    }

    void saveConfigEnhancedOnly() {
        QString home = realHomePath();
        QString path = home + "/.config/wosp/.osm_lockdata";
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out << "enhanced=" << (enhancedSecurity ? "1" : "0") << "\n";
            f.close();
        }
    }

    void updateSecurityToggleStyle() {
        if (enhancedSecurity)
            securityToggle->setStyleSheet("QPushButton { background:transparent; color:#00ff00; font-size:16px; }");
        else
            securityToggle->setStyleSheet("QPushButton { background:transparent; color:white; font-size:16px; }");
    }

    int requiredShapeCount() const { return enhancedSecurity ? 5 : 4; }
    int requiredPinLength() const { return enhancedSecurity ? 5 : 4; }

    void generateGrid() {
        shapes.clear();
        QStringList shapeList = {"circle","triangle","square","pentagon"};
        QList<QColor> colors = {Qt::red, Qt::blue, Qt::green, Qt::white};
        QVector<QPair<QString,QColor>> pool;
        for (const auto &sh : shapeList)
            for (const auto &cl : colors)
                pool.append(qMakePair(sh, cl));

        std::shuffle(pool.begin(), pool.end(), *QRandomGenerator::global());

        int size = 100, pad = 10, cols = 4, rows = 4;
        QRect screen = QApplication::primaryScreen()->geometry();
        int totalW = cols * (size + pad) - pad;
        int totalH = rows * (size + pad) - pad;
        int startX = (screen.width() - totalW) / 2;
        int startY = screen.height() / 2 + (screen.height() / 4 - totalH / 2);

        int idx = 0;
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                ShapeItem it;
                it.shape = pool[idx].first;
                it.color = pool[idx].second;
                it.rect = QRect(startX + c * (size + pad),
                                startY + r * (size + pad),
                                size, size);
                shapes.append(it);
                idx++;
            }
        }
        update();
    }

    QString colorName(const QColor &c) const {
        if (c == Qt::red) return "red";
        if (c == Qt::blue) return "blue";
        if (c == Qt::green) return "green";
        if (c == Qt::white) return "white";
        return "unknown";
    }

    void verifyPattern() {
        bool ok = (currentSeq == patternHash);
        if (ok) {
            if (onAuthenticated) onAuthenticated();
        } else {
            attemptCount++;
            currentSeq.clear();
            if (attemptCount < 3) generateGrid();
            update();
            if (attemptCount >= 3) {
                pinModeActive = true;
                pinSetupMode = false;
                pinSetupConfirm = false;
                pinInput.clear();
                titleLabel->setText("Enter Fallback PIN");
                buildPinPad();
                update();
            }
        }
    }

    void startPinSetupMode() {
        pinModeActive = true;
        pinSetupMode = true;
        pinSetupConfirm = false;
        pinInput.clear();
        titleLabel->setText("Set your Fallback PIN");
        buildPinPad();
        update();
    }

    void positionPinPad() {
        if (!pinWidget) return;
        int cols = enhancedSecurity ? 4 : 3;
        int btnSize = 90;
        int spacing = 10;
        int totalW = cols * (btnSize + spacing) - spacing;
        int x = (width() - totalW) / 2;
        int y = height() / 2 + 90;
        pinWidget->setGeometry(x, y, totalW, btnSize * 4 + spacing * 3);
    }

    void buildPinPad() {
        if (pinWidget) {
            pinWidget->deleteLater();
            pinWidget = nullptr;
        }

        pinWidget = new QWidget(this);
        QGridLayout *grid = new QGridLayout(pinWidget);
        grid->setSpacing(10);
        grid->setContentsMargins(0, 0, 0, 0);

        QStringList keys;
        if (enhancedSecurity) {
            keys << "1" << "2" << "3" << "!"
                 << "4" << "5" << "6" << "?"
                 << "7" << "8" << "9" << "<"
                 << "⌫" << "0" << "↵" << ">";
        } else {
            keys << "1" << "2" << "3"
                 << "4" << "5" << "6"
                 << "7" << "8" << "9"
                 << "⌫" << "0" << "↵";
        }

        int cols = enhancedSecurity ? 4 : 3;
        int btnSize = 90;
        for (int i = 0; i < keys.size(); i++) {
            QString label = keys[i];
            QPushButton *btn = new QPushButton(label);
            btn->setFixedSize(btnSize, btnSize);
            btn->setStyleSheet(
                "QPushButton { border:2px solid white; border-radius:45px; font-size:32px; color:white; background:#333; }"
                "QPushButton:hover { background:#555; }"
            );
            int r = i / cols;
            int c = i % cols;
            grid->addWidget(btn, r, c);
            connect(btn, &QPushButton::clicked, this, [this, label]() { handlePinPress(label); });
        }

        pinWidget->setLayout(grid);
        pinWidget->show();
        positionPinPad();
    }

    void handlePinPress(const QString &label) {
        int req = requiredPinLength();

        if (label == "⌫") {
            if (!pinInput.isEmpty()) {
                pinInput.chop(1);
                update();
            }
            return;
        }

        if (label == "↵") {
            submitPinEntry();
            return;
        }

        if (pinInput.size() < req) {
            pinInput.append(label);
            update();
            if (pinInput.size() == req)
                submitPinEntry();
        }
    }

    void submitPinEntry() {
        int req = requiredPinLength();
        if (pinInput.size() < req) return;

        if (pinSetupMode) {
            if (!pinSetupConfirm) {
                pinSetupFirst = pinInput;
                pinInput.clear();
                pinSetupConfirm = true;
                titleLabel->setText("Confirm your Fallback PIN");
                update();
            } else {
                if (pinInput == pinSetupFirst) {
                    passwordHash = sha256(pinInput);
                    patternHash = firstPatternSeq;
                    saveConfig();
                    if (onAuthenticated) onAuthenticated();
                } else {
                    pinInput.clear();
                    pinSetupFirst.clear();
                    pinSetupConfirm = false;
                    titleLabel->setText("Set your Fallback PIN");
                    update();
                }
            }
        } else {
            if (sha256(pinInput) == passwordHash) {
                if (onAuthenticated) onAuthenticated();
            } else {
                pinInput.clear();
                update();
            }
        }
    }
};

// ─────────────────────────────────────────────
// Main Container: two sibling pages + fade
// ─────────────────────────────────────────────
class WospLock : public QWidget {
public:
    WospLock() {
        setWindowFlags(Qt::FramelessWindowHint
                       | Qt::WindowStaysOnTopHint
                       | Qt::BypassWindowManagerHint);

        setWindowModality(Qt::ApplicationModal);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_ShowWithoutActivating, false);

        QScreen *scr = QGuiApplication::primaryScreen();
        setGeometry(scr->geometry());

        // IMPORTANT FUNCTIONAL CHANGE:
        // AUTH mode should NOT show LockscreenPage at all (no swipe screen).
        if (g_lockMode == LockMode::AUTH) {
            lockscreen = nullptr;
            auth = new AuthPage(this);
            auth->setGeometry(rect());
            auth->show();
            auth->raise();
            auth->setOnAuthenticated([this](){ unlockAndQuit(); });
            return;
        }

        // BOOT/SESSION behaviour unchanged: show slide screen then auth on swipe.
        lockscreen = new LockscreenPage(this);
        lockscreen->setGeometry(rect());

        auth = nullptr;      // PERF: lazy-create AuthPage on swipe
        authFx = nullptr;
        lockFx = new QGraphicsOpacityEffect(lockscreen);
        lockscreen->setGraphicsEffect(lockFx);
        lockFx->setOpacity(1.0);

        lockscreen->setOnUnlockRequested([this](){ fadeToAuth(); });
    }

protected:
    void resizeEvent(QResizeEvent *) override {
        if (lockscreen) lockscreen->setGeometry(rect());
        if (auth) auth->setGeometry(rect());
    }

    void closeEvent(QCloseEvent *e) override { e->ignore(); }

    void keyPressEvent(QKeyEvent *e) override {
        if ((e->key() == Qt::Key_F4 && (e->modifiers() & Qt::AltModifier)) ||
            ((e->key() == Qt::Key_W || e->key() == Qt::Key_Q) &&
             (e->modifiers() & (Qt::MetaModifier | Qt::ControlModifier))) ||
            (e->key() == Qt::Key_Tab && (e->modifiers() & Qt::AltModifier)) ||
            (e->key() == Qt::Key_Escape) ||
            ((e->key() == Qt::Key_Delete) &&
             (e->modifiers() & Qt::ControlModifier) && (e->modifiers() & Qt::AltModifier)))
        {
            e->ignore();
            return;
        }
        QWidget::keyPressEvent(e);
    }

private:
    LockscreenPage *lockscreen = nullptr;
    AuthPage *auth = nullptr;

    QGraphicsOpacityEffect *lockFx = nullptr;
    QGraphicsOpacityEffect *authFx = nullptr;

    bool fading = false;

    void ensureAuthPage() {
        if (auth) return;
        auth = new AuthPage(this);
        auth->setGeometry(rect());
        auth->hide();

        authFx = new QGraphicsOpacityEffect(auth);
        auth->setGraphicsEffect(authFx);
        authFx->setOpacity(1.0);

        auth->setOnAuthenticated([this](){ unlockAndQuit(); });
    }

    void fadeToAuth() {
        if (fading) return;
        fading = true;

        ensureAuthPage();

        auth->show();
        auth->raise();
        authFx->setOpacity(0.0);

        QPropertyAnimation *fadeOut = new QPropertyAnimation(lockFx, "opacity");
        QPropertyAnimation *fadeIn  = new QPropertyAnimation(authFx, "opacity");

        fadeOut->setDuration(250);
        fadeIn->setDuration(250);

        fadeOut->setStartValue(1.0);
        fadeOut->setEndValue(0.0);

        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);

        QParallelAnimationGroup *group = new QParallelAnimationGroup(this);
        group->addAnimation(fadeOut);
        group->addAnimation(fadeIn);

        connect(group, &QParallelAnimationGroup::finished, this, [this, group](){
            if (lockscreen) lockscreen->hide();
            fading = false;
            group->deleteLater();
            if (auth) auth->setFocus(Qt::OtherFocusReason);
        });

        group->start();
    }

    void unlockAndQuit() {
        if (auth) auth->deactivateInputGrab();
        if (lockscreen) lockscreen->deactivateInputGrab();

        hide();
        QApplication::processEvents();

        if (g_lockMode == LockMode::AUTH) {
            ::_exit(0);   // explicit auth success
        }

        QApplication::quit();
    }
};

// ─────────────────────────────────────────────
// main()
// ─────────────────────────────────────────────
int main(int argc, char *argv[]) {
    installSignalHardening();

    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);

    // Parse mode
    for (int i = 1; i < argc; ++i) {
        QString arg = argv[i];
        if (arg == "--boot") g_lockMode = LockMode::BOOT;
        else if (arg == "--auth") g_lockMode = LockMode::AUTH;
    }

    WospLock w;
    w.show();
    w.raise();
    w.activateWindow();
    w.showFullScreen();

    return app.exec();
}
