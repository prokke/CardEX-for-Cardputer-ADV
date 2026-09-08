#ifndef FEEDBACK_H
#define FEEDBACK_H

// ==================== AUDIO FEEDBACK ====================
// Short tones through the ADV's ES8311 codec, reached via M5Unified's
// M5Cardputer.Speaker. The board has a real amplifier and speaker that the app
// never used at all.
//
// Every call is a no-op when SOUND_ENABLED is off, and tones are fire and
// forget - nothing here blocks, so the input loop keeps draining the keyboard
// FIFO while a sound plays.
class Feedback {
public:
  static void begin();

  static void key();     // Key press
  static void confirm(); // Action completed
  static void error();   // Action refused or failed
  static void done();    // Long operation finished
};

#endif // FEEDBACK_H
