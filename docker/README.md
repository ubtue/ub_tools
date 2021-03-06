# Hilfe zum aktuellen Verzeichnis

In diesem Verzeichnis befinden sich mehrere Templates zum Anlegen von Docker Images.
Das "example"-Image ist ein grundsätzliches Funktionsbeispiel für eine Umgebung mit PHP/Apache
und sollte nicht verändert werden.

# Hinweise zur Verzeichnisstruktur / Dateien
* Makefile
  * Problem: Docker kann nur Verzeichnisse unterhalb des Dockerfile-Verzeichnisses einbinden. symlinks funktionieren nicht, hardlinks auf Verzeichnisse sind unter Linux nicht möglich.
  * Workaround: Im Makefile werden die benötigten Verzeichnisse temporär ins aktuelle Verzeichnis kopiert und nach dem Docker build Vorgang wieder gelöscht.
* Dockerfile
  * Von Docker benötigte Datei für den Buildvorgang
  * Es wird eine Variante eines Betriebssystems mit möglichst wenig installierten Paketen verwendet (z.B. Ubuntu)
  * Alle benötigten module müssen von Hand im Dockerfile nachinstalliert werden (z.B. apache2, php7.0, ...)


# Nützliche Befehle im Umgang mit Docker

## Image erstellen (Beispiel: "example")
Ins Verzeichnis mit Dockerfile wechseln, z.B. docker/example

Makefile verwenden falls vorhanden (Grund siehe weiter oben / Makefile).
Falls es nicht existiert, kann zum Erstellen und lokalen Registrieren folgender Befehl verwendet werden:

`docker build --no-cache -t example .`

Lässt man --no-cache weg, legt sich Docker einen Cache für alles an was er beim Buildvorgang herunterlädt. Das gilt nicht nur für das Ubuntu-Image und apt-Pakete, sondern auch für Dinge wie ub_tools usw. => Wenn man also Änderungen am Code vornimmt und diese Testen möchte (mehrere Builds hintereinander), sollte man immer unbedingt --no-cache verwenden, da die Änderungen sonst nicht im Image ankommen!

## Images anzeigen lassen
`docker images`

## Image in einem Container ausführen
`docker run -e ZOTERO_TRANSLATION_SERVER_URL="http://mb028.ub.uni-tuebingen.de:1969" -p 8080:80 -d example`
* -e => wird verwendet um beim Start des Containers spezielle Umgebungsvariablen zu setzen
* -p 8080:80 => Port 80 des Containers wird auf Port 8080 auf dem Host gemappt
* -d => detach, wird im Hintergrund ausgeführt

Ein Container beendet sich von selbst, sobald kein Prozess (z.B. Apache httpd) mehr im Vordergrund läuft.
Um dies zu vermeiden kann man beim Starten zusätzlich den Parameter -t angeben.

## Laufende Docker-Prozesse (=Container) anzeigen
`docker ps`

Wichtig: Pro Image können theoretisch mehrere Container laufen, für weitere Befehle muss die Container ID angegeben werden, also nicht "example" sondern z.B. 6714d8880fad

## Auf laufenden Container per Bash zugreifen
`docker exec -it 6714d8880fad bash`

Hinweis: Bei bestimmten Aktionen, z.B. Neustarten des Apache Servers kann der Container abstürzen!

## Container starten obwohl er kein Startskript hat (z.B. Standard CentOS Container)
docker run -d centos tail -f /dev/null

## File vom Host in laufenden Container kopieren
`docker cp test.js 6714d880fad:/tmp/test.js`

## Laufenden Container stoppen
`docker stop 6714d8880fad`

## Alle Docker Container und Images löschen
Container: `docker rm $(docker ps -a -q)`

Images: `docker rmi $(docker images -q)`


# CentOS 8
Es gibt aktuell noch keine Version von Docker für CentOS 8. Die Emulation über "podman-docker" ist ebenfalls nicht geeignet. Stattdessen sollte ein Repository für CentOS 7 eingebunden werden:

`dnf config-manager --add-repo=https://download.docker.com/linux/centos/docker-ce.repo`
`dnf install docker-ce`

Außerdem muss Masquerading für firewalld aktiviert werden, da sonst aus dem Docker-Container keine DNS-Abfragen möglich sind:

`firewall-cmd --zone=public --add-masquerade --permanent`
