# Change Directory Bookmark

This tool allows to `cd` to named directories.

## Implementation

There is a shell front end that calls a binary back end. The front end is merely
there to call `cd` after calling the back end, which does all the logic.

## Installation

Make it so the files from the front end are sourced. These file call the
executables `cdb-back` and `cdb-bc`. Compile the back end executables and either
make links to them in a location in the PATH or move them there and name them
appropriately.

#### Mac OS Caveat

On Mac OS, since El Capitan, terminal sessions are saved by default (this is
like logging every command and result). This results in the following text
appearing in some calls to a subshell (things like `$(...)`):
```
Saving session...
...saving history...truncating history files...
...completed.

```

To avoid this (annoyance), you can create the file `~/.bash_sessions_disable`.
More information [here][mac-1], [here][mac-2], and [here][mac-3].

[mac-1]: https://apple.stackexchange.com/q/301896/
[mac-2]: https://superuser.com/q/950403/
[mac-3]: https://apple.stackexchange.com/q/218731/
