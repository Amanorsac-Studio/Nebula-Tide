# Nebula Tide — publishing, step by step

Follow top to bottom. Do not skip ahead; each part needs the one before it.
Everything you download goes in **`C:\Drone Pad\signing\`** unless it says otherwise.

Where you are now: ✅ Android key created · ✅ Apple Distribution certificate created

---

# PART 1 — Add the 6 secrets you already have  (10 minutes)

These let the build machines sign your app. Nothing works until this is done.

**Go to:** https://github.com/amanorsac/Nebula-Tide/settings/secrets/actions

Click **New repository secret**, fill Name + Secret, click *Add secret*. Repeat 6×.

| # | Name (copy exactly) | Secret value |
|---|---|---|
| 1 | `ANDROID_KEYSTORE_B64` | open `C:\Drone Pad\signing\ANDROID_KEYSTORE_B64.txt` → Ctrl+A → Ctrl+C → paste |
| 2 | `ANDROID_KEYSTORE_PASSWORD` | `kDmTU4l491rdO4ijXgmhXlR9Tch3` |
| 3 | `ANDROID_KEY_ALIAS` | `nebula` |
| 4 | `APPLE_TEAM_ID` | `SGQVTNFK4Q` |
| 5 | `IOS_DIST_CERT_P12` | open `C:\Drone Pad\signing\IOS_DIST_CERT_P12.txt` → Ctrl+A → Ctrl+C → paste |
| 6 | `IOS_CERT_PASSWORD` | `jYUJQjgqTSVpaBLXHWifquxk` |

➡️ **Then tell Claude "secrets added".** Claude runs a build and you get a signed
Android APK + AAB. Android is then finished and ready to publish.

---

# PART 2 — Finish the Apple credentials  (20 minutes)

Three items left. All are web downloads — no Mac needed.

## 2a. Register the App ID

**Go to:** https://developer.apple.com/account/resources/identifiers/list

1. Click the blue **+**
2. Choose **App IDs** → Continue
3. Choose **App** → Continue
4. Description: `Nebula Tide`
5. Bundle ID: select **Explicit** and type exactly: `com.nebulatide.app`
6. Scroll the Capabilities list — leave defaults, nothing to tick
7. Continue → Register

## 2b. Create the provisioning profile

**Go to:** https://developer.apple.com/account/resources/profiles/list

1. Click **+**
2. Under *Distribution* choose **App Store Connect** → Continue
3. App ID: pick **Nebula Tide (com.nebulatide.app)** → Continue
4. Certificate: tick **Apple Distribution: Stephen Amanor Sackey** → Continue
5. Profile Name: `NebulaTide AppStore` → Generate
6. Click **Download**
7. **Move the downloaded `.mobileprovision` file into `C:\Drone Pad\signing\`**

## 2c. Create the App Store Connect API key

**Go to:** https://appstoreconnect.apple.com/access/integrations/api

1. Click **+** (Generate API Key)
2. Name: `Nebula Tide CI`
3. Access: **App Manager**
4. Generate
5. Click **Download API Key** — ⚠️ **you only get ONE chance**, it cannot be re-downloaded
6. **Move the downloaded `AuthKey_XXXXXXXXXX.p8` into `C:\Drone Pad\signing\`**
7. On that same page write down two values:
   - **KEY ID** — the 10 characters in the row you just created
   - **ISSUER ID** — a long code shown above the table

➡️ **Then tell Claude "apple files downloaded"** and give it the Key ID and Issuer ID.
Claude converts everything and gives you the last 3 secrets to paste.

---

# PART 3 — Create the app entries in each store

You can do these while waiting; they don't depend on the build.

## 3a. Google Play

**Go to:** https://play.google.com/console

1. **Create app**
2. App name: `Nebula Tide` · Language: English · Type: **App** · **Free**
3. Accept the declarations → Create app
4. Work down the **Set up your app** checklist on the dashboard:
   - App access → *All functionality is available without restrictions*
   - Ads → **No**, it contains no ads
   - Content rating → fill the questionnaire (it's a music app: answer "no" to everything about violence/sex/drugs)
   - Target audience → choose your age groups
   - Data safety → **No data collected** (the app collects nothing)
   - Government apps → No
   - Financial features → None
   - Store listing → see the assets list in PART 5

## 3b. Apple App Store

**Go to:** https://appstoreconnect.apple.com/apps

1. Click **+** → **New App**
2. Platforms: tick **iOS**
3. Name: `Nebula Tide`
4. Primary Language: English
5. Bundle ID: choose **com.nebulatide.app** (appears after PART 2a)
6. SKU: `nebulatide001` (any private code)
7. Full Access → Create

---

# PART 4 — Upload the builds

## 4a. Android → Google Play

1. Download `NebulaTide-V2-Android-Play.aab` from the release Claude builds for you
2. Play Console → left menu → **Testing → Internal testing**
3. **Create new release**
4. Drag the **.aab** into the upload box
5. Release name fills in automatically; add a line of release notes
6. **Next → Save → Start rollout to Internal testing**
7. On the *Testers* tab add your own email, then use the opt-in link on your phone

> ⚠️ If your Play account is a **personal** account created after Nov 2023, Google
> requires **12 testers for 14 continuous days** in closed testing before you may
> apply for production. Start this early — the clock is what costs time, not the work.

## 4b. iPad → TestFlight

Nothing to upload by hand. Once the 9 Apple secrets exist, every release Claude
tags is **sent to TestFlight automatically**.

1. Wait ~15 min after Claude says the build passed
2. https://appstoreconnect.apple.com → your app → **TestFlight** tab
3. The build appears as *Processing*, then *Ready to Test*
4. Answer the export-compliance question if asked → **No** (already declared in the app)
5. Add yourself under **Internal Testing**
6. Install **TestFlight** from the App Store on your iPad → open → install Nebula Tide

---

# PART 5 — Store assets you must create

Both stores refuse a submission without these. **You need the app running on a
real device to screenshot it**, so do PART 4 first.

**Google Play** (Store listing page)
- App icon **512×512 PNG** → use `C:\Drone Pad\assets\logo.png` (resize to 512)
- Feature graphic **1024×500** → make one from your marketing images
- Phone screenshots — at least 2
- Tablet screenshots — recommended
- Short description (80 chars) and full description (4000) →
  copy from `C:\Drone Pad\marketing\handoff.json`

**App Store** (App Information + version page)
- iPad screenshots — at least 1, 13-inch size (2064×2752). Take them on the iPad
  once TestFlight is installed: press **Top button + Volume Up** together
- Description, keywords, support URL, privacy policy URL →
  `C:\Drone Pad\marketing\handoff.json` has the copy
- Privacy: **Data Not Collected**

---

# PART 6 — Your website installers (Windows & Mac)

**Question: do the website installers need the certificate?**
The Apple certificate you just made is **only** for the App Store. Your website
downloads are separate, and each platform needs its own signing:

| Download | Certificate needed | Status | Effect if unsigned |
|---|---|---|---|
| `NebulaTide-…-Setup.exe` (Windows) | **Windows code-signing certificate** — must be purchased, ~$200–400/year (Sectigo, DigiCert) or Azure Trusted Signing | ❌ you don't have one | SmartScreen shows "Windows protected your PC" — user clicks *More info → Run anyway* |
| `NebulaTide-…-macOS.pkg` (Mac) | **Developer ID Application** + **Developer ID Installer** | ⏳ CSRs ready, certificates not created yet | Gatekeeper says "unidentified developer" — user right-clicks → *Open* |
| `NebulaTide-V2-Android-Website.apk` (website, asks for a licence key) | your Android key — **already done** ✅ | ✅ | Android asks once to allow installs from the browser |

**To fix the Mac warning** (optional, ~10 minutes):

**Go to:** https://developer.apple.com/account/resources/certificates/list

1. **+** → **Developer ID Application** → Continue → upload
   `C:\Drone Pad\signing\UPLOAD-THIS-for-Developer-ID-Application.certSigningRequest`
   → Download the `.cer` into `C:\Drone Pad\signing\`
2. **+** → **Developer ID Installer** → Continue → upload
   `C:\Drone Pad\signing\UPLOAD-THIS-for-Developer-ID-Installer.certSigningRequest`
   → Download that `.cer` into `C:\Drone Pad\signing\`
3. Tell Claude — it converts them and adds signing + Apple notarisation to the build

The Windows warning can only be removed by buying a certificate. Many small audio
developers ship unsigned for a year and add it once there's revenue — your call.

---

# Quick reference

| Thing | Where |
|---|---|
| All your keys & certificates | `C:\Drone Pad\signing\` |
| Secrets checklist | `C:\Drone Pad\signing\SECRETS-TO-ADD.txt` |
| Store copy, descriptions | `C:\Drone Pad\marketing\handoff.json` |
| Desktop screenshots | `C:\Drone Pad\marketing\screenshots\` |
| GitHub secrets page | https://github.com/amanorsac/Nebula-Tide/settings/secrets/actions |
| Apple certificates | https://developer.apple.com/account/resources/certificates/list |
| App Store Connect | https://appstoreconnect.apple.com |
| Play Console | https://play.google.com/console |

**⚠️ Back up `C:\Drone Pad\signing\` somewhere off this computer.** It holds both
your Android signing key and your Apple private keys. Losing them is painful to
recover from.
