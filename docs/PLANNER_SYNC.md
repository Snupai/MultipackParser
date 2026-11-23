# Palettenplaner sync (HMI-Abgleich)

The HMI pulls plans marked as finished ("Fertigstände") from the
Palettenplaner server and shows them next to the USB plans. A phone can
propose a plan via the QR code on the HMI ("QR-Code" tab, shown only while a
planner is configured); the operator confirms it on the HMI. Contract: `docs/hmi-api-v1.md` in the `palette_3d_viewer` repository
(HMI API v1).

## Principles

- **Pull only.** The HMI polls the planner over HTTPS. The planner never
  connects to the HMI; no inbound port is opened.
- **No automatic takeover.** Syncing only changes the plan list. The running
  plan changes only when the operator loads a Fertigstand or confirms a
  Planvorschlag. Neither happens while the robot executes a program
  (dashboard program state `PLAYING` or `PAUSED`). A Planvorschlag is also
  not taken over while the robot state is unknown; a manual load from the
  list is (robot off, operator at the machine).
- **Offline first.** Every downloaded Fertigstand stays in the local database;
  the HMI keeps working from those copies when the planner is unreachable.

## Setup on an HMI

1. In the planner: `/admin` → HMIs → create the HMI and download its
   configuration file `hmi-<name>.json` (`serverUrl`, `hmiId`, `token`).
   The token is shown only once.
2. Copy the file to the root of a USB stick. If the planner uses a
   certificate from the factory CA, put the CA certificate (PEM) next to it,
   either as `hmi-<name>.pem` or as the only `*.pem` on the stick.
3. On the HMI: Settings (password) → tab **Planer** → **Von USB importieren**.

The import validates the file (HTTPS URL, UUID, 43-character token) and the
CA (must parse as PEM), then writes `planner.serverUrl`, `planner.hmiId`,
`planner.token` and `planner.caCertPath` to `settings.json` in the working
directory and copies the CA to `planner/planner-ca.pem`. `settings.json` is
saved with owner-only permissions while a token is stored. The token is never
logged or displayed.

**Konfiguration entfernen** clears the `planner` section; downloaded
Fertigstände stay available.

## Security

- HTTPS with certificate verification. The factory CA is added with
  `QSslConfiguration::addCaCertificates`; verification is never disabled.
- Plain `http://` is accepted only for `localhost`, `127.0.0.1` and `::1`
  (development). Any other `http://` configuration is rejected and nothing is
  sent.
- Redirects are not followed (they could carry the bearer token elsewhere).
- Responses are size-limited (JSON 2 MiB, `.rob` 1 MiB).
- The QR code contains only `https://<planner>/hmi/<hmiId>`.

## Sync cycle

Every 10 s, in one cycle:

1. `GET /api/hmi/v1/finals` with `If-None-Match`. On `200` the local list is
   updated; Fertigstände missing from the list are marked withdrawn (hidden,
   never deleted).
2. For every listed Fertigstand not stored yet: `GET …/finals/{id}/rob`,
   check the SHA-256 over the raw bytes, validate with the HMI's own `.rob`
   parser, then store the bytes unchanged plus the parsed palette rows. The
   parser accepts any whitespace between values (tabs from Multipack, spaces
   from the planner) and LF or CRLF. A checksum mismatch is retried in the
   next cycle, a parser rejection after 5 minutes. Either way the list shows
   the entry in red ("Fehler: Plan ungültig, nicht ladbar" / "Prüfsumme
   falsch") instead of "wird geladen".
3. `GET /api/hmi/v1/proposal`. A proposed Fertigstand that is not stored yet
   is downloaded and verified first.

Errors back off: 10 s, 20 s, 40 s, … up to 5 minutes, each multiplied by a
random factor in [0.5, 1.5] (never above 5 minutes). `429` waits for
`Retry-After`. `401` or `404 server_storage_disabled` shows
"HMI nicht freigeschaltet" and retries every 5 minutes. A success resets to
10 s.

`POST /api/hmi/v1/status` reports the running Fertigstand (or `null` for a
USB plan) and the app version after every plan change, also when the robot
loads a plan via XML-RPC, and every 60 s.

## Planvorschlag on the HMI

- The dialog "Neuer Plan vorgeschlagen: {name} – Übernehmen?" with a preview
  opens automatically only while the robot program is stopped. While a
  program runs, the "Betrieb" page shows "Planvorschlag wartet (Roboter läuft)".
- **Übernehmen**: the HMI sends `accepted`; only after the planner answers
  `204`, and only if the robot is still stopped, it loads the plan and reports
  the new status. On `409`/`404` (taken back, superseded, expired) nothing
  changes. On a network error the dialog stays and the operator can retry.
- **Ablehnen** sends `rejected`. **Später** closes the dialog without an
  answer; the proposal can be reopened from the "Betrieb" page.
- The answer request gives up after 8 s, so the operator sees "nicht
  übermittelt" quickly and can retry.
- **Unloadable plan:** when the proposed Fertigstand cannot be loaded on this
  HMI (the `.rob` parser rejects it, or its checksum mismatches twice in a
  row, counted across list and proposal downloads), the HMI answers
  `rejected` by itself (the contract has no other result) so the proposal
  does not stay pending for 24 h, and shows the operator a notice naming the
  plan. Nothing is switched.

## Storage

- Table `planner_finals` (in `paletten.db`): list metadata, `in_list`
  (withdrawn = 0), raw `.rob` bytes.
- Table `planner_state`: the last list ETag.
- Palette rows under the file name `planner:<finalId>.rob`. USB plan lists,
  the plan-name autocompletion and substring lookups never return them;
  GlobalState and the robot's XML-RPC calls load them by exact name.

## Runtime requirements

Qt Network with a TLS backend: the `tls/libqopensslbackend.so` Qt plugin and
the system `libssl3`. The ARM64 bundle ships the plugin under
`plugins/tls/`.
