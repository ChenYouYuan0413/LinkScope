#ifndef OPENOCD_H
#define OPENOCD_H

#include <QObject>
#include <QProcess>
#include <qcoreapplication.h>
#ifdef Q_OS_WIN32
#include <windows.h>
#endif

class OpenOCD : public QObject
{
    Q_OBJECT
public:
    explicit OpenOCD(QObject *parent = nullptr);
    ~OpenOCD();
    void start(const QString &chosenInterface,const QString &chosenTarget,const QString &extraParam,int gdbPort);
    void stop();
    bool isRunning();

signals:
    void onErrorOccur(const QString &info);

private:
    QProcess *process;
};

#endif // OPENOCD_H
