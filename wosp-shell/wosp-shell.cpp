// wosp-shell.cpp

#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QMouseEvent>
#include <QDirIterator>
#include <QSettings>
#include <QProcess>
#include <QFrame>
#include <QLabel>
#include <QStandardPaths>
#include <QDir>
#include <QIcon>
#include <QScroller>
#include <QScreen>
#include <QEasingCurve>
#include <QSlider>
#include <QLibrary>
#include <QTime>
#include <QTimer>
#include <QDebug>
#include <algorithm>
#include <cmath>

/* ───────────────────────── CONFIG ───────────────────────── */

static constexpr int ACTIVATION_BAR_H = 50;
static constexpr int FADE_ALPHA = 160;
static constexpr int BRIGHTNESS_TOP_MARGIN = 10;
static constexpr int BRIGHTNESS_HEIGHT = 80;
static constexpr int ACTIVATION_BAR_W = 720 / 3; // 240

/* ───────────────────────── HELPERS ───────────────────────── */

static QString imgPath(const QString &name) {
    return QStandardPaths::writableLocation(
        QStandardPaths::ConfigLocation
    ) + "/wosp-shell/images/" + name;
}

static QString cleanExec(QString s) {
    for (auto r : {"%U","%u","%F","%f","%i","%c","%k"}) s.replace(r, "");
    return s.trimmed();
}

/* ───────────────────────── APP LOADING ───────────────────────── */

struct AppEntry {
    QString name;
    QString exec;
    QString icon;
};

static QList<AppEntry> loadApps() {
    QList<AppEntry> out;
    QStringList dirs = {
        QDir::homePath() + "/.local/share/applications",
        "/usr/share/applications"
    };

    for (const QString &d : dirs) {
        QDirIterator it(d, {"*.desktop"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QSettings s(it.next(), QSettings::IniFormat);
            s.beginGroup("Desktop Entry");
            if (s.value("NoDisplay","false").toString() == "true") continue;
            out.append({
                s.value("Name").toString(),
                cleanExec(s.value("Exec").toString()),
                s.value("Icon").toString()
            });
            s.endGroup();
        }
    }

    std::sort(out.begin(), out.end(),
        [](const AppEntry &a, const AppEntry &b){
            return a.name.toLower() < b.name.toLower();
        });

    return out;
}

/* ───────────────────────── FORWARD ───────────────────────── */

class ActivationBar;
class ActivationBarTop;
class WospShell;

/* ───────────────────────── HOME BUTTON ───────────────────────── */

class HomeButton : public QLabel {
    WospShell *shell;
    QPoint pressPos;
    QPoint startPos;
    bool dragging = false;
    bool moved = false;

    QPixmap normalPix;
    QPixmap pressPix;

public:
    explicit HomeButton(WospShell *s, QWidget *p=nullptr);

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
};

/* ───────────────────────── APP TILE ───────────────────────── */

class AppTile : public QFrame {
    AppEntry entry;
    WospShell *shell;

public:
    AppTile(const AppEntry &e, WospShell *s, QWidget *parent=nullptr);

protected:
    void mousePressEvent(QMouseEvent *e) override;
};

/* ───────────────────────── WospShell ───────────────────────── */

class WospShell : public QWidget {
    QLabel *topCurve = nullptr;
    QLabel *bottomCurve = nullptr;
    HomeButton *home = nullptr;

    QPixmap topPix, bottomPix;

    QWidget *appsPage = nullptr;
    QWidget *pageLeft = nullptr;
    QWidget *pageRight = nullptr;
    QWidget *pageUp = nullptr;

    QWidget *brightnessWidget = nullptr;

    QScrollArea *scroll = nullptr;

    bool openState = false;
    bool openToUp = false;

    ActivationBar *barBottom = nullptr;
    ActivationBarTop *barTop = nullptr;

    QLibrary appsLib;
    QLibrary quickLib;

public:
    WospShell();
    ~WospShell() override = default;

    void setActivationBar(ActivationBar *b) { barBottom = b; }
    void setTopActivationBar(ActivationBarTop *t) { barTop = t; }
    void requestOpenToUp(bool v) { openToUp = v; }

    void openOverlay();
    void closeOverlayAnimated();
    void finalHide();

    void showApps();
    void showLeft();
    void showRight();
    void showUp();

protected:
    QWidget* buildAppsPage();
    QWidget* buildPlaceholder(const QString &label);
    QWidget* buildBrightness();
    QWidget* loadPageSo(const QString &soPath, QLibrary &libKeepAlive, QWidget *fallback);
    void paintEvent(QPaintEvent *) override;
};

/* ───────────────────────── ACTIVATION BARS ───────────────────────── */

class ActivationBar : public QWidget {
    WospShell *overlay;
    QPoint start;
    bool dragging = false;

public:
    explicit ActivationBar(WospShell *o) : overlay(o) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                       Qt::BypassWindowManagerHint | Qt::WindowDoesNotAcceptFocus);
        setAttribute(Qt::WA_TranslucentBackground);

        QRect g = QApplication::primaryScreen()->geometry();
        setGeometry(
            g.x() + (g.width() - ACTIVATION_BAR_W) / 2,
            g.y() + g.height() - ACTIVATION_BAR_H,
            ACTIVATION_BAR_W,
            ACTIVATION_BAR_H
        );

        show();
        raise();
    }

protected:
    void mousePressEvent(QMouseEvent *e) override {
        start = e->globalPos();
        dragging = true;
    }

    void mouseMoveEvent(QMouseEvent *e) override {
        if (!dragging) return;
        if (start.y() - e->globalY() > 20) {
            dragging = false;
            hide();
            overlay->requestOpenToUp(false);
            overlay->openOverlay();
        }
    }

    void mouseReleaseEvent(QMouseEvent *) override {
        dragging = false;
    }
};

class ActivationBarTop : public QWidget {
    WospShell *overlay;
    QPoint start;
    bool dragging = false;

public:
    explicit ActivationBarTop(WospShell *o) : overlay(o) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                       Qt::BypassWindowManagerHint | Qt::WindowDoesNotAcceptFocus);
        setAttribute(Qt::WA_TranslucentBackground);

        QRect g = QApplication::primaryScreen()->geometry();
        setGeometry(
            g.x() + (g.width() - ACTIVATION_BAR_W) / 2,
            g.y(),
            ACTIVATION_BAR_W,
            ACTIVATION_BAR_H
        );

        show();
        raise();
    }

protected:
    void mousePressEvent(QMouseEvent *e) override {
        start = e->globalPos();
        dragging = true;
    }

    void mouseMoveEvent(QMouseEvent *e) override {
        if (!dragging) return;
        if (e->globalY() - start.y() > 20) {
            dragging = false;
            hide();
            overlay->requestOpenToUp(true);
            overlay->openOverlay();
        }
    }

    void mouseReleaseEvent(QMouseEvent *) override {
        dragging = false;
    }
};

/* ───────────────────────── IMPLEMENTATIONS ───────────────────────── */

HomeButton::HomeButton(WospShell *s, QWidget *p)
    : QLabel(p), shell(s)
{
    normalPix = QPixmap(imgPath("centre.png"));
    pressPix  = QPixmap(imgPath("centre_press.png"));
    setPixmap(normalPix);
    setFixedSize(normalPix.size());
}

void HomeButton::mousePressEvent(QMouseEvent *e) {
    pressPos = e->globalPos();
    startPos = pos();
    dragging = true;
    moved = false;
    setPixmap(pressPix);
}

void HomeButton::mouseMoveEvent(QMouseEvent *e) {
    if (!dragging) return;

    QPoint delta = e->globalPos() - pressPos;
    if (!moved && delta.manhattanLength() > 6) moved = true;

    QPoint p = startPos;
    int snap = width();

    if (std::abs(delta.x()) > std::abs(delta.y()))
        p.setX(startPos.x() + std::clamp(delta.x(), -snap, snap));
    else
        p.setY(startPos.y() + std::clamp(delta.y(), -snap, snap));

    move(p);
}

void HomeButton::mouseReleaseEvent(QMouseEvent*) {
    dragging = false;
    setPixmap(normalPix);

    QPoint delta = pos() - startPos;
    move(startPos);

    int snap = width();

    if (delta.x() >= snap) shell->showRight();
    else if (delta.x() <= -snap) shell->showLeft();
    else if (delta.y() <= -snap) shell->showUp();
    else if (delta.y() >= snap) shell->showApps();
    else if (!moved) shell->closeOverlayAnimated();
}

/* ───────────────────────── AppTile ───────────────────────── */

AppTile::AppTile(const AppEntry &e, WospShell *s, QWidget *p)
    : QFrame(p), entry(e), shell(s)
{
    setStyleSheet("QFrame { background:#00000099; border-radius:20px; }");

    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(16,16,16,16);

    QLabel *icon = new QLabel;
    icon->setAlignment(Qt::AlignCenter);

    QIcon ic = QIcon::fromTheme(entry.icon);
    QPixmap pix = ic.isNull() ? QPixmap() : ic.pixmap(64,64);
    if (pix.isNull()) icon->setText("🧩");
    else icon->setPixmap(pix);

    QLabel *lbl = new QLabel(entry.name);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setWordWrap(true);
    lbl->setStyleSheet("color:white;font-size:16pt;");

    v->addWidget(icon);
    v->addWidget(lbl);
}

void AppTile::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        shell->closeOverlayAnimated();
        QStringList a = entry.exec.split(' ');
        QString p = a.takeFirst();
        QProcess::startDetached(p, a);
    }
}

/* ───────────────────────── WospShell core ───────────────────────── */

QWidget* WospShell::buildPlaceholder(const QString &label) {
    QWidget *w = new QWidget(this);
    w->setGeometry(rect());

    QVBoxLayout *v = new QVBoxLayout(w);
    v->setContentsMargins(0,0,0,0);
    v->setAlignment(Qt::AlignCenter);

    QLabel *l = new QLabel(label);
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet("color:white;font-size:28px;");

    v->addWidget(l);
    return w;
}

QWidget* WospShell::buildBrightness() {
    QWidget *w = new QWidget(this);
    w->setAttribute(Qt::WA_TranslucentBackground);

    QVBoxLayout *v = new QVBoxLayout(w);
    v->setContentsMargins(40, 10, 40, 10);

    QLabel *lbl = new QLabel;
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet("color:white;font-size:20pt;");

    auto updateClock = [lbl]() {
        lbl->setText(QTime::currentTime().toString("HH:mm"));
    };

    updateClock();

    QTimer *timer = new QTimer(lbl);
    QObject::connect(timer, &QTimer::timeout, lbl, updateClock);
    timer->start(1000);

    int savedBrightness = 80;
    {
        QSettings s("Alternix", "wosp-shell");
        savedBrightness = s.value("brightness", 80).toInt();
    }

    QSlider *s = new QSlider(Qt::Horizontal);
    s->setRange(20, 100);
    s->setValue(savedBrightness);
    s->setFixedHeight(32);
    s->setAttribute(Qt::WA_StyledBackground, true);

    s->setStyleSheet(
        "QSlider::groove:horizontal { height: 12px; background: #505050; border-radius: 6px; }"
        "QSlider::handle:horizontal { width: 32px; height: 32px; "
        " background-color:#ffffff; border-radius: 16px; margin: -10px 0; "
        " outline:none; border:0px solid transparent; }"
        "QSlider::handle:horizontal:hover { background-color: #3a3a3a; border-radius: 16px; "
        " outline:none; border:0px solid transparent; }"
    );

    QObject::connect(s, &QSlider::valueChanged, this, [](int v){
        {
            QSettings s("Alternix", "wosp-shell");
            s.setValue("brightness", v);
        }

        double f = v / 100.0;
        QProcess::startDetached(
            "bash",
            {"-c", QString("xrandr --output $(xrandr | awk '/ primary/{print $1; exit}') --brightness %1").arg(f)}
        );
    });

    v->addWidget(lbl);
    v->addWidget(s);
    return w;
}

QWidget* WospShell::loadPageSo(const QString &soPath, QLibrary &libKeepAlive, QWidget *fallback) {
    libKeepAlive.setFileName(soPath);
    if (libKeepAlive.load()) {
        auto factory = reinterpret_cast<QWidget*(*)(QWidget*)>(libKeepAlive.resolve("make_page"));
        if (factory) {
            QWidget *p = factory(this);
            if (p) return p;
        }
    }
    return fallback;
}

WospShell::WospShell() {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    QRect g = QApplication::primaryScreen()->geometry();
    setGeometry(g);

    topPix    = QPixmap(imgPath("top_curve.png"));
    bottomPix = QPixmap(imgPath("bottom_curve.png"));

    topCurve = new QLabel(this);
    bottomCurve = new QLabel(this);
    home = new HomeButton(this, this);

    topCurve->setPixmap(topPix);
    bottomCurve->setPixmap(bottomPix);

    brightnessWidget = buildBrightness();
    brightnessWidget->hide();

    QWidget *appsFallback = buildAppsPage();
    appsFallback->hide();

    QWidget *quickFallback = buildPlaceholder("Quick Settings is under construction");
    quickFallback->hide();

    QWidget *leftFallback = buildPlaceholder("Left Page is under construction");
    leftFallback->hide();

    QWidget *rightFallback = buildPlaceholder("Right Page is under construction");
    rightFallback->hide();

    appsPage = loadPageSo("/usr/local/bin/launcher.so", appsLib, appsFallback);
    pageUp   = loadPageSo("/usr/local/bin/quicksettings.so", quickLib, quickFallback);
    pageLeft = loadPageSo("/usr/local/bin/pageLeft.so", leftLib, leftFallback);
    pageRight = loadPageSo("/usr/local/bin/pageRight.so", rightLib, rightFallback);

    

    #pageLeft  = buildPlaceholder("Left Page is under construction");
    #pageRight = buildPlaceholder("Right Page is under construction");

    appsPage->hide();
    pageLeft->hide();
    pageRight->hide();
    pageUp->hide();
    home->hide();

    hide();
}

QWidget* WospShell::buildAppsPage() {
    QWidget *w = new QWidget(this);
    w->setGeometry(0, 180, width(), height() - 300);

    scroll = new QScrollArea(w);
    scroll->setGeometry(w->rect());
    scroll->setStyleSheet("border:none;background:transparent;");
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget *container = new QWidget;
    QGridLayout *grid = new QGridLayout(container);
    grid->setSpacing(12);
    grid->setContentsMargins(20,20,20,20);

    auto apps = loadApps();
    for (int i = 0; i < apps.size(); ++i)
        grid->addWidget(new AppTile(apps[i], this), i/4, i%4);

    scroll->setWidget(container);
    scroll->setWidgetResizable(true);

    QScroller::grabGesture(scroll->viewport(), QScroller::TouchGesture);
    QScroller::grabGesture(scroll->viewport(), QScroller::LeftMouseButtonGesture);

    return w;
}

void WospShell::showApps() {
    pageLeft->hide();
    pageRight->hide();
    pageUp->hide();
    appsPage->show();
}

void WospShell::showLeft() {
    appsPage->hide();
    pageRight->hide();
    pageUp->hide();
    pageLeft->show();
}

void WospShell::showRight() {
    appsPage->hide();
    pageLeft->hide();
    pageUp->hide();
    pageRight->show();
}

void WospShell::showUp() {
    appsPage->hide();
    pageLeft->hide();
    pageRight->hide();
    pageUp->show();
}

void WospShell::openOverlay() {
    if (openState) return;
    openState = true;

    if (barBottom) barBottom->hide();
    if (barTop)    barTop->hide();

    showFullScreen();
    raise();

    topCurve->move(0, -topPix.height());
    bottomCurve->move(0, height());
    home->hide();
    brightnessWidget->hide();

    auto *aTop = new QPropertyAnimation(topCurve, "pos");
    auto *aBot = new QPropertyAnimation(bottomCurve, "pos");

    aTop->setEndValue(QPoint(0, 0));
    aBot->setEndValue(QPoint(0, height() - bottomPix.height()));
    aTop->setDuration(300);
    aBot->setDuration(300);

    auto *grp = new QParallelAnimationGroup(this);
    grp->addAnimation(aTop);
    grp->addAnimation(aBot);

    connect(grp, &QParallelAnimationGroup::finished, this, [this] {

        brightnessWidget->setGeometry(
            0,
            BRIGHTNESS_TOP_MARGIN,
            width(),
            BRIGHTNESS_HEIGHT
        );
        brightnessWidget->show();
        brightnessWidget->raise();

        int by = height() - bottomPix.height();
        int hy = by + (bottomPix.height() / 2) - (home->height() / 2);
        home->move(width() / 2 - home->width() / 2, hy);
        home->show();
        home->raise();

        if (openToUp) showUp();
        else showApps();

        openToUp = false;
    });

    grp->start(QAbstractAnimation::DeleteWhenStopped);
}

void WospShell::closeOverlayAnimated() {
    if (!openState) return;
    openState = false;

    appsPage->hide();
    pageLeft->hide();
    pageRight->hide();
    pageUp->hide();
    home->hide();
    brightnessWidget->hide();

    auto *aTop = new QPropertyAnimation(topCurve, "pos");
    auto *aBot = new QPropertyAnimation(bottomCurve, "pos");

    aTop->setEndValue(QPoint(0, -topPix.height()));
    aBot->setEndValue(QPoint(0, height()));
    aTop->setDuration(250);
    aBot->setDuration(250);

    auto *grp = new QParallelAnimationGroup(this);
    grp->addAnimation(aTop);
    grp->addAnimation(aBot);

    connect(grp, &QParallelAnimationGroup::finished, this, [this] {
        finalHide();
    });

    grp->start(QAbstractAnimation::DeleteWhenStopped);
}

void WospShell::finalHide() {
    hide();
    if (barBottom) {
        barBottom->show();
        barBottom->raise();
    }
    if (barTop) {
        barTop->show();
        barTop->raise();
    }
}

void WospShell::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor(0,0,0,FADE_ALPHA));
}

/* ───────────────────────── MAIN ───────────────────────── */

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    WospShell shell;
    ActivationBar bar(&shell);
    ActivationBarTop topBar(&shell);
    shell.setActivationBar(&bar);
    shell.setTopActivationBar(&topBar);
    return app.exec();
}
