#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QScreen>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include <cmath>

// ------------------------------------------------------------
// Globals
// ------------------------------------------------------------
static Display* dpy = nullptr;
static QWidget* keyboard = nullptr;

// ------------------------------------------------------------
// Send key
// ------------------------------------------------------------
void sendKey(KeySym sym) {
    if (!dpy) return;
    KeyCode kc = XKeysymToKeycode(dpy, sym);
    if (!kc) return;
    XTestFakeKeyEvent(dpy, kc, True, 0);
    XTestFakeKeyEvent(dpy, kc, False, 0);
    XFlush(dpy);
}

// ------------------------------------------------------------
// Spacebar
// ------------------------------------------------------------
class Spacebar : public QWidget {
    QPoint start;
    bool moved = false;

public:
    Spacebar() {
        setFixedHeight(70);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    void mousePressEvent(QMouseEvent *e) override {
        start = e->pos();
        moved = false;
    }

    void mouseMoveEvent(QMouseEvent *e) override {
        int dx = e->pos().x() - start.x();
        if (std::abs(dx) < 10) return;

        moved = true;
        int steps = dx / 14;
        if (!steps) return;

        KeySym sym = steps > 0 ? XK_Right : XK_Left;
        for (int i = 0; i < std::abs(steps); ++i)
            sendKey(sym);

        start = e->pos();
    }

    void mouseReleaseEvent(QMouseEvent*) override {
        if (!moved)
            sendKey(XK_space);
    }
};

// ------------------------------------------------------------
// Key
// ------------------------------------------------------------
class Key : public QLabel {
    KeySym sym;

public:
    Key(const char* s) {
        sym = XStringToKeysym(s);
        setText(s);
        setAlignment(Qt::AlignCenter);
        setFixedHeight(56);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setStyleSheet("background:#404040;border-radius:8px;font-size:20px;");
    }

protected:
    void mousePressEvent(QMouseEvent*) override {
        sendKey(sym);
    }
};

// ------------------------------------------------------------
// Keyboard window (created / destroyed)
// ------------------------------------------------------------
class KeyboardWindow : public QWidget {
    QPoint swipeStart;
    bool consumed = false;

public:
    KeyboardWindow() {
        setWindowFlags(Qt::FramelessWindowHint);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::NoFocus);

        QVBoxLayout* root = new QVBoxLayout(this);
        root->setContentsMargins(6,6,6,6);
        root->setSpacing(6);

        auto makeRow = [&](std::initializer_list<const char*> keys) {
            QHBoxLayout* row = new QHBoxLayout;
            for (auto k : keys)
                row->addWidget(new Key(k));
            root->addLayout(row);
        };

        makeRow({"Q","W","E","R","T","Y","U","I","O","P"});
        makeRow({"A","S","D","F","G","H","J","K","L"});
        makeRow({"Z","X","C","V","B","N","M"});
        root->addWidget(new Spacebar);

        QRect s = screen()->geometry();
        resize(s.width(), 280);
        move(0, s.height() - height());

        show();
        applyDockHints();
        applyStrut();
    }

    ~KeyboardWindow() {
        clearStrut();
    }

    void applyDockHints() {
        Atom dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
        Atom type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        XChangeProperty(dpy, winId(), type, XA_ATOM, 32,
                        PropModeReplace, (unsigned char*)&dock, 1);
    }

    void applyStrut() {
        long strut[12] = {};
        strut[3] = height();
        strut[9] = screen()->geometry().width();

        Atom p = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
        Atom f = XInternAtom(dpy, "_NET_WM_STRUT", False);

        XChangeProperty(dpy, winId(), p, XA_CARDINAL, 32,
                        PropModeReplace, (unsigned char*)strut, 12);
        XChangeProperty(dpy, winId(), f, XA_CARDINAL, 32,
                        PropModeReplace, (unsigned char*)strut, 4);
        XFlush(dpy);
    }

    void clearStrut() {
        Atom p = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
        Atom f = XInternAtom(dpy, "_NET_WM_STRUT", False);
        XDeleteProperty(dpy, winId(), p);
        XDeleteProperty(dpy, winId(), f);
        XFlush(dpy);
    }

protected:
    void mousePressEvent(QMouseEvent* e) override {
        swipeStart = e->pos();
        consumed = false;
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (consumed) return;
        if (e->pos().y() - swipeStart.y() > 90) {
            consumed = true;
            deleteLater();
            keyboard = nullptr;
        }
    }
};

// ------------------------------------------------------------
// Activation zone (RIGHTMOST 1/3 of 720px = 240px)
// ------------------------------------------------------------
class ActivationZone : public QWidget {
    QPoint start;

public:
    ActivationZone() {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoSystemBackground);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::NoFocus);

        QRect s = screen()->geometry();

        const int totalWidth = 720;
        const int third = totalWidth / 3; // 240

        resize(third, 60);
        move(
            s.x() + (s.width() - totalWidth) / 2 + (2 * third),
            s.y() + s.height() - height()
        );

        show();
    }

protected:
    void mousePressEvent(QMouseEvent* e) override {
        start = e->pos();
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (keyboard) return;
        if (start.y() - e->pos().y() > 40) {
            keyboard = new KeyboardWindow;
        }
    }
};

// ------------------------------------------------------------
// main
// ------------------------------------------------------------
int main(int argc, char** argv) {
    QApplication app(argc, argv);

    dpy = XOpenDisplay(nullptr);
    if (!dpy) return 1;

    ActivationZone zone;
    return app.exec();
}
