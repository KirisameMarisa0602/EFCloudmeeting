#ifndef REGIST_H
#define REGIST_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class Regist; }
QT_END_NAMESPACE

namespace Ui {
class Regist;
}

class Regist : public QWidget
{
    Q_OBJECT

public:
    explicit Regist(QWidget *parent = nullptr);
    ~Regist();

protected:
    void showEvent(QShowEvent *event) override; // 窗口淡入动画

signals:
    void registered(const QString &username, const QString &role);

public slots:
    void preset(const QString &role, const QString &user, const QString &pass);

private slots:
    void on_btnRegister_clicked();
    void on_btnBack_clicked();
    void onRoleChanged(int index); // 注册界面角色切换主题

private:
    bool sendRequest(const QJsonObject &obj, QJsonObject &reply, QString *errMsg = nullptr);
    QString selectedRole() const; // "expert" | "factory" | ""
    void applyRoleTheme(const QString& roleKey); // 应用主题（"expert"/"factory"/"none"）
    void installButtonHoverAnim();               // 按钮悬停动效
    void installPasswordEye();                   // 安装密码可见按钮（密码与确认密码）

private:
    Ui::Regist *ui;
};

void openRegistDialog(QWidget *parent, const QString &prefRole,
                      const QString &prefUser, const QString &prefPass);

#endif // REGIST_H
