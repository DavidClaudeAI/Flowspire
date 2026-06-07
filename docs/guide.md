# User guide — Flowspire

> **Languages**: **English** *(this page)* · [Français](guide.fr.md)
> [← Back to README](../README.md)

This guide walks you through everything from A to Z: preparing your scenes, installing the plugin, setting it up with the wizard, and understanding every setting. No need to be technical — we talk in terms of **scenes**, **people** and **behaviors**.

## Contents

1. [Prepare your scenes in OBS](#1-prepare-your-scenes-in-obs)
2. [Install Flowspire](#2-install-flowspire)
3. [Set up with the wizard](#3-set-up-with-the-wizard)
4. [The live dock](#4-the-live-dock)
5. [Force a shot by hand](#5-force-a-shot-by-hand)
6. [Every setting in detail](#6-every-setting-in-detail)
7. [Hotkeys (keyboard & Stream Deck)](#7-hotkeys-keyboard--stream-deck)
8. [Profiles](#8-profiles)
9. [Where settings are stored](#9-where-settings-are-stored)

---

## 1. Prepare your scenes in OBS

**Read this first.** Flowspire **never** creates scenes: it **drives the ones you made**. So before launching the wizard, prepare your scenes in OBS — otherwise you'd start the setup with nothing to map.

### Which scenes should you create?

The rule is simple:

- **One scene per speaker** = that person **center stage** (the "focus"). On the *"Speaker 1 focus"* scene you put speaker 1 center stage; on *"Speaker 2 focus"*, it's speaker 2 center stage; and so on.
- **One "wide shot" scene** (optional) = **everyone on screen** at the same time, used as a fallback when several people speak or nobody speaks. It isn't mandatory, but it makes the result more lively.

That's exactly what the three demo screenshots show: the **wide shot** (everyone), then **Mia in focus**, then **Ryan in focus** — each one is a separate **OBS scene** that you composed to your liking.

| Wide shot | Mia focus | Ryan focus |
| :---: | :---: | :---: |
| ![Wide shot](img/C-wide.png) | ![Mia focus](img/C-Mia.png) | ![Ryan focus](img/C-Ryan.png) |

### Composing each scene's layout

In **each** scene, lay things out however you like: the person large in the center, the others smaller around them, your background, your overlays, the show title, the chat… It's all up to you — Flowspire just **shows the right scene at the right moment**.

> **Variety tip:** you can give **several scenes** to the same person, and Flowspire **alternates** between them (how often each one appears is set with a **weight** — see [Cameras & weights](#cameras--weights)). Two ideas that make the result more natural:
> - a tight *close-up* **and** a *reaction shot* (you see them react, not just speak);
> - **the wide shot itself**, added to their pool with a **very low weight**: now and then, even when they're talking, you cut to a group shot — it lets the picture breathe.
>
> **Example** for speaker 1: *their camera **95** · wide shot **5** · reaction shot **5***. Most of the time you see them in close-up, but it varies just enough to breathe — even more natural.

### Bonus (not required): animated transitions

For a truly **smooth and original** look, you can install the third-party **Move transition** plugin: it enables **animated transitions** between your scenes (the thumbnails slide, grow, smoothly rearrange instead of a plain crossfade). The effect is stunning.

> **This is absolutely not required.** Flowspire works perfectly with OBS's native transitions (cut or fade). "Move transition" is just icing on the cake for those who want to push the production side.

---

## 2. Install Flowspire

1. Download the installer for your system from the [**latest release**](https://github.com/DavidClaudeAI/Flowspire/releases/latest):
   - **Windows**: `flowspire-*-windows-x64.exe` (double-click)
   - **macOS**: `flowspire-*-macos-universal.pkg`
   - **Linux**: `flowspire-*-x86_64-linux-gnu.deb`
2. Run it (it installs the plugin in the right place), then **close and restart OBS**.
3. In OBS, open the **Docks → Flowspire** menu to show the panel.

> ⚠️ **Don't skip step 3 — this is THE gotcha.** After installing, **OBS does not show the dock by itself**. Until you enable it via **Docks → Flowspire** (top menu bar), the plugin is loaded but **nothing appears on screen** — which makes it easy to think the install failed. The moment you show it, the **welcome card** greets you and guides you.

> Requires **OBS 28 or higher**.

### Getting past the "unknown publisher" warning

The plugin is **open source but unsigned** (code signing costs money). On first launch, your system may show a warning — that's normal, you just need to allow it once:

- **Windows (SmartScreen)**: click **"More info"** then **"Run anyway"**.
- **macOS (Gatekeeper)**: double-click the `.pkg` (dismiss the warning), then go to **System Settings → Privacy & Security** → **Security** section (at the bottom) → click **"Open Anyway"** → authenticate. *(On **macOS 15+ / 26**, Apple removed the old right-click → Open bypass; this Settings button is now the only way.)*

---

## 3. Set up with the wizard

**The first time, the dock greets you with a welcome card** ("No directing set up yet"): it reminds you to first prepare your **scenes** and **audio sources** in OBS, tells you how many audio sources it detected, and offers a **Run the assistant** button. As soon as a profile exists, the dock switches to its live dashboard ([section 4](#4-the-live-dock)).

Click **Wizard** in the dock. It creates a **complete configuration**, called a **profile**. You can have **several** of them — one per show type ("Duo", "4 guests + me", "Panel"…) — and **switch from one to another with a single click**; you then manage them (load, rename, duplicate, edit, delete) in the [Profiles section](#8-profiles).

The wizard guides you through **6 screens**. Everything stays editable at any time, and you can relaunch it whenever you want.

> As a safety measure, automatic directing starts **disabled**. As long as no speaker is configured, the plugin stays **read-only** and doesn't touch your scenes.

### Screen 0 — Prerequisites

![Wizard — prerequisites](img/assistant_1.png)

A reminder: first create your **scenes** and your **audio sources** in OBS (see step 1). That's the starting point.

### Screen 1 — Speakers

![Wizard — speakers](img/assistant_2.png)

Give each person a **name** and link them to their OBS **audio source** (the mic or feed we listen to in order to know whether they're speaking). This is the heart of the system.

### Screen 2 — Cameras & weights

![Wizard — cameras and weights](img/assistant_3.png)

For each person, choose **which scenes** to show when they speak, and give each one a **weight** (its chance of appearing relative to that person's other scenes). The percentage is recalculated automatically.

### Screen 3 — Directing

![Wizard — Directing (settings collapsed)](img/assistant_realisation.png)

All the **directing** settings fit on a single, deliberately simple screen. Two things to do; the rest is tucked into a drawer.

- **Wide shot (group view)**: pick the group scene. A **tip** strongly encourages you to set one — it makes directing much livelier (Flowspire alternates with the group view between turns). Without a wide shot, cutting is more static.
- **Directing style**: pick a **built-in** style (Chill / Cool / Speed) or one of **your saved** styles. The style sets the **whole temperament at once**.

That's enough to get going. To go further, expand **"Advanced directing settings"** *(editable later)*:

![Wizard — Directing (advanced drawer expanded)](img/assistant_realisation_advanced.png)

The drawer reveals the **tempo** (min/max shot time, anti ping-pong, silence reaction delay), the priorities **when several speak** / **when nobody speaks**, and the **sensitivity**. It's the exact same content as the **Directing** tab in the Advanced settings ([section 6](#6-every-setting-in-detail)) — picking a style positions all those sliders for you, and moving any slider by hand switches to **"Custom"**.

> Managing your saved styles (save / rename / delete) is done in the **Advanced settings**; in the wizard, you only **pick** a style.

### Screen 4 — Calibrate the thresholds

![Wizard — Calibrate the thresholds](img/assistant_calibration.png)

Just a **reminder**: there's **nothing to set here**. Flowspire detects who is speaking via a per-mic **threshold**, and it finds it **on its own** by listening to the voice. You'll do it in two clicks **later, once the wizard is done, from the dock** — see [section 4](#4-the-live-dock). For now, move on.

### Screen 5 — Summary

![Wizard — summary](img/assistant_6.png)

Everything is summed up and **stays editable**. A button **enables auto-directing**: from there, Flowspire takes over and follows whoever is speaking.

---

## 4. The live dock

![The Flowspire dock](img/dock.png)

The dock is your real-time dashboard:

- **The header** shows the logo and a **switch badge**: *Active* (the plugin is directing), *Paused* (frozen), *Read-only* (no config). **One click** enables or stops auto-directing.
- **The profile selector** lets you switch from one configuration to another.
- **The "Directing" selector** changes the **style** in one click, **live**: a grouped menu lists the **built-in** styles (Chill/Cool/Speed), then, under a **"My styles"** separator, the ones you've saved. Shows **"Custom"** if you've tweaked by hand.
- **Each speaker** has their **audio meter** and their **threshold slider**: the level moves under the slider, so you set the sensitivity **visually**. This adjustment takes effect **immediately** and is **saved** in the profile.
- **The "on-air" indicator (tally)**: a **red bar** appears to the left of the scene actually being broadcast and **moves** when the directing changes shot.
- **The wide shot** appears in the list (without an audio meter), so you can see it light up too.
- **Click a card to force that shot**: clicking a speaker's card puts **them** on air, clicking the wide-shot card switches to the **wide shot** — a **temporary force** (auto resumes after the minimum time), just like a hotkey. Dragging the **threshold slider** stays independent (it doesn't force).
- **Auto-calibrate thresholds**: the **wand button** on each card sets that person's threshold automatically — speak normally and it finds the right level (between your silence and your voice), then freezes (no drift). **Calibrate all thresholds** (button in the SCENES header) arms everyone at once: a progress bar shows who's done; click **Finish** when you're happy. Works with a noise-gated mic too (the level is placed proportionally in the gap, never glued to the floor). Redo it whenever you change **room** or **microphone**.

During calibration, a **progress banner** appears ("calibrated X/N") — click **Finish** when you're happy:

![The dock during auto-calibration](img/dock_calibration.png)

You can also drive it through your **OBS scenes** and your **hotkeys**: the dock stays deliberately uncluttered.

---

## 5. Force a shot by hand

Even in auto-directing, you keep control. The quickest way: **click a card in the Flowspire dock** (a speaker, or the wide-shot card) to put that shot on air. You can also **click any scene in OBS's native scene manager** (or a Stream Deck button) and Flowspire puts it on air:

- If the scene **is part of your directing setup** (a speaker's shot or the wide shot) → **temporary force**: the shot holds for the minimum time, then auto resumes.
- If the scene is **outside your directing setup** (an intro, a pause screen…) → the behavior is **configurable** in **Advanced settings → General settings**:
  - **Count it as a shot**: temporary force, auto resumes;
  - **Pause the directing** *(default)*: you stay on the scene until you re-enable it.

---

## 6. Every setting in detail

Everything is set from the **Wizard** (guided creation) or the **Advanced settings** (fine editing, via the dock button). Here's each panel.

### Speakers

![Settings — speakers](img/setting-speakers.png)

| Setting | Effect |
| --- | --- |
| **Name** | The person's displayed name (just so you can keep track). |
| **Audio source** | The OBS source we listen to in order to know whether this person is speaking. This is the heart of the system. |

### Cameras & weights

![Settings — cameras and weights](img/setting-camera.png)

| Setting | Effect |
| --- | --- |
| **Scene** | A scene to show when this person speaks. |
| **Weight** | The higher it is, the more often this scene appears **relative to that person's other scenes**. The % is recalculated automatically. |

Add **several** scenes to a person to get variety (e.g. *close-up 90* + *reaction shot 10*).

### Directing *(former "Rhythm" + "Wide shot" merged into one tab)*

![Settings — directing](img/setting-rythm.png)

**At the top of the page, the directing-style selector.** Pick a **temperament** in one click — **Chill** (calm, long shots), **Cool** (balanced, the default), **Speed** (snappy, clean cuts) — and **all the sliders move into place on their own**: the **tempo AND the wide-shot tendency**. As soon as you nudge a slider by hand, the style switches to **Custom** (your values, kept). A style defines **your whole directing temperament**, but **never** touches your **voice threshold** or attack/silence — that's mic/room calibration, set separately (at the bottom of the page).

**Your own styles ("My styles").** Once you have a setting you like, **"Save as…"** names it and adds it to the **"My styles"** dropdown — reusable on **any show** (it's a **global** library, independent of profiles). You can **rename** or **delete** your styles; the built-in ones (Chill/Cool/Speed) stay read-only. Chips = the **built-ins**; the dropdown = **yours**.

**Tempo:**

| Setting | Effect | Default |
| --- | --- | --- |
| **Minimum shot time** | Minimum time a shot is shown before a change is allowed (avoids jittery cuts). | 3 s |
| **Maximum shot time** | Past this time on the same shot, we refresh the view to vary it. | 8 s |
| **Pull back to wide on fast exchanges** | When two people trade lines back and forth (each alone in turn), instead of whip-paning we **pull back to the wide shot** for a beat, then resume. **0 = disabled**; **only acts if a wide shot exists** (otherwise the minimum time governs). | 0 (disabled) |
| **Silence reaction delay** | When **nobody** speaks, we **stay on the current shot** for this long before switching (wide shot / last speaker). Avoids dropping someone on a mere breath; if they resume within the delay, we never left them. **Only affects silence**: switching to a speaker who starts talking stays immediate. **"Immediate"** = switch without waiting. | 1.5 s |

**Wide-shot tendency** *(driven by the style)*:

| Setting | Effect | Default (Cool) |
| --- | --- | --- |
| **Wide shot scene** | The group scene, shown when several people speak or when a group shot is called for. **Leave empty to never use it** (the engine then adapts without a wide shot). | — |
| **When several speak** | Weight of the pick between *stay on the current one* / *wide shot*. Volume is **no longer a criterion** ("the loudest" was removed); featuring someone happens naturally when they keep the floor. | stay 10 / wide 94 |
| **When nobody speaks** | Weight of the pick between *last speaker* / *wide shot*. | last 10 / wide 94 |

**Sensitivity** *(independent of the style — mic/room calibration)*:

| Setting | Effect | Default |
| --- | --- | --- |
| **Voice threshold** | Sound level above which a person is considered to be "speaking". Lower = more sensitive. | −35 dB |
| **Attack delay** | Duration of continuous voice before confirming that a person **starts** speaking (ignores brief noises: a click, a knock). Shorter = more responsive. | short |
| **Silence delay** | Duration of a blank before considering that **one person** has **finished** speaking (per-person detection — not to be confused with "Silence reaction delay" above). | short |

> **About the wide shot:** as soon as we switch to it after someone, it **holds at least the "minimum time"** like any shot — no "flash" if speech resumes right after. *(Only exception: at the very start, before anyone has spoken, the first time someone speaks is followed with no delay.)*

> **Everything takes effect on the fly.** As soon as you save (or move a threshold slider in the dock), the engine applies the change **immediately** — without restarting the directing or OBS.

#### Per-speaker threshold (live adjustment)

The threshold slider under each speaker, in the dock, **overrides the global threshold for that one person**. Handy when one mic is louder than another. This setting is **saved in the active profile** and restored on the next launch.

### General settings

![Settings — general settings](img/setting-general.png)

**Global** settings (shared across all profiles), applied immediately:

| Setting | Effect |
| --- | --- |
| **Off-setup scene (manual click)** | What to do when you manually switch to a scene **that isn't in your directing setup**: *count it as a shot* (temporary force) or *pause the directing* *(default)*. |
| **Check for updates on startup** | The plugin checks whether a newer version exists and shows a banner if so. No automatic installation — just a link. |

---

## 7. Hotkeys (keyboard & Stream Deck)

Flowspire exposes its actions as **native OBS hotkeys**. They have **no default key**: open **OBS → Settings → Hotkeys** and assign the key of your choice (for example **F13–F18**, ideal for a Stream Deck). Once assigned, **the key is remembered** and kept across OBS restarts.

| Action (label in OBS) | Effect |
| --- | --- |
| **Flowspire: enable/disable auto-directing** | On/off for automatic directing. |
| **Flowspire: force the wide shot** | Switches immediately to the wide shot. |
| **Flowspire: force speaker 1 … 8** | Switches immediately to the chosen person (up to 8 hotkeys). |

> On a **Stream Deck**, add a *Key* / *Hotkey* button that sends the same key as the one assigned in OBS: the button then drives Flowspire. Beyond 8 people, **force the shot directly from OBS's native scene manager** (one click = one temporary force).

> **Your usual scene hotkeys keep working.** If you already had OBS hotkeys that switch directly to a scene (keyboard or Stream Deck), using them during auto-directing amounts to **forcing that shot** — exactly like clicking the scene. Nothing new to learn.

### Taking back control during the live

- **Force a shot** (a person or the wide shot): immediate switch, then the normal rules resume.
- **Pause** (Auto OFF): the plugin **freezes** the current scene and stops switching.
- **Resume** (Auto ON): it starts again from whoever is speaking at that moment.
- Changing a scene **manually** in OBS also hands control back to the plugin cleanly.

### Show the directing state on a button (Bitfocus Companion)

You can make a **button change color** depending on whether auto-directing is on — **without installing any module**.

1. In **Companion**, create a **custom variable** named `flowspire_active` (*Variables → Custom Variables*).
2. In **Flowspire → Settings → General settings → External connection**, tick *Send the directing status…*, then enter the **IP address** of the machine running Companion (`127.0.0.1` if it's the same one) and the **port** (**8000** by default).
3. On your Companion button, add a **"Variable value"** *feedback*: if `$(custom:flowspire_active)` equals `1` → green "ON AIR" background, otherwise grey.

Flowspire writes `1` when auto-directing is active, `0` otherwise — on every toggle, at startup, and every 15 s (re-sync if Companion restarts). Everything goes over **local HTTP**: no module, no cable.

> To **drive** directing from the same button (press = on/off), assign a key to the *"Flowspire: enable/disable auto-directing"* hotkey (above) and trigger it from Companion through the **OBS** module. One button both controls **and** reflects the state.

---

## 8. Profiles

![Settings — profiles](img/setting-profile.png)

A **profile** = a complete configuration (speakers, scenes, wide shot, rhythm). Create one per show type ("4 guests + me", "Duo", "Panel"…) and **switch from one to another with a single click** from the dock.

- Creation via the **wizard** (guided) or a **blank profile** (to fill in from the settings).
- Per-profile actions: **load**, **rename**, **duplicate**, **edit**, **delete**.
- The **active profile** is the **source of truth** read by the engine.

---

## 9. Where settings are stored

Profiles live in the OBS config folder:

```
Windows : %APPDATA%\obs-studio\plugin_config\flowspire\profiles\
macOS   : ~/Library/Application Support/obs-studio/plugin_config/flowspire/profiles/
Linux   : ~/.config/obs-studio/plugin_config/flowspire/profiles/
```

- `index.json` = catalog of profiles + active profile.
- `<id>.json` = the contents of a profile.

Your **app settings** (General settings) and the **hotkey keys** you assign live in the parent `flowspire/` folder (`prefs.json` and `hotkeys.json`), with the same protection.

Every write is **atomic** with a `.bak` backup; an unreadable file is recovered automatically. You **never need to edit these files by hand** — the interface does it all.

---

[← Back to README](../README.md)
