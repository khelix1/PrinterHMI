# Encrypted Configuration Backups

PrinterHMI can keep two independent configuration backups on the SD card:

- `config_backup.json` is the normal portable backup. It deliberately omits
  Moonraker API keys.
- `config_backup.phmb` is an optional encrypted backup. It includes configured
  Moonraker API keys inside authenticated encryption.

## Creating an encrypted backup

Open **Settings → Configuration Backup**, choose **ENCRYPT**, enter a
passphrase of 12–96 UTF-8 bytes, then enter it again to confirm. ASCII
passphrases have the same byte and character count; multibyte text reaches the
byte limit sooner. The HMI
shows live progress while it derives the key, encrypts the configuration, and
writes the SD-card file.

The encrypted format uses AES-256-GCM for confidentiality and tamper detection.
Its key is derived with PBKDF2-HMAC-SHA256 using a unique random salt for every
backup and 150,000 iterations. The passphrase is never saved in the backup,
NVS, logs, or a configuration export.

## Restoring

Choose **RESTORE ENCRYPTED**, enter the backup passphrase, and wait for the
verification progress dialog. PrinterHMI authenticates and validates the whole
backup before it presents the destructive restore confirmation. A wrong
passphrase or a changed file is rejected without changing live settings.

After a successful restore, reboot the controller to finish applying all
settings.

## Recovery and deletion

Each backup type is written through a temporary file and a recovery rotation so
a write failure preserves its earlier complete backup. **CLEAR ALL** removes
only plain/encrypted backup files and their temporary or recovery copies from
the SD card. It never removes live printer profiles, API keys, or interface
settings.

## Keep the passphrase

Encrypted backups are portable: they can be restored on a replacement HMI only
when the passphrase is available. PrinterHMI cannot recover or reset a lost
passphrase. Store it in an appropriate password manager before relying on an
encrypted backup for disaster recovery.

## What a backup does not protect

Neither format exports Wi-Fi passwords. CA certificates and additional camera
catalog slots remain panel-local; configure the intended CA after moving to a
replacement panel. Both formats include the primary camera URL, which can itself
contain credentials or tokens. Use encrypted export when that URL is sensitive.
Encrypted SD files do not encrypt the device's ordinary NVS storage.
