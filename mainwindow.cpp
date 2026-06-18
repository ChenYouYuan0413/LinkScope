#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("LinkScope");

    setStylesheet();//设置全局样式表

    // ---- 停靠窗口系统 ----
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::AllowNestedDocks);

    graph=new GraphWindow();//创建绘图窗口
    graph->setVarList(&varList);
    graphDock = new QDockWidget("图形", this);
    graphDock->setWidget(graph);
    graphDock->setObjectName("GraphDock");
    addDockWidget(Qt::BottomDockWidgetArea, graphDock);
    ui->action_show_graph->setChecked(false);
    connect(ui->action_show_graph, &QAction::toggled, graphDock, &QDockWidget::setVisible);
    connect(graphDock, &QDockWidget::visibilityChanged, this, [=](bool v){ ui->action_show_graph->setChecked(v); });

    listWindow=new ListWindow();//创建选择窗口
    connect(listWindow,SIGNAL(add2Edit(const QString &)),this,SLOT(slotOnVarAdd2Edit(const QString &)));
    connect(listWindow,SIGNAL(add2List(const QString &)),this,SLOT(slotOnVarAdd2List(const QString &)));
    listDock = new QDockWidget("变量选择", this);
    listDock->setWidget(listWindow);
    listDock->setObjectName("ListDock");
    addDockWidget(Qt::LeftDockWidgetArea, listDock);
    ui->action_show_selector->setChecked(false);
    connect(ui->action_show_selector, &QAction::toggled, listDock, &QDockWidget::setVisible);
    connect(listDock, &QDockWidget::visibilityChanged, this, [=](bool v){ ui->action_show_selector->setChecked(v); });

    logWindow=new LogWindow();//创建日志窗口
    logDock = new QDockWidget("日志", this);
    logDock->setWidget(logWindow);
    logDock->setObjectName("LogDock");
    addDockWidget(Qt::RightDockWidgetArea, logDock);
    ui->action_show_log->setChecked(false);
    connect(ui->action_show_log, &QAction::toggled, logDock, &QDockWidget::setVisible);
    connect(logDock, &QDockWidget::visibilityChanged, this, [=](bool v){ ui->action_show_log->setChecked(v); });

    stampTimer=new QElapsedTimer();//创建并运行时间戳定时器
    stampTimer->start();

    watchTimer=new QTimer(this);//创建watch定时器
    watchTimer->setInterval(0);
    watchTimer->stop();
    connect(watchTimer,SIGNAL(timeout()),this,SLOT(slotWatchTimerTrig()));

    tableTimer=new QTimer(this);//创建表格刷新定时器
    tableTimer->setInterval(200);
    tableTimer->stop();
    connect(tableTimer,SIGNAL(timeout()),this,SLOT(slotTableTimerTrig()));

    logTimer=new QTimer(this);//创建日志监视定时器
    logTimer->setInterval(100);
    logTimer->stop();
    connect(logTimer,SIGNAL(timeout()),this,SLOT(slotLogTimerTrig()));

    autosaveTimer=new QTimer(this);//创建自动保存定时器
    autosaveTimer->setInterval(5000);//5s自动保存
    autosaveTimer->start();
    connect(autosaveTimer,&QTimer::timeout,this,[=]{saveToFile(QDir::homePath()+"/.linkscope/autosave.ini");});

    tableModel=new QStandardItemModel(this);//创建并初始化表格
    initTable();

    tablePopMenu=new QMenu(ui->tb_var);//创建表格右键菜单
    tablePopMenu->addAction(ui->action_del_var);
    tablePopMenu->addAction(ui->action_del_all);

    openocd=new OpenOCD();//创建OpenOCD对象并连接错误处理槽
    connect(openocd,&OpenOCD::onErrorOccur,this,&MainWindow::slotOnConnErrorOccur,Qt::QueuedConnection);

    serialocd=new SerialOCD();//创建SerialOCD对象并连接错误处理槽
    connect(serialocd,&SerialOCD::onErrorOccur,this,&MainWindow::slotOnConnErrorOccur,Qt::QueuedConnection);

    gdb=new GDBProcess();//创建并启动GDB
    gdb->setTempSymbolFileName("tmp");//设定临时符号文件名
    gdb->start();//启动gdb进程

    // Cortex Watch 同步UI
    QHBoxLayout *syncLayout = new QHBoxLayout();
    cbSyncWatch = new QCheckBox("同步 Cortex Watch");
    cbSyncWatch->setChecked(false);
    leProjectDir = new QLineEdit();
    leProjectDir->setPlaceholderText("项目目录（含.vscode）");
    btnBrowse = new QPushButton("浏览...");
    syncLayout->addWidget(cbSyncWatch);
    syncLayout->addWidget(leProjectDir);
    syncLayout->addWidget(btnBrowse);
    connect(cbSyncWatch, SIGNAL(toggled(bool)), this, SLOT(on_cb_sync_watch_toggled(bool)));
    connect(btnBrowse, SIGNAL(clicked()), this, SLOT(on_bt_browse_project_clicked()));

    syncTimer = new QTimer(this);
    syncTimer->setInterval(2000);
    syncTimer->stop();
    connect(syncTimer, SIGNAL(timeout()), this, SLOT(slotSyncTimer()));

    // 把同步UI插入到主布局中（在gridLayout下方）
    QWidget *syncWidget = new QWidget(this);
    syncWidget->setLayout(syncLayout);
    syncWidget->setObjectName("syncWidget");
    // 找到verticalLayout_2并插入syncWidget到gridLayout后面
    QVBoxLayout *mainLayout = findChild<QVBoxLayout*>("verticalLayout_2");
    if (mainLayout) {
        int idx = mainLayout->indexOf(findChild<QWidget*>("tb_var")) + 1;
        mainLayout->insertWidget(idx, syncWidget);
    }

    // 高速采样模式复选框
    cbFastMode = new QCheckBox("高速采样模式（绕过GDB变量查找）");
    cbFastMode->setChecked(false);
    connect(cbFastMode, SIGNAL(toggled(bool)), this, SLOT(on_cb_fast_mode_toggled(bool)));
    if (mainLayout) {
        int idx = mainLayout->indexOf(findChild<QWidget*>("tb_var")) + 1;
        mainLayout->insertWidget(idx + 1, cbFastMode);
    }

    QDir().mkpath(QDir::homePath()+"/.linkscope");//确保配置目录存在

    // 隐藏已移到高级设置的控件，主窗口只保留变量表和操作栏
    ui->rb_openocd->setVisible(false);
    ui->rb_serialocd->setVisible(false);
    ui->box_openocd->setVisible(false);
    ui->box_serial->setVisible(false);
    if (QWidget *sw = findChild<QWidget*>("syncWidget")) sw->setVisible(false);
    cbFastMode->setVisible(false);
    ui->groupBox_2->setVisible(false);  // 符号文件选择整行移到高级设置

    // 默认布局：示波器在右侧
    addDockWidget(Qt::RightDockWidgetArea, graphDock);
    graphDock->show();

    // 暂停/继续按钮，插入到 bt_reset 后面
    bt_pause = new QPushButton("暂停");
    bt_pause->setEnabled(false);
    connect(bt_pause, &QPushButton::clicked, this, &MainWindow::on_bt_pause_clicked);
    if (ui->groupBox_4) {
        QVBoxLayout *vl = qobject_cast<QVBoxLayout*>(ui->groupBox_4->layout());
        if (vl) vl->insertWidget(vl->indexOf(ui->bt_reset) + 1, bt_pause);
    }

    loadConfFileList();//从openocd文件夹中读取配置文件列表
    loadGlobalConf();//加载软件全局配置
    loadFromFile(QDir::homePath()+"/.linkscope/autosave.ini");//加载自动保存的工程配置

    if (!fastMode)
        watchTimer->setInterval(1000/configWindowParam.sampleFreq);

    QTimer::singleShot(100,this,&MainWindow::checkOpenocdProcess);//窗口加载完成后检查是否有正在运行的openocd进程
}

MainWindow::~MainWindow()
{
    if(connected)//若处于连接状态则断开连接
    {
        gdb->disconnectFromRemote();
        gdb->unloadSymbolFile();
        if(ui->rb_openocd->isChecked())
            openocd->stop();
        else if(ui->rb_serialocd->isChecked())
            serialocd->stopConnect();
    }
    gdb->stop();//结束gdb进程
    delete ui;
    delete tableModel;
    delete tablePopMenu;
    delete openocd;
    delete gdb;
    delete stampTimer;
    delete watchTimer;
    delete tableTimer;
    delete logTimer;
    delete graph;
    delete listWindow;
    delete logWindow;
}

//按键事件，监听DEL键按下，用于删除单个变量
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if(event->key()==Qt::Key_Delete)//判定是DEL键
    {
        if(ui->tb_var->currentIndex().column()==0)//确定已选中某个变量的变量名
        {
            int index=ui->tb_var->currentIndex().row();//获取所选变量的下标
            if(index<varList.size())
            {
                QMessageBox msgBox;
                msgBox.setWindowTitle("提示");
                msgBox.setText("确认要删除变量 "+varList.at(index).name+" 吗？");
                msgBox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
                if(msgBox.exec()==QMessageBox::Yes)//用户确认后移除变量并重绘表格
                {
                    varList.removeAt(index);
                    redrawTable();

                    if(connected)//若正在连接状态则向gdb发送新的变量列表
                        updateGDBList();
                }
            }
        }
    }
}

//窗口关闭事件，处理软件退出
void MainWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    saveGlobalConf();//保存软件全局设置
}

//表格被编辑，添加变量或修改变量值
void MainWindow::slotTableEdit(QModelIndex topleft, QModelIndex bottomright)
{
    Q_UNUSED(bottomright);
    if(topleft.column()==0 && topleft.row()==varList.size())//若编辑的是最后一行的变量名，进行变量添加
    {
        QString name=tableModel->item(topleft.row(),0)->text();
        if(!name.isEmpty())
        {
            VarInfo var;//创建一个默认配置的变量并添加到列表
            var.name=name;
            var.rawValue="";
            var.enableScope=true;
            var.lineColor=QColor(Qt::black);
            varList.append(var);

            redrawTable();//重绘表格

            if(connected)//若正在连接状态则向gdb发送新的变量列表
                updateGDBList();
        }
    }
    else if(topleft.column()==2 && topleft.row()!=varList.size())//若编辑的是第二列，表示需进行变量值修改
    {
        QString name=varList.at(topleft.row()).name;
        QString valueStr=tableModel->item(topleft.row(),2)->text();
        if(!valueStr.isEmpty())
        {
            gdb->setVarValue(name,valueStr.toDouble());//向gdb发送命令写入变量值
            tableModel->item(topleft.row(),2)->setText("");//清空编辑框
        }
    }
}

//watch定时器触发，进行一次变量查看
void MainWindow::slotWatchTimerTrig()
{
    if(isWatchProcessing)//若上次查看还未完成则直接退出
        return;
    isWatchProcessing=true;

    qint64 timestamp=stampTimer->nsecsElapsed()/1000;//获取时间戳（微秒）

    // 每500帧算一次实际采样率，输出到日志窗口
    static int frameCount = 0;
    static qint64 lastLogTime = 0;
    frameCount++;
    if (frameCount >= 500) {
        qint64 elapsed = timestamp - lastLogTime;
        double fps = (elapsed > 0) ? (frameCount * 1e6 / elapsed) : 0;
        logWindow->addLog('I', "Perf",
            QString("采样率: %1 Hz").arg(fps, 0, 'f', 1),
            timestamp, "slotWatchTimerTrig");
        frameCount = 0;
        lastLogTime = timestamp;
    }

    if (fastMode) {
        // 批量读：一次 x/Nwx 读整块内存，本地拆值
        uint32_t minAddr = 0xFFFFFFFF, maxEnd = 0;
        for (int i = 0; i < varList.size(); i++) {
            if (varList[i].address) {
                if (varList[i].address < minAddr) minAddr = varList[i].address;
                uint32_t end = varList[i].address + varList[i].size;
                if (end > maxEnd) maxEnd = end;
            }
        }
        if (minAddr <= maxEnd) {
            int nWords = ((maxEnd - minAddr + 3) / 4);
            if (nWords < 1) nWords = 1;
            uint32_t alignedAddr = minAddr & ~3u;
            nWords = ((maxEnd - alignedAddr + 3) / 4);

            QString resp = gdb->runCmd(
                QString("x/%1wx 0x%2\r\n").arg(nWords)
                .arg(alignedAddr, 8, 16, QChar('0')));

            // 解析 x/Nwx 输出：逐行 "0xADDR: 0xVVVV 0xVVVV..."
            if (batchDebugOnce) {
                logWindow->addLog('I', "FastMode",
                    QString("resp(%1)=%2").arg(resp.length()).arg(resp.left(250)),
                    timestamp, "slotWatchTimerTrig");
            }
            QVector<uint32_t> words;
            QStringList lines = resp.split(QRegExp("[\\r\\n]+"));
            foreach (const QString &line, lines) {
                int colon = line.indexOf(':');
                if (colon < 0) continue;
                QString right = line.mid(colon + 1);
                QRegExp wordRx("0x([0-9a-fA-F]+)");
                int wp = 0;
                while ((wp = wordRx.indexIn(right, wp)) != -1) {
                    bool ok;
                    words.append(wordRx.cap(1).toUInt(&ok, 16));
                    wp += wordRx.matchedLength();
                }
            }

            if (batchDebugOnce) {
                logWindow->addLog('I', "FastMode",
                    QString("批量读: x/%1wx 0x%2 %3 words %4 vars debugOnce=%5")
                    .arg(nWords).arg(alignedAddr, 8, 16, QChar('0'))
                    .arg(words.size()).arg(varList.size()).arg(batchDebugOnce),
                    timestamp, "slotWatchTimerTrig");
            }
            // 按偏移拆值
            for (int i = 0; i < varList.size(); i++) {
                if (varList[i].address < alignedAddr) continue;
                int byteOff = (int)(varList[i].address - alignedAddr);
                int wordIdx = byteOff / 4;
                int byteInWord = byteOff % 4;
                if (wordIdx >= words.size()) continue;

                uint32_t raw = 0;
                if (byteInWord + varList[i].size <= 4 && wordIdx < words.size()) {
                    // 值在同一 word 内
                    uint64_t mask = (varList[i].size >= 4) ? 0xFFFFFFFFu
                                   : ((1u << (varList[i].size * 8)) - 1);
                    raw = (words[wordIdx] >> (byteInWord * 8)) & (uint32_t)mask;
                } else {
                    // 跨 word 边界
                    int remain = varList[i].size;
                    int shift = 0;
                    int wi = wordIdx;
                    int bo = byteInWord;
                    while (remain > 0 && wi < words.size()) {
                        int chunk = qMin(remain, 4 - bo);
                        uint64_t mask = (chunk >= 4) ? 0xFFFFFFFFu
                                        : ((1u << (chunk * 8)) - 1);
                        raw |= ((words[wi] >> (bo * 8)) & (uint32_t)mask) << shift;
                        shift += chunk * 8;
                        remain -= chunk;
                        wi++;
                        bo = 0;
                    }
                }
                // 转成字符串值
                QString str = valueToString(raw, varList[i]);
                varList[i].rawValue = str;
                if (frameCount % 500 == 0 && varList[i].name == "mdelay_time") {
                    logWindow->addLog('I', "FastMode",
                        QString("DBG %1 off=%2 wi=%3 raw=0x%4 w[0]=0x%5 w[wi]=0x%6 str=|%7|")
                        .arg(varList[i].name).arg(byteOff).arg(wordIdx)
                        .arg(raw, 0, 16)
                        .arg(words.size() > 0 ? words[0] : 0, 0, 16)
                        .arg(wordIdx < words.size() ? words[wordIdx] : 0, 0, 16)
                        .arg(str),
                        timestamp, "slotWatchTimerTrig");
                }
                SamplePoint sample;
                if (gdb->getDoubleFromDisplayValue(varList[i].rawValue, sample.value)) {
                    sample.timestamp = timestamp;
                    varList[i].samples.append(sample);
                }
            }
            batchDebugOnce = false;
        }
    } else {
        QString rawDisplay=gdb->runCmd("display\r\n");//获取gdb查看得到的原始值
        for(int index=0;index<varList.size();index++)//依次进行每个变量的匹配
        {
            varList[index].rawValue=gdb->captureValueFromDisplay(rawDisplay,varList.at(index).name);//进行正则匹配，截取出变量值部分
            SamplePoint sample;
            if(gdb->getDoubleFromDisplayValue(varList[index].rawValue,sample.value))//尝试转换为double，生成采样点并写入采样点列表
            {
                sample.timestamp=timestamp;
                varList[index].samples.append(sample);
            }
        }
    }

    isWatchProcessing=false;
}

//表格刷新定时器触发，刷新各变量当前值
void MainWindow::slotTableTimerTrig()
{
    for(int index=0;index<varList.size();index++)//遍历每个变量
    {
        QStandardItem *item=tableModel->item(index,1);
        if(item->text()!=varList[index].rawValue)//若列表中数据与当前显示的不同，表示变量发生变化
        {
            item->setText(varList[index].rawValue);//设置新值
            item->setBackground(QBrush(QColor("#99CCFF")));//设置单元格背景为淡蓝色
        }
        else
        {
            item->setBackground(QBrush(QColor(Qt::white)));//若不发生变化则设置单元格背景为白色
        }
    }
}

//日志监视定时器触发，获取并解析日志信息
void MainWindow::slotLogTimerTrig()
{
    const QString cmd="print/u logQueue.size>0?(logQueue.size--,logQueue.startPos=(logQueue.startPos+1)%logQueue.maxSize,*logQueue.buf[logQueue.startPos]@logQueue.lenBuf[logQueue.startPos]):{0}\r\n";
    QString raw=gdb->runCmd(cmd);//命令GDB从下位机出队一条日志
    QList<uint> numList=gdb->getUintArrayFromDisplay(raw);//从日志中解析缓冲区数组内容
    if(numList.size()>1)
    {
        char attr=numList.at(0);//日志属性字符
        QStringList logList;//将缓冲区数组数据拆分成几个字符串
        logList<<""<<""<<""<<"";
        for(int pos=1,strNum=0; pos<numList.size() && strNum<logList.size(); pos++)
        {
            if(numList[pos]==0)
                strNum++;
            else
                logList[strNum].append(numList[pos]);
        }
        logWindow->addLog(attr,logList[0],logList[1],logList[2].toULongLong(),logList[3]);//向日志窗口添加一条日志
    }
}

//选择器窗口添加到编辑框槽函数
void MainWindow::slotOnVarAdd2Edit(const QString &name)
{
    tableModel->disconnect(this,SLOT(slotTableEdit(QModelIndex,QModelIndex)));//断开信号连接，否则触发后会直接添加到列表
    tableModel->item(tableModel->rowCount()-1,0)->setText(name);
    connect(tableModel,SIGNAL(dataChanged(QModelIndex,QModelIndex)),this,SLOT(slotTableEdit(QModelIndex,QModelIndex)));//恢复信号连接
}

//选择器窗口添加到列表槽函数
void MainWindow::slotOnVarAdd2List(const QString &name)
{
    QStandardItem *editItem=tableModel->item(tableModel->rowCount()-1,0);
    editItem->setText("");//清空编辑框
    editItem->setText(name);//在编辑框中填入变量名，会直接添加到列表中
}

//发生连接错误时的槽函数
void MainWindow::slotOnConnErrorOccur(const QString &info)
{
    if(connected)
        setConnState(false);
    QMessageBox::information(this,"连接错误",info);
}

//连接按钮点击，触发连接状态切换
void MainWindow::on_bt_conn_clicked()
{
    if(!connected && !axfChosen)//用户未选择axf文件就点击连接按钮
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle("警告");
        msgBox.setText("还未选择符号文件，连接后仅能使用绝对地址查看变量，是否继续连接？");
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        if(msgBox.exec()!=QMessageBox::Ok)
            return;
    }
    setConnState(!connected);//切换连接状态
}

//设置连接状态（参数true表示进行连接，false表示断开连接）
void MainWindow::setConnState(bool connect)
{
    if(connect)//进行连接
    {
        ui->bt_conn->setEnabled(false);//先禁用连接按钮，防止多次点击

        bool ocdStartSuccess=false;

        if(ui->rb_openocd->isChecked())//选择的是OpenOCD模式
        {
            if(!ui->cb_ext_openocd->isChecked())//使用内置openocd进程
            {
                openocd->start(ui->cb_interface->currentText(),ui->cb_target->currentText(),configWindowParam.ocdParam,configWindowParam.gdbPort);//运行openocd进程进行目标连接
                sleep(500);//等待500ms
                if(openocd->isRunning())
                    ocdStartSuccess=true;
            }
            else//使用外部openocd
            {
                ocdStartSuccess=true;
            }
        }
        else if(ui->rb_serialocd->isChecked())//选择的是串口模式
        {
            if(!ui->cb_com->currentText().isEmpty())
            {
                serialocd->startConnect(SerialParam{
                    ui->cb_com->currentText(),
                    configWindowParam.baudrate,
                    configWindowParam.databits,
                    configWindowParam.stopbits,
                    configWindowParam.parity},
                    configWindowParam.gdbPort);//启动SerialOCD
                sleep(500);
                if(serialocd->isRunning())
                    ocdStartSuccess=true;
            }
            else
            {
                QMessageBox::information(this,"连接错误","请先选择串口号");
            }
        }

        if(ocdStartSuccess)//若ocd成功启动
        {
            foreach(QString cmd, configWindowParam.gdbParam.split(';'))//执行用户设定的指令
                if(cmd.length()>0)
                    gdb->runCmd(cmd+"\r\n");
            gdb->loadSymbolFile(ui->txt_axf_path->text());//设置gdb符号文件
            gdb->connectToRemote(QString("localhost:%1").arg(configWindowParam.gdbPort));//连接gdb到ocd
            updateGDBList();//向gdb发送变量列表

            for(int i=0;i<varList.size();i++)//清空变量列表历史采样点数据
                varList[i].samples.clear();

            stampTimer->restart();//开启时间戳计时
            watchTimer->start();//开启定时查看变量值
            tableTimer->start();//开启表格定时刷新

            if(ui->cb_log->isChecked())//开启日志监视定时器
                logTimer->start();
            logWindow->clearLog();//清空日志列表

            ui->bt_conn->setText("断开连接");
            ui->bt_reset->setEnabled(true);//使能复位按钮
            bt_pause->setEnabled(true);
            ui->rb_openocd->setEnabled(false);//失能连接方式选择
            ui->rb_serialocd->setEnabled(false);

            connected=true;//更新连接标志
            if (cbSyncWatch->isChecked() && !workspaceDbPath.isEmpty()) {
                syncWatchVars();
                syncTimer->start();
            }
            if (fastMode) {
                qint64 ts = stampTimer->nsecsElapsed() / 1000;
                logWindow->addLog('I', "FastMode",
                    QString("高速模式已启用，varList有%1个变量，开始解析地址").arg(varList.size()),
                    ts, "slotConnect");
                resolveVarAddresses();
                updateGDBList();
                logWindow->addLog('I', "FastMode",
                    QString("地址解析完成，共%1个变量").arg(varList.size()),
                    ts, "slotConnect");
            }
        }
        ui->bt_conn->setEnabled(true);//恢复连接按钮
    }
    else//断开连接
    {
        watchTimer->stop();//停止定时查看和定时刷新表格
        tableTimer->stop();
        if(ui->cb_log->isChecked())//停止日志监视定时器
            logTimer->stop();
        gdb->disconnectFromRemote();//断开gdb
        gdb->unloadSymbolFile();//卸载符号文件
        if(ui->rb_openocd->isChecked() && !ui->cb_ext_openocd->isChecked())//根据连接方式选择结束ocd
            openocd->stop();
        else if(ui->rb_serialocd->isChecked())
            serialocd->stopConnect();
        ui->bt_conn->setText("连接目标");
        ui->bt_reset->setEnabled(false);//禁用复位按钮
        bt_pause->setEnabled(false);
        ui->rb_openocd->setEnabled(true);//使能连接方式选择
        ui->rb_serialocd->setEnabled(true);
        syncTimer->stop();
        connected=false;//更新连接标志
        batchDebugOnce = true;  // 重连后重新输出调试信息
        // 重置 batch read debug flag（用 volatile trick 强制下次首帧输出）
        slotWatchTimerTrig(); // no-op trigger will be called anyway
    }
}

//延时指定毫秒数，进行子任务循环
void MainWindow::sleep(uint32_t ms)
{
    QTime untilTime=QTime::currentTime().addMSecs(ms);
    while(QTime::currentTime()<untilTime)
        QCoreApplication::processEvents(QEventLoop::AllEvents,100);
}

//读取ocd目录下的配置文件列表，添加到下拉框
void MainWindow::loadConfFileList()
{
    ui->cb_interface->clear();
    ui->cb_target->clear();
    QDir interfaceDir(QCoreApplication::applicationDirPath()+"/openocd/share/openocd/scripts/interface");
    interfaceDir.setFilter(QDir::Files);
    foreach(QFileInfo fileInfo,interfaceDir.entryInfoList())
        ui->cb_interface->addItem(fileInfo.fileName());
    QDir targetDir(QCoreApplication::applicationDirPath()+"/openocd/share/openocd/scripts/target");
    targetDir.setFilter(QDir::Files);
    foreach(QFileInfo fileInfo,targetDir.entryInfoList())
        ui->cb_target->addItem(fileInfo.fileName());
}

//设置axf文件按钮点击，弹出文件选框，读取用户选择的路径
void MainWindow::on_bt_set_axf_clicked()
{
    QFileDialog *fileDialog = new QFileDialog(this);//弹出文件选择框
    fileDialog->setWindowTitle(QStringLiteral("选中文件"));
    fileDialog->setDirectory(".");
    fileDialog->setNameFilters(QStringList()<<tr("AXF/ELF File (*.axf *.elf)")<<tr("所有文件 (*)"));//设置文件过滤器
    fileDialog->setFileMode(QFileDialog::ExistingFile);
    fileDialog->setViewMode(QFileDialog::Detail);
    if(fileDialog->exec())
    {
        QStringList fileList=fileDialog->selectedFiles();
        QString fileName=fileList.at(0);
        QFileInfo info(fileName);
        ui->txt_axf_path->setText(info.filePath());
        axfChosen=true;
        listWindow->loadNewAxfFile(info.filePath());
    }
}

//初始化表格
void MainWindow::initTable()
{
    redrawTable();
    connect(tableModel,SIGNAL(dataChanged(QModelIndex,QModelIndex)),this,SLOT(slotTableEdit(QModelIndex,QModelIndex)));
    ui->tb_var->setModel(tableModel);//绑定表格model
    ui->tb_var->horizontalHeader()->setMinimumSectionSize(150);//设置最小列宽150
}

//重绘表格，设置表头并添加各变量信息
void MainWindow::redrawTable()
{
    QByteArray horiHeaderState=ui->tb_var->horizontalHeader()->saveState();//读出列宽数据

    tableModel->clear();
    tableModel->setColumnCount(5);//设置表格为5列
    tableModel->setHeaderData(0,Qt::Horizontal,"变量名");//设置表头
    tableModel->setHeaderData(1,Qt::Horizontal,"当前值");
    tableModel->setHeaderData(2,Qt::Horizontal,"修改变量");
    tableModel->setHeaderData(3,Qt::Horizontal,"显示图像");
    tableModel->setHeaderData(4,Qt::Horizontal,"图线颜色");

    for(int i=0;i<varList.size();i++)//依次添加各变量信息
    {
        tableModel->setItem(i,0,new QStandardItem(varList.at(i).name));
        tableModel->setItem(i,1,new QStandardItem(varList.at(i).rawValue));
        tableModel->setItem(i,2,new QStandardItem(""));
        QStandardItem *checkItem=new QStandardItem();
        checkItem->setCheckable(true);
        checkItem->setCheckState(varList.at(i).enableScope?Qt::Checked:Qt::Unchecked);
        tableModel->setItem(i,3,checkItem);
        tableModel->setItem(i,4,new QStandardItem(varList.at(i).lineColor.name()));
        tableModel->item(i,4)->setForeground(QBrush(varList.at(i).lineColor));
        tableModel->item(i,0)->setFlags(Qt::ItemIsEnabled);
        tableModel->item(i,1)->setFlags(Qt::ItemIsEnabled);
        tableModel->item(i,4)->setFlags(Qt::ItemIsEnabled);
    }
    int lastRow=varList.size();
    tableModel->setItem(lastRow,0,new QStandardItem(""));//末尾添加空行，用于用户添加变量

    ui->tb_var->horizontalHeader()->restoreState(horiHeaderState);//恢复列宽数据
    ui->tb_var->resizeColumnToContents(0);//根据第一列内容自动调整列宽
}

//更新GDB的display列表
void MainWindow::updateGDBList()
{
    QStringList nameList;
    for(int index=0;index<varList.size();index++)
        nameList<<varList.at(index).name;
    gdb->setDisplayList(nameList);
}

//保存配置到指定路径的文件
void MainWindow::saveToFile(const QString &filename)
{
    QSettings settings(filename,QSettings::IniFormat);
    settings.setIniCodec("GBK");

    settings.beginGroup("Global");//写入全局配置
    settings.setValue("HoriHeader",ui->tb_var->horizontalHeader()->saveState());
    settings.setValue("OpenocdMode",ui->rb_openocd->isChecked());
    settings.setValue("SerialocdMode",ui->rb_serialocd->isChecked());
    settings.setValue("Interface",ui->cb_interface->currentText());
    settings.setValue("Target",ui->cb_target->currentText());
    settings.setValue("AxfChosen",axfChosen);
    settings.setValue("AxfPath",ui->txt_axf_path->text());
    settings.setValue("LogEnabled",ui->cb_log->isChecked());
    settings.setValue("ExtOpenocd",ui->cb_ext_openocd->isChecked());
    settings.setValue("SyncWatch",cbSyncWatch->isChecked());
    settings.setValue("ProjectDir",leProjectDir->text());
    settings.setValue("FastMode",cbFastMode->isChecked());
    settings.setValue("VarNum",varList.size());
    settings.endGroup();
    settings.beginGroup("ConfigWindow");//写入配置窗口数据
    settings.setValue("BaudRate",configWindowParam.baudrate);
    settings.setValue("Databits",configWindowParam.databits);
    settings.setValue("Stopbits",configWindowParam.stopbits);
    settings.setValue("Parity",configWindowParam.parity);
    settings.setValue("SampleFreq",configWindowParam.sampleFreq);
    settings.setValue("GdbPort",configWindowParam.gdbPort);
    settings.setValue("GdbParam",configWindowParam.gdbParam);
    settings.setValue("OcdParam",configWindowParam.ocdParam);
    settings.endGroup();

    for(int index=0;index<varList.size();index++)//写入每个变量的配置信息（不会写入采样点数据）
    {
        const VarInfo *varInfo=&varList.at(index);
        settings.beginGroup(QString("VarInfo%1").arg(index));
        settings.setValue("Name",varInfo->name);
        settings.setValue("EnableScope",varInfo->enableScope);
        settings.setValue("LineColor",varInfo->lineColor.name());
        settings.endGroup();
    }
}

//从指定路径的文件中读取配置
void MainWindow::loadFromFile(const QString &filename)
{
    QSettings settings(filename,QSettings::IniFormat);
    settings.setIniCodec("GBK");

    settings.beginGroup("Global");//读取全局配置
    if(settings.contains("HoriHeader"))
        ui->tb_var->horizontalHeader()->restoreState(settings.value("HoriHeader").toByteArray());
    ui->rb_openocd->setChecked(settings.value("OpenocdMode",true).toBool());
    ui->rb_serialocd->setChecked(settings.value("SerialocdMode",false).toBool());
    ui->cb_interface->setCurrentText(settings.value("Interface").toString());
    ui->cb_target->setCurrentText(settings.value("Target").toString());
    axfChosen=settings.value("AxfChosen",true).toBool();
    ui->txt_axf_path->setText(settings.value("AxfPath").toString());
    ui->cb_log->setChecked(settings.value("LogEnabled",false).toBool());
    ui->cb_ext_openocd->setChecked(settings.value("ExtOpenocd",false).toBool());
    cbSyncWatch->setChecked(settings.value("SyncWatch",false).toBool());
    leProjectDir->setText(settings.value("ProjectDir").toString());
    cbFastMode->setChecked(settings.value("FastMode",true).toBool());
    if (cbSyncWatch->isChecked() && !leProjectDir->text().isEmpty())
        workspaceDbPath = findWorkspaceDb(leProjectDir->text());
    int varNum=settings.value("VarNum").toInt();
    settings.endGroup();
    settings.beginGroup("ConfigWindow");//读取配置窗口数据
    configWindowParam.baudrate=settings.value("BaudRate",115200).toInt();
    configWindowParam.databits=(QSerialPort::DataBits)settings.value("Databits",8).toInt();
    configWindowParam.stopbits=(QSerialPort::StopBits)settings.value("Stopbits",1).toInt();
    configWindowParam.parity=(QSerialPort::Parity)settings.value("Parity",0).toInt();
    configWindowParam.sampleFreq=settings.value("SampleFreq",100).toInt();
    configWindowParam.gdbPort=settings.value("GdbPort",3333).toInt();
    configWindowParam.gdbParam=settings.value("GdbParam","").toString();
    configWindowParam.ocdParam=settings.value("OcdParam","").toString();
    settings.endGroup();

    varList.clear();
    for(int index=0;index<varNum;index++)//依次读取各变量配置
    {
        VarInfo varInfo;
        settings.beginGroup(QString("VarInfo%1").arg(index));
        varInfo.name=settings.value("Name").toString();
        varInfo.enableScope=settings.value("EnableScope").toBool();
        varInfo.lineColor=QColor(settings.value("LineColor").toString());
        settings.endGroup();
        varList.append(varInfo);
    }

    redrawTable();//重绘表格

    if(axfChosen)//刷新选择窗口
        listWindow->loadNewAxfFile(ui->txt_axf_path->text());
}

//导出变量采样数据到指定CSV文件中
bool MainWindow::exportCSV(const QString &filename)
{
    QFile file(filename);
    if(!file.open(QFile::WriteOnly))
        return false;
    QTextStream outStream(&file);//使用流方式输出

    int maxSampleNum=0;
    for(int i=0;i<varList.size();i++)//计算最大采样点数
        if(varList.at(i).samples.size()>maxSampleNum)
            maxSampleNum=varList.at(i).samples.size();

    for(int index=0;index<varList.size();index++)//写入表头
        outStream<<"Timestamp,"<<varList.at(index).name<<',';
    outStream<<'\n';

    for(int row=0;row<maxSampleNum;row++)//依次写入各变量的采样数据
    {
        for(int index=0;index<varList.size();index++)
        {
            const VarInfo &varInfo=varList.at(index);
            if(row<varInfo.samples.size())
                outStream<<varInfo.samples.at(row).timestamp<<','<<varInfo.samples.at(row).value<<',';
            else
                outStream<<",,";
        }
        outStream<<'\n';
    }

    return true;
}

//读取全局配置
void MainWindow::loadGlobalConf()
{
    QSettings settings(QDir::homePath()+"/.linkscope/conf.ini",QSettings::IniFormat);
    settings.setIniCodec("GBK");

    ui->action_show_graph->setChecked(settings.value("GraphWindowOpen",true).toBool());
    ui->action_show_selector->setChecked(settings.value("ListWindowOpen",true).toBool());
    ui->action_show_log->setChecked(settings.value("LogWindowOpen",true).toBool());
    if (settings.contains("DockState"))
        restoreState(settings.value("DockState").toByteArray());
}

//保存全局配置
void MainWindow::saveGlobalConf()
{
    QSettings settings(QDir::homePath()+"/.linkscope/conf.ini",QSettings::IniFormat);
    settings.setIniCodec("GBK");

    settings.setValue("GraphWindowOpen",graphDock->isVisible());
    settings.setValue("ListWindowOpen",listDock->isVisible());
    settings.setValue("LogWindowOpen",logDock->isVisible());
    settings.setValue("DockState",saveState());
}

//复位按钮点击，向gdb发送复位指令
void MainWindow::on_bt_pause_clicked()
{
    if (paused) {
        gdb->runCmd("monitor resume\r\n");
        watchTimer->start();
        tableTimer->start();
        bt_pause->setText("暂停");
        paused = false;
    } else {
        watchTimer->stop();
        tableTimer->stop();
        gdb->runCmd("monitor halt\r\n");
        bt_pause->setText("继续");
        paused = true;
    }
}

void MainWindow::on_bt_reset_clicked()
{
    if(connected)
        gdb->runCmd("monitor reset\r\n");
}

//表格双击事件，进行图线颜色修改
void MainWindow::on_tb_var_doubleClicked(const QModelIndex &index)
{
    if(index.column()==4&&index.row()<varList.size())//点击的是第4列，说明要修改颜色
    {
        QStandardItem *item=tableModel->item(index.row(),index.column());
        QColorDialog colorDialog(this);//弹出颜色选择框，请求用户选择颜色
        colorDialog.setWindowTitle("请选择颜色");
        colorDialog.setCurrentColor(QColor(item->text()));
        if(colorDialog.exec()==QColorDialog::Accepted)//用户确定了颜色选择
        {
            QColor color=colorDialog.selectedColor();//获取用户选择的颜色
            varList[index.row()].lineColor=color;
            item->setForeground(QBrush(color));
            item->setText(color.name());
        }
    }
}

//表格单击事件
void MainWindow::on_tb_var_clicked(const QModelIndex &index)
{
    if(index.column()==3&&index.row()<varList.size())//在第3列上，表示用户切换了绘图使能状态
    {
        varList[index.row()].enableScope=(tableModel->item(index.row(),index.column())->checkState()==Qt::Checked);
    }

    if(index.column()==0&&index.row()<varList.size())//在变量名列，表示用户选中了该变量
    {
        graph->setChosenIndex(index.row());//设置绘图界面关注该变量
    }
    else //若点击其他列则取消选择
    {
        graph->setChosenIndex(-1);
    }
}

//表格右键菜单槽函数
void MainWindow::on_tb_var_customContextMenuRequested(const QPoint &pos)
{
    QModelIndex index=ui->tb_var->indexAt(pos);//获取所点击的单元格索引
    if(index.column()==0 && index.row()<varList.size())
    {
        tablePopMenu->exec(QCursor::pos());//弹出右键菜单
    }
}

//保存配置菜单点击
void MainWindow::on_action_save_triggered()
{
    //弹出文件选择框
    QFileDialog *fileDialog = new QFileDialog(this);
    fileDialog->setWindowTitle(QStringLiteral("选中文件"));
    fileDialog->setDirectory(".");
    fileDialog->setNameFilter(tr("Ini File (*.ini)"));
    fileDialog->setFileMode(QFileDialog::AnyFile);
    fileDialog->setViewMode(QFileDialog::Detail);
    if(fileDialog->exec())//等待用户选择文件
    {
        QStringList fileList=fileDialog->selectedFiles();
        QString fileName=fileList.at(0);
        if(!fileName.endsWith(".ini"))//若用户输入的文件名不以ini结尾则加上后缀
            fileName.append(".ini");
        QFileInfo file(fileName);//判断是否已经存在文件，若存在则弹出覆盖警告框
        if(file.exists())
        {
            QMessageBox messageBox(QMessageBox::NoIcon,"警告", "文件已存在，是否覆盖？",QMessageBox::Yes | QMessageBox::No, NULL);
            if(messageBox.exec()!=QMessageBox::Yes)
                return;
        }
        saveToFile(fileName);//将配置写入文件
    }
}

//导入配置菜单点击
void MainWindow::on_action_load_triggered()
{
    //弹出文件选择框
    QFileDialog *fileDialog = new QFileDialog(this);
    fileDialog->setWindowTitle(QStringLiteral("选中文件"));
    fileDialog->setDirectory(".");
    fileDialog->setNameFilter(tr("Ini File (*.ini)"));
    fileDialog->setFileMode(QFileDialog::ExistingFile);
    fileDialog->setViewMode(QFileDialog::Detail);
    if(fileDialog->exec())//等待用户选择文件
    {
        QStringList fileList=fileDialog->selectedFiles();
        QString fileName=fileList.at(0);
        loadFromFile(fileName);//从文件中读取配置
    }
}

//导出数据菜单点击
void MainWindow::on_action_export_triggered()
{
    //弹出文件选择框
    QFileDialog *fileDialog = new QFileDialog(this);
    fileDialog->setWindowTitle(QStringLiteral("选中文件"));
    fileDialog->setDirectory(".");
    fileDialog->setNameFilter(tr("CSV File (*.csv)"));
    fileDialog->setFileMode(QFileDialog::AnyFile);
    fileDialog->setViewMode(QFileDialog::Detail);
    if(fileDialog->exec())//等待用户选择文件
    {
        QStringList fileList=fileDialog->selectedFiles();
        QString fileName=fileList.at(0);
        if(!fileName.endsWith(".csv"))//若用户输入的文件名不以csv结尾则加上后缀
            fileName.append(".csv");
        QFileInfo file(fileName);//判断是否已经存在文件，若存在则弹出覆盖警告框
        if(file.exists())
        {
            QMessageBox messageBox(QMessageBox::NoIcon,"警告", "文件已存在，是否覆盖？",QMessageBox::Yes | QMessageBox::No, NULL);
            if(messageBox.exec()!=QMessageBox::Yes)
                return;
        }
        if(!exportCSV(fileName))//将数据写入文件
        {
            QMessageBox::information(this,"错误","文件打开失败，请检查文件是否被占用");
        }
    }
}

//关于菜单栏点击
void MainWindow::on_action_about_triggered()
{
    AboutWindow().exec();
}

//帮助菜单栏点击
void MainWindow::on_action_help_triggered()
{
    HelpWindow().exec();
}

//刷新连接配置菜单点击
void MainWindow::on_action_refresh_conf_triggered()
{
    loadConfFileList();
}

//转到主页菜单点击
void MainWindow::on_action_homepage_triggered()
{
    QDesktopServices::openUrl(QUrl("https://gitee.com/skythinker/link-scope"));//打开仓库主页
}

//设置QSS全局样式
void MainWindow::setStylesheet()
{
    QFile qss(":/qss/light-blue.qss");
    if(qss.open(QIODevice::ReadOnly))
        qApp->setStyleSheet(QLatin1String(qss.readAll()));
}

//反馈菜单点击
void MainWindow::on_action_feedback_triggered()
{
    QDesktopServices::openUrl(QUrl("https://support.qq.com/product/378753"));//打开反馈页面
}

//删除选中变量槽函数
void MainWindow::on_action_del_var_triggered()
{
    if(ui->tb_var->currentIndex().column()==0)//确定已选中某个变量的变量名
    {
        int index=ui->tb_var->currentIndex().row();//获取所选变量的下标
        if(index<varList.size())
        {
            varList.removeAt(index);//删除变量
            redrawTable();

            if(connected)//若正在连接状态则向gdb发送新的变量列表
                updateGDBList();
        }
    }
}

//检查更新
void MainWindow::checkUpdate()
{
    QNetworkAccessManager *manager=new QNetworkAccessManager;
    if(manager->networkAccessible()!=QNetworkAccessManager::Accessible)
        manager->setNetworkAccessible(QNetworkAccessManager::Accessible);
    connect(manager,&QNetworkAccessManager::finished,
        [=](QNetworkReply *reply){ //解析收到的网络数据
            if(reply->error()==QNetworkReply::NoError)
            {
                QString raw=reply->readAll();
                QRegExp rx("\"tag_name\":\"v([0-9.]+)\"");//正则匹配原始字符串中的版本号
                rx.setMinimal(true);
                if(rx.indexIn(raw)!=-1)
                {
                    QString tag=rx.cap(1);//截取出版本号字符串
                    if(!tag.isEmpty())
                    {
                        if(tag!=APP_VERSION)//与宏定义中的当前版本号进行比较
                        {
                            QMessageBox msgBox;
                            msgBox.setWindowTitle("提示");
                            msgBox.setText("检查到新版本 V"+tag+" ，是否去更新？");
                            msgBox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
                            if(msgBox.exec()==QMessageBox::Yes)
                            {
                                QDesktopServices::openUrl(QUrl("https://gitee.com/skythinker/link-scope/releases"));//打开仓库release页面
                            }
                        }
                        else
                        {
                            QMessageBox::information(this,"提示","当前已是最新版本");
                        }
                    }
                }
                else//正则匹配失败，说明服务器返回的格式有误，无法继续解析
                {
                    QMessageBox::warning(this,"错误","检查失败，建议点击菜单转到主页查看");
                }
            }
            else
            {
                QMessageBox::warning(this,"错误","网络错误，请检查网络");
            }
        }
    );
    QNetworkRequest request;
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    request.setUrl(QUrl("https://gitee.com/api/v5/repos/skythinker/link-scope/releases/latest"));//发送get请求到gitee服务器
    manager->get(request);
}

//检查后台是否存在openocd进程，存在的话询问用户是否关闭
void MainWindow::checkOpenocdProcess()
{
#ifdef Q_OS_WIN32
    QProcess listProcess(0);//使用tasklist查找openocd进程
    listProcess.setProgram("tasklist");
    listProcess.setNativeArguments("/fi \"imagename eq openocd.exe\"");
    listProcess.start();
    listProcess.waitForFinished();
    if(listProcess.readAllStandardOutput().contains("openocd.exe"))//后台存在openocd进程
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle("提示");
        msgBox.setText("发现正在运行的OpenOCD进程，可能是由于上次不正常退出导致的，是否强制结束？（若您没有其他正在调试的程序建议结束）");
        msgBox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
        if(msgBox.exec()==QMessageBox::Yes)
        {
            QProcess killProcess(0);//用taskkill强行结束进程
            killProcess.setProgram("taskkill");
            killProcess.setNativeArguments("/F /IM openocd.exe");
            killProcess.start();
            killProcess.waitForFinished();
        }
    }
#else
    Q_UNUSED(this);
#endif
}

//检查更新菜单点击
void MainWindow::on_action_checkupdate_triggered()
{
    checkUpdate();
}

//调试器模式选择切换
void MainWindow::on_rb_openocd_toggled(bool checked)
{
    ui->box_openocd->setEnabled(checked);
}

//串口模式选择切换
void MainWindow::on_rb_serialocd_toggled(bool checked)
{
    ui->box_serial->setEnabled(checked);
}

//刷新串口点击
void MainWindow::on_bt_refresh_serial_clicked()
{
    ui->cb_com->clear();
    ui->cb_com->addItems(serialocd->getSerialList());
}

//日志使能状态切换
void MainWindow::on_cb_log_toggled(bool checked)
{
    if(connected)
    {
        if(checked)
            logTimer->start();
        else
            logTimer->stop();
    }
}

//内外openocd进程切换
void MainWindow::on_cb_ext_openocd_toggled(bool checked)
{
    ui->cb_interface->setEnabled(!checked);
    ui->cb_target->setEnabled(!checked);
}

//高级配置菜单点击
void MainWindow::on_action_config_triggered()
{
    ConfigWindow configWindow;
    // 同步当前主窗口设置到 configWindowParam
    configWindowParam.interface = ui->cb_interface->currentText();
    configWindowParam.target = ui->cb_target->currentText();
    configWindowParam.extOpenocd = ui->cb_ext_openocd->isChecked();
    configWindowParam.syncWatch = cbSyncWatch->isChecked();
    configWindowParam.projectDir = leProjectDir->text();
    configWindowParam.fastMode = fastMode;
    configWindowParam.openocdMode = ui->rb_openocd->isChecked();
    configWindowParam.axfPath = ui->txt_axf_path->text();
    configWindowParam.interfaceList.clear();
    configWindowParam.targetList.clear();
    for (int i = 0; i < ui->cb_interface->count(); i++)
        configWindowParam.interfaceList << ui->cb_interface->itemText(i);
    for (int i = 0; i < ui->cb_target->count(); i++)
        configWindowParam.targetList << ui->cb_target->itemText(i);
    configWindow.setParam(configWindowParam);

    if(configWindow.exec()==QDialog::Accepted)
    {
        configWindow.getParam(configWindowParam);
        // 同步回主窗口的隐藏控件
        ui->cb_interface->setCurrentText(configWindowParam.interface);
        ui->cb_target->setCurrentText(configWindowParam.target);
        ui->cb_ext_openocd->setChecked(configWindowParam.extOpenocd);
        cbSyncWatch->setChecked(configWindowParam.syncWatch);
        leProjectDir->setText(configWindowParam.projectDir);
        ui->rb_openocd->setChecked(configWindowParam.openocdMode);
        ui->rb_serialocd->setChecked(!configWindowParam.openocdMode);
        ui->txt_axf_path->setText(configWindowParam.axfPath);
        if (configWindowParam.syncWatch && !configWindowParam.projectDir.isEmpty())
            workspaceDbPath = findWorkspaceDb(configWindowParam.projectDir);
        if (configWindowParam.fastMode != fastMode)
            cbFastMode->setChecked(configWindowParam.fastMode);
        if (!fastMode)
            watchTimer->setInterval(1000/configWindowParam.sampleFreq);
        saveToFile(QDir::homePath()+"/.linkscope/autosave.ini");
    }
}

//清空查看列表
void MainWindow::on_action_del_all_triggered()
{
    varList.clear();//删除所有变量
    redrawTable();//重绘表格

    if(connected)//若正在连接状态则向gdb发送新的变量列表
        updateGDBList();
}

// 扫描所有VSCode workspaceStorage，找到匹配项目目录的state.vscdb路径
QString MainWindow::findWorkspaceDb(const QString &projectDir)
{
    QString wsRoot = QDir::homePath() + "/Library/Application Support/Code/User/workspaceStorage";
    QDir wsDir(wsRoot);
    if (!wsDir.exists()) return "";

    QString folderUri = "file://" + projectDir;
    QFileInfoList subDirs = wsDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (QFileInfo info, subDirs) {
        QFile wsJson(info.absoluteFilePath() + "/workspace.json");
        if (wsJson.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(wsJson.readAll());
            wsJson.close();
            if (doc.object().value("folder").toString() == folderUri) {
                return info.absoluteFilePath() + "/state.vscdb";
            }
        }
    }
    return "";
}

// 递归提取watchTree中的变量名。struct/class 自动展开为其成员（用 expr 字段获取 p.x 等全名）
void MainWindow::extractVarNamesFromNode(const QJsonObject &node, QStringList &varList)
{
    QJsonArray children = node.value("children").toArray();
    if (!children.isEmpty()) {
        // 有子节点 → struct/class，不添加自身，递归展开成员
        for (int i = 0; i < children.size(); i++)
            extractVarNamesFromNode(children[i].toObject(), varList);
    } else {
        // 叶子节点 → 普通变量，取 expr（如 p.x、mdelay_time）
        QString expr = node.value("expr").toString();
        if (!expr.isEmpty())
            varList.append(expr);
    }
}

// 从VSCode state.vscdb读取Cortex-Debug watch变量并同步
void MainWindow::syncWatchVars()
{
    if (workspaceDbPath.isEmpty() || !cbSyncWatch->isChecked() || !connected)
        return;

    QProcess sqlite;
    sqlite.setProgram("sqlite3");
    sqlite.setArguments({workspaceDbPath,
        "SELECT value FROM ItemTable WHERE key='marus25.cortex-debug';"});
    sqlite.start();
    if (!sqlite.waitForFinished(3000))
        return;

    QByteArray output = sqlite.readAllStandardOutput();
    if (output.isEmpty()) return;

    QJsonDocument doc = QJsonDocument::fromJson(output);
    QJsonObject watchTree = doc.object().value("livewatch.watchTree").toObject();

    QStringList watchVars;
    extractVarNamesFromNode(watchTree, watchVars);

    // 自动展开 struct → p.x, p.y, p.color.r, p.color.g（支持多级嵌套）
    QStringList expandedVars;
    QStringList pending = watchVars;
    while (!pending.isEmpty()) {
        QString varName = pending.takeFirst();
        QString ptype = gdb->runCmd("ptype " + varName + "\r\n");
        if (ptype.contains("struct") && ptype.contains("{")) {
            // 是 struct → 解析字段名，加入待展开队列
            QRegExp fieldRx("\\n\\s+(?:const\\s+)?(?:volatile\\s+)?\\w+(?:\\s+\\w+)*\\s+(\\w+)(?:\\[\\d+\\])?;");
            int pos = 0;
            while ((pos = fieldRx.indexIn(ptype, pos)) != -1) {
                pending.append(varName + "." + fieldRx.cap(1));
                pos += fieldRx.matchedLength();
            }
        } else {
            // 叶子节点 → 加入结果列表
            expandedVars.append(varName);
        }
    }

    // 对比已有列表，加入新变量
    bool changed = false;
    foreach (QString varName, expandedVars) {
        bool found = false;
        for (int i = 0; i < varList.size(); i++) {
            if (varList[i].name == varName) { found = true; break; }
        }
        if (!found) {
            VarInfo info;
            info.name = varName;
            info.enableScope = true;
            static const QColor palette[] = {
                QColor("#E53935"), QColor("#43A047"), QColor("#1E88E5"),
                QColor("#FB8C00"), QColor("#8E24AA"), QColor("#00ACC1"),
                QColor("#F4511E"), QColor("#3949AB"), QColor("#00897B"),
                QColor("#6D4C41"), QColor("#C0CA33"), QColor("#5C6BC0"),
            };
            info.lineColor = palette[varList.size() % 12];
            varList.append(info);
            changed = true;
        }
    }
    // 移除不在watch中的变量
    for (int i = varList.size() - 1; i >= 0; i--) {
        if (!expandedVars.contains(varList[i].name)) {
            varList.removeAt(i);
            changed = true;
        }
    }
    if (changed) {
        if (fastMode) resolveVarAddresses();  // 新增变量需要解析地址和类型
        redrawTable();
        updateGDBList();
    }
}

void MainWindow::on_cb_sync_watch_toggled(bool checked)
{
    leProjectDir->setEnabled(checked);
    btnBrowse->setEnabled(checked);
    if (checked) {
        // 尝试自动找workspace
        if (!leProjectDir->text().isEmpty()) {
            workspaceDbPath = findWorkspaceDb(leProjectDir->text());
            if (workspaceDbPath.isEmpty()) {
                qDebug() << "Cortex sync: no workspace found for" << leProjectDir->text();
            }
        }
        if (connected) syncTimer->start();
    } else {
        syncTimer->stop();
    }
}

void MainWindow::on_bt_browse_project_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择项目目录（含.vscode）",
        leProjectDir->text().isEmpty() ? QDir::homePath() : leProjectDir->text());
    if (!dir.isEmpty()) {
        leProjectDir->setText(dir);
        workspaceDbPath = findWorkspaceDb(dir);
        if (workspaceDbPath.isEmpty())
            qDebug() << "Cortex sync: no VSCode workspace found for" << dir;
        if (cbSyncWatch->isChecked() && connected)
            syncTimer->start();
    }
}

void MainWindow::slotSyncTimer()
{
    syncWatchVars();
}

// 用GDB print &var 解析所有变量地址，供高速模式使用
void MainWindow::resolveVarAddresses()
{
    QString elfPath = ui->txt_axf_path->text();
    qint64 ts = stampTimer->nsecsElapsed() / 1000;

    // 一次性读取 nm -S 输出（含符号大小）
    QProcess nm;
    nm.setProgram("arm-none-eabi-nm");
    nm.setArguments({"-S", elfPath});
    nm.start();
    nm.waitForFinished(5000);
    QString nmOutput = nm.readAllStandardOutput();

    for (int i = 0; i < varList.size(); i++) {
        if (varList[i].address) continue;  // 已有地址，跳过

        // ---- 解析 nm -S 输出获取地址和大小 ----
        // nm -S 格式: "20000014 00000004 b u32_val"
        QRegExp nmRx(QString("^([0-9a-fA-F]+)\\s+([0-9a-fA-F]+)\\s+\\w\\s+%1$")
                        .arg(QRegExp::escape(varList[i].name)));
        nmRx.setMinimal(true);
        QTextStream ts_nm(&nmOutput);
        while (!ts_nm.atEnd()) {
            QString line = ts_nm.readLine();
            if (nmRx.indexIn(line) != -1) {
                bool ok;
                varList[i].address = nmRx.cap(1).toUInt(&ok, 16);
                varList[i].size    = nmRx.cap(2).toUInt(&ok, 16);
                if (varList[i].size == 0 || varList[i].size > 8)
                    varList[i].size = 4;
                break;
            }
        }

        // ---- nm 找不到（struct 成员如 p.x），用 GDB 兜底 ----
        if (varList[i].address == 0) {
            QString resp = gdb->runCmd("print &" + varList[i].name + "\r\n");
            QRegExp gdbAddr("0x([0-9a-fA-F]+)");
            if (gdbAddr.indexIn(resp) != -1) {
                bool ok;
                varList[i].address = gdbAddr.cap(1).toUInt(&ok, 16);
            }
            // GDB print sizeof 获取精确大小
            QString sizeResp = gdb->runCmd(
                QString("print sizeof(%1)\r\n").arg(varList[i].name));
            QRegExp sizeRx("=\\s*(\\d+)");
            if (sizeRx.indexIn(sizeResp) != -1) {
                varList[i].size = (uint8_t)sizeRx.cap(1).toUInt();
            }
            // 检测类型：nm 输出的符号表中也能看到类型字母（D=数据, b=BSS等）
            // 这里靠 GDB 查找类型字符串
        }

        // ---- 检测 signed/float 属性（用 GDB ptype 获取精确类型）----
        if (varList[i].address) {
            QString ptype = gdb->runCmd("whatis " + varList[i].name + "\r\n");
            QString tl = ptype.toLower();
            if (tl.contains("float") || tl.contains("double"))
                varList[i].isFloat = true;
            else if (tl.contains("int") && !tl.contains("uint"))
                varList[i].isSigned = true;
            logWindow->addLog('I', "FastMode",
                QString("  type %1 → %2 float=%3 signed=%4")
                .arg(varList[i].name).arg(tl.trimmed().left(80))
                .arg(varList[i].isFloat).arg(varList[i].isSigned),
                ts, "resolveVarAddresses");
        }

        // ---- 日志 ----
        if (varList[i].address) {
            logWindow->addLog('I', "FastMode",
                QString("解析 %1 → 0x%2 (%3B)").arg(varList[i].name)
                .arg(varList[i].address, 8, 16, QChar('0')).arg(varList[i].size),
                ts, "resolveVarAddresses");
        } else {
            logWindow->addLog('W', "FastMode",
                QString("无法解析 %1").arg(varList[i].name), ts, "resolveVarAddresses");
        }
    }
}

// 解析 monitor mdw 的返回值，如 "0x20000004: 00000003 " → "3"
QString MainWindow::valueToString(uint32_t raw, const VarInfo &var)
{
    if (var.isFloat) {
        float f;
        memcpy(&f, &raw, 4);
        return QString::number(f, 'f', 2);
    }
    if (var.isSigned) {
        if (var.size == 1)      return QString::number((int8_t)raw);
        else if (var.size == 2) return QString::number((int16_t)raw);
        else                    return QString::number((int32_t)raw);
    }
    return QString::number(raw);
}

void MainWindow::on_cb_fast_mode_toggled(bool checked)
{
    fastMode = checked;
    if (fastMode) {
        watchTimer->setInterval(0);  // 以最快速度触发，不限Hz
        if (connected) {
            resolveVarAddresses();
            if (cbSyncWatch->isChecked())
                syncWatchVars();
        }
    } else {
        watchTimer->setInterval(1000 / configWindowParam.sampleFreq);
    }
}
