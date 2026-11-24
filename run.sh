#!/bin/bash
dosbox -c "mount c $(pwd)/bin" -c "c:" -c "cwsdpmi.exe" -c "match3.exe"
