# AOSV 2018/2019 Final Project

## Description

It's a subsystem that makes the user able to access a file using sessions. In this way every modification is not visible until the session is close. Many processes can access concurrently a given file. The system provides also some statistical information into the /sys/kernel/session-module directory where the user can:

    *See the number of total open sessions
    *See the number of sessions per-process and per-file
    *See and change the path where the user can open sessions

## Documentation

You can check the documentation at this address [Docs](https://federicoalfano.github.io/fs_sessions/html/)

## Author

* Federico Alfano
