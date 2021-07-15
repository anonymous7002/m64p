#include "settingsdialog.h"
#include "mainwindow.h"
#include "interface/core_commands.h"

#include <QPushButton>
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>

void SettingsDialog::handleCoreButton()
{
    QString fileName = QFileDialog::getExistingDirectory(this,
        tr("Locate Core Library"), NULL, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!fileName.isNull()) {
        corePath->setText(fileName);
        w->getSettings()->setValue("coreLibPath", fileName);
    }
}

void SettingsDialog::handlePluginButton()
{
    QString fileName = QFileDialog::getExistingDirectory(this, tr("Locate Plugin Directory"),
                                                         NULL,
                                                         QFileDialog::ShowDirsOnly
                                                         | QFileDialog::DontResolveSymlinks);
    if (!fileName.isNull()) {
        pluginPath->setText(fileName);
        w->getSettings()->setValue("pluginDirPath", fileName);

        w->getSettings()->remove("inputPlugin");
        w->updatePlugins();
    }
}

void SettingsDialog::handleConfigButton()
{
    QString fileName = QFileDialog::getExistingDirectory(this, tr("Set Config Directory"),
                                                         NULL,
                                                         QFileDialog::ShowDirsOnly
                                                         | QFileDialog::DontResolveSymlinks);
    if (!fileName.isNull()) {
        configPath->setText(fileName);
        w->getSettings()->setValue("configDirPath", fileName);
    }
}

void SettingsDialog::handleClearConfigButton()
{
    configPath->setText("");
    w->getSettings()->remove("configDirPath");
}

void SettingsDialog::handleCoreEdit()
{
    w->getSettings()->setValue("coreLibPath", corePath->text());
}

void SettingsDialog::handlePluginEdit()
{
    w->getSettings()->setValue("pluginDirPath", pluginPath->text());

    w->getSettings()->remove("inputPlugin");
    w->updatePlugins();
}

void SettingsDialog::handleConfigEdit()
{
    w->getSettings()->setValue("configDirPath", configPath->text());
}

void SettingsDialog::initStuff()
{
    layout = new QGridLayout(this);
	
	QLabel *inputLabel = new QLabel("Video Plugin", this);
    layout->addWidget(inputLabel,0,0);
    QComboBox *inputChoice = new QComboBox(this);
    Filter.replace(0,"mupen64plus-video*");
    current = PluginDir.entryList(Filter);
    inputChoice->addItems(current);
    QString qtVideoPlugin = w->getSettings()->value("gfxPlugin").toString();
    int my_index = inputChoice->findText(qtVideoPlugin);
    if (my_index == -1) {
        inputChoice->addItem(qtVideoPlugin);
        my_index = inputChoice->findText(qtVideoPlugin);
    }
    inputChoice->setCurrentIndex(my_index);
    connect(inputChoice, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated),
        [=](const QString &text) {
            w->getSettings()->setValue("gfxPlugin", text);
            w->updatePlugins();
    });
    layout->addWidget(inputChoice,0,1);
	
	QLabel *inputLabel = new QLabel("Audio Plugin", this);
    layout->addWidget(inputLabel,1,0);
    QComboBox *inputChoice = new QComboBox(this);
    Filter.replace(0,"mupen64plus-audio*");
    current = PluginDir.entryList(Filter);
    inputChoice->addItems(current);
    QString qtAudioPlugin = w->getSettings()->value("audioPlugin").toString();
    int my_index = inputChoice->findText(qtAudioPlugin);
    if (my_index == -1) {
        inputChoice->addItem(qtAudioPlugin);
        my_index = inputChoice->findText(qtAudioPlugin);
    }
    inputChoice->setCurrentIndex(my_index);
    connect(inputChoice, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated),
        [=](const QString &text) {
            w->getSettings()->setValue("audioPlugin", text);
            w->updatePlugins();
    });
    layout->addWidget(inputChoice,1,1);
	
    QLabel *inputLabel = new QLabel("Input Plugin", this);
    layout->addWidget(inputLabel,2,0);
    QComboBox *inputChoice = new QComboBox(this);
    Filter.replace(0,"mupen64plus-input*");
    current = PluginDir.entryList(Filter);
    inputChoice->addItems(current);
    QString qtInputPlugin = w->getSettings()->value("inputPlugin").toString();
    int my_index = inputChoice->findText(qtInputPlugin);
    if (my_index == -1) {
        inputChoice->addItem(qtInputPlugin);
        my_index = inputChoice->findText(qtInputPlugin);
    }
    inputChoice->setCurrentIndex(my_index);
    connect(inputChoice, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated),
        [=](const QString &text) {
            w->getSettings()->setValue("inputPlugin", text);
            w->updatePlugins();
    });
    layout->addWidget(inputChoice,2,1);
	
	QLabel *inputLabel = new QLabel("RSP Plugin", this);
    layout->addWidget(inputLabel,3,0);
    QComboBox *inputChoice = new QComboBox(this);
    Filter.replace(0,"mupen64plus-rsp*");
    current = PluginDir.entryList(Filter);
    inputChoice->addItems(current);
    QString qtRSPPlugin = w->getSettings()->value("rspPlugin").toString();
    int my_index = inputChoice->findText(qtRSPPlugin);
    if (my_index == -1) {
        inputChoice->addItem(qtRSPPlugin);
        my_index = inputChoice->findText(qtRSPPlugin);
    }
    inputChoice->setCurrentIndex(my_index);
    connect(inputChoice, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated),
        [=](const QString &text) {
            w->getSettings()->setValue("rspPlugin", text);
            w->updatePlugins();
    });
    layout->addWidget(inputChoice,3,1);

    setLayout(layout);
}

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    initStuff();
}

void SettingsDialog::closeEvent(QCloseEvent *event)
{
    w->getSettings()->sync();
    int value;
    if (w->getCoreLib())
    {
        (*CoreDoCommand)(M64CMD_CORE_STATE_QUERY, M64CORE_EMU_STATE, &value);
        if (value == M64EMU_STOPPED)
            w->resetCore();
    }
    else
        w->resetCore();

    event->accept();
}
