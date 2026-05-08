# Bug Tracker

| # | Name | Description | Module | Severity | Discoverer | Date | Action Taken | Status |
|---|------|-------------|--------|----------|------------|------|--------------|--------|
| 001 | delay-uint16-truncation | `delay_us` casts `us` to `uint16_t`, truncating values > 65535 µs | delay | medium | external audit | 2026-04-21 | fix planned | fixed |
