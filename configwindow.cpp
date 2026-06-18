#include "configwindow.h"
#include "ui_configwindow.h"
#include <qdebug.h>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include <QRadioButton>
#include <QButtonGroup>

// 新增的 UI 控件（不在 .ui 文件中，动态创建）
struct ExtraWidgets {
    QComboBox *cbInterface, *cbTarget;
    QCheckBox *cbExtOpenocd;
    QCheckBox *cbSyncWatch, *cbFastMode;
    QLineEdit *leProjectDir, *leAxfPath;
    QPushButton *btnBrowse, *btnAxfBrowse;
    QRadioButton *rbOcd, *rbSerial;
};

static ExtraWidgets *ex = nullptr;

ConfigWindow::ConfigWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigWindow)
{
    ui->setupUi(this);
    setWindowTitle("高级设置");

    // 动态添加「调试器 & 同步设置」组
    QGroupBox *gb = new QGroupBox("调试器 & 同步设置");
    QFormLayout *fl = new QFormLayout(gb);

    ex = new ExtraWidgets;
    ex->cbInterface = new QComboBox();
    ex->cbTarget = new QComboBox();
    ex->cbExtOpenocd = new QCheckBox("外部OpenOCD");
    ex->cbSyncWatch = new QCheckBox("同步Cortex Watch");
    ex->cbFastMode = new QCheckBox("高速采样模式");
    ex->leProjectDir = new QLineEdit();
    ex->leProjectDir->setPlaceholderText("项目目录（含.vscode）");
    ex->btnBrowse = new QPushButton("浏览...");
    QHBoxLayout *dirLayout = new QHBoxLayout();
    dirLayout->addWidget(ex->leProjectDir);
    dirLayout->addWidget(ex->btnBrowse);

    fl->addRow("接口:", ex->cbInterface);
    fl->addRow("目标:", ex->cbTarget);
    fl->addRow("", ex->cbExtOpenocd);
    fl->addRow("", ex->cbSyncWatch);
    fl->addRow("项目目录:", new QWidget()); // placeholder
    fl->addRow("", dirLayout); // 实际用 layout
    // 调试模式选择
    ex->rbOcd = new QRadioButton("OpenOCD模式");
    ex->rbSerial = new QRadioButton("串口模式");
    QButtonGroup *bg = new QButtonGroup(this);
    bg->addButton(ex->rbOcd);
    bg->addButton(ex->rbSerial);
    ex->rbOcd->setChecked(true);
    QHBoxLayout *modeLayout = new QHBoxLayout();
    modeLayout->addWidget(ex->rbOcd);
    modeLayout->addWidget(ex->rbSerial);
    fl->addRow("调试模式:", modeLayout);

    // AXF 符号文件
    ex->leAxfPath = new QLineEdit();
    ex->leAxfPath->setPlaceholderText("选择 .elf / .axf 符号文件");
    ex->btnAxfBrowse = new QPushButton("浏览...");
    QHBoxLayout *axfLayout = new QHBoxLayout();
    axfLayout->addWidget(ex->leAxfPath);
    axfLayout->addWidget(ex->btnAxfBrowse);
    fl->addRow("符号文件:", axfLayout);

    fl->addRow("", ex->cbFastMode);

    connect(ex->btnAxfBrowse, &QPushButton::clicked, [=]{
        QString path = QFileDialog::getOpenFileName(this, "选择符号文件", ex->leAxfPath->text(),
            "ELF/AXF文件 (*.elf *.axf);;所有文件 (*)");
        if (!path.isEmpty()) ex->leAxfPath->setText(path);
    });
    connect(ex->btnBrowse, &QPushButton::clicked, [=]{
        QString dir = QFileDialog::getExistingDirectory(this, "选择项目目录", ex->leProjectDir->text());
        if (!dir.isEmpty()) ex->leProjectDir->setText(dir);
    });

    // 插入到最底部（确定/取消按钮之前）
    QVBoxLayout *vl = qobject_cast<QVBoxLayout*>(layout());
    if (vl) vl->insertWidget(vl->count() - 1, gb);

    // 增大窗口高度
    resize(460, 520);
}

ConfigWindow::~ConfigWindow()
{
    delete ex;
    delete ui;
}

void ConfigWindow::setParam(ConfigWindowParam &param)
{
    ui->sb_baudrate->setValue(param.baudrate);
    ui->cb_databit->setCurrentText(QString("%1").arg(param.databits));
    ui->cb_stopbit->setCurrentText(param.stopbits==1 ? "1" : (param.stopbits==2?"2":"1.5"));
    ui->cb_parity->setCurrentText(param.parity==0 ? "无" : (param.parity==2?"偶校验":"奇校验"));
    ui->cb_sample_freq->setCurrentText(QString("%1Hz").arg(param.sampleFreq));
    ui->sb_gdb_port->setValue(param.gdbPort);
    ui->et_gdb_param->setText(param.gdbParam);
    ui->et_ocd_param->setText(param.ocdParam);
    ex->cbInterface->clear();
    ex->cbInterface->addItems(param.interfaceList);
    ex->cbInterface->setCurrentText(param.interface);
    ex->cbTarget->clear();
    ex->cbTarget->addItems(param.targetList);
    ex->cbTarget->setCurrentText(param.target);
    ex->cbExtOpenocd->setChecked(param.extOpenocd);
    ex->cbSyncWatch->setChecked(param.syncWatch);
    ex->leProjectDir->setText(param.projectDir);
    ex->cbFastMode->setChecked(param.fastMode);
    ex->rbOcd->setChecked(param.openocdMode);
    ex->rbSerial->setChecked(!param.openocdMode);
    ex->leAxfPath->setText(param.axfPath);
}

void ConfigWindow::getParam(ConfigWindowParam &param)
{
    param.baudrate=ui->sb_baudrate->value();
    param.databits=(QSerialPort::DataBits)ui->cb_databit->currentText().toInt();
    param.stopbits=(QSerialPort::StopBits)QList<int>{1,3,2}.at(ui->cb_stopbit->currentIndex());
    param.parity=(QSerialPort::Parity)QList<int>{0,2,3}.at(ui->cb_parity->currentIndex());
    param.sampleFreq=QList<int>{100,50,20,10,5,2,1}.at(ui->cb_sample_freq->currentIndex());
    param.gdbPort=ui->sb_gdb_port->value();
    param.gdbParam=ui->et_gdb_param->text();
    param.ocdParam=ui->et_ocd_param->text();
    param.interface = ex->cbInterface->currentText();
    param.target = ex->cbTarget->currentText();
    param.extOpenocd = ex->cbExtOpenocd->isChecked();
    param.syncWatch = ex->cbSyncWatch->isChecked();
    param.projectDir = ex->leProjectDir->text();
    param.fastMode = ex->cbFastMode->isChecked();
    param.openocdMode = ex->rbOcd->isChecked();
    param.axfPath = ex->leAxfPath->text();
}

void ConfigWindow::on_bt_ok_clicked()
{
    accept();
}

void ConfigWindow::on_bt_cancel_clicked()
{
    close();
}
