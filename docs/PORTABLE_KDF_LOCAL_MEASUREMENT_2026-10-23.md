# Argon2id-Leistungsmessung – ausschließlich Forschungswerkzeug

**Nicht Teil der RcloneBrowser-Anwendung. Kein Release. Kein Produktiv-Einsatz.**

Dieses Entwicklerwerkzeug dient ausschließlich dazu, zwei Kandidaten für
eine spätere Passwort-Schlüsselableitung auf deinem **eigenen Windows-PC**
vergleichbar zu messen. Es verwendet das öffentlich bekannte, fest
eingebaute Testpasswort `synthetic-benchmark-password-ONLY` und zufällige
Test-Salts. Es liest **keine** echten Passwörter, Windows-Zugangsdaten,
`tasks.bin`, rclone.conf oder Dateien aus deinem Benutzerprofil.

## Woher stammt das Werkzeug?

Dieses Paket wird nur bei einem **manuell gestarteten** Test-Workflow auf
`security/portable-kdf-local-benchmark-2026-10-23` erstellt. Das
Windows-Programm wird aus dem öffentlichen Quelltext
`tests/benchmark_portable_kdf.cpp` gebaut. Die benötigte `libsodium.dll`
stammt aus dem zuvor per GPG-Signatur und SHA-256 geprüften offiziellen
libsodium-Archiv der festgelegten Version **1.0.22**. Beide Dateien werden
anschließend im separaten Linux-ClamAV-Job geprüft, bevor dieser Download
bereitgestellt wird.

Das Paket enthält: `portable_kdf_benchmark.exe`, `libsodium.dll`,
`README.md`, `libsodium-LICENSE.txt`, `source_commit.txt` und
`SHA256SUMS.txt`. Die Hashliste bezieht sich auf alle Dateien außer sich
selbst. Das Tool ist nicht mit einem Herausgeberzertifikat signiert:
Antivirenwarnungen **nicht** umgehen oder den Virenschutz abschalten.
Eine erfolgreiche ClamAV-Prüfung ist keine Garantie für Schadsoftwarefreiheit.

## Ausführung (nur nach eigener Entscheidung)

1. GitHub Actions: den **erfolgreichen** manuellen Run für genau diesen
   Entwicklungszweig auswählen.
2. Im Bereich **Artifacts** nur
   **`RESEARCH-ONLY-portable-kdf-windows`** herunterladen.
   Andere Build-Dateien sind **nicht** dieses Messwerkzeug.
3. Archiv in einen neuen, leeren Ordner entpacken.
4. In PowerShell im **entpackten Ordner** prüfen:
   `Get-Content .\source_commit.txt`
   `Get-Content .\SHA256SUMS.txt`
   `Get-FileHash .\portable_kdf_benchmark.exe -Algorithm SHA256`
   `Get-FileHash .\libsodium.dll -Algorithm SHA256`
   Die gemessenen SHA-256-Hashes müssen exakt den entsprechenden Einträgen
   in `SHA256SUMS.txt` entsprechen. Prüfe auch, ob der Quellcommit dem
   gerade getesteten GitHub-Actions-Lauf entspricht. Eine Hashdatei aus
   demselben ZIP ist **kein** unabhängiger Herkunftsnachweis.
5. Test ohne Berechnung:
   `.\portable_kdf_benchmark.exe --help`
6. **Wenn die Sicherheitsprüfung keinen Anlass zur Sorge ergibt**, die
   freiwillige Leistungsmessung starten:
   `.\portable_kdf_benchmark.exe --run-synthetic`

Das Programm führt je **drei sequenzielle** Messungen für INTERACTIVE
(2 Durchläufe, 64 MiB) und MODERATE (3 Durchläufe, 256 MiB) aus.
Es zeigt in Millisekunden **Minimum, Median und Maximum** an.
Weder Passwörter noch geheime Schlüssel werden ausgegeben. Beim
Speicherfehler gibt es **keinen automatischen Rückfall** auf schwächere
Einstellungen. Die Benutzung verbraucht vorübergehend bis zu 256 MiB
Argon2-Arbeitsspeicher und Prozessorleistung; daher andere intensive
Programme möglichst ruhen lassen. Nicht auf Systemen mit knappen
Arbeitsspeicherreserven ausführen.

## Was kann man übermitteln?

Nur die beiden ausgegebenen Zeilen `INTERACTIVE,...` und
`MODERATE,...`, plus bei Bedarf `research-only,libsodium=...`.
**Keine** persönlichen Dateien, System-IDs oder Passwörter übermitteln.

## Was bedeuten die Ergebnisse?

Sie messen die **Ausführung auf genau dieser Hardware unter den aktuellen
Bedingungen**. Die GitHub-Runner liefern keine belastbaren Werte für
deinen PC. Der Benchmark wählt keinen Produktions-KDF und führt
keine Verschlüsselung realer Aufgabendateien durch. Ein späterer
Produktionswert muss hinsichtlich Speicherbedarf, Benutzererlebnis,
Versionswechsel, Angriffskosten und Wiederherstellbarkeit unabhängig
geprüft werden. Produktionsverschlüsselung bleibt deaktiviert.

Quelle: https://doc.libsodium.org/password_hashing/default_phf
