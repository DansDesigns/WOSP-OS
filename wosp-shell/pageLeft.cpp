// history.cpp — FULL FILE
// NO moc, NO Q_OBJECT, make_page preserved

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QLineEdit>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QXmlStreamReader>
#include <QMouseEvent>
#include <QStringList>
#include <functional>

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

/* ───────────────────────── Style helpers ───────────────────────── */

static QLabel* sectionLabel(const QString &t) {
    QLabel *l = new QLabel(t);
    l->setStyleSheet(
        "background:transparent;"
        "color:white;"
        "font-size:22px;"
        "padding:6px 6px;"
    );
    return l;
}

static QFrame* dividerLine() {
    QFrame *f = new QFrame;
    f->setFixedHeight(2);
    f->setStyleSheet("background:rgba(255,255,255,150); border:none;");
    return f;
}

static QPushButton* pillButton(const QString &t) {
    QPushButton *b = new QPushButton(t);
    b->setFixedHeight(34);
    b->setStyleSheet(
        "QPushButton{"
        " background:rgba(160,160,160,170);"
        " border:none;"
        " border-radius:17px;"
        " color:white;"
        " font-size:16px;"
        " padding:0px 12px;"
        "}"
        "QPushButton:pressed{"
        " background:rgba(190,190,190,190);"
        "}"
    );
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

static QString niceNameForUrl(const QUrl &u) {
    if (u.isEmpty()) return "";
    if (u.scheme().startsWith("http")) {
        QString host = u.host();
        QString path = u.path();
        if (path.size() > 18) path = path.left(18) + "…";
        if (!host.isEmpty() && !path.isEmpty() && path != "/") return host + path;
        if (!host.isEmpty()) return host;
        return u.toString().left(22) + "…";
    }

    if (u.isLocalFile()) {
        QFileInfo fi(u.toLocalFile());
        return fi.fileName().isEmpty() ? fi.absoluteFilePath() : fi.fileName();
    }

    return u.toString();
}

/* ───────────────────────── Recents loader (XBEL) ───────────────────────── */

static QList<QUrl> loadRecentXbel(int maxItems = 50) {
    QList<QUrl> out;

    QString path = QDir::homePath() + "/.local/share/recently-used.xbel";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return out;

    QXmlStreamReader xr(&f);
    while (!xr.atEnd()) {
        xr.readNext();
        if (xr.isStartElement() && xr.name() == "bookmark") {
            auto attrs = xr.attributes();
            if (attrs.hasAttribute("href")) {
                QString href = attrs.value("href").toString();
                QUrl u(href);
                if (u.isValid()) out.append(u);
                if (out.size() >= maxItems) break;
            }
        }
    }
    return out;
}

static bool extIn(const QString &ext, const QStringList &set) {
    QString e = ext.toLower();
    return set.contains(e);
}

static QString categoryForUrl(const QUrl &u) {
    if (!u.isValid()) return "System";

    // URLs = Comms-ish
    if (u.scheme().startsWith("http")) return "Comms";

    // Local file heuristics
    if (u.isLocalFile()) {
        QString p = u.toLocalFile();
        QFileInfo fi(p);
        QString ext = fi.suffix().toLower();

        // documents bucket
        static QStringList docExt = {
            "pdf","txt","md","rtf","doc","docx","odt","ppt","pptx","xls","xlsx","csv"
        };

        // system-ish bucket
        static QStringList sysExt = {
            "log","conf","ini","json","yaml","yml","service","desktop","sh","bashrc","zshrc"
        };

        if (extIn(ext, docExt)) return "Documents";
        if (extIn(ext, sysExt)) return "System";

        // Paths that scream "system"
        if (p.startsWith("/etc/") || p.startsWith("/var/") || p.contains("/.config/"))
            return "System";

        // default
        return "Documents";
    }

    return "System";
}

/* ───────────────────────── Section widget ───────────────────────── */

static QWidget* makeSection(const QString &title, const QList<QUrl> &items, int takeN = 4) {
    QWidget *w = new QWidget;
    w->setAttribute(Qt::WA_TranslucentBackground);
    w->setStyleSheet("background:transparent;");

    QVBoxLayout *v = new QVBoxLayout(w);
    v->setContentsMargins(0,0,0,0);
    v->setSpacing(8);

    v->addWidget(sectionLabel(title));
    v->addWidget(dividerLine());

    QHBoxLayout *row = new QHBoxLayout;
    row->setContentsMargins(0,0,0,0);
    row->setSpacing(12);

    int added = 0;
    for (const QUrl &u : items) {
        if (added >= takeN) break;
        QString label = niceNameForUrl(u);
        if (label.isEmpty()) continue;

        // keep pills compact
        if (label.size() > 20) label = label.left(20) + "…";

        QPushButton *b = pillButton(label);

        QObject::connect(b, &QPushButton::clicked, b, [=]() {
            QString target = u.toString();
            if (u.isLocalFile()) target = u.toLocalFile();
            QProcess::startDetached("xdg-open", { target });
        });

        row->addWidget(b);
        added++;
    }

    // placeholders if empty
    while (added < takeN) {
        QPushButton *b = pillButton(" ");
        b->setEnabled(false);
        b->setStyleSheet(
            "QPushButton{"
            " background:rgba(160,160,160,120);"
            " border:none;"
            " border-radius:17px;"
            " color:transparent;"
            "}"
        );
        row->addWidget(b);
        added++;
    }

    row->addStretch();
    v->addLayout(row);
    return w;
}

/* ───────────────────────── Entry point ───────────────────────── */

extern "C"
QWidget* make_page(QWidget *parent) {

    QWidget *root = new QWidget(parent);
    root->setAttribute(Qt::WA_TranslucentBackground);
    root->setStyleSheet("background:transparent;");

    // Column matches your quicksettings sizing approach
    QWidget *column = new QWidget(root);
    column->setAttribute(Qt::WA_TranslucentBackground);
    column->setStyleSheet("background:transparent;");
    column->setFixedWidth(560);

    QVBoxLayout *pv = new QVBoxLayout(column);
    pv->setSpacing(22);
    pv->setContentsMargins(20,20,20,20);

    // Search bar
    QLineEdit *search = new QLineEdit;
    search->setPlaceholderText("Type your intent…");
    search->setFixedHeight(34);
    search->setStyleSheet(
        "QLineEdit{"
        " background:rgba(255,255,255,60);"
        " border:1px solid rgba(255,255,255,120);"
        " border-radius:10px;"
        " color:white;"
        " font-size:16px;"
        " padding-left:10px;"
        " padding-right:10px;"
        "}"
        "QLineEdit:focus{"
        " border:1px solid rgba(255,255,255,170);"
        "}"
    );

    QObject::connect(search, &QLineEdit::returnPressed, search, [=]() {
        QString q = search->text().trimmed();
        if (q.isEmpty()) {
            QProcess::startDetached("xdg-open", { "https://google.com" });
        } else {
            QString url = "https://www.google.com/search?q=" + QUrl::toPercentEncoding(q);
            QProcess::startDetached("xdg-open", { url });
        }
    });

    pv->addWidget(search);

    // Load and split recents
    QList<QUrl> recents = loadRecentXbel(80);

    QList<QUrl> comms, docs, sys;
    for (const QUrl &u : recents) {
        QString c = categoryForUrl(u);
        if (c == "Comms") comms.append(u);
        else if (c == "Documents") docs.append(u);
        else sys.append(u);
    }

    pv->addSpacing(8);
    pv->addWidget(makeSection("Comms", comms, 4));
    pv->addWidget(makeSection("Documents", docs, 4));
    pv->addWidget(makeSection("System", sys, 4));
    pv->addStretch();

    // Center the column like quicksettings page
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
