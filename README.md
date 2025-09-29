# wrex - wrapped execution

A simple wrapper that will run a provided command and log all output as a JSON payload to syslog.

# Usage

```
./wrex echo "hello, world"

# journalctl -f
Sep 29 11:11:37 host cmdlogger[817516]: {"user":"user","pid":817517,"tty":"/dev/pts/0","fd":"stdout","host":"myhost","message":"hello, world","timestamp":1759158697}
```

# Why?

Wrapping commands like this allows for output to be captured and written to syslog, to be forwarded to a syslog host for monitoring. It also
decorates additional metadata, like the current user, tty (indicates if it was run interactively or not), host, and pid (for tracing).
