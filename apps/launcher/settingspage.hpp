#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QCompleter>
#include <QStringListModel>

#include "ui_settingspage.h"

#include <components/settings/settings.hpp>

namespace Config
{
    class GameSettings;
}

namespace Launcher
{
    class SettingsPage : public QWidget, private Ui::SettingsPage
    {
        Q_OBJECT

    public:
        explicit SettingsPage(Config::GameSettings& gameSettings, QWidget* parent = nullptr);

        bool loadSettings();
        void saveSettings();

    public slots:
        void slotLoadedCellsChanged(QStringList cellNames);

    private slots:
        void on_skipMenuCheckBox_stateChanged(int state);
        void on_runScriptAfterStartupBrowseButton_clicked();
        void slotAnimSourcesToggled(bool checked);
        void slotPostProcessToggled(bool checked);
        void slotSkyBlendingToggled(bool checked);

    private:
        Config::GameSettings& mGameSettings;
        QCompleter mCellNameCompleter;
        QStringListModel mCellNameCompleterModel;

        /**
         * Load the cells associated with the given content files for use in autocomplete
         * @param filePaths the file paths of the content files to be examined
         */
        void loadCellsForAutocomplete(QStringList filePaths);
        static void loadSettingBool(QCheckBox* checkbox, const std::string& setting, const std::string& group);
        static void saveSettingBool(QCheckBox* checkbox, const std::string& setting, const std::string& group);
        static void loadSettingInt(QComboBox* comboBox, const std::string& setting, const std::string& group);
        static void saveSettingInt(QComboBox* comboBox, const std::string& setting, const std::string& group);
        static void loadSettingInt(QSpinBox* spinBox, const std::string& setting, const std::string& group);
        static void saveSettingInt(QSpinBox* spinBox, const std::string& setting, const std::string& group);
    };
}
#endif
