# JellyFloat im App Store einreichen

Alles, was in App Store Connect einzutragen ist, in der Reihenfolge der Formulare. Die Texte liegen
zum Kopieren in `metadata/en-US/` und `metadata/de-DE/`, die Bildschirmfotos in `screenshots/`.
Die Texte für die Prüfer stehen unten unter "App-Überprüfungsinformationen".

## Vorher, einmalig

1. **Apple Developer Program** (99 USD/Jahr) mit der Apple-ID, die in Xcode unter Settings > Accounts
   angemeldet ist. Die Team-ID steht in `ios/Local.xcconfig`.
2. **Bundle-ID registrieren**: https://developer.apple.com/account/resources/identifiers, "+", App IDs,
   Bundle ID explicit `at.guggug.jellyfloat`, Capability **Hotspot** anhaken (die App wechselt damit das
   WLAN). Xcode legt die ID beim ersten Archivieren auch selbst an, mit angehaktem Hotspot.
3. **Fork-Verbindung lösen** (optional, für die Unabhängigkeit vom Sheffield-Repo): GitHub, Repository
   Settings, General, ganz unten "Danger Zone", **Leave fork network**. Nur dort möglich, nicht per API.

## App anlegen (App Store Connect > Apps > "+" > Neue App)

| Feld | Eintrag |
|---|---|
| Plattformen | iOS |
| Name | `JellyFloat` (muss im Store einmalig sein; wenn belegt: `JellyFloat Jellyfish Lights`) |
| Primäre Sprache | Englisch (USA); Deutsch kommt als zweite Lokalisierung dazu |
| Bundle-ID | `at.guggug.jellyfloat` |
| SKU | `jellyfloat-ios` |
| Benutzerzugriff | Vollzugriff |

**Zur SKU:** Das ist eine interne Artikelnummer, die nur du siehst. Sie muss innerhalb deines Kontos
einmalig sein, darf Buchstaben, Ziffern, Bindestrich, Unterstrich und Punkt enthalten und lässt sich
später nicht mehr ändern. Apple nutzt sie in den Verkaufsberichten. `jellyfloat-ios` ist kurz und
eindeutig; falls du eine Nummernlogik willst: `JF-IOS-001`.

## Version 1.0.0: App-Informationen

| Feld | Eintrag |
|---|---|
| Name | JellyFloat |
| Untertitel | en: `Control your LED jellyfish` · de: `Steuert deine LED-Quallen` |
| Primäre Kategorie | Dienstprogramme (Utilities) |
| Sekundäre Kategorie | Unterhaltung (Entertainment) |
| Inhaltsrechte | "Enthält keine Inhalte Dritter, für die Rechte nötig wären" |
| Altersfreigabe | alle Fragen "Nein/Keine" → 4+ |
| Lizenzvereinbarung | Standard-EULA von Apple |

## Version 1.0.0: Vorbereitung zur Einreichung

Lokalisierungen: **Englisch (USA)** und **Deutsch**, jeweils:

| Feld | Datei |
|---|---|
| Werbetext (≤ 170 Zeichen) | `metadata/<sprache>/promotional_text.txt` |
| Beschreibung (≤ 4000) | `metadata/<sprache>/description.txt` |
| Schlüsselwörter (≤ 100) | `metadata/<sprache>/keywords.txt` |
| Support-URL | https://github.com/MacBachi/Jellyfish/issues |
| Marketing-URL | https://github.com/MacBachi/Jellyfish |
| Neue Funktionen | `metadata/<sprache>/release_notes.txt` |
| Bildschirmfotos iPhone 6,9" | `screenshots/<sprache>/` (1320 × 2868, vier Stück: Beitritt, Schwarm, Quallen, Einstellungen) |

iPad-Fotos sind nicht nötig, die App ist nur für iPhone gebaut (`TARGETED_DEVICE_FAMILY = 1`).

| Feld | Eintrag |
|---|---|
| Version | `1.0.0` (kommt aus `VERSION`; der Build trägt sie automatisch) |
| Copyright | `2026 <dein Name>` |
| Veröffentlichung | Manuell, nach der Freigabe |
| Preis | Kostenlos, alle Länder |

## Datenschutz

| Feld | Eintrag |
|---|---|
| URL der Datenschutzrichtlinie | https://github.com/MacBachi/Jellyfish/blob/main/PRIVACY.md |
| App-Datenschutz ("Nutrition Label") | **Daten werden nicht erfasst** (Data Not Collected) |
| Datenschutz-Kontakt | kann leer bleiben |

Begründung: kein Konto, kein Netzwerkzugriff außer ins lokale Quallen-WLAN, keine Analyse, keine IDs.

## Exportbestimmungen (Verschlüsselung)

`ITSAppUsesNonExemptEncryption = false` steht in der Info.plist, die Frage erscheint deshalb nicht.
Falls doch: "Nein, die App verwendet keine Verschlüsselung" (das WLAN-Passwort übergibt die App an
eine iOS-Systemfunktion; die App selbst verschlüsselt nichts).

## App-Überprüfungsinformationen (für die Prüfer)

| Feld | Eintrag |
|---|---|
| Anmeldung erforderlich | **Nein** (kein Konto) |
| Kontakt | dein Name, Telefon, E-Mail |
| Hinweise | Text unten, Englisch reicht; die deutsche Fassung ist die Übersetzung |
| Anhang | optional ein kurzes Video einer echten Qualle mit der App |

**Notes for App Review (englisch, dieses Feld ausfüllen):**

```
JellyFloat controls hand-built LED jellyfish lamps (Raspberry Pi Pico 2 W running the open-source
JellyFloatOS firmware, github.com/MacBachi/Jellyfish). The jellies open their own Wi-Fi network; the
app joins it and sends short text commands over UDP. Nothing leaves the local network, there is no
account and no server.

HOW TO TEST WITHOUT THE HARDWARE
1. Launch the app. On the first screen ("Join the jelly network?") tap "No jellyfish yet? Try the
   demo bloom". (Also in Settings: "Demo bloom (no hardware)".)
2. The status pill reads "Demo bloom". The Bloom tab shows the virtual jellyfish animating; tap any
   mode tile, move the brightness and colour sliders, and the picture follows.
3. The Jellies tab lists the pretend bloom (0451, 1b3c, c7d1, 9e02) with firmware versions; "Roll
   call" and "Identify" work (Identify makes the on-screen jelly blink).
4. Settings: the language follows the phone; German and English are included.

WITH THE HARDWARE
"Join 🪼" asks iOS to switch Wi-Fi to the jellies' network (NEHotspotConfiguration, hence the Hotspot
Configuration entitlement) and iOS shows its own prompt. The local-network permission prompt appears
on the first UDP send. Both are expected.

The app never connects to the internet. Everything is open source at github.com/MacBachi/Jellyfish.
```

**Dieselben Hinweise auf Deutsch** (zur Sicherheit, falls ein deutscher Prüfer dran ist; kann zusätzlich
unter den englischen Text):

```
JellyFloat steuert selbst gebaute LED-Quallen (Raspberry Pi Pico 2 W mit der quelloffenen Firmware
JellyFloatOS, github.com/MacBachi/Jellyfish). Die Quallen eröffnen ein eigenes WLAN; die App tritt
ihm bei und schickt kurze Textbefehle über UDP. Nichts verlässt das lokale Netz, es gibt kein Konto
und keinen Server.

TEST OHNE HARDWARE
1. App starten. Auf dem ersten Bildschirm ("Dem Quallen-Netz beitreten?") auf "Noch keine Quallen?
   Demo-Schwarm ausprobieren" tippen. (Auch in den Einstellungen: "Demo-Schwarm (ohne Hardware)".)
2. Die Statusanzeige zeigt "Demo-Schwarm". Der Tab "Schwarm" zeigt die animierte virtuelle Qualle;
   jede Modus-Kachel, der Helligkeits- und der Farbregler wirken sofort auf das Bild.
3. Der Tab "Quallen" listet den vorgetäuschten Schwarm (0451, 1b3c, c7d1, 9e02) mit Firmware-
   Versionen; "Aufruf" und "Identifizieren" funktionieren (die Qualle auf dem Bildschirm blinkt).
4. Einstellungen: Die Sprache folgt dem Telefon; Deutsch und Englisch sind enthalten.

MIT HARDWARE
"🪼 beitreten" bittet iOS, ins Quallen-WLAN zu wechseln (NEHotspotConfiguration, daher das Hotspot-
Configuration-Entitlement); iOS zeigt seinen eigenen Dialog. Die Abfrage für das lokale Netzwerk
erscheint beim ersten UDP-Senden. Beides ist so gewollt.

Die App geht nie ins Internet. Alles ist quelloffen unter github.com/MacBachi/Jellyfish.
```

## Build hochladen

```bash
ios/scripts/archive.sh
```

Das Skript erzeugt das Xcode-Projekt, archiviert Release für iOS und lädt zu App Store Connect hoch
(Xcode muss mit der Apple-ID angemeldet sein, `-allowProvisioningUpdates` holt Zertifikat und Profil).
Version ist der Zahlenteil aus `VERSION`, die Build-Nummer die Zahl der Commits; beide steigen mit
jedem Release von selbst. Nach 10 bis 30 Minuten erscheint der Build unter "Builds"; ihn der Version
zuordnen, dann "Zur Überprüfung senden".

Alternativ in Xcode: `cd ios && xcodegen generate && open JellyFloat.xcodeproj`, Ziel "Any iOS
Device", Product > Archive, im Organizer "Distribute App" > "App Store Connect".

## Nach der Freigabe

Bei jeder neuen Version: `VERSION` setzen, Tag `vX.Y.Z` pushen (Firmware-Release), `archive.sh`, in
App Store Connect neue Version anlegen, `release_notes.txt` aktualisieren.
