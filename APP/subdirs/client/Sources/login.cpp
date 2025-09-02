#include "login.h"
#include "ui_login.h"
#include "regist.h"
#include "client_factory.h"
#include "client_expert.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QHostAddress>
#include <QTcpSocket>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QStyle>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

static const char* SERVER_HOST = "127.0.0.1";
static const quint16 SERVER_PORT = 5555;

// 将“眼睛”按钮嵌入到 QLineEdit 的小工具（稳定实现，不依赖图形特效）
class PasswordEyeHelper : public QObject {
public:
    explicit PasswordEyeHelper(QLineEdit* edit)
        : QObject(edit), edit_(edit)
    {
        btn_ = new QToolButton(edit_);
        btn_->setCursor(Qt::PointingHandCursor);
        btn_->setCheckable(true);
        btn_->setAutoRaise(true);
        btn_->setToolTip(QStringLiteral("显示/隐藏密码"));
        btn_->setFocusPolicy(Qt::NoFocus);
        btn_->setIconSize(QSize(18, 18));

        // 初始为“隐藏”状态（黑点）
        btn_->setChecked(false);
        btn_->setIcon(makeEyeIcon(false));

        // 仅对本控件设置右侧内边距，避免影响兄弟控件
        edit_->setStyleSheet(edit_->styleSheet() + "padding-right: 34px;");

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
        // 父子坐标一致（btn 为 edit 的子控件）
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

Login::Login(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Login)
{
    ui->setupUi(this);

    // 主按钮 primary 标记（颜色由 QSS + roleTheme 决定）
    ui->btnLogin->setProperty("primary", true);

    // 初始化角色下拉
    ui->cbRole->clear();
    ui->cbRole->addItem("请选择身份"); // 0
    ui->cbRole->addItem("专家");        // 1
    ui->cbRole->addItem("工厂");        // 2
    ui->cbRole->setCurrentIndex(0);

    // 连接角色切换信号
    connect(ui->cbRole, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Login::onRoleChanged);

    // 初始主题：未选择 -> 灰色
    applyRoleTheme(QStringLiteral("none"));

    // 密码可见按钮
    installPasswordEye();
}

Login::~Login()
{
    delete ui;
}

void Login::closeEvent(QCloseEvent *event)
{
    QCoreApplication::quit();
    QWidget::closeEvent(event);
}

QString Login::selectedRole() const
{
    switch (ui->cbRole->currentIndex()) {
    case 1: return "expert";
    case 2: return "factory";
    default: return "";
    }
}

bool Login::sendRequest(const QJsonObject &obj, QJsonObject &reply, QString *errMsg)
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

void Login::on_btnLogin_clicked()
{
    const QString username = ui->leUsername->text().trimmed();
    const QString password = ui->lePassword->text();
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入账号和密码");
        return;
    }
    const QString role = selectedRole();
    if (role.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择身份");
        return;
    }

    QJsonObject req{
        {"action",  "login"},
        {"role",    role},
        {"username",username},
        {"password",password}
    };
    QJsonObject rep;
    QString err;
    if (!sendRequest(req, rep, &err)) {
        QMessageBox::warning(this, "登录失败", err);
        return;
    }
    if (!rep.value("ok").toBool(false)) {
        QMessageBox::warning(this, "登录失败", rep.value("msg").toString("未知错误"));
        return;
    }

    if (role == "expert") {
        if (!expertWin) expertWin = new ClientExpert;
        expertWin->show();
    } else {
        if (!factoryWin) factoryWin = new ClientFactory;
        factoryWin->show();
    }
    this->hide();
}

void Login::on_btnToReg_clicked()
{
    // 独立顶层打开注册窗口，并隐藏当前登录窗口
    class Regist;
    extern void openRegistDialog(QWidget *login,
                                 const QString &prefRole,
                                 const QString &prefUser,
                                 const QString &prefPass);
    openRegistDialog(this, selectedRole(), ui->leUsername->text(), ui->lePassword->text());
}

void Login::onRoleChanged(int index)
{
    QString key = QStringLiteral("none");
    if (index == 1) key = QStringLiteral("expert");
    else if (index == 2) key = QStringLiteral("factory");
    applyRoleTheme(key);
}

void Login::applyRoleTheme(const QString& roleKey)
{
    this->setProperty("roleTheme", roleKey);
    this->style()->unpolish(this);
    this->style()->polish(this);
    this->update();
}

void Login::installPasswordEye()
{
    ui->lePassword->setEchoMode(QLineEdit::Password);
    new PasswordEyeHelper(ui->lePassword);
}
