#pragma once
// G18: desktop window chrome law (pure, offline-tested).
// Pain point #2: never force GMod -noborder; keep framed title-bar chrome.
// Desktop is a mirror only; HMD is the product surface.
// Product defaults: windowed + framed (noborder=false). User may opt into
// borderless via settings/last_play when the key is present; corrupt/missing
// snapshots never invent noborder=true.
#include <sstream>
#include <string>

struct WindowChromeDecision {
  bool valid = true;
  bool windowed = true;
  bool noborder = false;
  int winW = 720;
  int winH = 480;
  /// Product path must never force -noborder on.
  bool force_noborder_forbidden = true;
  /// none | borderless_opt_in | fullscreen
  std::string risk = "none";
  std::string reason = "ok";
};

struct WindowChromeHmdExpect {
  std::string verdict = "idle"; // idle | expect_framed | expect_borderless_opt_in | expect_fullscreen
  bool expect_title_chrome = true;
  std::string checklist = "G18 · IDLE · no window chrome decision";
  std::string pass_line = "N/A";
  std::string fail_line = "N/A";
};

// Cube pin: framed windowed desktop mirror.
inline bool WindowChrome_CubeWindowed() { return true; }
inline bool WindowChrome_CubeNoborder() { return false; }
inline int WindowChrome_CubeWinW() { return 720; }
inline int WindowChrome_CubeWinH() { return 480; }

/// Product never *forces* -noborder (pain point #2). Unit-only override via force_test context.
inline bool WindowChrome_ShouldForceNoborder(const char* context = "product") {
  if (context && std::string(context) == "force_test") return true;
  return false;
}

/// Missing key → false. Never invent borderless from corrupt/absent snapshot.
inline bool WindowChrome_SanitizeNoborder(bool requested, bool key_present) {
  if (!key_present) return false;
  return requested;
}

inline int WindowChrome_ClampW(int w) {
  if (w < 320) return WindowChrome_CubeWinW();
  if (w > 7680) return 7680;
  return w;
}

inline int WindowChrome_ClampH(int h) {
  if (h < 240) return WindowChrome_CubeWinH();
  if (h > 4320) return 4320;
  return h;
}

/// Pure argv fragment: " -windowed -w W -h H" [+ " -noborder"] or fullscreen variant.
inline std::string WindowChrome_BuildArgs(bool windowed, bool noborder, int w, int h) {
  w = WindowChrome_ClampW(w);
  h = WindowChrome_ClampH(h);
  std::ostringstream win;
  if (windowed)
    win << " -windowed";
  else
    win << " -fullscreen";
  win << " -w " << w << " -h " << h;
  if (noborder) win << " -noborder";
  return win.str();
}

/// Pure decision snapshot.
/// key_present: false when last_play/settings omitted noborder (default framed).
inline WindowChromeDecision WindowChrome_Decide(bool windowed, bool noborder, int w, int h,
                                                bool key_present = true) {
  WindowChromeDecision d;
  d.valid = true;
  d.windowed = windowed;
  d.noborder = WindowChrome_SanitizeNoborder(noborder, key_present);
  d.winW = WindowChrome_ClampW(w);
  d.winH = WindowChrome_ClampH(h);
  d.force_noborder_forbidden = !WindowChrome_ShouldForceNoborder("product");
  if (!d.windowed) {
    d.risk = "fullscreen";
    d.reason = "fullscreen_desktop";
  } else if (d.noborder) {
    d.risk = "borderless_opt_in";
    d.reason = "user_or_snapshot_noborder";
  } else {
    d.risk = "none";
    d.reason = "framed_windowed_pin";
  }
  return d;
}

inline WindowChromeDecision WindowChrome_CubeDefault() {
  return WindowChrome_Decide(WindowChrome_CubeWindowed(), WindowChrome_CubeNoborder(),
                             WindowChrome_CubeWinW(), WindowChrome_CubeWinH(), true);
}

inline std::string WindowChrome_StatusLabel(const WindowChromeDecision& d) {
  if (!d.valid) return "WIN · IDLE";
  if (!d.windowed) return "WIN · FULLSCREEN";
  if (d.noborder) return "WIN · BORDERLESS";
  return "WIN · FRAMED";
}

/// Pure desktop-observer contract (offline ≠ HMD OK).
inline WindowChromeHmdExpect WindowChrome_HmdExpect(const WindowChromeDecision& d) {
  WindowChromeHmdExpect e;
  if (!d.valid) return e;
  if (!d.force_noborder_forbidden) {
    e.verdict = "expect_force_fail";
    e.expect_title_chrome = false;
    e.checklist = "G18 · FORCE NOBORDER VIOLATION";
    e.pass_line = "Must not ship — product never forces -noborder";
    e.fail_line = "Launcher injects -noborder without user opt-in";
    return e;
  }
  if (!d.windowed) {
    e.verdict = "expect_fullscreen";
    e.expect_title_chrome = false;
    e.checklist = "G18 · FULLSCREEN · desktop not framed mirror";
    e.pass_line = "Fullscreen if user chose; HMD still product surface";
    e.fail_line = "Accidental fullscreen from corrupt defaults";
    return e;
  }
  if (d.noborder) {
    e.verdict = "expect_borderless_opt_in";
    e.expect_title_chrome = false;
    e.checklist = "G18 · BORDERLESS OPT-IN · title chrome off";
    e.pass_line = "Only if settings/last_play key present true";
    e.fail_line = "Borderless invented from missing/corrupt snapshot";
    return e;
  }
  e.verdict = "expect_framed";
  e.expect_title_chrome = true;
  e.checklist = "G18 · FRAMED · windowed " + std::to_string(d.winW) + "x" +
                std::to_string(d.winH) + " · title chrome";
  e.pass_line = "Desktop mirror has title bar; movable/resizable; no -noborder";
  e.fail_line = "Forced -noborder or missing windowed flag";
  return e;
}

inline bool WindowChrome_IsForceRisk(const WindowChromeDecision& d) {
  return !d.force_noborder_forbidden;
}
