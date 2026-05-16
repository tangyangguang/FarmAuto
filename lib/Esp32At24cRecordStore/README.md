# Esp32At24cRecordStore

Wear-levelled record store skeleton for AT24C EEPROM devices.

Current status:

- Defines the public configuration, result, inspection, and device interfaces.
- Provides validation and CRC-32/ISO-HDLC helper code.
- Does not yet implement full Writing -> Valid record commits.
- Does not support AT24C02/04/08/16 special address-bit mapping in the first skeleton.

The core library does not depend on Esp32Base. Applications decide logging, Web diagnostics, configuration persistence, and migration policy.
