# Hilfe zum aktuellen Verzeichnis

In diesem Verzeichnis befinden sich mehrere Templates zum Anlegen von Docker Images.
Das "dummy"-Image ist ein grundsätzliches Funktionsbeispiel für eine Umgebung mit PHP/Apache
und sollte nicht verändert werden.

# Hinweise zur Verzeichnisstruktur / Dateien
* Makefile
  * Problem: Docker kann nur Verzeichnisse unterhalb des Dockerfile-Verzeichnisses einbinden. symlinks funktionieren nicht, hardlinks auf Verzeichnisse sind unter Linux nicht möglich.
  * Workaround: Im Makefile werden die benötigten Verzeichnisse temporär ins aktuelle Verzeichnis kopiert und nach dem docker build Vorgang wieder gelöscht.
* Dockerfile
  * Von docker benötigte Datei für den Buildvorgang
  * Es wird eine rohe Variante eines Betriebssystems verwendet (z.B. Ubuntu)
  * Alle benötigten module müssen von Hand im Dockerfile nachinstalliert ewrden (z.B. apache2, php7.0, ...)


# Nützliche Befehle im Umgang mit docker

## Image erstellen (Beispiel: "dummy")
Ins Verzeichnis mit Dockerfile wechseln, z.B. docker/dummy

Bei Abhängigkeiten sollte immer `make` verwendet werden! (Grund siehe weiter oben / Makefile).
Falls es keine gibt, kann zum erstellen und lokalen registrieren folgender Befehl verwendet werden!

`docker build -t dummy .`

## Images anzeigen lassen
`docker images`

## Image in einem Container ausführen
`docker run -p 8080:80 -d dummy`
* -p 8080:80 => Port 80 des Containers wird auf Port 8080 auf dem Host gemappt
* -d => detach, wird im Hintergrund ausgeführt

## Laufende Docker-Prozesse (=Container) anzeigen
`docker ps`

Wichtig: Pro Image können theoretisch mehrere Container laufen, für weitere Befehle muss die Container ID angegeben werden, also nicht "dummy" sondern z.B. 6714d8880fad

## Auf laufenden Container per Bash zugreifen
`docker exec -it 6714d8880fad bash`

Hinweis: Bei bestimmten Aktionen, z.B. Neustarten des Apache Servers kann der Container abstürzen!

## Laufenden Container stoppen
`docker stop 6714d8880fad`
