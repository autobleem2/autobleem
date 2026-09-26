AutoBleem SSH keys
==================

On a console running the AutoBleem kernel (psc-kernel-payload), drop a public key file here
named

    System/ssh/authorized_keys

and reboot: the launcher's boot.sh adds it to dropbear's root account for you, without ever
writing to the console's own storage - only the stick and RAM are touched. Log in as root; a
password is also set on the AutoBleem kernel by default (see its own documentation).

This does nothing on a stock (unmodified) console - there is no SSH server there to use it.
Losing the stick, or reinstalling/updating the launcher, does not remove your key: put the
file back here (or keep a copy of it) and it takes effect at the next reboot.
