#ifndef LOGIN_H
#define LOGIN_H

#include <QWidget>
#include <QPointer>

QT_BEGIN_NAMESPACE
namespace Ui { class Login; }
QT_END_NAMESPACE

class ClientExpert;
class ClientFactory;

namespace Ui {
class Login;
}

class Login : public QWidget
{
    Q_OBJECT

public:
    explicit Login(QWidget *parent = nullptr);
    ~Login();

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override; // 窗口淡入动画

private slots:
    void on_btnLogin_clicked();
    void on_btnToReg_clicked();
    void onRoleChanged(int index); // 根据角色切换主题

private:
    bool sendRequest(const QJsonObject &obj, QJsonObject &reply, QString *errMsg = nullptr);
    QString selectedRole() const; // "expert" | "factory" | ""
    void applyRoleTheme(const QString& roleKey); // 应用主题（"expert"/"factory"/"none"）
    void installButtonHoverAnim();               // 按钮悬停动效
    void installPasswordEye();                   // 安装“密码可见”按钮

private:
    Ui::Login *ui;
    QPointer<ClientExpert> expertWin;
    QPointer<ClientFactory> factoryWin;
};

#endif // LOGIN_H
