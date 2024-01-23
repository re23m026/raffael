# TURTLEBOT PROJEKT
APR Project

Ziel: Zunächst soll eine lineare Bewegung ausgeführt werden mit fix definiertem Abstand.
Ab diesem Punkt soll der Roboter eine Kreisbewegung um einen Gegenstand mit fix definiertem Radius absolvieren.

# System-Architektur

Zur Funktionalität werden 3 verschiedene Programme benötigt:
    - OdomClient.cpp
    - RangesClient.cpp
    - CommandServer.cpp

Die ersten beiden fungieren als Client und beziehen Daten vom Server des Roboters. Diese Daten werden über die Shared Memory in einem gemeinsamen Pointer (LidarPtr) gespeichert. Mit Hilfe der hierfür verwendeten Semaphoren ist diese parallele Nutzung der Shared Memory möglich.

Das letztgenannte Programme hingegen fungiert als Server und sendet Befehle an den Turtlebot.
Es kann als "Hauptprogramm" angesehen werden, in welchem auch die eigentliche Pfadplanung stattfindet.
Hierfür wird zunächst auf die relevanten Daten des OdomClients zugegriffen (Über SharedMemory (LidarPtr)) - Da dieser stets aktualisiert wird und neue Daten sendet, können mit Hilfe dieser Ist-Werte die entsprechenden Parameter berechnet werden, um die Soll-Werte zu erreichen.
Dies erfolgt mit Hilfe der geometrischen Beziehungen (Lineare Regelung)

Über die Parameter v (Lineargeschwindigkeit) und w (Winkelgeschwindigkeit) kann der Roboter angesteuert werden.
Diese Parameter werden je nach aktueller Position und Orientierung stets angepasst, wodurch der Roboter neue Fahranweisungen erhält.

# Starten des Programms

Zuächst müsse die drei oben genannten Programme kompiliert werden. Hierzu geht man wie folgt vor:

    - Im Terminal in das entsprechende Verzeichnis der Quelldateien gehen
    - g++ <Programmname> DieWithError.cpp -o <Wunschname-Exe-Datei>
    - Anschließend kann das ausführbare Programme gestartet werden

Bevor der CommandServer im dritten Terminal gestartet werden kann, müssen zuvor sowohl der OdomClient als auch der RangesClient in zwei seperaten Terminals gestartet worden sein.

Zum Starten der ausführbaren Dateien werden zudem Angaben der Server-IP und des entsprechenden Ports benötigt. 
Letzter unterscheidet sich, je nach Progamm:
    - Ranges : 9997
    - Odom : 9998
    - Command : 9999

