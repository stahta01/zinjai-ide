#!/bin/bash
if uname -a | grep x86_64 >/dev/null; then echo l64; else echo l32; fi
