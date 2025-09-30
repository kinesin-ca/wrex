# wrex - wrapped execution

A simple wrapper that will run a provided command and log all output as a JSON payload to syslog.

# Usage

```
./wrex echo "hello, world"

# journalctl -f
Sep 29 11:11:37 host wrex[817516]: {"user":"user","pid":817517,"tty":"/dev/pts/0","fd":"stdout","host":"myhost","message":"hello, world","timestamp":1759158697}
```

# Why?

Wrapping commands like this allows for output to be captured and written to syslog, to be forwarded to a syslog host for monitoring. It also
decorates additional metadata, like the current user, tty (indicates if it was run interactively or not), host, and pid (for tracing).

# Deployment
## rsyslog

### Receiver

1. Enable TCP or UDP reception by uncommenting these lines in `/etc/rsyslog.conf`

For UDP:

```
module(load="imudp")
input(type="imudp" port="514")
```

For TCP:
```

module(load="imtcp")
input(type="imtcp" port="514")
```

2. Configure where to store incoming logs (optional but recommended):

- Create a template configuration file in `/etc/rsyslog.d/remote.conf`

```bash
# Template for organizing logs by hostname
$template RemoteLogs,"/var/log/remote/%HOSTNAME%/%PROGRAMNAME%.log"
*.* ?RemoteLogs
& stop
```

This creates separate directories for each client hostname under /var/log/remote/.

3. Create the log directory:

```
mkdir -p /var/log/remote
chmod 755 /var/log/remote
```

4. Restart rsyslog:
bashsudo systemctl restart rsyslog

```bash

```

### Sender

```bash

apt-get install rsyslog

# Forward all logs to 192.168.1.5 via UDP (@)
echo "*.* @192.168.1.5:514" >> /etc/rsyslog.d/fwdudp.conf

# Forward all logs to 192.168.1.5 via TCP (@@)
echo "*.* @@192.168.1.5:514" >> /etc/rsyslog.d/fwdtcp.conf
```


