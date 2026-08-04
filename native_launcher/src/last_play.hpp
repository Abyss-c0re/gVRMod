#pragma once
// G11: Quick Play — pure parse/format for last map + gfx snapshot (offline-tested).
// File I/O lives with the UI; this header has no OpenXR / filesystem deps.
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>

struct LastPlaySnapshot {
  std::string map = "gm_construct";
  std::string gamemode = "sandbox";
  int maxPlayers = 1;
  bool svLan = true;
  bool p2p = false;
  bool p2pFriends = false;

  int gfxPreset = 2;
  int matPicmip = -1;
  int rRootLod = 0;
  int matAntialias = 4;
  int matForceAniso = 8;
  int matHdrLevel = 2;
  bool shadows = true;
  bool multicore = true;
  int fpsMax = 0;
  int winW = 720;
  int winH = 480;
  bool windowed = true;
  bool noborder = false; // framed by default (pain point: no force -noborder)

  int xrSsIdx = 3;
  float xrViewScale = 1.0f;
  float xrScaleFactor = 1.0f;
  int xrDesktopView = 1;

  bool valid = false;
};

inline void LastPlay_Trim(std::string& s) {
  while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
  if (i) s = s.substr(i);
}

inline std::string LastPlay_Format(const LastPlaySnapshot& s) {
  std::ostringstream o;
  o << "v=1\n"
    << "map=" << s.map << "\n"
    << "gamemode=" << s.gamemode << "\n"
    << "maxplayers=" << s.maxPlayers << "\n"
    << "sv_lan=" << (s.svLan ? 1 : 0) << "\n"
    << "p2p=" << (s.p2p ? 1 : 0) << "\n"
    << "p2p_friends=" << (s.p2pFriends ? 1 : 0) << "\n"
    << "gfx_preset=" << s.gfxPreset << "\n"
    << "mat_picmip=" << s.matPicmip << "\n"
    << "r_rootlod=" << s.rRootLod << "\n"
    << "mat_aa=" << s.matAntialias << "\n"
    << "mat_aniso=" << s.matForceAniso << "\n"
    << "mat_hdr=" << s.matHdrLevel << "\n"
    << "shadows=" << (s.shadows ? 1 : 0) << "\n"
    << "multicore=" << (s.multicore ? 1 : 0) << "\n"
    << "fps_max=" << s.fpsMax << "\n"
    << "win_w=" << s.winW << "\n"
    << "win_h=" << s.winH << "\n"
    << "windowed=" << (s.windowed ? 1 : 0) << "\n"
    << "noborder=" << (s.noborder ? 1 : 0) << "\n"
    << "xr_ss_idx=" << s.xrSsIdx << "\n"
    << "xr_viewscale=" << s.xrViewScale << "\n"
    << "xr_scalefactor=" << s.xrScaleFactor << "\n"
    << "xr_desktopview=" << s.xrDesktopView << "\n";
  return o.str();
}

inline bool LastPlay_Parse(const std::string& body, LastPlaySnapshot& out) {
  out = LastPlaySnapshot{};
  bool gotMap = false;
  std::istringstream in(body);
  std::string line;
  while (std::getline(in, line)) {
    LastPlay_Trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = line.substr(0, eq);
    std::string v = line.substr(eq + 1);
    LastPlay_Trim(k);
    LastPlay_Trim(v);
    if (k == "map") {
      out.map = v;
      gotMap = !v.empty();
    } else if (k == "gamemode")
      out.gamemode = v.empty() ? "sandbox" : v;
    else if (k == "maxplayers")
      out.maxPlayers = std::atoi(v.c_str());
    else if (k == "sv_lan")
      out.svLan = (v != "0");
    else if (k == "p2p")
      out.p2p = (v == "1" || v == "true");
    else if (k == "p2p_friends")
      out.p2pFriends = (v == "1" || v == "true");
    else if (k == "gfx_preset")
      out.gfxPreset = std::atoi(v.c_str());
    else if (k == "mat_picmip")
      out.matPicmip = std::atoi(v.c_str());
    else if (k == "r_rootlod")
      out.rRootLod = std::atoi(v.c_str());
    else if (k == "mat_aa")
      out.matAntialias = std::atoi(v.c_str());
    else if (k == "mat_aniso")
      out.matForceAniso = std::atoi(v.c_str());
    else if (k == "mat_hdr")
      out.matHdrLevel = std::atoi(v.c_str());
    else if (k == "shadows")
      out.shadows = (v != "0");
    else if (k == "multicore")
      out.multicore = (v != "0");
    else if (k == "fps_max")
      out.fpsMax = std::atoi(v.c_str());
    else if (k == "win_w")
      out.winW = std::atoi(v.c_str());
    else if (k == "win_h")
      out.winH = std::atoi(v.c_str());
    else if (k == "windowed")
      out.windowed = (v != "0");
    else if (k == "noborder")
      out.noborder = (v == "1" || v == "true");
    else if (k == "xr_ss_idx")
      out.xrSsIdx = std::atoi(v.c_str());
    else if (k == "xr_viewscale")
      out.xrViewScale = std::strtof(v.c_str(), nullptr);
    else if (k == "xr_scalefactor")
      out.xrScaleFactor = std::strtof(v.c_str(), nullptr);
    else if (k == "xr_desktopview")
      out.xrDesktopView = std::atoi(v.c_str());
  }
  if (out.maxPlayers < 1) out.maxPlayers = 1;
  if (out.winW < 320) out.winW = 720;
  if (out.winH < 240) out.winH = 480;
  if (out.xrSsIdx < 0) out.xrSsIdx = 0;
  if (out.xrSsIdx > 5) out.xrSsIdx = 5;
  // G23: desktop view enum 1..4 only
  if (out.xrDesktopView < 1) out.xrDesktopView = 1;
  if (out.xrDesktopView > 4) out.xrDesktopView = 4;
  // G18: missing noborder key leaves default false (never invent borderless).
  // Explicit noborder=1 in snapshot is user/opt-in; product never forces it.
  out.valid = gotMap;
  return out.valid;
}
