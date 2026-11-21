# Installer and releases

Releases ship one file, `pawse-<version>-x64-setup.exe`, plus a signature for it. Pawse uses the signature to check updates before installing them.

## Building the installer

Install [Inno Setup](https://jrsoftware.org/isinfo.php) 6.3 or later. Build the release preset first (see [src/README.md](../src/README.md#building)), then run this from the repository root:

```powershell
iscc /DAppVersion=0.3.0 installer\pawse.iss
```

The installer is written to `build/installer/`.

## Publishing a release

1. Change the version in `CMakeLists.txt` and commit it.
2. Push a tag named after it, for example `v0.3.0`. The tag must match the version, or the build fails.

The workflow then builds the installer, signs it and publishes the release.

## Release signing

The `release` job in `.github/workflows/build.yml` signs every installer with an ECDSA P-256 key:

- **What's signed:** one line in the form `<sha256 of the installer>  <installer file name>`, followed by a newline. Because the file name contains the version, an older signed installer can't be passed off as a newer one.
- **Signature file:** `<installer file name>.sig`, a DER-encoded signature published next to the installer.
- **Private key:** the `PAWSE_SIGNING_KEY` Actions secret, in PEM form. GitHub doesn't let anyone read it back, so keep a backup in a password manager.
- **Public key:** `release-signing-key.pub.pem` in this folder. The same key is built into Pawse as `kReleaseSigningKey` in `src/updater.cpp`.

To check a release by hand, put the installer and its `.sig` in one folder and create the signed line (Git Bash or Linux):

```sh
printf '%s  %s\n' "$(sha256sum < pawse-0.3.0-x64-setup.exe | cut -d' ' -f1)" pawse-0.3.0-x64-setup.exe > signed-line.txt
```

Then verify it against the public key:

```sh
openssl dgst -sha256 -verify release-signing-key.pub.pem -signature pawse-0.3.0-x64-setup.exe.sig signed-line.txt
```

### Replacing the key

Installed copies of Pawse trust only the key they were built with. If the private key is lost or leaked:

1. Generate a new key.
2. Update the public key here and in `src/updater.cpp`.
3. Update the `PAWSE_SIGNING_KEY` secret.
4. Publish a release.

Existing installs reject updates signed with the new key, so their users have to download that release by hand once.

Generate a new private key:

```sh
openssl ecparam -name prime256v1 -genkey -noout -out release-signing-key.pem
```

Export its public key in PEM form:

```sh
openssl ec -in release-signing-key.pem -pubout -out installer/release-signing-key.pub.pem
```

Print the 64 bytes (X and Y) that go into `kReleaseSigningKey`:

```sh
openssl ec -pubin -in installer/release-signing-key.pub.pem -outform DER | tail -c 64 | od -An -tx1
```

Store the private key as the Actions secret:

```sh
gh secret set PAWSE_SIGNING_KEY < release-signing-key.pem
```
