# Host Camera AI (report-only foundation)

This service belongs on the Klipper host. It reads the configured camera
directly and publishes summarized state through Moonraker. PrinterHMI never
receives or forwards camera frames.

The first milestone is report-only: `normal`, `check`, or `failure`. Automatic
pause and live parameter changes are intentionally out of scope until the
state contract and confidence history are validated on real prints.
