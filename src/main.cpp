#include <QApplication>
#include <QFile>
#include <QSettings>
#include "hubwindow.h"
#include "gamerprofile.h"
#include "profiledialog.h"
#include "profilechooser.h"
#include "thememanager.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Glue Hub");
    app.setOrganizationName("etonedemid");

    // Load theme (loads saved preference, defaults to bundled QSS)
    ThemeManager::instance().loadPreference();

    auto& gph = GamerProfileHub::instance();

    // Migrate legacy flat layout to per-XUID folders
    gph.migrateLegacyData();

    QSettings settings;
    quint64 rememberedXuid = settings.value("profile/remembered_xuid", 0).toULongLong();

    auto profiles = gph.enumerateProfiles();

    if (rememberedXuid != 0) {
        // User chose "remember forever" — use their saved XUID
        gph.setActiveXuid(rememberedXuid);
        if (!gph.load()) {
            // Remembered profile gone, clear and fall through
            settings.remove("profile/remembered_xuid");
            rememberedXuid = 0;
        }
    }

    if (rememberedXuid == 0) {
        if (profiles.isEmpty()) {
            // No profiles in index — try loading the default XUID (B13EBABEBABEBABE)
            // in case the user already has data from a previous install.
            if (!gph.load()) {
                // No existing data — create a fresh profile at the default XUID
                ProfileDialog dlg;
                if (dlg.exec() == QDialog::Accepted) {
                    gph.createProfile(dlg.gamertag(), dlg.selectedGamepic());
                } else {
                    gph.createProfile("Player");
                }
            }
        } else if (profiles.size() == 1) {
            // Single profile — auto-select
            gph.setActiveXuid(profiles[0].xuid);
            gph.load();
        } else {
            // Multiple profiles — show chooser
            ProfileChooser chooser(profiles);
            if (chooser.exec() == QDialog::Accepted) {
                if (chooser.selectedXuid() == 0) {
                    // "Create New" was clicked
                    ProfileDialog dlg;
                    if (dlg.exec() == QDialog::Accepted) {
                        gph.createProfile(dlg.gamertag(), dlg.selectedGamepic());
                    } else {
                        gph.createProfile("Player");
                    }
                } else {
                    gph.setActiveXuid(chooser.selectedXuid());
                    gph.load();
                }

                if (chooser.rememberForever()) {
                    settings.setValue("profile/remembered_xuid", gph.activeXuid());
                }
            } else {
                // Cancelled — use first profile
                gph.setActiveXuid(profiles[0].xuid);
                gph.load();
            }
        }
    }

    HubWindow w;
    w.show();

    return app.exec();
}
