
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QProcess>
#include <QDir>
#include <QMouseEvent>
#include <QTimer>
#include <functional>

/* ───────────────────────── Paths ───────────────────────── */

static QString imgPath(const QString &n) {
    return QDir::homePath() + "/.config/wosp-shell/images/" + n;
}

/* ───────────────────────── Clickable row ───────────────────────── */

class ClickRow : public QWidget {
public:
    std::function<void()> onClick;

    ClickRow() {
        setAttribute(Qt::WA_TranslucentBackground);
        setStyleSheet("background:transparent;");
    }

protected:
    void mousePressEvent(QMouseEvent*) override {
        if (onClick) onClick();
    }
};

/* ───────────────────────── Toggle Light ───────────────────────── */

class ToggleLight : public QLabel {
public:
    enum State { On, Off, Disabled };
    State state{Disabled};
    std::function<void()> onClick;

    ToggleLight() {
        setFixedSize(72,36);
        setAlignment(Qt::AlignCenter);
        setStyleSheet("background:transparent;border-radius:18px;");
        setState(Disabled);
    }

    void setState(State s) {
        state = s;
        QString fn =
            (s == On)  ? "on.png" :
            (s == Off) ? "off.png" : "disabled.png";

        QPixmap px(imgPath(fn));
        setPixmap(px.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

protected:
    void mousePressEvent(QMouseEvent*) override {
        if (onClick) onClick();
    }
};

/* ─────────────────────────  info label (NO background) ───────────────────────── */

static QLabel* infoLabel(const QString &t) {
    QLabel *l = new QLabel(t);
    l->setStyleSheet(
        "background:transparent;"
        "color:white;"
        "font-size:20px;"
        "line-height:26px;"
        "padding:6px 6px;"
    );
    return l;
}

/* ───────────────────────── Dropdown item label (rounded pill) ───────────────────────── */

static QLabel* dropItemLabel(const QString &t) {
    QLabel *l = new QLabel(t);
    l->setStyleSheet(
        "background:rgba(255,255,255,30);"
        "border-radius:18px;"
        "color:white;"
        "font-size:20px;"
        "line-height:26px;"
        "padding:10px 12px;"
    );
    return l;
}

/* ───────────────────────── Dropdown panel ───────────────────────── */

class DropPanel : public QFrame {
    QWidget *body;
    bool expanded{false};

public:
    DropPanel(QWidget *b) : body(b) {
        QVBoxLayout *l = new QVBoxLayout(this);
        l->setContentsMargins(14,14,14,14);
        l->addWidget(body);
        body->setVisible(false);

        setStyleSheet(
            "background:rgba(80,80,80,180);"
            "border-radius:22px;"
            "border:1px solid rgba(255,255,255,60);"
        );
    }

    void toggle() {
        expanded = !expanded;
        body->setVisible(expanded);
        updateGeometry();
    }
};

/* ───────────────────────── Card setup ───────────────────────── */

static QWidget* makeCard(
    const QString &title,
    ToggleLight *toggle,
    QLabel *summary,
    QWidget *dropBody
) {
    QFrame *card = new QFrame;
    card->setStyleSheet(
        "background:rgba(140,135,125,190);"
        "border-radius:30px;"
    );

    QVBoxLayout *v = new QVBoxLayout(card);
    v->setContentsMargins(22,18,22,22);
    v->setSpacing(14);

    ClickRow *header = new ClickRow;
    QHBoxLayout *h = new QHBoxLayout(header);
    h->setContentsMargins(0,0,0,0);

    QLabel *lbl = new QLabel(title);
    lbl->setStyleSheet(
        "background:transparent;"
        "color:white;"
        "font-size:36px;"
    );

    h->addWidget(lbl);
    h->addStretch();
    h->addWidget(toggle);

    DropPanel *drop = new DropPanel(dropBody);
    header->onClick = [=]() { drop->toggle(); };

    ClickRow *summaryRow = new ClickRow;
    QHBoxLayout *sr = new QHBoxLayout(summaryRow);
    sr->setContentsMargins(0,0,0,0);
    sr->addWidget(summary);
    summaryRow->onClick = [=]() { drop->toggle(); };

    v->addWidget(header);
    v->addWidget(summaryRow);
    v->addWidget(drop);

    return card;
}

/* ───────────────────────── Wi-Fi card ───────────────────────── */

static QWidget* wifiCard() {
    QWidget *body = new QWidget;
    body->setAttribute(Qt::WA_TranslucentBackground);
    body->setStyleSheet("background:transparent;");
    QVBoxLayout *v = new QVBoxLayout(body);
    v->setSpacing(12);

    QLabel *ssid = dropItemLabel("SSID:");
    QLabel *ip   = dropItemLabel("IP Address:");
    QLabel *dns  = dropItemLabel("DNS Server:");

    QPushButton *scan = new QPushButton("SCAN");
    scan->setStyleSheet(
        "background:#3cff3c;"
        "border-radius:18px;"
        "border:none;"
        "background-clip:padding;"
        "padding:12px;"
        "font-size:20px;"
    );

    v->addWidget(ssid);
    v->addWidget(ip);
    v->addWidget(dns);
    v->addWidget(scan);

    QLabel *summary = infoLabel("SSID:");
    ToggleLight *t = new ToggleLight;

    auto refresh = [=]() {
        QProcess p;

        p.start("nmcli", {"-t","-f","ACTIVE,SSID","dev","wifi"});
        p.waitForFinished();
        QString ss;
        for (const QString &l : QString::fromUtf8(p.readAll()).split('\n')) {
            if (l.startsWith("yes:")) ss = l.mid(4);
        }

        p.start("nmcli", {"-g","IP4.ADDRESS","dev","show"});
        p.waitForFinished();
        QString ipaddr = QString::fromUtf8(p.readAll()).split('\n').value(0);

        p.start("nmcli", {"-g","IP4.DNS","dev","show"});
        p.waitForFinished();
        QString dnsaddr = QString::fromUtf8(p.readAll()).split('\n').value(0);

        ssid->setText("SSID: " + ss);
        ip->setText("IP Address: " + ipaddr);
        dns->setText("DNS Server: " + dnsaddr);
        summary->setText("SSID: " + ss);

        p.start("nmcli", {"radio","wifi"});
        p.waitForFinished();
        bool on = QString::fromUtf8(p.readAll()).trimmed() == "enabled";
        t->setState(on ? ToggleLight::On : ToggleLight::Off);
    };

    t->onClick = [=]() {
        bool on = (t->state == ToggleLight::On);
        QProcess::startDetached("nmcli", {"radio","wifi", on ? "off" : "on"});
        QTimer::singleShot(400, refresh);
    };

    QObject::connect(scan, &QPushButton::clicked, scan, refresh);

    refresh();
    return makeCard("WIFI", t, summary, body);
}

/* ───────────────────────── Bluetooth card ───────────────────────── */

static QWidget* btCard() {
    QWidget *body = new QWidget;
    body->setAttribute(Qt::WA_TranslucentBackground);
    body->setStyleSheet("background:transparent;");
    QVBoxLayout *v = new QVBoxLayout(body);
    v->setSpacing(12);

    QLabel *label = dropItemLabel("Devices:");
    QLabel *list  = dropItemLabel("");

    QPushButton *scan = new QPushButton("SCAN");
    scan->setStyleSheet(
        "background:#3cff3c;"
        "border-radius:18px;"
        "border:none;"
        "background-clip:padding;"
        "padding:12px;"
        "font-size:20px;"
    );

    v->addWidget(label);
    v->addWidget(list);
    v->addWidget(scan);

    QLabel *summary = infoLabel("Connected:");
    ToggleLight *t = new ToggleLight;

    auto refreshBt = [=]() {
        QProcess p;
        p.start("bluetoothctl", {"show"});
        p.waitForFinished();
        bool on = QString::fromUtf8(p.readAll()).contains("Powered: yes");
        t->setState(on ? ToggleLight::On : ToggleLight::Off);
    };

    t->onClick = [=]() {
        bool on = (t->state == ToggleLight::On);
        QProcess::startDetached("bluetoothctl", {"power", on ? "off" : "on"});
        QTimer::singleShot(400, refreshBt);
    };

    QObject::connect(scan, &QPushButton::clicked, scan, [=]() {
        QProcess::startDetached("bluetoothctl", {"scan","on"});
        QTimer::singleShot(2000, [=]() {
            QProcess p;
            p.start("bluetoothctl", {"devices"});
            p.waitForFinished();
            list->setText(QString::fromUtf8(p.readAll()).trimmed());
            refreshBt();
        });
    });

    refreshBt();
    return makeCard("Bluetooth", t, summary, body);
}

/* ───────────────────────── Simple cards (place holders) ───────────────────────── */

static QWidget* simpleCard(const QString &title, const QString &text) {
    QLabel *summary = infoLabel(text);

    QWidget *body = new QWidget;
    body->setAttribute(Qt::WA_TranslucentBackground);
    body->setStyleSheet("background:transparent;");
    QVBoxLayout *v = new QVBoxLayout(body);
    v->setSpacing(12);

    v->addWidget(dropItemLabel(text));

    ToggleLight *t = new ToggleLight;
    t->setState(ToggleLight::Disabled);

    return makeCard(title, t, summary, body);
}

/* ───────────────────────── Entry point ───────────────────────── */

extern "C"
QWidget* make_page(QWidget *parent) {

    QWidget *root = new QWidget(parent);
    root->setAttribute(Qt::WA_TranslucentBackground);
    root->setStyleSheet("background:transparent;");

    QWidget *column = new QWidget(root);
    column->setAttribute(Qt::WA_TranslucentBackground);
    column->setStyleSheet("background:transparent;");
    column->setFixedWidth(560); // +1/3 width

    QVBoxLayout *pv = new QVBoxLayout(column);
    pv->setSpacing(24);
    pv->setContentsMargins(20,20,20,20);

    pv->addWidget(wifiCard());
    pv->addWidget(btCard());
    pv->addWidget(simpleCard("GPS","Visible Satellites"));
    pv->addWidget(simpleCard("Mobile Data","Visible Stations"));
    pv->addWidget(simpleCard("Battery Saver","Battery %, Time Remaining"));
    pv->addStretch();

    QHBoxLayout *center = new QHBoxLayout(root);
    center->setContentsMargins(0,230,0,0);
    center->addStretch();
    center->addWidget(column);
    center->addStretch();

    root->setLayout(center);
    root->setGeometry(parent->rect());
    root->lower();
    root->show();
    return root;
}
