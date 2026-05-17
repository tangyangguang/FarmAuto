# Esp32At24cRecordStore

Wear-levelled record store skeleton for AT24C EEPROM devices.

Current status:

- Defines the public configuration, result, inspection, and device interfaces.
- Provides validation, CRC-32/ISO-HDLC helper code, and Writing -> Valid record commits.
- Provides a small `At24cI2cDevice` adapter for 2-byte-address AT24C devices such as AT24C128.
- Splits writes by EEPROM page boundary and transfer size, then uses ACK polling after each page write.
- Does not support AT24C02/04/08/16 special address-bit mapping in the first skeleton.

The core library does not depend on Esp32Base. Applications decide logging, Web diagnostics, configuration persistence, and migration policy.
