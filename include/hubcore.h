#pragma once
#include <stdint.h>
#include "config.h"

namespace hub {


enum class State : uint8_t { Pairing = 0, Connected = 1, Streaming = 2, Muted = 3, LowBatt = 4, Shutdown = 5, IdleSleep = 6 };
enum class Gesture : uint8_t { None = 0, Click = 1, Double = 2, Triple = 3, Hold = 4, PairHold = 5 };

struct Actions {
  bool dacMute = false, dacUnmute = false;
  bool avrcpPlayPause = false, avrcpNext = false, avrcpPrev = false;
  bool avrcpPause = false, avrcpPlay = false;
  bool cycleEq = false, enterPairing = false, deepSleep = false;
  Gesture gesture = Gesture::None;  
};

struct LedDuty { uint8_t blue = 0, red = 0; };

class GestureDecoder {
 public:
  Gesture update(bool pressedRaw, uint32_t now) {
    if (pressedRaw != cand_) { cand_ = pressedRaw; candSince_ = now; }
    bool changed = false;
    if (cand_ != stable_ && (uint32_t)(now - candSince_) >= cfg::DEBOUNCE_MS) {
      stable_ = cand_;
      changed = true;
    }
    if (changed && stable_) { pressStart_ = now; pairSent_ = false; }

    if (stable_ && !pairSent_ && (uint32_t)(now - pressStart_) >= cfg::PAIR_HOLD_MS) {
      pairSent_ = true;
      clicks_ = 0;
      return Gesture::PairHold;
    }
    if (changed && !stable_) {  
      uint32_t held = now - pressStart_;
      if (pairSent_) { clicks_ = 0; return Gesture::None; }
      if (held >= cfg::LONG_PRESS_MS) { clicks_ = 0; return Gesture::Hold; }
      if (clicks_ < 3) clicks_++;
      lastRelease_ = now;
    }
    if (!stable_ && clicks_ > 0 && (uint32_t)(now - lastRelease_) >= cfg::CLICK_WINDOW_MS) {
      Gesture g = clicks_ == 1 ? Gesture::Click : (clicks_ == 2 ? Gesture::Double : Gesture::Triple);
      clicks_ = 0;
      return g;
    }
    return Gesture::None;
  }

 private:
  bool cand_ = false, stable_ = false, pairSent_ = false;
  uint32_t candSince_ = 0, pressStart_ = 0, lastRelease_ = 0;
  uint8_t clicks_ = 0;
};

class BatteryMonitor {
 public:
  void reset(float v, uint32_t now) {
    filt_ = v; init_ = true; last_ = now;
    low_ = shut_ = belowLow_ = belowShut_ = false;
  }
  void update(float v, uint32_t now) {
    if (!init_) { filt_ = v; init_ = true; last_ = now; }
    uint32_t dt = now - last_;
    last_ = now;
    filt_ += ((float)dt / ((float)cfg::BATT_FILTER_MS + (float)dt)) * (v - filt_);

    if (!low_) {
      if (filt_ < cfg::LOW_BATT_V) {
        if (!belowLow_) { belowLow_ = true; lowSince_ = now; }
        else if ((uint32_t)(now - lowSince_) >= cfg::BATT_HOLD_MS) low_ = true;
      } else {
        belowLow_ = false;
      }
    } else if (filt_ > cfg::LOW_BATT_CLEAR_V) {
      low_ = false; belowLow_ = false;
    }

    if (!shut_) {  
      if (filt_ < cfg::SHUTDOWN_V) {
        if (!belowShut_) { belowShut_ = true; shutSince_ = now; }
        else if ((uint32_t)(now - shutSince_) >= cfg::BATT_HOLD_MS) shut_ = true;
      } else {
        belowShut_ = false;
      }
    }
  }
  float volts() const { return filt_; }
  bool lowBatt() const { return low_; }
  bool shutdown() const { return shut_; }
  int percent() const {
    static const struct { float v; uint8_t pct; } t[] = {
        {3.45f, 0}, {3.55f, 10}, {3.65f, 20}, {3.75f, 40}, {3.85f, 60}, {3.95f, 80}, {4.10f, 100}};
    const int n = (int)(sizeof(t) / sizeof(t[0]));
    if (filt_ <= t[0].v) return 0;
    if (filt_ >= t[n - 1].v) return 100;
    for (int i = 1; i < n; i++) {
      if (filt_ <= t[i].v) {
        float f = (filt_ - t[i - 1].v) / (t[i].v - t[i - 1].v);
        return (int)(t[i - 1].pct + f * (t[i].pct - t[i - 1].pct) + 0.5f);
      }
    }
    return 100;
  }

 private:
  float filt_ = 0;
  bool init_ = false, low_ = false, shut_ = false, belowLow_ = false, belowShut_ = false;
  uint32_t last_ = 0, lowSince_ = 0, shutSince_ = 0;
};

class LedEngine {
 public:
  void connected(uint32_t now) { hsStart_ = now; hsActive_ = true; }

  LedDuty frame(uint32_t now, State s, bool lowBatt, bool discoverable, bool dark) {
    LedDuty d;
    if (dark || s == State::Shutdown || s == State::IdleSleep || s == State::Muted) return d;
    if (s == State::Pairing) {
      if (discoverable) d.blue = blink(now);
    } else if (hsActive_) {
      uint32_t t = now - hsStart_;
      if (t < cfg::LED_HANDSHAKE_MS) d.blue = breathe(t, cfg::LED_BREATHE_PERIOD_MS);
      else hsActive_ = false;
    }
    if (lowBatt) d.red = breathe(now, cfg::LED_LOWBATT_PERIOD_MS);
    return d;
  }


  static uint8_t breathe(uint32_t t, uint32_t period) {
    uint32_t ph = t % period, half = period / 2;
    uint32_t tri = ph < half ? ph * 255 / half : (period - ph) * 255 / half;
    return (uint8_t)((tri * tri) / 255);
  }

  static uint8_t blink(uint32_t now) {
    uint32_t ph = now % 1200;
    return (ph < 100 || (ph >= 200 && ph < 300)) ? 255 : 0;
  }

 private:
  uint32_t hsStart_ = 0;
  bool hsActive_ = false;
};

class HubCore {
 public:
  
  Actions begin(uint32_t now, bool hasBond, float vbat) {
    Actions a;
    bat_.reset(vbat, now);
    discoverable_ = !hasBond;
    activity_ = now;
    a.dacMute = true;
    if (vbat < cfg::SHUTDOWN_V) { shutdown_ = true; a.deepSleep = true; }
    recompute();
    return a;
  }

  Actions onAudioConnected(uint32_t now) {
    Actions a;
    linked_ = true; discoverable_ = false; activity_ = now;
    led_.connected(now);
    if (!muted_ && !shutdown_) a.dacUnmute = true;
    recompute();
    return a;
  }

  Actions onAudioDisconnected(uint32_t now) {
    Actions a;
    linked_ = false; flowing_ = false; pausedByHub_ = false; activity_ = now;
    a.dacMute = true;
    recompute();
    return a;
  }

  Actions onAudioFlow(bool flowing, uint32_t now) {
    Actions a;
    flowing_ = flowing; activity_ = now;
    recompute();
    return a;
  }

  Actions onMuteSwitch(bool muted, uint32_t now) {
    Actions a;
    if (muted == muted_) return a;
    activity_ = now;
    if (muted) {
      if (state_ == State::Streaming) { a.avrcpPause = true; pausedByHub_ = true; }
      a.dacMute = true;
    } else {
      if (linked_ && !shutdown_) a.dacUnmute = true;
      if (pausedByHub_) { a.avrcpPlay = true; pausedByHub_ = false; }  
    }
    muted_ = muted;
    recompute();
    return a;
  }

  Actions onButton(bool pressedRaw, uint32_t now) {
    Gesture g = gestures_.update(pressedRaw, now);
    Actions a;
    if (g == Gesture::None) return a;
    a.gesture = g;
    activity_ = now;
    switch (g) {
      case Gesture::Click:    a.avrcpPlayPause = linked_; break;
      case Gesture::Double:   a.avrcpNext = linked_; break;
      case Gesture::Triple:   a.avrcpPrev = linked_; break;
      case Gesture::Hold:     a.cycleEq = true; break;
      case Gesture::PairHold: discoverable_ = true; a.enterPairing = true; break;
      default: break;
    }
    return a;
  }

  Actions onBattery(float v, uint32_t now) {
    Actions a;
    bat_.update(v, now);
    if (bat_.shutdown() && !shutdown_) {
      shutdown_ = true; a.dacMute = true; a.deepSleep = true;
    }
    recompute();
    return a;
  }


  Actions tick(uint32_t now) {
    Actions a;
    if (!shutdown_ && !sleeping_ && !flowing_ && (uint32_t)(now - activity_) >= cfg::IDLE_SLEEP_MS) {
      sleeping_ = true; a.dacMute = true; a.deepSleep = true;
      recompute();
    }
    return a;
  }

  LedDuty led(uint32_t now, bool dark) { return led_.frame(now, state_, bat_.lowBatt(), discoverable_, dark); }

  State state() const { return state_; }
 
  State reportedState() const {
    if (bat_.lowBatt() && (state_ == State::Connected || state_ == State::Streaming)) return State::LowBatt;
    return state_;
  }
  bool discoverable() const { return discoverable_; }
  bool muted() const { return muted_; }
  bool lowBatt() const { return bat_.lowBatt(); }
  const BatteryMonitor& battery() const { return bat_; }

 private:
  void recompute() {
    if (shutdown_) state_ = State::Shutdown;
    else if (sleeping_) state_ = State::IdleSleep;
    else if (!linked_) state_ = State::Pairing;
    else if (muted_) state_ = State::Muted;
    else state_ = flowing_ ? State::Streaming : State::Connected;
  }

  State state_ = State::Pairing;
  GestureDecoder gestures_;
  BatteryMonitor bat_;
  LedEngine led_;
  bool linked_ = false, flowing_ = false, muted_ = false, discoverable_ = false;
  bool pausedByHub_ = false, shutdown_ = false, sleeping_ = false;
  uint32_t activity_ = 0;
};

}  // NAMING LATER
