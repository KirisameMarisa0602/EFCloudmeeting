#include "regist.h"
#include "ui_regist.h"
#include "login.h"

#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMessageBox>
#include <QHostAddress>
#include <QComboBox>
#include <QLineEdit>
#include <QRegularExpression>
#include <QStyle>
#include <QPushButton>
#include <QGraphicsOpacityEffect>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QToolButton>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

// 悬停动效（与 Login 同风格）
class HoverFilter_R : public QObject {
public:
    explicit HoverFilter_R(QPushButton* btn) : QObject(btn), btn_(btn) {
        auto *eff = new QGraphicsDropShadowEffect(btn_);
        eff->setBlurRadius(8);
        eff->setOffset(0, 2);
        eff->setColor(QColor(0, 0, 0, 60));
        btn_->setGraphicsEffect(eff);
        effect_ = eff;

        animLift_ = new QVariantAnimation(this);
        animLift_->setDuration(140);
        animLift_->setEasingCurve(QEasingCurve::OutCubic);
        connect(animLift_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v){
            effect_->setOffset(0, v.toReal());
        });

        animBlur_ = new QVariantAnimation(this);
        animBlur_->setDuration(140);
        animBlur_->setEasingCurve(QEasingCurve::OutCubic);
        connect(animBlur_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v){
            effect_->setBlurRadius(v.toReal());
        });
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == btn_) {
            if (event->type() == QEvent::Enter) {
                animLift_->stop();
                animLift_->setStartValue(effect_->offset().y());
                animLift_->setEndValue(5.0);
                animLift_->start();

                animBlur_->stop();
                animBlur_->setStartValue(effect_->blurRadius());
                animBlur_->setEndValue(18.0);
                animBlur_->start();
            } else if (event->type() == QEvent::Leave) {
                animLift_->stop();
                animLift_->setStartValue(effect_->offset().y());
                animLift_->setEndValue(2.0);
                animLift_->start();

                animBlur_->stop();
                animBlur_->setStartValue(effect_->blurRadius());
                animBlur_->setEndValue(8.0);
                animBlur_->start();
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QPushButton* btn_{nullptr};
    QGraphicsDropShadowEffect* effect_{nullptr};
    QVariantAnimation* animLift_{nullptr};
    QVariantAnimation* animBlur_{nullptr};
};

// 与 Login 中一致的“密码小眼睛”工具
class PasswordEyeHelper_R : public QObject {
public:
    explicit PasswordEyeHelper_R(QLineEdit* edit)
        : QObject(edit), edit_(edit)
    {
        btn_ = new QToolButton(edit_);
        btn_->setCursor(Qt::PointingHandCursor);
        btn_->setCheckable(true);
        btn_->setAutoRaise(true);
        btn_->setToolTip(QStringLiteral("显示/隐藏密码"));
        btn_->setFocusPolicy(Qt::NoFocus);
        btn_->setIconSize(QSize(18, 18));

        btn_->setChecked(false);
        btn_->setIcon(makeEyeIcon(false));

        edit_->setStyleSheet(edit_->styleSheet() + " QLineEdit{ padding-right: 34px; }");

        connect(btn_, &QToolButton::toggled, this, [this](bool on){
            edit_->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
            btn_->setIcon(makeEyeIcon(on));
        });

        edit_->installEventFilter(this);
        reposition();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == edit_) {
            if (event->type() == QEvent::Resize || event->type() == QEvent::StyleChange) {
                reposition();
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void reposition() {
        const int m = 6;
        const int h = edit_->height();
        const int side = qMin(22, h - m - m);
        btn_->setIconSize(QSize(side - 4, side - 4));
        btn_->setGeometry(edit_->rect().right() - side - m, (h - side)/2, side, side);
    }

    static QIcon makeEyeIcon(bool open) {
        const int D = 64;
        QPixmap pm(D, D);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);

        QPainterPath eye;
        QRectF r(8, 18, 48, 28);
        eye.moveTo(r.center().x() - r.width()/2, r.center().y());
        eye.cubicTo(r.left()+8, r.top()-8, r.right()-8, r.top()-8, r.right(), r.center().y());
        eye.cubicTo(r.right()-8, r.bottom()+8, r.left()+8, r.bottom()+8, r.left(), r.center().y());

        QPen pen(QColor(60,60,60));
        pen.setWidthF(4.0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(eye);

        if (open) {
            p.setBrush(QColor(60,60,60));
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(D/2.0, r.center().y()), 6.5, 6.5);
        } else {
            QPen pen2(QColor(60,60,60));
            pen2.setWidthF(4.0);
            p.setPen(pen2);
            p.drawLine(QPointF(14, 50), QPointF(50, 14));
        }
        p.end();
        return QIcon(pm);
    }

    QLineEdit* edit_;
    QToolButton* btn_;
};

static const char* SERVER_HOST = "127.0.0.1";
static const quint16 SERVER_PORT = 5555;

Regist::Regist(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Regist)
{
    ui->setupUi(this);

    setWindowFlag(Qt::Window, true);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->btnRegister->setProperty("primary", true);
    ui->cbRole->clear();
        ui->cbRole->addItem("请选择身份"); // 0
        ui->cbRole->addItem("专家");        // 1
        ui->cbRole->addItem("工厂");        // 2
        ui->cbRole->setCurrentIndex(0);

    // 连接角色切换
    connect(ui->cbRole, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Regist::onRoleChanged);

    // 初始主题：未选择 -> 灰色
    applyRoleTheme(QStringLiteral("none"));

    // 悬停动效
    installButtonHoverAnim();

    // 密码可见按钮（密码与确认密码均添加）
    installPasswordEye();
}

Regist::~Regist()
{
    delete ui;
}

void Regist::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 窗口淡入动画
    auto *eff = new QGraphicsOpacityEffect(this);
    eff->setOpacity(0.0);
    this->setGraphicsEffect(eff);
    auto *anim = new QPropertyAnimation(eff, "opacity", this);
    anim->setDuration(220);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void Regist::preset(const QString &role, const QString &user, const QString &pass)
{
    if (role == "expert") ui->cbRole->setCurrentIndex(1);
    else if (role == "factory") ui->cbRole->setCurrentIndex(2);
    else ui->cbRole->setCurrentIndex(0);

    ui->leUsername->setText(user);
    ui->lePassword->setText(pass);
    ui->leConfirm->clear();

    // 预填角色后，立即应用对应主题
    onRoleChanged(ui->cbRole->currentIndex());
}

QString Regist::selectedRole() const
{
    switch (ui->cbRole->currentIndex()) {
    case 1: return "expert";
    case 2: return "factory";
    default: return "";
    }
}

bool Regist::sendRequest(const QJsonObject &obj, QJsonObject &reply, QString *errMsg)
{
    QTcpSocket sock;
    sock.connectToHost(QHostAddress(QString::fromLatin1(SERVER_HOST)), SERVER_PORT);
    if (!sock.waitForConnected(3000)) {
        if (errMsg) *errMsg = "服务器连接失败";
        return false;
    }
    const QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
    if (sock.write(line) == -1 || !sock.waitForBytesWritten(2000)) {
        if (errMsg) *errMsg = "请求发送失败";
        return false;
    }
    if (!sock.waitForReadyRead(5000)) {
        if (errMsg) *errMsg = "服务器无响应";
        return false;
    }
    QByteArray resp = sock.readAll();
    int nl = resp.indexOf('\n');
    if (nl >= 0) resp = resp.left(nl);

    QJsonParseError pe{};
    QJsonDocument rdoc = QJsonDocument::fromJson(resp, &pe);
    if (pe.error != QJsonParseError::NoError || !rdoc.isObject()) {
        if (errMsg) *errMsg = "响应解析失败";
        return false;
    }
    reply = rdoc.object();
    return true;
}

void Regist::on_btnRegister_clicked()
{
    const QString username = ui->leUsername->text().trimmed();
    const QString password = ui->lePassword->text();
    const QString confirm  = ui->leConfirm->text();

    if (username.isEmpty() || password.isEmpty() || confirm.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入账号、密码与确认密码");
        return;
    }
    if (password != confirm) {
        QMessageBox::warning(this, "提示", "两次输入的密码不一致");
        return;
    }
    const QString role = selectedRole();
    if (role.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择身份");
        return;
    }

    // 本地密码格式校验（与服务端一致）
    const QRegularExpression rx(QStringLiteral("^(?=.*[A-Za-z])(?=.*\\d)[A-Za-z\\d]{9,}$"));
    if (!rx.match(password).hasMatch()) {
        QMessageBox::warning(this, "提示", "密码需至少9位，且同时包含字母和数字，仅支持数字和字母");
        return;
    }

    QJsonObject req{
        {"action",  "register"},
        {"role",    role},
        {"username",username},
        {"password",password}
    };
    QJsonObject rep;
    QString err;
    if (!sendRequest(req, rep, &err)) {
        QMessageBox::warning(this, "注册失败", err);
        return;
    }
    if (!rep.value("ok").toBool(false)) {
        QMessageBox::warning(this, "注册失败", rep.value("msg").toString("未知错误"));
        return;
    }

    QMessageBox::information(this, "注册成功", "账号初始化完成");
    emit registered(username, role);
    close(); // 关闭注册窗口
}

void Regist::on_btnBack_clicked()
{
    close(); // 关闭注册窗口，登录窗口将由外部连接恢复显示
}

void Regist::onRoleChanged(int index)
{
    QString key = QStringLiteral("none");
    if (index == 1) key = QStringLiteral("expert");
    else if (index == 2) key = QStringLiteral("factory");
    applyRoleTheme(key);
}

void Regist::applyRoleTheme(const QString& roleKey)
{
    this->setProperty("roleTheme", roleKey);
    this->style()->unpolish(this);
    this->style()->polish(this);
    this->update();
}

void Regist::installButtonHoverAnim()
{
    auto fit = [&](QPushButton* b){
        if (!b) return;
        auto *f = new HoverFilter_R(b);
        b->installEventFilter(f);
        b->setAttribute(Qt::WA_Hover, true);
    };
    fit(ui->btnRegister);
    fit(ui->btnBack);
}

void Regist::installPasswordEye()
{
    // 在“密码”和“确认密码”右侧添加“小眼睛”
    ui->lePassword->setEchoMode(QLineEdit::Password);
    ui->leConfirm->setEchoMode(QLineEdit::Password);
    new PasswordEyeHelper_R(ui->lePassword);
    new PasswordEyeHelper_R(ui->leConfirm);
}

// 顶层打开注册窗口：隐藏登录窗口，注册窗口关闭时恢复
void openRegistDialog(QWidget *login, const QString &prefRole,
                      const QString &prefUser, const QString &prefPass)
{
    // 1) 创建为顶层窗口（无 parent）
    Regist *r = new Regist(nullptr);
    r->setAttribute(Qt::WA_DeleteOnClose);
    r->preset(prefRole, prefUser, prefPass);

    // 2) 隐藏登录窗口，避免残留
    if (login) login->hide();

    // 3) 注册成功时：回填登录界面并恢复显示
    QObject::connect(r, &Regist::registered, r, [login](const QString &u, const QString &role){
        if (!login) return;
        if (auto cb = login->findChild<QComboBox*>("cbRole")) {
            if (role == "expert") cb->setCurrentIndex(1);
            else if (role == "factory") cb->setCurrentIndex(2);
            else cb->setCurrentIndex(0);
        }
        if (auto leUser = login->findChild<QLineEdit*>("leUsername")) leUser->setText(u);
        if (auto lePass = login->findChild<QLineEdit*>("lePassword")) { lePass->clear(); lePass->setFocus(); }
        login->show(); login->raise(); login->activateWindow();
    });

    // 4) 无论何种方式关闭注册窗口，都恢复显示登录窗口
    QObject::connect(r, &QObject::destroyed, login, [login](){
        if (!login) return;
        login->show(); login->raise(); login->activateWindow();
    });

    // 5) 将注册窗口居中到登录窗口位置
    if (login) {
        const QRect lg = login->geometry();
        r->resize(r->sizeHint().expandedTo(QSize(720, 680))); // 略增尺寸以匹配更大字体
        const QPoint p = lg.center() - QPoint(r->width()/2, r->height()/2);
        r->move(p);
    }

    r->show();
}
