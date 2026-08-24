# Prompt for your website builder — Nebula Tide update endpoint

Copy everything in the box below and give it to whoever (or whatever) builds your
website. When it's live, send me the URL and I'll wire the app to it.

---

```
Please add a small public JSON file to my website that my desktop app reads to
check for updates. Requirements:

1. PATH — serve it at exactly this permanent URL (do not move or rename it later):
       https://MYDOMAIN.com/nebulatide/latest.json

2. CONTENT — exactly this shape (values are examples; I will edit them each release):

{
  "version": "1.2.0",
  "released": "2026-08-22",
  "notes": "Encrypted sound library. Fixes silent plugin on macOS.",
  "downloads": {
    "windows": "https://MYDOMAIN.com/downloads/NebulaTide-1.2.0-Setup.exe",
    "macos":   "https://MYDOMAIN.com/downloads/NebulaTide-1.2.0-macOS.pkg",
    "android": "https://MYDOMAIN.com/downloads/NebulaTide-1.2.0.apk"
  },
  "page": "https://MYDOMAIN.com/nebulatide"
}

3. SERVING RULES — these matter, the app is not a browser:
   - Content-Type must be application/json
   - Publicly readable: no login, no cookie wall, no CAPTCHA, no country blocking
   - No redirect to an HTML page — the URL must return the raw JSON directly
   - Must work over HTTPS with a valid certificate
   - Cache-Control: max-age=300 (so a new version is picked up within 5 minutes)
   - Keep it tiny — this file is fetched once per app launch by every user

4. DO NOT put it behind a CMS route that could 404 after a theme change or site
   rebuild. A static file in a folder is ideal.

5. Also confirm the file is reachable by running this and pasting me the output:
       curl -i https://MYDOMAIN.com/nebulatide/latest.json
   I need to see "200 OK", "content-type: application/json", and the JSON body.
```

---

## What happens after it's live

- Send me the final URL. I'll compile it into the app.
- On launch the app fetches that one small file. If `version` is newer than the
  running build, a quiet "Update available" notice appears with a link to `page`.
  No internet = no check; the app runs completely normally offline.
- **Nothing else is ever downloaded.** Sounds always ship inside the app.

## Keeping it updated each release

Each time we tag a release, edit `latest.json`: bump `version`, `released`,
`notes`, and the three download links. That's the whole maintenance job.

Optional: I can make the build produce a ready-made `latest.json` as a release
artifact, so you just upload that file to the site instead of hand-editing it.
