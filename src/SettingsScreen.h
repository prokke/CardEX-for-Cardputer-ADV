#ifndef SETTINGSSCREEN_H
#define SETTINGSSCREEN_H

// ==================== SETTINGS SCREEN ====================
// Modal editor for everything in the Settings registry. Runs its own input
// loop, like the other full-screen views, and writes /CardEX.ini on exit.
//
// It exists because the only way to change a setting used to be to pull the
// card and hand-edit the .ini - and half the keys in that file did nothing.
class SettingsScreen {
public:
  // Blocks until the user leaves. Returns true if anything was changed, so the
  // caller knows to redraw and re-sort.
  static bool run();
};

#endif // SETTINGSSCREEN_H
