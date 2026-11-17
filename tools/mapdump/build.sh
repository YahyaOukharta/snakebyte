#!/bin/sh
# Compiles the official map generator (referee-src, not in this repository: clone
# https://github.com/CodinGame/WinterChallenge2026-Exotec into referee-src/) with MapDump.
# Needs a JDK and the guava / guice jars of a Maven repository (~/.m2).
cd "$(dirname "$0")/../.." || exit 1
SRC=referee-src/WinterChallenge2026-Exotec/src/main/java
CP=$(find ~/.m2 -name "guava-*.jar" | head -1):$(find ~/.m2 -name "guice-[0-9]*.jar" | head -1):$(find ~/.m2 -name "javax.inject-*.jar" | head -1)
mkdir -p build/mapdump
javac -nowarn -cp "$CP" -d build/mapdump $SRC/com/codingame/game/grid/*.java tools/mapdump/MapDump.java 2>&1 | grep -v "^Note"
echo "$CP" > build/mapdump/classpath
