# MGS4 stage-selector micro-mod

Replaces Master Collection launcher with a small forwarding executable that
starts registered stage from `Launcher/mgs4-stage-selector.ini` directly with
`MGS4/mgs4.exe --stage CODE`. Original launcher remains beside replacement.
Standalone `mgs4_stage_selector.asi` adds an F6 picker. It shares no code or
state with bbtracker.

```bash
./install.sh
./uninstall.sh
```

Keep Steam launch options unchanged:

```text
WINEDLLOVERRIDES="winmm=n,b" %command%
```

Change stage before launching:

```ini
[fastLoad]
stage=s01a20l
```

In game, press F6, choose named stage, then select **Restart into stage**.
