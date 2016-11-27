# TCHunt-ng

TCHunt-ng attempts to reveal encrypted files stored on a filesystem. The program is successful in finding **TrueCrypt/VeraCrypt** containers, **EncFS** encrypted files, **PGP/GPG** messages (and keys), and other files made up of random data. The code is based on ideas laid out in the project of *Stephen Judge* named *TCHunt*, hence the name. The original code has aged badly, having unnecessary dependencies and unfixed bugs; a rewrite seemed like a good idea.

TCHunt-ng is a free software licensed under **GPLv3**.

## Methodology

TCHunt-ng performs following tests against content of a file to determine if it is encrypted:

1. Test against a database of well known file-types provided by *libmagic*.
2. *Chi-squared test*.

## Usage

    Usage: tchuntng [options] <file>

    Options:
     -r  recursively traverse a directory
     -v  show version information

## Exit status

TCHunt-ng exits with one of the following exit codes:

* `0` - content of a file is *most likely* encrypted.
* `1` - a generic error occured.
* `2` - no encrypted files were found.

## Requirements

* libmagic >= 5.0

* glibc >= 2.0

## Installation

`make && sudo make install`

## Limitations

Small files (< 13kB) are harder to pick up on. False positives are rare, yet they do happen for files with high enough randomness. TCHunt-ng has no way to tell apart a genuinely encrypted file and a file made up of random data.

## References

https://github.com/stephenjudge/TCHunt

https://github.com/file/file

https://en.wikipedia.org/wiki/Chi-squared_test

