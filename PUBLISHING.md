# Publishing Nebula Tide — Google Play, App Store & TestFlight

Everything is built automatically. You add a set of **repository secrets** once,
then every `git tag vX.Y.Z` produces signed, store-ready builds.

Add secrets at: **repo → Settings → Secrets and variables → Actions → New repository secret**

---

## A. Android (Google Play)

### 1. Create your signing key (once — keep it forever)

Run this on any machine with Java (Android Studio includes it):

```
keytool -genkeypair -v -keystore nebula-upload.jks -alias nebula ^
        -keyalg RSA -keysize 2048 -validity 10000
```

It asks for a password and your name/organisation. **Back up `nebula-upload.jks`
and the password somewhere safe** — with Play App Signing you can recover a lost
upload key, but it's a slow support process.

### 2. Turn the keystore into a secret

```
certutil -encode nebula-upload.jks keystore-b64.txt
```

Open `keystore-b64.txt`, delete the `-----BEGIN/END CERTIFICATE-----` lines, and
copy the remaining text.

### 3. Add these three secrets

| Secret | Value |
|---|---|
| `ANDROID_KEYSTORE_B64` | the base64 text from step 2 |
| `ANDROID_KEYSTORE_PASSWORD` | the password you chose |
| `ANDROID_KEY_ALIAS` | `nebula` |

### 4. Publish

Each release now produces two Android files:

- **`NebulaTide-V2-Android-Play.aab`** → upload to **Play Console → Production → Create release**
  (Play requires the `.aab` format for new apps). No licence key screen: Google takes the payment.
- **`NebulaTide-V2-Android-Website.apk`** → put on your website for direct download. It asks for
  a licence key on first launch (internet needed once, to activate), carries all the sounds, and
  installs beside a Play copy because it has its own app id (`com.amanorsac.nebulatide.direct`).

Play Console first-time setup also needs: app name, short & full description
(use `marketing/handoff.json`), screenshots (`marketing/screenshots/`), a
512×512 icon (`assets/logo.png`), a feature graphic, privacy policy URL, and the
content-rating questionnaire.

---

## B. iPhone / iPad (App Store + TestFlight)

### 1. In the Apple Developer portal

- **Certificates** → create an **Apple Distribution** certificate. Download it,
  double-click to install into Keychain, then export it as **`.p12`** with a
  password (right-click the certificate → Export).
- **Identifiers** → register App ID **`com.nebulatide.app`**
  (and `com.nebulatide.app.auv3` if you want the AUv3 plugin listed separately).
- **Profiles** → create an **App Store** provisioning profile for that App ID and
  download the `.mobileprovision`.

### 2. In App Store Connect

- **Users and Access → Integrations → App Store Connect API** → generate a key
  with the **App Manager** role. Download the `.p8` (one chance only!) and note
  the **Key ID** and **Issuer ID**.
- **My Apps → +** → create the app record for `com.nebulatide.app`.

### 3. Convert the files to base64

On a Mac:
```
base64 -i Distribution.p12          | pbcopy    # → IOS_DIST_CERT_P12
base64 -i NebulaTide.mobileprovision | pbcopy   # → IOS_PROVISION_PROFILE
base64 -i AuthKey_XXXXXXXX.p8        | pbcopy   # → ASC_KEY_P8
```

### 4. Add these secrets

| Secret | Value |
|---|---|
| `IOS_DIST_CERT_P12` | base64 of the distribution `.p12` |
| `IOS_CERT_PASSWORD` | password you set when exporting the `.p12` |
| `IOS_PROVISION_PROFILE` | base64 of the `.mobileprovision` |
| `APPLE_TEAM_ID` | 10-character Team ID (top-right of the developer portal) |
| `ASC_KEY_ID` | App Store Connect API Key ID |
| `ASC_ISSUER_ID` | App Store Connect Issuer ID |
| `ASC_KEY_P8` | base64 of the `.p8` file |

### 5. Publish

With those set, every tagged release **uploads straight to TestFlight**. From
App Store Connect you then add testers, and hit *Submit for Review* when ready.

Already handled for you in the build: iPhone **and** iPad support, landscape
orientation, full-screen, background audio (pads keep playing when you switch
apps), and the encryption-compliance declaration so submissions aren't held up.

---

## C. macOS signing (optional, removes the "unidentified developer" warning)

| Secret | Value |
|---|---|
| `MAC_CERT_P12` | base64 of a **Developer ID Application** certificate `.p12` |
| `MAC_CERT_PASSWORD` | its export password |
| `APPLE_ID` | your Apple ID email |
| `APPLE_TEAM_ID` | same Team ID as above |
| `APPLE_APP_PASSWORD` | an app-specific password from appleid.apple.com |

---

## Sound library size, by platform

| Platform | Format | Library size | Why |
|---|---|---|---|
| Windows / macOS | FLAC (lossless) | ~255 MB | no size limits, best quality |
| iOS / iPadOS / Android | Ogg Vorbis q7 | ~94 MB | keeps the app under Google Play's 150 MB limit and avoids App Store cellular warnings |

Both are encrypted `.ntlib` containers; the audio is never a playable file on
the user's device. Nothing is downloaded after installation on any platform.
