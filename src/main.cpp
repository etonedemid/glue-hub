#include <QApplication>
#include <QFile>
#include "hubwindow.h"
#include "gamerprofile.h"
#include "profiledialog.h"
#include "thememanager.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("ReXGlue Hub");
    app.setOrganizationName("etonedemid");

    // Load theme (loads saved preference, defaults to bundled QSS)
    ThemeManager::instance().loadPreference();

    // Check if gamer profile exists, prompt creation if not
    auto& gph = GamerProfileHub::instance();
    if (!gph.profileExists()) {
        ProfileDialog dlg;
        if (dlg.exec() == QDialog::Accepted) {
            gph.createProfile(dlg.gamertag(), dlg.selectedGamepic());
        } else {
            // Create default profile if user cancels
            gph.createProfile("Player");
        }
    } else {
        gph.load();
    }

    HubWindow w;
    w.show();

    return app.exec();
}
