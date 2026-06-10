## **🥽 [G]VRMod: Ultimate**

<img width="1000" height="1000" alt="15378236_Thumbnail" src="https://github.com/user-attachments/assets/d262fbf2-649e-4ab2-82a7-3e65bbac821a" />




### ⚠️ Optimization Issues

VRMod and its components—such as hand physics, melee attacks, and item interaction—are maintained by different authors. This often results in compatibility issues, broken features, or abandoned modules.

This build focuses on **optimization** by merging essential features from semi-official forks and third-party addons, with an emphasis on performance, cross-platform stability, and code de-duplication.

---

### ✅ Key Features

- Refactored codebase for improved stability and cross-platform compatibility  
- Fixed rendering issues on Linux (native x64 builds)  
- Fully supported on Windows (both x64 and Legacy branches)  
- Improved UI with new rendering settings  
- Cursor stability fixed in spawn menu and popups  
- Better performance and reduced latency across systems  
- Integrated hand collision physics for props (no more unintended prop sounds)
- Added clientside wall collisions for hands and SWEPs   
- Rewritten pickup system:  
    - Manual item pickup  
    - Multiplayer-friendly design  
    - Adds halos for visual clarity
    - Serverside weight limit   
    - Clientside precalculation to reduce server load  
    - Supports picking up NPCs  
- Interactive world buttons
- Keypad tool support 
- Support for dropping and picking up non-VR weapons  
- Melee system overhauled: trace-based with velocity-scaled damage + bonus for weapon impact  
- Functional numpad input in VR
- Glide support
- Motion driving with wheel gripping (engine based vehicles + Glide) Don't forget to bind pickups for grip buttons
- Shooting while driving. (ArcVR works for all vehicles, standard SWEPs work only if collisions allow it, like jalopy or glide motorbikes and some roofless cars) Need to bind "weaponmenu", "reload", "turret" for primary and  "alt_turret" for secondary fire in vehicle tab
- Motion-controlled physgun: rotation and movement based on hand motion  
- Gravity gun now supports prop rotation, just like HL2 VR  
- UI now works correctly while in vehicles (given the mouse click is set in bindings for vehicle)
- Likely more small fixes and improvements under the hood


### 📦 Installation

**Requirements:**

- Ensure your system supports **GMod x64**.
- On native Linux, run the following script first:[GModCEFCodecFix](https://github.com/solsticegamestudios/GModCEFCodecFix)
- For trully native experience, use [Steam-Play-None](https://github.com/Scrumplex/Steam-Play-None)
- Please note that only ALVR is now supported on Linux.
- It's recommended to use latest Linux dev modules or compile your own for Linux.
- If you are on Windows and comming from the original, you can keep the old modules.

**Installation:**

1. Download the latest precompiled modules: [Releases Page](https://github.com/Abyss-c0re/vrmod-module-master/releases)
2. Subscribe to the Workshop addon:
   [Steam Workshop – VRMod](https://steamcommunity.com/sharedfiles/filedetails/?id=3442302711)

**OR**

   Clone or download this repository manually:
   - Rename the folder to `vrmod` (do **not** use dashes `-`)
   - Place it in:
     `./GarrysMod/garrysmod/addons/vrmod`

## Why the New License?

I’ve always loved sharing VRMod-x64 with the community and seeing what everyone creates with it. Unfortunately, a recent situation forced me to rethink the licensing.

A commissioner hired a content creator to make a paid/custom fork of the project. While they were profiting from the work, the commissioner repeatedly pressured me — for free — to debug and fix issues specific to their paid version. This took up hours of my personal time while they monetized the result.

Because of this, I’ve updated the license to a **Custom Restricted Share-Alike License**. Here’s what it means for you:

- ✅ **Non-commercial use is still completely free** — personal play, free mods, community servers, educational use, etc.
- ✅ **All modifications must be shared publicly** (so the community benefits from your improvements).
- ✅ **Commercial use** (monetized content, commissioned forks, paid addons, etc.) now requires my explicit written approval. I’m happy to grant it **for free** if no profit is being made.
- ❌ No more “take the code, profit from it, then demand free support” situations.

If you’d like to use VRMod-x64 commercially or commission a custom fork/addon, just email me at **info@abyss-core.com** and we’ll work something out.

I want VRMod-x64 to keep growing and stay fun for everyone. This change simply protects the time and effort that goes into maintaining it. Thank you for understanding — and thank you for being part of the community! ❤️
