# Master App Runtime

This directory contains ESP32-specific application wiring for the FarmAuto master.

Current scope:

- Boot and loop integration with `Esp32Base`.
- One active action runtime for the first vertical slice.
- Poll the active station with `GET_STATUS` every `250 ms`.
- Append the terminal action record to the LittleFS action record ring.
- Mark an active action as communication failed after 5 consecutive status poll failures.
- Keep the first LittleFS-backed device registry for business devices and scanned stations.
- Poll enabled stations at low frequency when no action is running, updating online/offline/error state.
- Log action start, poll failures, terminal records, station online/offline transitions and bus scan summaries for hardware bring-up.

Out of scope:

- Multi-action queue.
- Automatic schedule dispatch.
- FRAM pending-action journal.

The current runtime intentionally tracks only one active action. That keeps the visible manual-feed vertical loop complete without introducing the scheduler and queue machinery early.
