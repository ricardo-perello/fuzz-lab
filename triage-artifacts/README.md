# Triage Artifacts

This directory contains the compact AFL++ evidence bundle for the team.

It intentionally excludes full `queue/` corpora, `fastresume.bin`, `fuzz_bitmap`, and other bulky generated files. The goal is to share enough material for crash/hang triage and report evidence without committing complete campaign directories.

## Layout

- `local/`: selected files from local campaigns.
- `pwnbox/`: selected files copied from pwnbox through Docker as root, because the long-run findings directories are root-owned there.

Each copied campaign keeps:

- `cmdline`
- `fuzzer_stats`
- `plot_data`
- saved crash inputs under `crashes/id:*`
- saved hang inputs under `hangs/id:*`

## Included Evidence

- Text API CVE campaign crash inputs and stats.
- Local decoder ASan fork stats and hang inputs.
- Local short QEMU stats.
- Local clean metadata API rerun stats.
- Pwnbox persistent decoder 6h stats, non-reproducing crash inputs, and hang inputs.
- Pwnbox write API 6h stats.
- Pwnbox metadata API 6h stats.
- Pwnbox QEMU long-run stats and hang inputs.
- Pwnbox short QEMU baseline stats.

## Triage Notes

- Treat the 90 text API crash files as likely duplicates until minimized/deduplicated.
- Treat persistent decoder crash files as low-confidence until someone reproduces an ASan fault outside AFL++; previous replay did not reproduce a standalone ASan crash.
- Treat hang inputs as pending findings. They need timeout reproduction, minimization, and root-cause analysis before they are report-worthy.
