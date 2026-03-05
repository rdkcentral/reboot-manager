# reboot-manager Documentation

This directory contains technical documentation for reboot-manager.

## Documentation Structure

```text
docs/
├── README.md          # This index
├── reboot-manager-hld.md  # High-level design
├── reboot-manager-lld.md  # Low-level design
├── architecture.md    # Component architecture and data flow
└── testing.md         # Build, unit test, and functional test execution
```

## Document Contents

### 1. Architecture

See [architecture.md](architecture.md) for:
- Module-level responsibility split
- Reboot metadata/log flow
- Reboot execution lifecycle
- Key external interfaces (RBUS, T2, logger, secure wrapper)

### 2. High-Level Design (HLD)

See [reboot-manager-hld.md](reboot-manager-hld.md) for:
- System scope and goals
- End-to-end reboot flow
- Component decomposition
- Reliability and safety model

### 3. Low-Level Design (LLD)

See [reboot-manager-lld.md](reboot-manager-lld.md) for:
- Module and function-level behavior
- Interface contracts
- Cyclic reboot decision logic
- PID guard and RBUS access model

### 4. Build and Test

See [testing.md](testing.md) for:
- Prerequisites
- Autotools build steps
- Unit test and coverage flow
- Functional test execution and reports

## Quick Start

```bash
autoreconf -fi
./configure
make -j$(nproc)
./unit_test.sh
```

## Related Files

- Top-level overview: [../README.md](../README.md)
- Contribution guide: [../CONTRIBUTING.md](../CONTRIBUTING.md)
