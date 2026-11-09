// Host tests for hubcore.h. Plain asserts, no framework: run with bash test/host/run.sh
#include <cstdio>
#include <vector>
#include "hubcore.h"

using namespace hub;

static int fails = 0;
#define CHECK(c)                                                  \
  do {                                                            \
    if (!(c)) {                                                   \
      std::printf("FAIL line %d: %s\n", __LINE__, #c);            \
      fails++;                                                    \
    }                                                             \
  } while (0)

// ---------------------------------------------------------------- gestures
static void feed(GestureDecoder& d, uint32_t& t, bool level, uint32_t ms, std::vector<Gesture>& out) {
  for (uint32_t i = 0; i < ms; i += 5) {
    t += 5;
    Gesture g = d.update(level, t);
    if (g != Gesture::None) out.push_back(g);
  }
}

static void tap(GestureDecoder& d, uint32_t& t, uint32_t pressMs, uint32_t gapMs, std::vector<Gesture>& out) {
  feed(d, t, true, pressMs, out);
  feed(d, t, false, gapMs, out);
}

static void test_gestures() {
  {  // single click
    GestureDecoder d; uint32_t t = 0; std::vector<Gesture> g;
    tap(d, t, 80, 600, g);
    CHECK(g.size() == 1 && g[0] == Gesture::Click);
  }
  {  // double click
    GestureDecoder d; uint32_t t = 0; std::vector<Gesture> g;
    tap(d, t, 80, 150, g);
    tap(d, t, 80, 600, g);
    CHECK(g.size() == 1 && g[0] == Gesture::Double);
  }
  {  // triple click
    GestureDecoder d; uint32_t t = 0; std::vector<Gesture> g;
    tap(d, t, 80, 150, g);
    tap(d, t, 80, 150, g);
    tap(d, t, 80, 600, g);
    CHECK(g.size() == 1 && g[0] == Gesture::Triple);
  }
  {  // hold: nothing while pressed, Hold on release
    GestureDecoder d; uint32_t t = 0; std::vector<Gesture> g;
    feed(d, t, true, 1200, g);
    CHECK(g.empty());
    feed(d, t, false, 600, g);
    CHECK(g.size() == 1 && g[0] == Gesture::Hold);
  }
  {  // 4 s hold is still a Hold
    GestureDecoder d; uint32_t t = 0; std::vector<Gesture> g;
    tap(d, t, 4000, 600, g);
    CHECK(g.size() == 1 && g[0] == Gesture::Hold);
  }
  {  // pairing hold fires while held, and the release adds nothing
    GestureDecoder d; uint32_t t = 0; std::vector<Gesture> g;
    feed(d, t, true, 5200, g);
    CHECK(g.size() == 1 && g[0] == Gesture::PairHold);
    feed(d, t, false, 600, g);
    CHECK(g.size() == 1);
  }
  {  // a 10 ms glitch is ignored
    GestureDecoder d; uint32_t t = 0; std::vector<Gesture> g;
    feed(d, t, true, 10, g);
    feed(d, t, false, 600, g);
    CHECK(g.empty());
  }
  {  // four quick clicks cap at triple
    GestureDecoder d; uint32_t t = 0; std::vector<Gesture> g;
    for (int i = 0; i < 3; i++) tap(d, t, 80, 150, g);
    tap(d, t, 80, 600, g);
    CHECK(g.size() == 1 && g[0] == Gesture::Triple);
  }
}

// ---------------------------------------------------------------- battery
static void test_battery() {
  {  // low battery needs filter plus hold time, then clears with hysteresis
    BatteryMonitor b; uint32_t t = 0;
    b.reset(4.0f, t);
    for (int i = 0; i < 100; i++) { t += 100; b.update(3.5f, t); }  // 10 s
    CHECK(!b.lowBatt());
    for (int i = 0; i < 200; i++) { t += 100; b.update(3.5f, t); }  // 30 s total
    CHECK(b.lowBatt());
    CHECK(!b.shutdown());
    for (int i = 0; i < 300; i++) { t += 100; b.update(3.9f, t); }
    CHECK(!b.lowBatt());
  }
  {  // shutdown after a sustained low voltage
    BatteryMonitor b; uint32_t t = 0;
    b.reset(3.7f, t);
    for (int i = 0; i < 400; i++) { t += 100; b.update(3.3f, t); }
    CHECK(b.shutdown());
  }
  {  // a short dip does not shut down
    BatteryMonitor b; uint32_t t = 0;
    b.reset(4.0f, t);
    for (int i = 0; i < 50; i++) { t += 100; b.update(3.3f, t); }
    for (int i = 0; i < 300; i++) { t += 100; b.update(4.0f, t); }
    CHECK(!b.shutdown());
  }
  {  // percent curve
    BatteryMonitor b;
    b.reset(4.2f, 0);  CHECK(b.percent() == 100);
    b.reset(3.3f, 0);  CHECK(b.percent() == 0);
    b.reset(3.75f, 0); CHECK(b.percent() == 40);
    b.reset(3.70f, 0); CHECK(b.percent() == 30);
  }
}

// ---------------------------------------------------------------- LEDs
static void test_leds() {
  {  // handshake: breathes for 5 s, then dark
    LedEngine l; l.connected(0);
    CHECK(l.frame(0, State::Connected, false, false, false).blue == 0);
    CHECK(l.frame(1000, State::Connected, false, false, false).blue >= 250);
    CHECK(l.frame(5000, State::Connected, false, false, false).blue == 0);
    CHECK(l.frame(6000, State::Connected, false, false, false).blue == 0);
  }
  {  // discoverable pairing: double blink, dark when not discoverable
    LedEngine l;
    CHECK(l.frame(50, State::Pairing, false, true, false).blue == 255);
    CHECK(l.frame(150, State::Pairing, false, true, false).blue == 0);
    CHECK(l.frame(250, State::Pairing, false, true, false).blue == 255);
    CHECK(l.frame(700, State::Pairing, false, true, false).blue == 0);
    CHECK(l.frame(50, State::Pairing, false, false, false).blue == 0);
  }
  {  // low battery red pulse
    LedEngine l;
    CHECK(l.frame(0, State::Streaming, true, false, false).red == 0);
    CHECK(l.frame(2000, State::Streaming, true, false, false).red == 255);
    CHECK(l.frame(2000, State::Streaming, false, false, false).red == 0);
  }
  {  // muted, asleep and dark mode are fully dark
    LedEngine l; l.connected(0);
    LedDuty m = l.frame(1000, State::Muted, true, false, false);
    CHECK(m.blue == 0 && m.red == 0);
    LedDuty s = l.frame(1000, State::IdleSleep, true, false, false);
    CHECK(s.blue == 0 && s.red == 0);
    LedDuty d = l.frame(1000, State::Connected, true, false, true);
    CHECK(d.blue == 0 && d.red == 0);
  }
}

// ---------------------------------------------------------------- state machine
static Actions pressCore(HubCore& c, uint32_t& t, uint32_t pressMs, uint32_t gapMs) {
  Actions out;
  for (uint32_t i = 0; i < pressMs; i += 5) { t += 5; Actions a = c.onButton(true, t); if (a.gesture != Gesture::None) out = a; }
  for (uint32_t i = 0; i < gapMs; i += 5) { t += 5; Actions a = c.onButton(false, t); if (a.gesture != Gesture::None) out = a; }
  return out;
}

static void test_core() {
  {  // boot, connect, stream, mute, unmute
    HubCore c; uint32_t t = 1000;
    Actions a = c.begin(t, false, 4.0f);
    CHECK(c.state() == State::Pairing);
    CHECK(c.discoverable());
    CHECK(a.dacMute && !a.deepSleep);

    a = c.onAudioConnected(t + 10);
    CHECK(c.state() == State::Connected);
    CHECK(a.dacUnmute);
    CHECK(!c.discoverable());

    a = c.onAudioFlow(true, t + 20);
    CHECK(c.state() == State::Streaming);

    a = c.onMuteSwitch(true, t + 30);
    CHECK(a.avrcpPause && a.dacMute);
    CHECK(c.state() == State::Muted);
    a = c.onAudioFlow(false, t + 40);
    CHECK(c.state() == State::Muted);

    a = c.onMuteSwitch(false, t + 50);
    CHECK(a.avrcpPlay && a.dacUnmute);
    CHECK(c.state() == State::Connected);

    // muting while not streaming must not pause, and unmuting must not resume
    a = c.onMuteSwitch(true, t + 60);
    CHECK(!a.avrcpPause && a.dacMute);
    a = c.onMuteSwitch(false, t + 70);
    CHECK(!a.avrcpPlay && a.dacUnmute);
  }
  {  // bonded boot is not discoverable; unmute waits for a link
    HubCore c; uint32_t t = 0;
    c.begin(t, true, 4.0f);
    CHECK(!c.discoverable());
    Actions a = c.onMuteSwitch(true, t + 5);
    a = c.onMuteSwitch(false, t + 10);
    CHECK(!a.dacUnmute);  // not linked yet
  }
  {  // button gestures map to media actions only when linked
    HubCore c; uint32_t t = 0;
    c.begin(t, true, 4.0f);
    Actions a = pressCore(c, t, 80, 600);
    CHECK(a.gesture == Gesture::Click && !a.avrcpPlayPause);  // not linked
    c.onAudioConnected(t);
    a = pressCore(c, t, 80, 600);
    CHECK(a.avrcpPlayPause);
    pressCore(c, t, 80, 150);
    a = pressCore(c, t, 80, 600);
    CHECK(a.gesture == Gesture::Double && a.avrcpNext);
    pressCore(c, t, 80, 150);
    pressCore(c, t, 80, 150);
    a = pressCore(c, t, 80, 600);
    CHECK(a.gesture == Gesture::Triple && a.avrcpPrev);
    a = pressCore(c, t, 1200, 600);
    CHECK(a.cycleEq);
    a = pressCore(c, t, 5200, 600);
    CHECK(a.enterPairing && c.discoverable());
  }
  {  // idle sleep
    HubCore c; uint32_t t0 = 5000;
    c.begin(t0, true, 4.0f);
    c.onAudioConnected(t0);
    Actions a = c.tick(t0 + cfg::IDLE_SLEEP_MS - 1);
    CHECK(!a.deepSleep);
    a = c.tick(t0 + cfg::IDLE_SLEEP_MS);
    CHECK(a.deepSleep && a.dacMute);
    CHECK(c.state() == State::IdleSleep);
  }
  {  // no idle sleep while audio flows; the timer restarts when it stops
    HubCore c; uint32_t t0 = 0;
    c.begin(t0, true, 4.0f);
    c.onAudioConnected(t0);
    c.onAudioFlow(true, t0 + 1000);
    Actions a = c.tick(t0 + 2UL * cfg::IDLE_SLEEP_MS);
    CHECK(!a.deepSleep);
    uint32_t t1 = t0 + 2UL * cfg::IDLE_SLEEP_MS;
    c.onAudioFlow(false, t1);
    a = c.tick(t1 + cfg::IDLE_SLEEP_MS - 1);
    CHECK(!a.deepSleep);
    a = c.tick(t1 + cfg::IDLE_SLEEP_MS);
    CHECK(a.deepSleep);
  }
  {  // low battery is reported while connected; shutdown follows a deeper drop
    HubCore c; uint32_t t = 0;
    c.begin(t, true, 4.0f);
    c.onAudioConnected(t);
    for (int i = 0; i < 300; i++) { t += 100; c.onBattery(3.5f, t); }
    CHECK(c.lowBatt());
    CHECK(c.reportedState() == State::LowBatt);
    bool slept = false;
    for (int i = 0; i < 400; i++) { t += 100; if (c.onBattery(3.3f, t).deepSleep) slept = true; }
    CHECK(slept);
    CHECK(c.state() == State::Shutdown);
  }
  {  // a wake with an empty cell goes straight back to sleep
    HubCore c;
    Actions a = c.begin(0, true, 3.3f);
    CHECK(a.deepSleep);
    CHECK(c.state() == State::Shutdown);
  }
}

int main() {
  test_gestures();
  test_battery();
  test_leds();
  test_core();
  if (fails == 0) std::printf("All hubcore tests passed\n");
  else std::printf("%d check(s) failed\n", fails);
  return fails == 0 ? 0 : 1;
}
