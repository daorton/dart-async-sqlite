dart-async-sqlite
=================

Dart native library for asynchronous access to SQLite.

Features:
* Asynchronous dart SQLite access
* Prepared statement support
* Very rudimentary transaction support
* No dependencies

Basic build instructions. Note only Linux build information is provided.
* make build -- compile c code and create library
* pub upgrade -- create packages drectory and links for dart import
* dart example/test.dart -- run example program creating /tmp/test.sqlite
